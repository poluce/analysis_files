#include "application/algorithm/algorithm_coordinator.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/curve/curve_manager.h"
#include "domain/algorithm/algorithm_result.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>
#include <QMetaType>
#include <QSet>
#include <QVariant>

AlgorithmCoordinator::AlgorithmCoordinator(
    AlgorithmManager* manager, CurveManager* curveManager, AlgorithmContext* context, QObject* parent)
    : QObject(parent)
    , m_algorithmManager(manager)
    , m_curveManager(curveManager)
    , m_context(context)
{
    Q_ASSERT(m_algorithmManager);
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_context);

    qRegisterMetaType<AlgorithmDescriptor>("AlgorithmDescriptor");
    qRegisterMetaType<AlgorithmParameterDefinition>("AlgorithmParameterDefinition");
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");
    qRegisterMetaType<AlgorithmResult>("AlgorithmResult");
    qRegisterMetaType<ResultType>("ResultType");

    // 连接同步执行信号（向后兼容）
    connect(
        m_algorithmManager, &AlgorithmManager::algorithmResultReady, this,
        &AlgorithmCoordinator::onAlgorithmResultReady);

    // 连接异步执行信号
    connect(m_algorithmManager, &AlgorithmManager::algorithmStarted,
            this, &AlgorithmCoordinator::onAsyncAlgorithmStarted);
    connect(m_algorithmManager, &AlgorithmManager::algorithmProgress,
            this, &AlgorithmCoordinator::onAsyncAlgorithmProgress);
    connect(m_algorithmManager, &AlgorithmManager::algorithmFinished,
            this, &AlgorithmCoordinator::onAsyncAlgorithmFinished);
    connect(m_algorithmManager, &AlgorithmManager::algorithmFailed,
            this, &AlgorithmCoordinator::onAsyncAlgorithmFailed);

    qDebug() << "[AlgorithmCoordinator] 已连接异步执行信号";
}

std::optional<AlgorithmDescriptor> AlgorithmCoordinator::descriptorFor(const QString& algorithmName)
{
    // m_algorithmManager 由依赖注入保证非空（构造函数中已 Q_ASSERT）
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (!algorithm) {
        qWarning() << "AlgorithmCoordinator::descriptorFor - 找不到算法" << algorithmName;
        return std::nullopt;
    }

    AlgorithmDescriptor descriptor = algorithm->descriptor();
    if (descriptor.name.isEmpty()) {
        descriptor.name = algorithmName;
    }

    // 若交互需求未显式指定，则依据输入类型推断（向后兼容）
    // 注意：现在所有算法都应该明确定义 needsParameters/needsPointSelection
    // 这里的推断逻辑仅作为向后兼容的兜底方案
    if (!descriptor.needsParameters && !descriptor.needsPointSelection) {
        if (algorithm->inputType() == IThermalAlgorithm::InputType::PointSelection) {
            descriptor.needsPointSelection = true;
            if (descriptor.requiredPointCount <= 0) {
                descriptor.requiredPointCount = 2;  // 默认2个点
            }
        }
    }

    // 注意：纯上下文驱动模式下，算法通过 descriptor() 提供所有元数据
    // descriptor.parameters 已经包含完整的参数定义和默认值

    return descriptor;
}

void AlgorithmCoordinator::handlePointSelectionResult(const QVector<ThermalDataPoint>& points)
{
    // 向后兼容方法：调用新的 submitPoints()
    qDebug() << "[AlgorithmCoordinator] handlePointSelectionResult (向后兼容) - 转发到 submitPoints()";
    submitPoints(points);
}

void AlgorithmCoordinator::cancelPendingRequest()
{
    // 向后兼容方法：调用新的 cancel()
    qDebug() << "[AlgorithmCoordinator] cancelPendingRequest (向后兼容) - 转发到 cancel()";
    cancel();
}

void AlgorithmCoordinator::saveResultToContext(
    const QString& algorithmName, const AlgorithmResult& result)
{
    // Store the entire result object in context
    m_context->setValue(OutputKeys::latestResult(algorithmName),
                       QVariant::fromValue(result),
                       QStringLiteral("AlgorithmCoordinator"));

    // Store the result type for quick access
    m_context->setValue(
        OutputKeys::resultType(algorithmName),
        QVariant::fromValue(static_cast<int>(result.type())),
        QStringLiteral("AlgorithmCoordinator"));
}

void AlgorithmCoordinator::onAlgorithmResultReady(
    const QString& algorithmName, const AlgorithmResult& result)
{
    saveResultToContext(algorithmName, result);
    emit algorithmSucceeded(algorithmName);
}

void AlgorithmCoordinator::resetState()
{
    m_pending.reset();
    m_currentTaskId.clear();
}

void AlgorithmCoordinator::handleError(const QString& algorithmName, const QString& reason)
{
    qWarning() << "[AlgorithmCoordinator] 错误:" << algorithmName << "-" << reason;
    resetState();  // 自动清理所有状态
    emit algorithmFailed(algorithmName, reason);
}

void AlgorithmCoordinator::onAsyncAlgorithmStarted(const QString& taskId, const QString& algorithmName)
{
    qDebug() << "[AlgorithmCoordinator] 异步任务开始执行:" << algorithmName << "taskId:" << taskId;

    // 转发信号到 UI 层（用于显示进度对话框等）
    emit algorithmStarted(taskId, algorithmName);
}

void AlgorithmCoordinator::onAsyncAlgorithmProgress(const QString& taskId, int percentage, const QString& message)
{
    // 转发进度信号到 UI 层
    emit algorithmProgress(taskId, percentage, message);

    // 可选：调试日志（避免过度输出）
    if (percentage % 20 == 0) {  // 每20%输出一次
        qDebug() << "[AlgorithmCoordinator] 任务进度:" << taskId << percentage << "%" << message;
    }
}

void AlgorithmCoordinator::onAsyncAlgorithmFinished(
    const QString& taskId, const QString& algorithmName, const AlgorithmResult& result, qint64 elapsedMs)
{
    qDebug() << "[AlgorithmCoordinator] 异步任务完成:" << algorithmName
             << "taskId:" << taskId << "耗时:" << elapsedMs << "ms";

    // 清除所有状态（统一清理）
    if (m_currentTaskId == taskId) {
        resetState();
    }

    // 保存结果到上下文（复用统一方法）
    saveResultToContext(algorithmName, result);

    // 发出成功信号
    emit algorithmSucceeded(algorithmName);

    qDebug() << "[AlgorithmCoordinator] 结果已保存到上下文，算法成功完成";
}

void AlgorithmCoordinator::onAsyncAlgorithmFailed(
    const QString& taskId, const QString& algorithmName, const QString& errorMessage)
{
    qWarning() << "[AlgorithmCoordinator] 异步任务失败:" << algorithmName
               << "taskId:" << taskId << "错误:" << errorMessage;

    // 清除所有状态（统一清理）
    if (m_currentTaskId == taskId) {
        resetState();
    }

    // 发出失败信号
    emit algorithmFailed(algorithmName, errorMessage);
}

void AlgorithmCoordinator::run(const QString& algorithmName)
{
    qDebug() << "[AlgorithmCoordinator] 执行算法:" << algorithmName;

    // 1. 获取算法的自描述信息
    auto descriptorOpt = descriptorFor(algorithmName);
    if (!descriptorOpt.has_value()) {
        handleError(algorithmName, "找不到算法或获取描述符失败");
        return;
    }

    AlgorithmDescriptor descriptor = descriptorOpt.value();

    // 2. 创建待处理请求
    PendingRequest pending;
    pending.algorithmName = algorithmName;
    pending.descriptor = descriptor;
    pending.currentStepIndex = 0;
    m_pending = pending;

    qDebug() << "[AlgorithmCoordinator] 算法描述符:";
    qDebug() << "  needsParameters:" << descriptor.needsParameters;
    qDebug() << "  needsPointSelection:" << descriptor.needsPointSelection;
    qDebug() << "  interactionOrder:" << descriptor.interactionOrder;

    // 3. 开始处理交互流程
    processNextStep();
}

void AlgorithmCoordinator::processNextStep()
{
    if (!m_pending.has_value()) {
        qWarning() << "[AlgorithmCoordinator] processNextStep - 没有待处理的请求";
        return;
    }

    auto& pending = m_pending.value();
    auto& descriptor = pending.descriptor;

    // 构建交互顺序
    QStringList order = descriptor.interactionOrder;
    if (order.isEmpty()) {
        // 默认顺序：先参数，后点选
        if (descriptor.needsParameters) {
            order.append("parameters");
        }
        if (descriptor.needsPointSelection) {
            order.append("points");
        }
    }

    qDebug() << "[AlgorithmCoordinator] processNextStep - 当前步骤索引:" << pending.currentStepIndex
             << "/" << order.size();

    // 检查是否所有交互都完成
    if (pending.currentStepIndex >= order.size()) {
        qDebug() << "[AlgorithmCoordinator] 所有交互完成，执行算法";
        execute();
        return;
    }

    // 处理当前步骤
    QString currentStep = order[pending.currentStepIndex];
    qDebug() << "[AlgorithmCoordinator] 当前步骤:" << currentStep;

    if (currentStep == "parameters") {
        emit requestParameterDialog(pending.algorithmName, descriptor);

    } else if (currentStep == "points") {
        emit requestPointSelection(
            pending.algorithmName,
            descriptor.requiredPointCount,
            descriptor.pointSelectionHint);

    } else {
        qWarning() << "[AlgorithmCoordinator] 未知的交互步骤类型:" << currentStep;
        handleError(pending.algorithmName, "未知的交互步骤类型: " + currentStep);
    }
}

void AlgorithmCoordinator::submitParameters(const QVariantMap& parameters)
{
    if (!m_pending.has_value()) {
        qWarning() << "[AlgorithmCoordinator] submitParameters - 没有待处理的请求";
        return;
    }

    qDebug() << "[AlgorithmCoordinator] 收到参数提交:" << parameters;

    // 保存参数
    m_pending->parameters = parameters;

    // 移动到下一步
    m_pending->currentStepIndex++;
    processNextStep();
}

void AlgorithmCoordinator::submitPoints(const QVector<ThermalDataPoint>& points)
{
    if (!m_pending.has_value()) {
        qWarning() << "[AlgorithmCoordinator] submitPoints - 没有待处理的请求";
        return;
    }

    qDebug() << "[AlgorithmCoordinator] 收到点选提交:" << points.size() << "个点";

    // 验证点数
    int required = m_pending->descriptor.requiredPointCount;
    if (points.size() < required) {
        QString error = QString("需要至少 %1 个点，实际只有 %2 个").arg(required).arg(points.size());
        handleError(m_pending->algorithmName, error);
        return;
    }

    // 保存点选结果
    m_pending->points = points;

    // 移动到下一步
    m_pending->currentStepIndex++;
    processNextStep();
}

void AlgorithmCoordinator::execute()
{
    if (!m_pending.has_value()) {
        qWarning() << "[AlgorithmCoordinator] execute - 没有待处理的请求";
        return;
    }

    auto& pending = m_pending.value();

    // 获取活动曲线
    ThermalCurve* curve = m_curveManager->getActiveCurve();
    if (!curve) {
        handleError(pending.algorithmName, "没有活动曲线");
        return;
    }

    qDebug() << "[AlgorithmCoordinator] 执行算法:" << pending.algorithmName;
    qDebug() << "  活动曲线:" << curve->id();
    qDebug() << "  参数数量:" << pending.parameters.size();
    qDebug() << "  选点数量:" << pending.points.size();

    // 注入数据到上下文（使用 executeAlgorithm 中的逻辑）
    // 清空旧数据并注入新数据
    m_context->setValue(ContextKeys::ActiveCurve, QVariant::fromValue(*curve), "AlgorithmCoordinator");

    // 自动查找并注入活动曲线的基线
    QVector<ThermalCurve*> baselines = m_curveManager->getBaselines(curve->id());
    if (!baselines.isEmpty()) {
        m_context->setValue(ContextKeys::BaselineCurves, QVariant::fromValue(baselines), "AlgorithmCoordinator");
        qDebug() << "[AlgorithmCoordinator] 找到" << baselines.size() << "条基线曲线";
    } else {
        m_context->remove(ContextKeys::BaselineCurves);
    }

    // 注入参数
    for (auto it = pending.parameters.constBegin(); it != pending.parameters.constEnd(); ++it) {
        m_context->setValue(QString("param.%1").arg(it.key()), it.value(), "AlgorithmCoordinator");
    }

    // 注入选点
    if (!pending.points.isEmpty()) {
        m_context->setValue(ContextKeys::SelectedPoints, QVariant::fromValue(pending.points), "AlgorithmCoordinator");
    }

    // 保存历史记录
    m_context->setValue(QStringLiteral("history/%1/lastParameters").arg(pending.algorithmName),
                       pending.parameters, "AlgorithmCoordinator");
    if (!pending.points.isEmpty()) {
        m_context->setValue(QStringLiteral("history/%1/lastPoints").arg(pending.algorithmName),
                           QVariant::fromValue(pending.points), "AlgorithmCoordinator");
    }

    // 提交到异步队列
    QString taskId = m_algorithmManager->executeAsync(pending.algorithmName, m_context);

    if (taskId.isEmpty()) {
        handleError(pending.algorithmName, "算法提交失败");
        return;
    }

    m_currentTaskId = taskId;
    qDebug() << "[AlgorithmCoordinator] 算法已提交到异步队列，taskId =" << taskId;

    // 清空待处理请求（任务已提交）
    m_pending.reset();
}

void AlgorithmCoordinator::cancel()
{
    qDebug() << "[AlgorithmCoordinator] 取消当前操作";

    // 1. 取消待处理的交互请求
    if (m_pending.has_value()) {
        const QString algorithmName = m_pending->algorithmName;
        resetState();
        emit showMessage(QStringLiteral("已取消算法 %1 的操作").arg(algorithmName));
    }

    // 2. 取消正在执行的异步任务
    if (!m_currentTaskId.isEmpty()) {
        bool cancelled = m_algorithmManager->cancelTask(m_currentTaskId);
        if (cancelled) {
            qDebug() << "[AlgorithmCoordinator] 任务取消成功:" << m_currentTaskId;
            resetState();
            emit showMessage(QStringLiteral("已取消正在执行的算法任务"));
        } else {
            qWarning() << "[AlgorithmCoordinator] 任务取消失败（任务可能已完成）:" << m_currentTaskId;
        }
    }
}

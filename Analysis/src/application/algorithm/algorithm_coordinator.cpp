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
#include <QUuid>
#include <QDateTime>

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

    // 连接同步执行信号
    connect(
        m_algorithmManager, &AlgorithmManager::algorithmResultReady, this,
        &AlgorithmCoordinator::onSyncAlgorithmResultReady);

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

    // 若交互需求未显式指定，则依据输入类型推断
    // 注意：现在所有算法都应该明确定义 needsParameters/needsPointSelection
    // 这里的推断逻辑仅作为默认行为的兜底方案
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
    // 方法别名：转发到 submitPoints()
    qDebug() << "[AlgorithmCoordinator] handlePointSelectionResult - 转发到 submitPoints()";
    submitPoints(points);
}

void AlgorithmCoordinator::cancelPendingRequest()
{
    // 方法别名：转发到 cancel()
    qDebug() << "[AlgorithmCoordinator] cancelPendingRequest - 转发到 cancel()";
    cancel();
}

void AlgorithmCoordinator::onSyncAlgorithmResultReady(
    const QString& algorithmName, const AlgorithmResult& result)
{
    // 1. 生成 taskId（算法名 + 时间戳 + UUID）
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QString taskId = QString("%1-%2-%3").arg(algorithmName).arg(timestamp).arg(uuid);

    qDebug() << "[AlgorithmCoordinator] 同步算法完成:" << algorithmName << "taskId:" << taskId;

    // 2. 获取父曲线ID
    QString parentCurveId = result.parentCurveId();

    // 3. 保存结果到上下文
    m_context->saveResult(taskId, algorithmName, parentCurveId, result);

    // 4. 发射 algorithmCompleted 信号
    emit algorithmCompleted(taskId, algorithmName, parentCurveId, result);

    // 5. 推进工作流（如有）
    advanceWorkflow(taskId, algorithmName, result);
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

    // 获取父曲线ID
    QString parentCurveId = result.parentCurveId();

    // 保存结果到上下文
    m_context->saveResult(taskId, algorithmName, parentCurveId, result);

    // 发射 algorithmCompleted 信号
    emit algorithmCompleted(taskId, algorithmName, parentCurveId, result);

    // 推进工作流（如有）
    advanceWorkflow(taskId, algorithmName, result);

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

    // 检查算法依赖（工作流支持）
    if (!checkPrerequisites(pending.algorithmName)) {
        handleError(pending.algorithmName, "依赖检查失败");
        return;
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

// ========== 工作流实现 ==========

QString AlgorithmCoordinator::runWorkflow(const QStringList& steps, const QStringList& curveIds)
{
    if (steps.isEmpty()) {
        qWarning() << "[AlgorithmCoordinator] runWorkflow - 步骤列表为空";
        return QString();
    }

    if (curveIds.isEmpty()) {
        qWarning() << "[AlgorithmCoordinator] runWorkflow - 输入曲线列表为空";
        return QString();
    }

    if (m_currentWorkflow.has_value()) {
        qWarning() << "[AlgorithmCoordinator] runWorkflow - 已有工作流正在运行，请先取消或等待完成";
        return QString();
    }

    // 生成工作流 ID
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    QString workflowId = QString("workflow-%1-%2").arg(timestamp).arg(uuid);

    // 初始化工作流状态
    PendingWorkflow workflow;
    workflow.id = workflowId;
    workflow.steps = steps;
    workflow.currentStepIndex = 0;
    workflow.inputCurveIds = curveIds;
    workflow.status = WorkflowStatus::Running;

    m_currentWorkflow = workflow;

    qInfo() << "[AlgorithmCoordinator] 工作流启动:"
            << "id=" << workflowId
            << "步骤数=" << steps.size()
            << "输入曲线=" << curveIds.size();

    // 执行第一步算法
    QString firstAlgorithm = steps.first();
    QString firstInputCurve = curveIds.first();

    qDebug() << "[AlgorithmCoordinator] 工作流执行步骤 1/" << steps.size() << ": " << firstAlgorithm;
    run(firstAlgorithm);

    return workflowId;
}

void AlgorithmCoordinator::cancelWorkflow(const QString& workflowId)
{
    if (!m_currentWorkflow.has_value()) {
        qDebug() << "[AlgorithmCoordinator] cancelWorkflow - 无活动工作流";
        return;
    }

    if (m_currentWorkflow->id != workflowId) {
        qWarning() << "[AlgorithmCoordinator] cancelWorkflow - 工作流ID不匹配:"
                   << "请求=" << workflowId
                   << "当前=" << m_currentWorkflow->id;
        return;
    }

    // 标记状态为已取消
    m_currentWorkflow->status = WorkflowStatus::Cancelled;
    m_currentWorkflow->errorMessage = "用户取消";

    qInfo() << "[AlgorithmCoordinator] 工作流已取消:" << workflowId;

    // 发射失败信号
    emit workflowFailed(workflowId, "用户取消");

    // 清理工作流状态
    m_currentWorkflow.reset();
}

void AlgorithmCoordinator::advanceWorkflow(
    const QString& taskId,
    const QString& algorithmName,
    const AlgorithmResult& result)
{
    if (!m_currentWorkflow.has_value()) {
        return;  // 无活动工作流
    }

    auto& wf = m_currentWorkflow.value();

    // 验证算法名匹配当前步骤
    if (wf.currentStepIndex >= wf.steps.size()) {
        return;  // 已超出步骤范围
    }

    QString expectedAlgorithm = wf.steps[wf.currentStepIndex];
    if (algorithmName != expectedAlgorithm) {
        qDebug() << "[AlgorithmCoordinator] advanceWorkflow - 算法名不匹配，跳过:"
                 << "期望=" << expectedAlgorithm
                 << "实际=" << algorithmName;
        return;  // 算法名不匹配
    }

    // 记录当前步骤的输出曲线ID
    QStringList outputCurveIds;
    if (result.hasCurves()) {
        for (const ThermalCurve& curve : result.curves()) {
            outputCurveIds.append(curve.id());
        }
    }
    wf.stepOutputs[wf.currentStepIndex] = outputCurveIds;

    qDebug() << "[AlgorithmCoordinator] 工作流步骤完成:"
             << "taskId=" << taskId
             << "步骤" << (wf.currentStepIndex + 1) << "/" << wf.steps.size()
             << "算法=" << algorithmName
             << "输出曲线数=" << outputCurveIds.size();

    // 推进到下一步
    wf.currentStepIndex++;

    // 检查是否完成
    if (wf.currentStepIndex >= wf.steps.size()) {
        // 工作流完成
        wf.status = WorkflowStatus::Completed;
        QString wfId = wf.id;
        QStringList finalOutputs = wf.stepOutputs[wf.steps.size() - 1];

        emit workflowCompleted(wfId, finalOutputs);
        m_currentWorkflow.reset();

        qInfo() << "[AlgorithmCoordinator] 工作流完成:" << wfId
                << "最终输出曲线数=" << finalOutputs.size();
        return;
    }

    // 执行下一步
    QString nextAlgorithm = wf.steps[wf.currentStepIndex];
    QString nextInputCurve = outputCurveIds.isEmpty() ? wf.inputCurveIds.first() : outputCurveIds.first();

    qInfo() << "[AlgorithmCoordinator] 工作流推进:"
            << "步骤" << (wf.currentStepIndex + 1) << "/" << wf.steps.size()
            << "算法=" << nextAlgorithm
            << "输入曲线=" << nextInputCurve;

    run(nextAlgorithm);
}

bool AlgorithmCoordinator::checkPrerequisites(const QString& algorithmName)
{
    // 1. 获取算法描述符
    auto descriptor = descriptorFor(algorithmName);
    if (!descriptor.has_value()) {
        qWarning() << "[AlgorithmCoordinator] checkPrerequisites - 算法不存在:" << algorithmName;
        emit showMessage(QStringLiteral("算法 %1 不存在").arg(algorithmName));
        return false;
    }

    const AlgorithmDescriptor& desc = descriptor.value();

    // 2. 检查每个 prerequisite
    for (const QString& prerequisite : desc.prerequisites) {
        if (!m_context->contains(prerequisite)) {
            QString message = QStringLiteral("算法 %1 缺少必需依赖: %2")
                                .arg(algorithmName)
                                .arg(prerequisite);
            qWarning() << "[AlgorithmCoordinator] checkPrerequisites -" << message;
            emit showMessage(message);
            return false;
        }
    }

    qDebug() << "[AlgorithmCoordinator] checkPrerequisites - 算法" << algorithmName
             << "的所有依赖已满足，共" << desc.prerequisites.size() << "项";
    return true;
}

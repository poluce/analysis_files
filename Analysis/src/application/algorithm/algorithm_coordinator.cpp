#include "application/algorithm/algorithm_coordinator.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/algorithm/metadata_descriptor_registry.h"
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

    // 若交互类型未显式指定，则依据输入类型推断
    // 注意：现在所有算法都应该明确定义 descriptor.interaction，
    // 这里的推断逻辑仅作为向后兼容的兜底方案
    if (descriptor.interaction == AlgorithmInteraction::None) {
        if (algorithm->inputType() == IThermalAlgorithm::InputType::PointSelection) {
            descriptor.interaction = AlgorithmInteraction::PointSelection;
        }
        // 其他情况保持 None，不再根据 parameters 自动推断
        // 原因：算法明确设置 interaction = None 表示无需用户交互，
        // descriptor.parameters 中的参数可能只是内部可选配置，不应强制弹窗
    }

    // 默认的点选提示/数量回退
    if (descriptor.interaction == AlgorithmInteraction::PointSelection && descriptor.requiredPointCount <= 0) {
        descriptor.requiredPointCount = 1;
    }

    // 注意：纯上下文驱动模式下，算法通过 descriptor() 提供所有元数据
    // descriptor.parameters 已经包含完整的参数定义和默认值

    return descriptor;
}

void AlgorithmCoordinator::handlePointSelectionResult(const QVector<ThermalDataPoint>& points)
{
    // ==================== 元数据驱动路径的点选处理 ====================
    if (!m_metadataPending.has_value()) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 无待处理的点选请求";
        return;
    }

    if (m_metadataPending->phase != PendingPhase::AwaitPoints) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 当前阶段不需要点选";
        return;
    }

    if (points.size() < m_metadataPending->requiredPointCount) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 点数量不足，期待"
                   << m_metadataPending->requiredPointCount << "个，实际" << points.size();
        handleError(m_metadataPending->algorithmName, QStringLiteral("选点数量不足"));
        m_metadataPending.reset();
        return;
    }

    // 保存选择的点
    m_metadataPending->collectedPoints = points;

    // 获取曲线
    ThermalCurve* curve = m_curveManager->getCurve(m_metadataPending->curveId);
    if (!curve) {
        handleError(m_metadataPending->algorithmName, "找不到目标曲线");
        m_metadataPending.reset();
        return;
    }

    // 获取算法描述符
    auto descriptor = descriptorFor(m_metadataPending->algorithmName);
    if (!descriptor.has_value()) {
        handleError(m_metadataPending->algorithmName, "无法获取算法描述符");
        m_metadataPending.reset();
        return;
    }

    // 执行算法
    executeAlgorithm(descriptor.value(), curve, m_metadataPending->parameters, m_metadataPending->collectedPoints);

    // 清除待处理请求
    m_metadataPending.reset();
}

void AlgorithmCoordinator::cancelPendingRequest()
{
    // 1. 取消待处理的交互请求（参数收集、点选等）
    if (m_metadataPending.has_value()) {
        const QString algorithmName = m_metadataPending->algorithmName;
        resetState();
        emit showMessage(QStringLiteral("已取消算法 %1 的待处理操作").arg(algorithmName));
    }

    // 2. 取消正在执行的异步任务
    if (!m_currentTaskId.isEmpty()) {
        qDebug() << "[AlgorithmCoordinator] 尝试取消正在执行的任务:" << m_currentTaskId;

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

bool AlgorithmCoordinator::ensurePrerequisites(const AlgorithmDescriptor& descriptor, ThermalCurve* curve)
{
    Q_UNUSED(curve);
    for (const QString& key : descriptor.prerequisites) {
        if (!m_context->contains(key)) {
            qWarning() << "AlgorithmCoordinator::ensurePrerequisites - 缺少上下文键" << key;
            return false;
        }
    }
    return true;
}

bool AlgorithmCoordinator::populateDefaultParameters(const AlgorithmDescriptor& descriptor, QVariantMap& parameters) const
{
    bool allResolved = true;
    for (const auto& param : descriptor.parameters) {
        if (parameters.contains(param.key)) {
            continue;
        }
        if (param.defaultValue.isValid()) {
            parameters.insert(param.key, param.defaultValue);
        } else if (param.required) {
            allResolved = false;
        }
    }
    return allResolved;
}

void AlgorithmCoordinator::executeAlgorithm(
    const AlgorithmDescriptor& descriptor, ThermalCurve* curve, const QVariantMap& parameters, const QVector<ThermalDataPoint>& points)
{
    if (!curve) {
        handleError(descriptor.name, QStringLiteral("曲线数据无效"));
        return;
    }

    // 验证算法是否注册
    if (!m_algorithmManager->getAlgorithm(descriptor.name)) {
        qWarning() << "AlgorithmCoordinator::executeAlgorithm - 找不到算法实例:" << descriptor.name;
        handleError(descriptor.name, QStringLiteral("算法未注册"));
        return;
    }

    // 将主曲线设置到上下文（存储副本以确保线程安全）
    // 当上下文被克隆时，ThermalCurve 会被深拷贝，确保工作线程拥有独立的数据副本
    // setValue() 会自动覆盖旧值，无需手动清空
    m_context->setValue(ContextKeys::ActiveCurve, QVariant::fromValue(*curve), "AlgorithmCoordinator");

    // 自动查找并注入活动曲线的基线（如果存在）
    QVector<ThermalCurve*> baselines = m_curveManager->getBaselines(curve->id());
    if (!baselines.isEmpty()) {
        // 注入所有基线，由算法自己决定如何使用
        m_context->setValue(ContextKeys::BaselineCurves, QVariant::fromValue(baselines), "AlgorithmCoordinator");
        qDebug() << "AlgorithmCoordinator::executeAlgorithm - 找到" << baselines.size()
                 << "条基线曲线，由算法决定使用哪条";
    } else {
        // 清除之前可能存在的基线（避免使用过期数据）
        m_context->remove(ContextKeys::BaselineCurves);
    }

    // 将参数设置到上下文（setValue 自动覆盖旧值）
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        m_context->setValue(QString("param.%1").arg(it.key()), it.value(), "AlgorithmCoordinator");
    }

    // 将选择的点设置到上下文（setValue 自动覆盖旧值）
    if (!points.isEmpty()) {
        m_context->setValue(ContextKeys::SelectedPoints, QVariant::fromValue(points), "AlgorithmCoordinator");
    }

    // 保存历史记录
    m_context->setValue(QStringLiteral("history/%1/lastParameters").arg(descriptor.name), parameters, "AlgorithmCoordinator");
    if (!points.isEmpty()) {
        m_context->setValue(QStringLiteral("history/%1/lastPoints").arg(descriptor.name), QVariant::fromValue(points), "AlgorithmCoordinator");
    }

    qDebug() << "[AlgorithmCoordinator] 提交算法" << descriptor.name;
    qDebug() << "  上下文包含:" << m_context->keys().size() << "个键";
    qDebug() << "  参数数量:" << parameters.size();
    qDebug() << "  选点数量:" << points.size();

    // 使用异步执行接口（单线程模式，FIFO队列）
    QString taskId = m_algorithmManager->executeAsync(descriptor.name, m_context);

    if (taskId.isEmpty()) {
        qCritical() << "[AlgorithmCoordinator] executeAsync 返回空 taskId，执行失败！";
        handleError(descriptor.name, QStringLiteral("算法提交失败"));
        return;
    }

    // 保存任务ID，用于未来可能的取消操作
    m_currentTaskId = taskId;

    qDebug() << "[AlgorithmCoordinator] 算法已提交到异步队列，taskId =" << taskId;
}

void AlgorithmCoordinator::resetState()
{
    m_metadataPending.reset();
    m_currentTaskId.clear();
}

void AlgorithmCoordinator::handleError(const QString& algorithmName, const QString& reason)
{
    qWarning() << "[AlgorithmCoordinator] 错误:" << algorithmName << "-" << reason;
    resetState();  // 自动清理所有状态
    emit algorithmFailed(algorithmName, reason);
}

// ==================== 异步执行槽函数实现 ====================

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

// ==================== 元数据驱动流程实现（方案B）====================

void AlgorithmCoordinator::runByName(const QString& algorithmName)
{
    qDebug() << "[AlgorithmCoordinator] 元数据驱动执行算法:" << algorithmName;

    // 1. 检查注册表中是否有元数据描述
    auto& registry = App::AlgorithmDescriptorRegistry::instance();
    if (!registry.has(algorithmName)) {
        handleError(algorithmName, "元数据注册表中未找到算法描述");
        return;
    }

    // 2. 获取活动曲线
    ThermalCurve* curve = m_curveManager->getActiveCurve();
    if (!curve) {
        handleError(algorithmName, "没有活动曲线");
        return;
    }

    // 3. 获取元数据描述
    App::AlgorithmDescriptor metaDesc = registry.get(algorithmName);

    // 4. 初始化元数据待处理请求
    MetadataPendingRequest pending;
    pending.algorithmName = algorithmName;
    pending.curveId = curve->id();

    // 5. 检查是否需要选点
    if (metaDesc.pointSelection.has_value()) {
        pending.needsPointSelection = true;
        pending.requiredPointCount = metaDesc.pointSelection->minCount;
        pending.pointSelectionHint = metaDesc.pointSelection->hint;
    }

    // 6. 检查是否需要参数
    bool needsParameters = !metaDesc.params.isEmpty();

    if (needsParameters) {
        // 需要参数：保存待处理请求，发出参数对话框请求
        pending.phase = PendingPhase::AwaitParameters;
        m_metadataPending = pending;

        // 将描述符转换为 QVariant 传递
        QVariant descriptorVariant;
        descriptorVariant.setValue(metaDesc);
        emit requestGenericParameterDialog(algorithmName, descriptorVariant);
        qDebug() << "[AlgorithmCoordinator] 已请求参数对话框";
    } else if (pending.needsPointSelection) {
        // 不需要参数但需要选点：直接进入选点阶段
        pending.phase = PendingPhase::AwaitPoints;
        m_metadataPending = pending;

        emit requestPointSelection(algorithmName, curve->id(), pending.requiredPointCount, pending.pointSelectionHint);
        qDebug() << "[AlgorithmCoordinator] 已请求选点:" << pending.requiredPointCount << "个点";
    } else {
        // 既不需要参数也不需要选点：直接执行
        qDebug() << "[AlgorithmCoordinator] 无需参数和选点，直接执行";
        QVariantMap emptyParams;
        QVector<ThermalDataPoint> emptyPoints;

        // 使用现有的 executeAlgorithm，但需要构造一个 AlgorithmDescriptor
        // 从 AlgorithmManager 获取算法的 descriptor
        auto descriptor = descriptorFor(algorithmName);
        if (!descriptor.has_value()) {
            handleError(algorithmName, "无法获取算法描述符");
            return;
        }
        executeAlgorithm(descriptor.value(), curve, emptyParams, emptyPoints);
    }
}

void AlgorithmCoordinator::handleGenericParameterSubmission(const QString& algorithmName, const QVariantMap& parameters)
{
    qDebug() << "[AlgorithmCoordinator] 收到通用参数对话框提交:" << algorithmName;
    qDebug() << "[AlgorithmCoordinator] 参数:" << parameters;

    // 1. 检查是否有待处理的请求
    if (!m_metadataPending.has_value() || m_metadataPending->algorithmName != algorithmName) {
        qWarning() << "[AlgorithmCoordinator] 没有匹配的待处理请求或算法名不匹配";
        return;
    }

    // 2. 保存参数
    m_metadataPending->parameters = parameters;

    // 3. 检查是否需要选点
    if (m_metadataPending->needsPointSelection) {
        // 需要选点：进入选点阶段
        m_metadataPending->phase = PendingPhase::AwaitPoints;
        emit requestPointSelection(
            algorithmName,
            m_metadataPending->curveId,
            m_metadataPending->requiredPointCount,
            m_metadataPending->pointSelectionHint);
        qDebug() << "[AlgorithmCoordinator] 参数已收集，进入选点阶段";
    } else {
        // 不需要选点：直接执行
        qDebug() << "[AlgorithmCoordinator] 参数已收集，无需选点，直接执行";

        // 获取曲线
        ThermalCurve* curve = m_curveManager->getCurve(m_metadataPending->curveId);
        if (!curve) {
            handleError(algorithmName, "找不到目标曲线");
            m_metadataPending.reset();
            return;
        }

        // 获取算法描述符
        auto descriptor = descriptorFor(algorithmName);
        if (!descriptor.has_value()) {
            handleError(algorithmName, "无法获取算法描述符");
            m_metadataPending.reset();
            return;
        }

        // 执行算法
        QVector<ThermalDataPoint> emptyPoints;
        executeAlgorithm(descriptor.value(), curve, m_metadataPending->parameters, emptyPoints);

        // 清除待处理请求
        m_metadataPending.reset();
    }
}

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

    // 若交互类型未显式指定，则依据输入类型/参数回退
    if (descriptor.interaction == AlgorithmInteraction::None) {
        switch (algorithm->inputType()) {
        case IThermalAlgorithm::InputType::PointSelection:
            descriptor.interaction = AlgorithmInteraction::PointSelection;
            break;
        case IThermalAlgorithm::InputType::None:
            descriptor.interaction = descriptor.parameters.isEmpty() ? AlgorithmInteraction::None : AlgorithmInteraction::ParameterDialog;
            break;
        default:
            descriptor.interaction = AlgorithmInteraction::ParameterDialog;
            break;
        }
    }

    // 默认的点选提示/数量回退
    if (descriptor.interaction == AlgorithmInteraction::PointSelection && descriptor.requiredPointCount <= 0) {
        descriptor.requiredPointCount = 1;
    }

    // 注意：纯上下文驱动模式下，算法通过 descriptor() 提供所有元数据
    // descriptor.parameters 已经包含完整的参数定义和默认值

    return descriptor;
}

void AlgorithmCoordinator::handleAlgorithmTriggered(const QString& algorithmName, const QVariantMap& presetParameters)
{
    if (algorithmName.isEmpty()) {
        return;
    }

    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        emit algorithmFailed(algorithmName, QStringLiteral("没有选中的曲线，无法执行算法"));
        return;
    }

    auto descriptorOpt = descriptorFor(algorithmName);
    if (!descriptorOpt.has_value()) {
        emit algorithmFailed(algorithmName, QStringLiteral("找不到算法或算法描述信息"));
        return;
    }

    const AlgorithmDescriptor descriptor = descriptorOpt.value();

    if (!ensurePrerequisites(descriptor, activeCurve)) {
        emit algorithmFailed(algorithmName, QStringLiteral("算法前置条件未满足"));
        return;
    }

    QVariantMap parameters = presetParameters;
    const bool hasPresetParameters = !presetParameters.isEmpty();

    switch (descriptor.interaction) {
    case AlgorithmInteraction::None: {
        QVariantMap effectiveParams = parameters;
        populateDefaultParameters(descriptor, effectiveParams);
        executeAlgorithm(descriptor, activeCurve, effectiveParams, {});
        break;
    }
    case AlgorithmInteraction::ParameterDialog: {
        QVariantMap effectiveParams = parameters;
        const bool autoExecutable = populateDefaultParameters(descriptor, effectiveParams);

        // 如果参数就绪（自动填充或预设），直接执行
        if ((autoExecutable && !hasPresetParameters) || hasPresetParameters) {
            executeAlgorithm(descriptor, activeCurve, effectiveParams, {});
        } else {
            // 需要用户输入参数
            PendingRequest request;
            request.descriptor = descriptor;
            request.curveId = activeCurve->id();
            request.parameters = effectiveParams;
            request.phase = PendingPhase::AwaitParameters;
            m_pending = request;
            emit requestParameterDialog(descriptor.name, descriptor.parameters, effectiveParams);
        }
        break;
    }
    case AlgorithmInteraction::PointSelection: {
        PendingRequest request;
        request.descriptor = descriptor;
        request.curveId = activeCurve->id();
        request.parameters = parameters;
        request.pointsRequired = qMax(1, descriptor.requiredPointCount);
        request.phase = PendingPhase::AwaitPoints;
        m_pending = request;
        emit requestPointSelection(descriptor.name, request.curveId, request.pointsRequired, descriptor.pointSelectionHint);
        break;
    }
    case AlgorithmInteraction::ParameterThenPoint: {
        PendingRequest request;
        request.descriptor = descriptor;
        request.curveId = activeCurve->id();
        request.parameters = parameters;
        request.pointsRequired = qMax(1, descriptor.requiredPointCount);

        const bool paramsReady = populateDefaultParameters(descriptor, request.parameters);

        // 如果参数就绪（自动填充或预设），进入点选阶段
        if ((paramsReady && !hasPresetParameters) || hasPresetParameters) {
            request.phase = PendingPhase::AwaitPoints;
            m_pending = request;
            emit requestPointSelection(descriptor.name, request.curveId, request.pointsRequired, descriptor.pointSelectionHint);
        } else {
            // 需要用户输入参数
            request.phase = PendingPhase::AwaitParameters;
            m_pending = request;
            emit requestParameterDialog(descriptor.name, descriptor.parameters, request.parameters);
        }
        break;
    }
    }
}

void AlgorithmCoordinator::handleParameterSubmission(const QString& algorithmName, const QVariantMap& parameters)
{
    if (!m_pending.has_value() || m_pending->descriptor.name != algorithmName) {
        qWarning() << "AlgorithmCoordinator::handleParameterSubmission - 没有匹配的待处理请求";
        return;
    }

    m_pending->parameters = parameters;

    if (m_pending->descriptor.interaction == AlgorithmInteraction::ParameterDialog) {
        ThermalCurve* curve = m_curveManager->getCurve(m_pending->curveId);
        if (!curve) {
            emit algorithmFailed(algorithmName, QStringLiteral("无法获取曲线，算法终止"));
            resetPending();
            return;
        }
        executeAlgorithm(m_pending->descriptor, curve, parameters, {});
        resetPending();
        return;
    }

    if (m_pending->descriptor.interaction == AlgorithmInteraction::ParameterThenPoint) {
        m_pending->phase = PendingPhase::AwaitPoints;
        emit requestPointSelection(
            m_pending->descriptor.name, m_pending->curveId, m_pending->pointsRequired, m_pending->descriptor.pointSelectionHint);
        return;
    }
}

void AlgorithmCoordinator::handlePointSelectionResult(const QVector<ThermalDataPoint>& points)
{
    if (!m_pending.has_value()) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 无待处理的点选请求";
        return;
    }

    if (m_pending->phase != PendingPhase::AwaitPoints) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 当前阶段不需要点选";
        return;
    }

    if (points.size() < m_pending->pointsRequired) {
        qWarning() << "AlgorithmCoordinator::handlePointSelectionResult - 点数量不足，期待" << m_pending->pointsRequired
                   << "个，实际" << points.size();
        emit algorithmFailed(m_pending->descriptor.name, QStringLiteral("选点数量不足"));
        resetPending();
        return;
    }

    m_pending->collectedPoints = points;
    ThermalCurve* curve = m_curveManager->getCurve(m_pending->curveId);
    if (!curve) {
        emit algorithmFailed(m_pending->descriptor.name, QStringLiteral("无法获取曲线，算法终止"));
        resetPending();
        return;
    }

    executeAlgorithm(m_pending->descriptor, curve, m_pending->parameters, m_pending->collectedPoints);
    resetPending();
}

void AlgorithmCoordinator::cancelPendingRequest()
{
    // 1. 取消待处理的交互请求（参数收集、点选等）
    if (m_pending.has_value()) {
        const QString algorithmName = m_pending->descriptor.name;
        resetPending();
        emit showMessage(QStringLiteral("已取消算法 %1 的待处理操作").arg(algorithmName));
    }

    // 2. 取消正在执行的异步任务
    if (!m_currentTaskId.isEmpty()) {
        qDebug() << "[AlgorithmCoordinator] 尝试取消正在执行的任务:" << m_currentTaskId;

        bool cancelled = m_algorithmManager->cancelTask(m_currentTaskId);
        if (cancelled) {
            qDebug() << "[AlgorithmCoordinator] 任务取消成功:" << m_currentTaskId;
            emit showMessage(QStringLiteral("已取消正在执行的算法任务"));
            m_currentTaskId.clear();
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
        emit algorithmFailed(descriptor.name, QStringLiteral("曲线数据无效"));
        return;
    }

    // 验证算法是否注册
    if (!m_algorithmManager->getAlgorithm(descriptor.name)) {
        qWarning() << "AlgorithmCoordinator::executeAlgorithm - 找不到算法实例:" << descriptor.name;
        emit algorithmFailed(descriptor.name, QStringLiteral("算法未注册"));
        return;
    }

    // 清空上下文中的算法相关数据，准备新的执行
    m_context->remove(ContextKeys::ActiveCurve);
    m_context->remove(ContextKeys::BaselineCurves);
    m_context->remove(ContextKeys::SelectedPoints);
    QStringList paramKeys = m_context->keys("param.");
    for (const QString& key : paramKeys) {
        m_context->remove(key);
    }

    // 将主曲线设置到上下文（存储副本以确保线程安全）
    // 当上下文被克隆时，ThermalCurve 会被深拷贝，确保工作线程拥有独立的数据副本
    m_context->setValue(ContextKeys::ActiveCurve, QVariant::fromValue(*curve), "AlgorithmCoordinator");

    // 自动查找并注入活动曲线的基线（如果存在）
    QVector<ThermalCurve*> baselines = m_curveManager->getBaselines(curve->id());

    if (!baselines.isEmpty()) {
        // 注入所有基线，由算法自己决定如何使用
        m_context->setValue(ContextKeys::BaselineCurves, QVariant::fromValue(baselines), "AlgorithmCoordinator");

        qDebug() << "AlgorithmCoordinator::executeAlgorithm - 找到" << baselines.size()
                 << "条基线曲线，由算法决定使用哪条";
    } else {
        // 确保清除之前可能存在的基线
        m_context->remove(ContextKeys::BaselineCurves);
    }

    // 将参数设置到上下文（使用 param. 前缀）
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        m_context->setValue(QString("param.%1").arg(it.key()), it.value(), "AlgorithmCoordinator");
    }

    // 将选择的点设置到上下文（如果有）
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
        emit algorithmFailed(descriptor.name, QStringLiteral("算法提交失败"));
        return;
    }

    // 保存任务ID，用于未来可能的取消操作
    m_currentTaskId = taskId;

    qDebug() << "[AlgorithmCoordinator] 算法已提交到异步队列，taskId =" << taskId;
}

void AlgorithmCoordinator::resetPending() { m_pending.reset(); }

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

    // 清除任务ID
    if (m_currentTaskId == taskId) {
        m_currentTaskId.clear();
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

    // 清除任务ID
    if (m_currentTaskId == taskId) {
        m_currentTaskId.clear();
    }

    // 发出失败信号
    emit algorithmFailed(algorithmName, errorMessage);
}

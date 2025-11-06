#include "application/algorithm/algorithm_coordinator.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/curve/curve_manager.h"
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

    connect(
        m_algorithmManager, &AlgorithmManager::algorithmResultReady, this,
        &AlgorithmCoordinator::onAlgorithmResultReady);
}

std::optional<AlgorithmDescriptor> AlgorithmCoordinator::descriptorFor(const QString& algorithmName)
{
    if (!m_algorithmManager) {
        return std::nullopt;
    }

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
    if (descriptor.pointSelectionHint.isEmpty()) {
        descriptor.pointSelectionHint = algorithm->userPrompt();
    }

    // 合并算法参数图中的默认值
    const QVariantMap currentParams = algorithm->parameters();
    if (!currentParams.isEmpty()) {
        // 第一步：更新已定义参数的默认值，并记录已存在的参数键
        QSet<QString> definedKeys;
        for (int i = 0; i < descriptor.parameters.size(); ++i) {
            auto& param = descriptor.parameters[i];
            definedKeys.insert(param.key);  // 记录键（O(1) 插入）

            if (currentParams.contains(param.key)) {
                const QVariant value = currentParams.value(param.key);
                if (!param.defaultValue.isValid()) {
                    param.defaultValue = value;
                }
                if (param.valueType == QVariant::Invalid) {
                    param.valueType = static_cast<QVariant::Type>(value.type());
                }
            }
        }

        // 第二步：添加未定义的参数（使用 QSet 进行 O(1) 查找）
        for (auto it = currentParams.constBegin(); it != currentParams.constEnd(); ++it) {
            if (!definedKeys.contains(it.key())) {  // O(1) 查找
                AlgorithmParameterDefinition def;
                def.key = it.key();
                def.label = it.key();
                def.valueType = static_cast<QVariant::Type>(it.value().type());
                def.defaultValue = it.value();
                def.required = false;
                descriptor.parameters.append(def);
            }
        }
    }

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

void AlgorithmCoordinator::handlePointSelectionResult(const QVector<QPointF>& points)
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
    if (!m_pending.has_value()) {
        return;
    }
    const QString algorithmName = m_pending->descriptor.name;
    resetPending();
    emit showMessage(QStringLiteral("已取消算法 %1 的待处理操作").arg(algorithmName));
}

void AlgorithmCoordinator::onAlgorithmResultReady(
    const QString& algorithmName, IThermalAlgorithm::OutputType outputType, const QVariant& result)
{
    m_context->setValue(QStringLiteral("result/%1/latest").arg(algorithmName), result, QStringLiteral("AlgorithmCoordinator"));
    m_context->setValue(
        QStringLiteral("result/%1/outputType").arg(algorithmName), QVariant::fromValue(static_cast<int>(outputType)),
        QStringLiteral("AlgorithmCoordinator"));
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
    const AlgorithmDescriptor& descriptor, ThermalCurve* curve, const QVariantMap& parameters, const QVector<QPointF>& points)
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
    m_context->remove("activeCurve");
    m_context->remove("mainCurve");
    m_context->remove("selectedPoints");
    QStringList paramKeys = m_context->keys("param.");
    for (const QString& key : paramKeys) {
        m_context->remove(key);
    }

    // 将主曲线设置到上下文
    m_context->setValue("activeCurve", QVariant::fromValue(curve), "AlgorithmCoordinator");

    // 将参数设置到上下文（使用 param. 前缀）
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        m_context->setValue(QString("param.%1").arg(it.key()), it.value(), "AlgorithmCoordinator");
    }

    // 将选择的点设置到上下文（如果有）
    if (!points.isEmpty()) {
        m_context->setValue("selectedPoints", QVariant::fromValue(points), "AlgorithmCoordinator");
    }

    // 保存历史记录
    m_context->setValue(QStringLiteral("history/%1/lastParameters").arg(descriptor.name), parameters, "AlgorithmCoordinator");
    if (!points.isEmpty()) {
        m_context->setValue(QStringLiteral("history/%1/lastPoints").arg(descriptor.name), QVariant::fromValue(points), "AlgorithmCoordinator");
    }

    qDebug() << "AlgorithmCoordinator::executeAlgorithm - 提交算法" << descriptor.name;
    qDebug() << "  上下文包含:" << m_context->keys().size() << "个键";
    qDebug() << "  参数数量:" << parameters.size();
    qDebug() << "  选点数量:" << points.size();

    // 使用上下文驱动的执行接口
    m_algorithmManager->executeWithContext(descriptor.name, m_context);
}

void AlgorithmCoordinator::resetPending() { m_pending.reset(); }

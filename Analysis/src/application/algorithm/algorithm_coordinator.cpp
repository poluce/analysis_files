#include "application/algorithm/algorithm_coordinator.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/curve/curve_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>
#include <QMetaType>

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
        for (int i = 0; i < descriptor.parameters.size(); ++i) {
            auto& param = descriptor.parameters[i];
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

        for (auto it = currentParams.constBegin(); it != currentParams.constEnd(); ++it) {
            bool exists = false;
            for (const auto& definedParam : descriptor.parameters) {
                if (definedParam.key == it.key()) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
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
        if (autoExecutable && !hasPresetParameters) {
            executeAlgorithm(descriptor, activeCurve, effectiveParams, {});
        } else if (hasPresetParameters) {
            executeAlgorithm(descriptor, activeCurve, effectiveParams, {});
        } else {
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
        if (paramsReady && !hasPresetParameters) {
            request.phase = PendingPhase::AwaitPoints;
            m_pending = request;
            emit requestPointSelection(descriptor.name, request.curveId, request.pointsRequired, descriptor.pointSelectionHint);
        } else if (hasPresetParameters) {
            request.phase = PendingPhase::AwaitPoints;
            m_pending = request;
            emit requestPointSelection(descriptor.name, request.curveId, request.pointsRequired, descriptor.pointSelectionHint);
        } else {
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

    // 在提交算法前，将参数应用到算法实例（兼容旧接口依赖 setParameter 的实现）
    if (IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(descriptor.name)) {
        if (!parameters.isEmpty()) {
            algorithm->setParameter(parameters);
        }
    } else {
        qWarning() << "AlgorithmCoordinator::executeAlgorithm - 找不到算法实例:" << descriptor.name;
        emit algorithmFailed(descriptor.name, QStringLiteral("算法未注册"));
        return;
    }

    QVariantMap inputs = parameters;
    inputs.insert(QStringLiteral("mainCurve"), QVariant::fromValue(curve));
    if (!points.isEmpty()) {
        inputs.insert(QStringLiteral("points"), QVariant::fromValue(points));
    }

    qDebug() << "AlgorithmCoordinator::executeAlgorithm - 提交算法" << descriptor.name << "参数:" << inputs.keys();
    m_context->setValue(QStringLiteral("history/%1/lastParameters").arg(descriptor.name), parameters, QStringLiteral("AlgorithmCoordinator"));
    if (!points.isEmpty()) {
        m_context->setValue(QStringLiteral("history/%1/lastPoints").arg(descriptor.name), QVariant::fromValue(points), QStringLiteral("AlgorithmCoordinator"));
    }

    m_algorithmManager->executeWithInputs(descriptor.name, inputs);
}

void AlgorithmCoordinator::resetPending() { m_pending.reset(); }

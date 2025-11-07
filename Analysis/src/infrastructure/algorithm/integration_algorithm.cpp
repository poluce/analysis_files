#include "integration_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <QtGlobal>

IntegrationAlgorithm::IntegrationAlgorithm()
{
    qDebug() << "构造: IntegrationAlgorithm";
}

QString IntegrationAlgorithm::name() const
{
    return "integration";
}

QString IntegrationAlgorithm::displayName() const
{
    return "积分";
}

QString IntegrationAlgorithm::category() const
{
    return "Analysis";
}

SignalType IntegrationAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 积分算法将微分信号转换回原始信号（适用所有仪器类型）
    if (inputType == SignalType::Derivative) {
        return SignalType::Raw;
    }
    // 如果输入已经是原始信号，则保持不变
    return inputType;
}

IThermalAlgorithm::InputType IntegrationAlgorithm::inputType() const
{
    // A类算法：单曲线，无需用户交互
    return InputType::None;
}

IThermalAlgorithm::OutputType IntegrationAlgorithm::outputType() const
{
    // 输出新曲线（积分曲线）
    return OutputType::Curve;
}

AlgorithmDescriptor IntegrationAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::None;  // 简单算法，无需交互
    // 暂无可配置参数，预留扩展（如方法/归一化等）
    return desc;
}

// ==================== 上下文驱动执行接口实现 ====================

bool IntegrationAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "IntegrationAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "IntegrationAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 积分算法暂无可配置参数，预留扩展
    // 未来可以添加：积分方法（梯形/辛普森）、归一化选项等

    qDebug() << "IntegrationAlgorithm::prepareContext - 数据就绪";
    return true;
}

AlgorithmResult IntegrationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("integration", "上下文为空");
    }

    // 2. 拉取曲线
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("integration", "无法获取活动曲线");
    }

    ThermalCurve* inputCurve = curve.value();

    // 3. 获取输入数据
    const QVector<ThermalDataPoint>& inputData = inputCurve->getProcessedData();

    // 4. 执行核心算法逻辑（梯形法则积分）
    QVector<ThermalDataPoint> outputData;
    const int n = inputData.size();
    if (n == 0) {
        qWarning() << "IntegrationAlgorithm::executeWithContext - 输入数据为空！";
        return AlgorithmResult::failure("integration", "输入数据为空");
    }

    outputData.resize(n);
    double cum = 0.0;

    // 第一个点的积分为0
    outputData[0] = inputData[0];
    outputData[0].value = 0.0;

    for (int i = 1; i < n; ++i) {
        const auto& p0 = inputData[i - 1];
        const auto& p1 = inputData[i];
        const double dx = (p1.temperature - p0.temperature);
        if (!qFuzzyIsNull(dx)) {
            const double area = 0.5 * (p0.value + p1.value) * dx; // 梯形法则
            cum += area;
        }
        outputData[i] = p1;
        outputData[i].value = cum;
    }

    qDebug() << "IntegrationAlgorithm::executeWithContext - 完成，输出数据点数:" << outputData.size();

    // 5. 创建结果对象
    AlgorithmResult result = AlgorithmResult::success(
        "integration",
        inputCurve->id(),
        ResultType::Curve
    );

    // 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), displayName());
    outputCurve.setProcessedData(outputData);
    outputCurve.setInstrumentType(inputCurve->instrumentType());
    outputCurve.setSignalType(getOutputSignalType(inputCurve->signalType()));
    outputCurve.setParentId(inputCurve->id());
    outputCurve.setProjectName(inputCurve->projectName());
    outputCurve.setMetadata(inputCurve->getMetadata());

    // 填充结果
    result.setCurve(outputCurve);
    result.setMeta("method", "Trapezoidal");
    result.setMeta("label", "积分曲线");

    return result;
}

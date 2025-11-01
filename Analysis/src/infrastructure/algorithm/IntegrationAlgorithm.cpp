#include "IntegrationAlgorithm.h"
#include "domain/model/ThermalDataPoint.h"
#include <QtGlobal>

QVector<ThermalDataPoint> IntegrationAlgorithm::process(const QVector<ThermalDataPoint>& inputData)
{
    QVector<ThermalDataPoint> out;
    const int n = inputData.size();
    if (n == 0) return out;

    out.resize(n);
    double cum = 0.0;

    // 第一个点的积分为0
    out[0] = inputData[0];
    out[0].value = 0.0;

    for (int i = 1; i < n; ++i) {
        const auto& p0 = inputData[i-1];
        const auto& p1 = inputData[i];
        const double dx = (p1.temperature - p0.temperature);
        if (!qFuzzyIsNull(dx)) {
            const double area = 0.5 * (p0.value + p1.value) * dx; // 梯形法则
            cum += area;
        }
        out[i] = p1;
        out[i].value = cum;
    }

    return out;
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

QVariantMap IntegrationAlgorithm::parameters() const
{
    // 暂无可配置参数，预留扩展（如方法/归一化等）
    return QVariantMap();
}

void IntegrationAlgorithm::setParameter(const QString& key, const QVariant& value)
{
    Q_UNUSED(key);
    Q_UNUSED(value);
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


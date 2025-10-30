#include "DifferentiationAlgorithm.h"
#include "domain/model/ThermalDataPoint.h"
#include <QDebug>

QVector<ThermalDataPoint> DifferentiationAlgorithm::process(const QVector<ThermalDataPoint>& inputData)
{
    QVector<ThermalDataPoint> outputData;
    if (inputData.size() < 2) {
        qWarning() << "Differentiation requires at least 2 data points.";
        return outputData;
    }

    outputData.reserve(inputData.size() - 1);

    for (int i = 0; i < inputData.size() - 1; ++i) {
        const auto& p1 = inputData[i];
        const auto& p2 = inputData[i+1];

        double temp_diff = p2.temperature - p1.temperature;
        if (qFuzzyCompare(temp_diff, 0.0)) {
            // 避免除以零
            continue;
        }

        double value_diff = p2.value - p1.value;
        double derivative = value_diff / temp_diff;

        // 导数位于温度区间的中点
        double new_temp = (p1.temperature + p2.temperature) / 2.0;
        
        ThermalDataPoint new_point;
        new_point.temperature = new_temp;
        new_point.value = derivative;
        new_point.time = (p1.time + p2.time) / 2.0;

        outputData.append(new_point);
    }

    return outputData;
}

QString DifferentiationAlgorithm::name() const
{
    return "differentiation";
}

QString DifferentiationAlgorithm::category() const
{
    return "Analysis";
}

QVariantMap DifferentiationAlgorithm::parameters() const
{
    // 暂时没有参数
    return QVariantMap();
}

void DifferentiationAlgorithm::setParameter(const QString& key, const QVariant& value)
{
    // 暂时没有参数
    Q_UNUSED(key);
    Q_UNUSED(value);
}

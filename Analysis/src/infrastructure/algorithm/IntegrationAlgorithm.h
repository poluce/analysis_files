#ifndef INTEGRATIONALGORITHM_H
#define INTEGRATIONALGORITHM_H

#include "domain/algorithm/IThermalAlgorithm.h"

// 数值积分算法（对信号随温度的曲线进行累计积分）。
// 使用梯形法则，输出为同长度的累积积分曲线：
//   out[i].value = ∫(x0->xi) y(x) dx
class IntegrationAlgorithm : public IThermalAlgorithm
{
public:
    QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) override;
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    QVariantMap parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;
    SignalType getOutputSignalType(SignalType inputType) const override;
};

#endif // INTEGRATIONALGORITHM_H


#ifndef INTEGRATIONALGORITHM_H
#define INTEGRATIONALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 数值积分算法（对信号随温度的曲线进行累计积分）。
// 使用梯形法则，输出为同长度的累积积分曲线：
//   out[i].value = ∫(x0->xi) y(x) dx
class IntegrationAlgorithm : public IThermalAlgorithm {
public:
    // 旧接口方法
    IntegrationAlgorithm();
    QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) override;
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    QVariantMap parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;
    void setParameter(const QVariantMap& params) override;
    SignalType getOutputSignalType(SignalType inputType) const override;

    // 新接口方法（A类算法：单曲线，无交互）
    InputType inputType() const override;
    OutputType outputType() const override;
};

#endif // INTEGRATIONALGORITHM_H

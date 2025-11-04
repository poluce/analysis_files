#ifndef DIFFERENTIATIONALGORITHM_H
#define DIFFERENTIATIONALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

/**
 * @brief 微分算法类 - 基于DTG大窗口平滑中心差分法
 * @details 使用前后各halfWin个点的和之差计算导数
 *          derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 */
class DifferentiationAlgorithm : public IThermalAlgorithm {
public:
    // 旧接口方法
    DifferentiationAlgorithm();
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

private:
    int m_halfWin = 50;         // DTG半窗口大小，默认50点
    double m_dt = 0.1;          // 虚拟时间步长，默认0.1
    bool m_enableDebug = false; // 是否启用调试输出
};

#endif // DIFFERENTIATIONALGORITHM_H

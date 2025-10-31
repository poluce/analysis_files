#ifndef DIFFERENTIATIONALGORITHM_H
#define DIFFERENTIATIONALGORITHM_H

#include "domain/algorithm/IThermalAlgorithm.h"

/**
 * @brief 微分算法类 - 基于DTG大窗口平滑中心差分法
 * @details 使用前后各halfWin个点的和之差计算导数
 *          derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 */
class DifferentiationAlgorithm : public IThermalAlgorithm
{
public:
    QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) override;
    QString name() const override;
    QString category() const override;
    QVariantMap parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;

private:
    int m_halfWin = 50;          // DTG半窗口大小，默认50点
    double m_dt = 0.1;           // 虚拟时间步长，默认0.1
    bool m_enableDebug = false;  // 是否启用调试输出
};

#endif // DIFFERENTIATIONALGORITHM_H

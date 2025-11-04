#ifndef MOVINGAVERAGEFILTERALGORITHM_H
#define MOVINGAVERAGEFILTERALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 移动平均滤波算法（简单平滑）。
// - 参数：window（窗口大小，奇数更佳），默认 5。
// - 思路：对每个点，取其两侧窗口范围内的值做平均，
//         起始与末尾处自动缩小窗口以避免越界。
class MovingAverageFilterAlgorithm : public IThermalAlgorithm {
public:
    // 旧接口方法
    MovingAverageFilterAlgorithm();
    QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) override;
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    QVariantMap parameters() const override;
    void setParameter(const QString& key, const QVariant& value) override;
    void setParameter(const QVariantMap& input) override;
    SignalType getOutputSignalType(SignalType inputType) const override;

    // 新接口方法（A类算法：单曲线，无交互）
    InputType inputType() const override;
    OutputType outputType() const override;

private:
    int m_window = 5; // 滤波窗口大小（点数）
};

#endif // MOVINGAVERAGEFILTERALGORITHM_H

#ifndef MOVINGAVERAGEFILTERALGORITHM_H
#define MOVINGAVERAGEFILTERALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 移动平均滤波算法 - 简单平滑
 * @details 对每个点，取其两侧窗口范围内的值做平均，
 *          起始与末尾处自动缩小窗口以避免越界。
 *          参数：window（窗口大小，奇数更佳），默认 500。
 */
class MovingAverageFilterAlgorithm : public IThermalAlgorithm {
public:
    MovingAverageFilterAlgorithm();

    // 核心接口方法
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    InputType inputType() const override;
    OutputType outputType() const override;
    AlgorithmDescriptor descriptor() const override;

    // 曲线属性声明接口
    bool isAuxiliaryCurve() const override;
    bool isStronglyBound() const override;

    // 上下文驱动执行接口
    bool prepareContext(AlgorithmContext* context) override;
    AlgorithmResult executeWithContext(AlgorithmContext* context) override;

private:
    int m_window = 500; // 滤波窗口大小（点数）
};

#endif // MOVINGAVERAGEFILTERALGORITHM_H

#ifndef DIFFERENTIATIONALGORITHM_H
#define DIFFERENTIATIONALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 微分算法类 - 基于DTG大窗口平滑中心差分法
 * @details 使用前后各halfWin个点的和之差计算导数
 *          derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 */
class DifferentiationAlgorithm : public IThermalAlgorithm {
public:
    DifferentiationAlgorithm();

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

    // 上下文驱动执行接口（两阶段执行）
    bool prepareContext(AlgorithmContext* context) override;
    AlgorithmResult executeWithContext(AlgorithmContext* context) override;

private:
    int m_halfWin = 50;         // DTG半窗口大小，默认50点
    double m_dt = 0.1;          // 虚拟时间步长，默认0.1
    bool m_enableDebug = false; // 是否启用调试输出
};

#endif // DIFFERENTIATIONALGORITHM_H

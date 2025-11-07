#ifndef INTEGRATIONALGORITHM_H
#define INTEGRATIONALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 数值积分算法 - 对信号随温度的曲线进行累计积分
 * @details 使用梯形法则，输出为同长度的累积积分曲线：
 *          out[i].value = ∫(x0->xi) y(x) dx
 */
class IntegrationAlgorithm : public IThermalAlgorithm {
public:
    IntegrationAlgorithm();

    // 核心接口方法
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    SignalType getOutputSignalType(SignalType inputType) const override;
    InputType inputType() const override;
    OutputType outputType() const override;
    AlgorithmDescriptor descriptor() const override;

    // 上下文驱动执行接口
    bool prepareContext(AlgorithmContext* context) override;
    AlgorithmResult executeWithContext(AlgorithmContext* context) override;
};

#endif // INTEGRATIONALGORITHM_H

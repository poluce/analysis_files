#ifndef PEAKAAREAALGORITHM_H
#define PEAK_AREA_ALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 峰面积计算算法 (B类算法)
 *
 * 功能：
 * - 用户选择2个点定义积分范围
 * - 使用梯形积分法计算两点之间的峰面积
 * - 输出结果：
 *   1. 标注点（起点、终点）
 *   2. 文本标签（显示峰面积数值）
 *
 * 使用场景：
 * - TGA曲线峰面积计算
 * - DSC曲线峰面积计算
 * - 质量损失计算
 * - 焓变计算
 *
 * 梯形积分公式：
 * A = Σ [(y[i] + y[i+1]) / 2] * (x[i+1] - x[i])
 */
class PeakAreaAlgorithm : public IThermalAlgorithm {
public:
    PeakAreaAlgorithm();

    // 核心接口方法
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    SignalType getOutputSignalType(SignalType inputType) const override;
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
    /**
     * @brief 使用梯形积分法计算峰面积
     *
     * @param curveData 曲线数据
     * @param temp1 起始温度
     * @param temp2 终止温度
     * @return 峰面积值
     */
    double calculateArea(const QVector<ThermalDataPoint>& curveData, double temp1, double temp2) const;

    /**
     * @brief 格式化峰面积输出文本
     *
     * @param area 峰面积值
     * @param instrumentType 仪器类型（用于确定单位）
     * @return 格式化的文本字符串
     */
    QString formatAreaText(double area, InstrumentType instrumentType) const;
};

#endif // PEAK_AREA_ALGORITHM_H

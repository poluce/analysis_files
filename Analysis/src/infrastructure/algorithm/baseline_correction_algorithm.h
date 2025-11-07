#ifndef BASELINECORRECTIONALGORITHM_H
#define BASELINECORRECTIONALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 基线校正算法 (B类算法)
 *
 * 功能：
 * - 用户选择2个点定义基线范围
 * - 在两点之间进行线性插值生成基线曲线
 * - 可选择输出：
 *   1. 基线曲线本身
 *   2. 扣除基线后的曲线（原始曲线 - 基线）
 *
 * 使用场景：
 * - TGA曲线基线校正
 * - DSC曲线基线校正
 * - 去除系统漂移
 */
class BaselineCorrectionAlgorithm : public IThermalAlgorithm {
public:
    BaselineCorrectionAlgorithm();

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
    QVariant executeWithContext(AlgorithmContext* context) override;

private:
    /**
     * @brief 在两点之间生成线性基线
     *
     * @param curveData 原始曲线数据
     * @param point1 起点（温度，值）
     * @param point2 终点（温度，值）
     * @return 基线曲线数据
     */
    QVector<ThermalDataPoint>
    generateBaseline(const QVector<ThermalDataPoint>& curveData, const QPointF& point1, const QPointF& point2) const;

    /**
     * @brief 从曲线数据中找到最接近指定温度的点
     *
     * @param curveData 曲线数据
     * @param temperature 目标温度
     * @return 最接近的数据点
     */
    ThermalDataPoint findNearestPoint(const QVector<ThermalDataPoint>& curveData, double temperature) const;
};

#endif // BASELINECORRECTIONALGORITHM_H

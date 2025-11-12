#ifndef TEMPERATURE_EXTRAPOLATION_ALGORITHM_H
#define TEMPERATURE_EXTRAPOLATION_ALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 温度外推算法 (B类算法)
 *
 * 功能：
 * - 计算 Onset（起始温度）或 Endset（结束温度）
 * - 用户选择至少3个点：
 *   1. 前2个点定义切线区域
 *   2. 第3个点定义基线水平
 * - 算法拟合切线并计算与基线的交点（外推温度）
 *
 * 使用场景：
 * - TGA 起始分解温度
 * - DSC 熔融起始温度
 * - 反应开始/结束温度的精确确定
 *
 * 算法流程：
 * 1. 使用最小二乘法拟合切线（前2个点附近的数据）
 * 2. 确定基线水平（第3个点的 Y 值）
 * 3. 计算切线与基线的交点（外推温度）
 * 4. 输出：外推点标注 + 切线 + 数值
 *
 * 切线拟合公式：
 * y = k * x + b
 * 其中 k 为斜率，b 为截距
 */
class TemperatureExtrapolationAlgorithm : public IThermalAlgorithm {
public:
    TemperatureExtrapolationAlgorithm();

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
     * @brief 线性回归拟合切线
     *
     * 使用最小二乘法拟合直线 y = k*x + b
     *
     * @param points 用于拟合的点集
     * @param slope 输出：斜率 k
     * @param intercept 输出：截距 b
     * @return true - 拟合成功，false - 拟合失败（点数不足或数据异常）
     */
    bool fitTangentLine(const QVector<ThermalDataPoint>& points,
                        double& slope,
                        double& intercept) const;

    /**
     * @brief 计算切线与基线的交点
     *
     * @param slope 切线斜率
     * @param intercept 切线截距
     * @param baselineY 基线的 Y 值
     * @return 外推温度（交点的 X 坐标）
     */
    double calculateIntersection(double slope,
                                  double intercept,
                                  double baselineY) const;

    /**
     * @brief 提取拟合区域的数据点
     *
     * 提取用户选择的2个切线点附近的曲线数据，用于拟合
     *
     * @param curveData 完整的曲线数据
     * @param point1 切线区域起点
     * @param point2 切线区域终点
     * @return 拟合区域的数据点
     */
    QVector<ThermalDataPoint> extractFittingRegion(
        const QVector<ThermalDataPoint>& curveData,
        const ThermalDataPoint& point1,
        const ThermalDataPoint& point2) const;

    /**
     * @brief 格式化外推温度输出文本
     *
     * @param temperature 外推温度
     * @param instrumentType 仪器类型
     * @return 格式化的文本字符串
     */
    QString formatTemperatureText(double temperature,
                                   InstrumentType instrumentType) const;
};

#endif // TEMPERATURE_EXTRAPOLATION_ALGORITHM_H

#ifndef TEMPERATURE_EXTRAPOLATION_ALGORITHM_H
#define TEMPERATURE_EXTRAPOLATION_ALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"

// 前置声明
class AlgorithmContext;

/**
 * @brief 温度外推算法 (B类算法，支持基线曲线引用)
 *
 * 功能：
 * - 计算 Onset（起始温度）或 Endset（结束温度）
 * - 自动引用基线曲线（从当前曲线的子曲线中查找）
 * - 用户选择2个点定义切线区域
 * - 算法拟合切线并计算与基线曲线的交点（外推温度）
 *
 * 使用场景：
 * - TGA 起始分解温度
 * - DSC 熔融起始温度
 * - 反应开始/结束温度的精确确定
 *
 * 工作流程：
 * 1. 用户先使用"基线校正"功能绘制基线 → 生成基线曲线
 * 2. 选择主曲线，点击"外推温度"按钮
 * 3. 算法自动查找关联的基线曲线（SignalType::Baseline）
 * 4. 用户选择2个点定义切线区域（峰的前沿或后沿）
 * 5. 使用最小二乘法拟合切线
 * 6. 计算切线与基线曲线的交点（外推温度）
 * 7. 输出：外推点标注 + 切线 + 数值
 *
 * 验证逻辑：
 * - 如果没有基线曲线 → 提示用户先画基线
 * - 如果峰范围超出基线范围 → 提示错误
 * - 如果峰范围在基线范围内 → 计算外推温度
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
     * @brief 从上下文中查找基线曲线
     *
     * 在活动曲线的子曲线中查找 SignalType::Baseline 的曲线
     *
     * @param context 算法上下文
     * @param baselineCurve 输出：基线曲线（如果找到）
     * @return true - 找到基线曲线，false - 未找到
     */
    bool findBaselineCurve(AlgorithmContext* context, ThermalCurve& baselineCurve) const;

    /**
     * @brief 获取基线曲线在指定温度的 Y 值（插值）
     *
     * @param baselineData 基线曲线数据
     * @param temperature 目标温度
     * @return 基线在该温度的 Y 值
     */
    double getBaselineYAtTemperature(const QVector<ThermalDataPoint>& baselineData,
                                      double temperature) const;

    /**
     * @brief 验证峰范围是否在基线范围内
     *
     * @param baselineData 基线曲线数据
     * @param peakTemp1 峰范围起点温度
     * @param peakTemp2 峰范围终点温度
     * @return true - 峰范围在基线范围内，false - 超出范围
     */
    bool validatePeakRange(const QVector<ThermalDataPoint>& baselineData,
                           double peakTemp1,
                           double peakTemp2) const;

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
     * @brief 计算切线与基线曲线的交点
     *
     * 通过迭代查找切线与基线曲线的最近交点
     *
     * @param slope 切线斜率
     * @param intercept 切线截距
     * @param baselineData 基线曲线数据
     * @param searchRangeMin 搜索范围最小温度
     * @param searchRangeMax 搜索范围最大温度
     * @param intersectionTemp 输出：交点温度
     * @return true - 找到交点，false - 未找到交点
     */
    bool calculateIntersectionWithBaseline(double slope,
                                            double intercept,
                                            const QVector<ThermalDataPoint>& baselineData,
                                            double searchRangeMin,
                                            double searchRangeMax,
                                            double& intersectionTemp) const;

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

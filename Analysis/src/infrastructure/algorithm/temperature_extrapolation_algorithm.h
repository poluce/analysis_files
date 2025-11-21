#ifndef TEMPERATURE_EXTRAPOLATION_ALGORITHM_H
#define TEMPERATURE_EXTRAPOLATION_ALGORITHM_H

#include "domain/algorithm/i_thermal_algorithm.h"
#include <QPointF>

// 前置声明
class AlgorithmContext;

/**
 * @brief 基线质量评估结果
 */
struct BaselineQuality {
    double r2;              // 决定系数
    double slopeNormalized; // 归一化斜率 (slope / Y_range)
    double varianceRatio;   // 导数方差比
    bool isAcceptable;      // 是否达标
    QString rejectReason;   // 拒绝原因（如有）

    BaselineQuality()
        : r2(0), slopeNormalized(0), varianceRatio(0)
        , isAcceptable(false), rejectReason("未评估") {}
};

/**
 * @brief 线性拟合结果结构体
 */
struct LinearFit {
    double slope;      // 斜率
    double intercept;  // 截距
    double r2;         // R² 拟合优度
    bool valid;        // 是否有效
    BaselineQuality quality;  // 质量评估

    LinearFit() : slope(0), intercept(0), r2(0), valid(false) {}
    LinearFit(double s, double i, double r = 1.0)
        : slope(s), intercept(i), r2(r), valid(true) {}

    // 计算给定 x 处的 y 值
    double valueAt(double x) const { return slope * x + intercept; }
};

/**
 * @brief 拐点信息结构体
 */
struct InflectionPoint {
    int index;           // 数据点索引
    double temperature;  // 温度
    double value;        // 值
    double slope;        // 该点的斜率
    bool valid;          // 是否有效

    InflectionPoint() : index(-1), temperature(0), value(0), slope(0), valid(false) {}
};

/**
 * @brief 温度外推算法 - 标准外推法 (ISO 11358-1 / ASTM E2550)
 *
 * 功能：
 * - 计算外推起始温度 (Extrapolated Onset Temperature)
 * - 自动拐点检测：在用户选择的范围内找最大斜率点
 * - 混合基线策略：优先使用现有基线，降级为局部拟合
 * - 输出：切线曲线 + 基线延长线 + 外推点标注
 *
 * 使用场景：
 * - TGA 起始分解温度
 * - DSC 熔融起始温度
 * - 反应开始/结束温度的精确确定
 *
 * 工作流程（优化后）：
 * 1. 用户选择2个点定义反应特征区域
 *    - 点1：反应台阶开始前的平坦基线处（左侧）
 *    - 点2：反应台阶结束后的平坦处（右侧）
 * 2. 算法自动执行：
 *    - 基线拟合：混合策略（优先现有基线，降级局部拟合）
 *    - 拐点检测：在范围内找 |dy/dx|_max 的点
 *    - 切线计算：在拐点处计算切线方程
 *    - 交点计算：计算基线与切线的交点
 * 3. 输出：
 *    - 辅助曲线：切线、基线延长线
 *    - 标注点：外推点、拐点
 *    - 数值：外推温度
 *
 * 核心算法：
 * - 外推温度 = 基线 L_baseline 与切线 L_tangent 的交点
 * - L_baseline: y = a1*x + b1 (从 T1 左侧数据拟合)
 * - L_tangent: y = k*x + b (在拐点处，斜率 k = dy/dx)
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
    // ==================== 新增：标准外推法核心函数 ====================

    /**
     * @brief 拟合初始基线（局部拟合策略）
     *
     * 从 T_end 左侧提取一段数据，使用最小二乘法拟合直线
     *
     * @param data 曲线数据
     * @param T_end 拟合区域右边界（T1）
     * @param pointCount 使用的数据点数（默认20）
     * @return LinearFit 拟合结果
     */
    LinearFit fitInitialBaseline(const QVector<ThermalDataPoint>& data,
                                  double T_end,
                                  int pointCount = 20) const;

    /**
     * @brief 检测拐点（最大斜率点）
     *
     * 在指定温度范围内计算一阶微分，找到 |dy/dx| 最大的点
     *
     * @param data 曲线数据
     * @param T_start 搜索范围起点
     * @param T_end 搜索范围终点
     * @return InflectionPoint 拐点信息
     */
    InflectionPoint detectInflectionPoint(const QVector<ThermalDataPoint>& data,
                                           double T_start,
                                           double T_end) const;

    /**
     * @brief 计算两条直线的交点
     *
     * @param line1 第一条直线
     * @param line2 第二条直线
     * @return QPointF 交点坐标，如果平行则返回无效点
     */
    QPointF calculateLineIntersection(const LinearFit& line1,
                                       const LinearFit& line2) const;

    /**
     * @brief 混合基线策略 - 获取基线
     *
     * 优先搜索现有基线曲线，降级为局部拟合
     *
     * @param context 算法上下文
     * @param curveData 源曲线数据
     * @param T1 基线区域右边界
     * @param fallbackPointCount 降级拟合的点数
     * @return LinearFit 基线拟合结果
     */
    LinearFit getBaselineWithHybridStrategy(AlgorithmContext* context,
                                             const QVector<ThermalDataPoint>& curveData,
                                             double T1,
                                             int fallbackPointCount = 20) const;

    /**
     * @brief 创建切线辅助曲线
     *
     * @param parent 父曲线
     * @param tangent 切线拟合参数
     * @param T_start 切线起点温度
     * @param T_end 切线终点温度
     * @return ThermalCurve 切线曲线
     */
    ThermalCurve createTangentCurve(const ThermalCurve& parent,
                                     const LinearFit& tangent,
                                     double T_start,
                                     double T_end) const;

    /**
     * @brief 创建基线延长线辅助曲线
     *
     * @param parent 父曲线
     * @param baseline 基线拟合参数
     * @param T_start 基线起点温度
     * @param T_end 基线终点温度
     * @return ThermalCurve 基线延长线曲线
     */
    ThermalCurve createBaselineExtensionCurve(const ThermalCurve& parent,
                                               const LinearFit& baseline,
                                               double T_start,
                                               double T_end) const;

    // ==================== 保留：原有辅助函数 ====================

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

    // ==================== Phase 2: 稳健算法函数 ====================

    /**
     * @brief 自适应稳健基线拟合
     *
     * 算法流程：
     * 1. 在 [T1 - deltaT_max, T1 - deltaT_min] 范围内滑动窗口搜索
     * 2. 对每个窗口计算一阶导数方差
     * 3. 选择方差最小的子窗做 OLS
     * 4. 应用质量门槛验证
     * 5. 如果失败，使用两点基线兜底
     *
     * @param data 曲线数据
     * @param T1 用户选择的起始温度
     * @param T2 用户选择的结束温度（用于兜底）
     * @param yRange 数据 Y 轴范围（用于归一化斜率）
     * @return LinearFit 拟合结果（含质量评估）
     */
    LinearFit fitBaselineAdaptive(const QVector<ThermalDataPoint>& data,
                                   double T1,
                                   double T2,
                                   double yRange) const;

    /**
     * @brief 兜底基线：使用用户选择的两点连线
     *
     * 当自适应搜索无法找到合适的平稳基线段时，
     * 使用用户选择的 T1 和 T2 两点直接连线作为基线
     *
     * @param data 曲线数据
     * @param T1 用户选择的起始温度
     * @param T2 用户选择的结束温度
     * @return LinearFit 两点连线拟合结果
     */
    LinearFit fitBaselineTwoPoint(const QVector<ThermalDataPoint>& data,
                                   double T1,
                                   double T2) const;

    /**
     * @brief 计算数据段的一阶导数方差
     *
     * @param data 曲线数据
     * @param startIdx 起始索引
     * @param endIdx 结束索引
     * @return 导数方差
     */
    double calculateDerivativeVariance(const QVector<ThermalDataPoint>& data,
                                        int startIdx,
                                        int endIdx) const;

    /**
     * @brief 线性回归拟合（指定索引范围）
     *
     * @param data 曲线数据
     * @param startIdx 起始索引
     * @param endIdx 结束索引
     * @return LinearFit 拟合结果
     */
    LinearFit fitLinear(const QVector<ThermalDataPoint>& data,
                         int startIdx,
                         int endIdx) const;

    /**
     * @brief 稳健拐点检测
     *
     * 算法改进：
     * 1. 使用滑动窗口 OLS 估计局部斜率（代替三点差分）
     * 2. 只在 leading half 搜索
     * 3. SNR/阈值过滤
     *
     * @param data 曲线数据
     * @param T1 搜索范围起点
     * @param T2 搜索范围终点
     * @return InflectionPoint 拐点信息
     */
    InflectionPoint detectInflectionPointRobust(const QVector<ThermalDataPoint>& data,
                                                  double T1,
                                                  double T2) const;

    /**
     * @brief 计算局部斜率（滑动窗口 OLS）
     *
     * @param data 曲线数据
     * @param startIdx 起始索引
     * @param endIdx 结束索引
     * @return 局部斜率
     */
    double calculateLocalSlope(const QVector<ThermalDataPoint>& data,
                                int startIdx,
                                int endIdx) const;

    /**
     * @brief 检查二阶导数过零
     *
     * @param data 曲线数据
     * @param centerIdx 中心索引
     * @param halfWindow 半窗口大小
     * @return true=过零，false=未过零
     */
    bool checkSecondDerivativeZeroCrossing(const QVector<ThermalDataPoint>& data,
                                            int centerIdx,
                                            int halfWindow) const;

    /**
     * @brief 计算两条直线的交点（带合理性约束）
     *
     * @param baseline 基线拟合
     * @param tangent 切线拟合
     * @param T1 用户选择的起始温度
     * @param T2 用户选择的结束温度
     * @param outConfidence 输出：结果可信度 (0-1)
     * @param outWarning 输出：警告信息
     * @return QPointF 交点坐标
     */
    QPointF calculateLineIntersectionConstrained(const LinearFit& baseline,
                                                   const LinearFit& tangent,
                                                   double T1,
                                                   double T2,
                                                   double& outConfidence,
                                                   QString& outWarning) const;

    /**
     * @brief 根据温度从父曲线插值计算时间值
     *
     * @param parentData 父曲线数据
     * @param temperature 目标温度
     * @return 插值得到的时间值
     */
    double interpolateTimeFromTemperature(const QVector<ThermalDataPoint>& parentData,
                                           double temperature) const;
};

#endif // TEMPERATURE_EXTRAPOLATION_ALGORITHM_H

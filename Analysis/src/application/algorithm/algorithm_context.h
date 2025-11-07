#ifndef APPLICATION_ALGORITHM_CONTEXT_H
#define APPLICATION_ALGORITHM_CONTEXT_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <functional>
#include <optional>

// ============================================================================
// 标准键名常量定义 (Standard Context Keys)
// ============================================================================
/**
 * @brief 算法上下文标准键名
 *
 * 为算法插件开发者提供类型安全的键名常量，避免字符串拼写错误。
 * 使用这些常量而非字符串字面量可以获得编译时检查和自动补全支持。
 *
 * @code
 * // 推荐使用：
 * auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
 *
 * // 避免使用：
 * auto curve = context->get<ThermalCurve*>("activeCurve");  // 容易拼写错误
 * @endcode
 */
namespace ContextKeys {
    // ========== 基础曲线数据 (Basic Curve Data) ==========
    /** 当前活动曲线 (ThermalCurve*) - 算法将在此曲线上执行 */
    inline constexpr const char* ActiveCurve = "activeCurve";

    /** 输入曲线 (ThermalCurve*) - 算法的数据来源曲线 */
    inline constexpr const char* InputCurve = "inputCurve";

    /** 输出曲线 (ThermalCurve*) - 算法生成的新曲线 */
    inline constexpr const char* OutputCurve = "outputCurve";

    /** 活动曲线的基线曲线列表 (QVector<ThermalCurve*>) - 由 AlgorithmCoordinator 自动注入
     *
     * 说明：可能包含多条基线（线性、多项式、样条等），由算法决定如何使用：
     * - 使用第一条：baselines.value().first() 或 baselines.value()[0]
     * - 使用最新的：baselines.value().last()
     * - 遍历所有：for (auto* baseline : baselines.value())
     */
    inline constexpr const char* BaselineCurves = "baselineCurves";

    /** 目标曲线ID (QString) - 用于标识特定曲线 */
    inline constexpr const char* TargetCurveId = "targetCurveId";

    // ========== 用户交互选点 (User Interaction Points) ==========
    /** 用户选择的点集合 (QVector<QPointF>) - 用于基线、峰面积等算法 */
    inline constexpr const char* SelectedPoints = "selectedPoints";

    /** 选中点的X坐标列表 (QList<double>) */
    inline constexpr const char* SelectedXPoints = "selectedXPoints";

    /** 选中点的Y坐标列表 (QList<double>) */
    inline constexpr const char* SelectedYPoints = "selectedYPoints";

    /** 最后一次点击的点 (QPointF) */
    inline constexpr const char* LastClickedPoint = "lastClickedPoint";

    // ========== 通用算法参数 (Common Algorithm Parameters) ==========
    /** 窗口大小 (int) - 用于移动平均算法 */
    inline constexpr const char* ParamWindow = "param.window";

    /** 半窗口大小 (int) - 用于微分算法 */
    inline constexpr const char* ParamHalfWin = "param.halfWin";

    /** 时间步长 (double) - 用于微分等数值计算 */
    inline constexpr const char* ParamDt = "param.dt";

    /** 启用调试输出 (bool) - 用于算法调试 */
    inline constexpr const char* ParamEnableDebug = "param.enableDebug";

    /** 阈值 (double) - 用于峰值检测等算法 */
    inline constexpr const char* ParamThreshold = "param.threshold";

    /** 步长 (double) - 用于数值计算 */
    inline constexpr const char* ParamStepSize = "param.stepSize";

    /** 平滑因子 (double) - 用于滤波算法 */
    inline constexpr const char* ParamSmoothingFactor = "param.smoothingFactor";

    // ========== 基线校正参数 (Baseline Correction Parameters) ==========
    /** 基线类型 (int) - 0=线性, 1=多项式 */
    inline constexpr const char* BaselineType = "baselineType";

    /** 基线起点 (QPointF) */
    inline constexpr const char* BaselineP1 = "baselineP1";

    /** 基线终点 (QPointF) */
    inline constexpr const char* BaselineP2 = "baselineP2";

    /** 基线曲线数据 (QVector<QPointF>) */
    inline constexpr const char* BaselineSeries = "baselineSeries";

    /** 基线拟合系数 (QList<double>) */
    inline constexpr const char* BaselineCoefficients = "baselineCoefficients";

    /** 多项式拟合次数 (int) */
    inline constexpr const char* PolynomialDegree = "polynomialDegree";

    // ========== 峰值分析参数 (Peak Analysis Parameters) ==========
    /** 峰值点坐标 (QPointF) */
    inline constexpr const char* PeakPoint = "peakPoint";

    /** 多个峰值的X坐标 (QList<double>) */
    inline constexpr const char* PeakXValues = "peakXValues";

    /** 峰面积 (double) */
    inline constexpr const char* PeakArea = "peakArea";

    /** 积分区间起点 (QPointF) */
    inline constexpr const char* IntegrationStart = "integration.start";

    /** 积分区间终点 (QPointF) */
    inline constexpr const char* IntegrationEnd = "integration.end";

    // ========== 滤波与微分参数 (Filtering & Derivative Parameters) ==========
    /** 滤波类型 (QString) - "FFT", "MovingAverage" */
    inline constexpr const char* FilterType = "filterType";

    /** DTG（导数）曲线数据 (QVector<QPointF>) */
    inline constexpr const char* DtgSeries = "dtgSeries";

    /** 滤波后的曲线数据 (QVector<QPointF>) */
    inline constexpr const char* FilteredSeries = "filteredSeries";

    // ========== 拟合参数 (Fitting Parameters) ==========
    /** 拟合类型 (QString) - "polynomial", "linear" */
    inline constexpr const char* FitType = "fitType";

    /** 拟合阶数 (int) */
    inline constexpr const char* FitDegree = "fitDegree";

    /** 拟合系数 (QList<double>) */
    inline constexpr const char* FitCoefficients = "fitCoefficients";

    /** 拟合优度 R² (double) */
    inline constexpr const char* FitRSquared = "fitRSquared";

    /** 拟合曲线数据 (QVector<QPointF>) */
    inline constexpr const char* FitSeries = "fitSeries";

    // ========== 动力学分析参数 (Kinetic Analysis Parameters) ==========
    /** 活化能 Ea (double) - 单位 kJ/mol */
    inline constexpr const char* ActivationEnergy = "activationEnergy";

    /** 指前因子 A (double) */
    inline constexpr const char* PreExponentialFactor = "preExponentialFactor";

    /** 反应级数 n (double) */
    inline constexpr const char* ReactionOrder = "reactionOrder";

    // ========== 显示与坐标轴配置 (Display & Axis Configuration) ==========
    /** X轴数据键名 (QString) */
    inline constexpr const char* XAxisKey = "xAxisKey";

    /** Y轴数据键名 (QString) */
    inline constexpr const char* YAxisKey = "yAxisKey";

    /** X轴标签 (QString) */
    inline constexpr const char* XAxisLabel = "xAxisLabel";

    /** Y轴标签 (QString) */
    inline constexpr const char* YAxisLabel = "yAxisLabel";

    /** X轴单位 (QString) */
    inline constexpr const char* XAxisUnit = "xAxisUnit";

    /** Y轴单位 (QString) */
    inline constexpr const char* YAxisUnit = "yAxisUnit";

    // ========== 交互状态 (Interaction State) ==========
    /** 选点模式 (QString) - "baseline", "peak", "area" 等 */
    inline constexpr const char* SelectionMode = "selectionMode";

    /** 图表交互模式 (QString) - "RubberBand", "Drag", "Disabled" */
    inline constexpr const char* ChartInteractionMode = "chartInteractionMode";
}

// ============================================================================
// AlgorithmContext 类定义
// ============================================================================
/**
 * @brief 算法上下文 - 统一的运行时数据容器
 *
 * AlgorithmContext 是所有算法执行时的数据存储和传递中心，采用键值对方式管理：
 * - 算法输入参数（窗口大小、阈值等）
 * - 用户交互数据（选择的点、曲线引用等）
 * - 算法中间结果和输出数据
 *
 * ## 核心特性
 *
 * 1. **类型安全访问**: 使用模板方法 `get<T>()` 提供编译时类型检查
 * 2. **标准键名常量**: `ContextKeys` 命名空间定义所有标准键名，避免拼写错误
 * 3. **数据来源追踪**: 每个值都记录来源（UI/Algorithm/File）和时间戳
 * 4. **信号通知**: 值变化时自动发出信号，支持响应式编程
 *
 * ## 算法开发者指南
 *
 * ### 1. 读取上下文数据
 *
 * 算法应该从上下文中"拉取"所需数据，而不是通过函数参数传递：
 *
 * @code
 * QVariant MyAlgorithm::executeWithContext(AlgorithmContext* context) {
 *     // 1. 获取活动曲线（必需）
 *     auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
 *     if (!curve.has_value() || !curve.value()) {
 *         qWarning() << "MyAlgorithm: activeCurve 未设置";
 *         return QVariant();
 *     }
 *
 *     // 2. 获取算法参数（使用 value_or 提供默认值）
 *     int windowSize = context->get<int>(ContextKeys::ParamWindowSize).value_or(50);
 *     double threshold = context->get<double>(ContextKeys::ParamThreshold).value_or(0.1);
 *
 *     // 3. 获取用户选择的点（如果算法需要）
 *     auto points = context->get<QVector<QPointF>>(ContextKeys::SelectedPoints);
 *     if (points.has_value() && points.value().size() >= 2) {
 *         // 使用选点数据
 *     }
 *
 *     // 4. 获取曲线数据并执行算法
 *     const auto& inputData = curve.value()->getProcessedData();
 *     QVector<ThermalDataPoint> result = performCalculation(inputData, windowSize);
 *
 *     return QVariant::fromValue(result);
 * }
 * @endcode
 *
 * ### 2. 设置默认参数（可选）
 *
 * 如果算法有默认参数，可以在 `prepareContext()` 中注入：
 *
 * @code
 * void MyAlgorithm::prepareContext(AlgorithmContext* context) {
 *     // 只在上下文中不存在时才设置默认值
 *     if (!context->contains(ContextKeys::ParamWindowSize)) {
 *         context->setValue(ContextKeys::ParamWindowSize, 50);
 *     }
 * }
 * @endcode
 *
 * ### 3. 标准数据类型速查
 *
 * | 键名常量                          | 数据类型                  | 说明                    |
 * |-----------------------------------|---------------------------|-------------------------|
 * | `ContextKeys::ActiveCurve`        | `ThermalCurve*`           | 当前活动曲线            |
 * | `ContextKeys::BaselineCurves`     | `QVector<ThermalCurve*>`  | 活动曲线的所有基线      |
 * | `ContextKeys::SelectedPoints`     | `QVector<QPointF>`        | 用户选择的点集合        |
 * | `ContextKeys::ParamWindow`        | `int`                     | 窗口大小（移动平均）    |
 * | `ContextKeys::ParamThreshold`     | `double`                  | 阈值（峰值检测等）      |
 * | `ContextKeys::BaselineType`       | `int`                     | 基线类型 (0=线性, 1=多项式) |
 * | `ContextKeys::FilterType`         | `QString`                 | 滤波类型 ("FFT", "MovingAverage") |
 * | `ContextKeys::PeakArea`           | `double`                  | 峰面积计算结果          |
 * | `ContextKeys::ActivationEnergy`   | `double`                  | 活化能 (kJ/mol)         |
 *
 * 完整的数据清单请参考：`新设计文档/AlgorithmContext_数据清单.md`
 *
 * ### 4. 注意事项
 *
 * - ✅ **使用标准键名常量** (`ContextKeys::*`) 而非字符串字面量
 * - ✅ **使用 `value_or()`** 提供回退默认值，避免崩溃
 * - ✅ **检查指针有效性** 在使用 `ThermalCurve*` 前检查 `!= nullptr`
 * - ✅ **返回 QVariant** 算法结果应包装为 `QVariant::fromValue(result)`
 * - ❌ **不要修改输入曲线** 使用 `getProcessedData()` 获取只读数据
 * - ❌ **不要在上下文中存储大对象** 对于大数据，存储指针或引用
 *
 * ## 高级用法
 *
 * ### 监听上下文变化
 *
 * @code
 * connect(context, &AlgorithmContext::valueChanged, this,
 *         [](const QString& key, const QVariant& value) {
 *     qDebug() << "上下文更新:" << key << "=" << value;
 * });
 * @endcode
 *
 * ### 批量读取参数
 *
 * @code
 * // 获取所有以 "param." 开头的参数
 * QVariantMap params = context->values("param.");
 * // params = {"param.windowSize": 50, "param.threshold": 0.1, ...}
 * @endcode
 */
class AlgorithmContext : public QObject {
    Q_OBJECT

public:
    explicit AlgorithmContext(QObject* parent = nullptr);

    /**
     * @brief 检查上下文中是否包含指定键
     * @param key 键名
     * @return 如果存在返回 true，否则返回 false
     */
    bool contains(const QString& key) const;

    /**
     * @brief 获取指定键的值（QVariant 类型）
     * @param key 键名
     * @param defaultValue 默认值（如果键不存在）
     * @return 键对应的值，如果不存在则返回默认值
     */
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 获取指定键的值（类型安全的模板方法）
     *
     * 使用 std::optional 包装结果，避免在键不存在时返回默认构造的对象。
     *
     * @tparam T 目标类型
     * @param key 键名
     * @return std::optional<T> - 如果键存在且类型匹配返回值，否则返回 std::nullopt
     *
     * 示例：
     * @code
     * auto curve = context->get<ThermalCurve*>("activeCurve");
     * if (curve.has_value()) {
     *     // 使用 curve.value()
     * }
     * @endcode
     */
    template <typename T>
    std::optional<T> get(const QString& key) const
    {
        if (!m_entries.contains(key)) {
            return std::nullopt;
        }
        return m_entries.value(key).storedValue.template value<T>();
    }

    /**
     * @brief 设置键值对
     * @param key 键名
     * @param value 值
     * @param source 数据来源（可选，用于调试和追踪）
     */
    void setValue(const QString& key, const QVariant& value, const QString& source = QString());

    /**
     * @brief 移除指定键
     * @param key 键名
     */
    void remove(const QString& key);

    /**
     * @brief 清空所有键值对
     */
    void clear();

    /**
     * @brief 获取所有键的列表（可选按前缀过滤）
     * @param prefix 键名前缀（可选，如 "param." 获取所有参数键）
     * @return 键名列表
     */
    QStringList keys(const QString& prefix = QString()) const;

    /**
     * @brief 获取所有键值对（可选按前缀过滤）
     * @param prefix 键名前缀（可选）
     * @return 键值对映射表
     */
    QVariantMap values(const QString& prefix = QString()) const;

signals:
    /**
     * @brief 当键值发生改变时发射此信号
     * @param key 改变的键名
     * @param value 新值
     */
    void valueChanged(const QString& key, const QVariant& value);

    /**
     * @brief 当键被移除时发射此信号
     * @param key 被移除的键名
     */
    void valueRemoved(const QString& key);

private:
    struct Entry {
        QVariant storedValue;
        QString source;
    };

    QHash<QString, Entry> m_entries;
};

#endif // APPLICATION_ALGORITHM_CONTEXT_H

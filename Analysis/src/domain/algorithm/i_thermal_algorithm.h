#ifndef ITHERMALALGORITHM_H
#define ITHERMALALGORITHM_H

#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/model/thermal_curve.h"
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

// 前置声明
class AlgorithmContext;

/*
 * ───────────────────────────────────────────────
 *  Algorithm Input Key Specification (v1.0)
 * ───────────────────────────────────────────────
 *  这些 key 用于在 Algorithm::execute(inputs) 的参数输入中传递数据。
 *  插件算法可复用这些标准字段，或在 "custom" map 中定义扩展字段。
 *
 *  基本输入：
 *   - "mainCurve"        需要计算的主曲线的数据
 *   - "refCurve"         参考曲线（用于相交/积分类算法）
 *   - "points"           用户选中的x轴点集合 QVector<float>
 *   - "region"           X区间范围 {x1, x2}
 *   - "baselineMode"     基线计算模式 ("linear", "poly")
 *   - "smoothWindow"     平滑算法窗口大小
 *   - "derivativeOrder"  微分阶数
 *   - "annotationText"   标注内容
 *
 *  扩展输入：
 *   - "selectedCurves"   多曲线集合 QVector<Curve*>
 *   - "intersectionCurves" 相交曲线对
 *   - "custom"           插件自定义参数 QVariantMap
 *
 *  所有字段均为可选（除 mainCurve 外），由算法根据需要解析。
 *  ───────────────────────────────────────────────
 */

// 前置声明
class ThermalDataPoint;

/**
 * @brief IThermalAlgorithm 接口为所有热分析算法定义了标准。
 *
 * 这个接口遵循策略模式，允许算法作为插件被执行和配置。
 *
 * 新设计支持五种算法类型（A-E类）：
 * - A类：单曲线算法（如微分、滤波）
 * - B类：选点算法（如基线校正）
 * - C类：双曲线算法（如峰面积计算）
 * - D类：曲线相交算法（如曲线差值）
 * - E类：多选点多曲线算法（如多峰分析）
 */
class IThermalAlgorithm {
public:
    /**
     * @brief 算法输入类型枚举
     *
     * 定义算法需要的输入数据类型，决定是否需要用户交互。
     */
    enum class InputType {
        None,           // 无需额外输入（A类：单曲线算法）
        PointSelection, // 需要选点（B/C类：基线校正、峰面积）
        DualCurve,      // 需要选择第二条曲线（C类：双曲线算法）
        Intersect,      // 需要选点和另一条曲线（D类：曲线相交）
        MultiPoint      // 需要多点选取（E类：多峰分析）
    };

    /**
     * @brief 算法输出类型枚举
     *
     * 定义算法的输出结果类型。
     */
    enum class OutputType {
        Curve,         // 输出新曲线（最常见）
        Area,          // 输出面积值和区域图
        Intersection,  // 输出交点集合
        Annotation,    // 输出标注（如峰位置、切线）
        MultipleCurves // 输出多条曲线
    };

    // 接口必须有虚析构函数
    virtual ~IThermalAlgorithm() = default;

    // ==================== 核心接口方法 ====================

    /**
     * @brief 返回算法的唯一名称（用于注册和查找）。
     * @return 算法名称（例如："differentiation", "integration"）。
     */
    virtual QString name() const = 0;

    /**
     * @brief 返回算法的中文显示名称。
     * @return 算法的中文名称（例如："微分", "积分"）。
     */
    virtual QString displayName() const = 0;

    /**
     * @brief 返回算法的分类。
     * @return 算法分类（例如："Analysis", "Preprocessing"）。
     */
    virtual QString category() const = 0;

    /**
     * @brief 根据输入信号类型获取输出信号类型。
     *
     * 算法只影响信号类型（Raw <-> Derivative），不改变仪器类型。
     * 例如：
     * - 微分算法：Raw → Derivative
     * - 积分算法：Derivative → Raw
     * - 滤波算法：保持不变
     *
     * @param inputType 输入信号的类型。
     * @return 输出信号的类型。
     */
    virtual SignalType getOutputSignalType(SignalType inputType) const = 0;

    /**
     * @brief 返回算法的输入类型（决定交互方式）。
     *
     * 决定算法是否需要用户交互（选点、选择曲线等）。
     * 默认实现返回 InputType::None（A类算法）。
     *
     * @return 算法的输入类型。
     */
    virtual InputType inputType() const { return InputType::None; }

    /**
     * @brief 返回算法的输出类型（决定结果形式）。
     *
     * 决定算法输出结果的形式（新曲线、面积、标注等）。
     * 默认实现返回 OutputType::Curve（输出新曲线）。
     *
     * @return 算法的输出类型。
     */
    virtual OutputType outputType() const { return OutputType::Curve; }

    /**
     * @brief 返回算法的交互描述信息（用于UI和流程控制）。
     *
     * 定义算法需要的参数、交互类型、提示信息等元数据。
     * 推荐算法重写此方法以提供完整的交互配置。
     *
     * @return 算法描述符。
     */
    virtual AlgorithmDescriptor descriptor() const
    {
        AlgorithmDescriptor desc;
        desc.name = name();
        desc.interaction = [this]() {
            switch (inputType()) {
            case InputType::None:
                return AlgorithmInteraction::None;
            case InputType::PointSelection:
                return AlgorithmInteraction::PointSelection;
            default:
                return AlgorithmInteraction::ParameterDialog;
            }
        }();
        desc.requiredPointCount = 0;
        desc.pointSelectionHint = QString();
        return desc;
    }

    // ==================== 上下文驱动执行接口 ====================

    /**
     * @brief 准备算法执行前的上下文（可选重写）。
     *
     * 算法可以重写此方法，在执行前向上下文注入所需的默认参数。
     * 例如：微分算法可以设置默认的 halfWin 和 dt 参数。
     *
     * 调用时机：在 executeWithContext() 之前调用。
     *
     * @param context 算法上下文。
     */
    virtual void prepareContext(AlgorithmContext* context) { (void)context; }

    /**
     * @brief 执行算法（上下文驱动，纯虚函数）。
     *
     * ✅ **核心执行接口**：算法从上下文中拉取所需数据，避免繁琐的参数传递。
     *
     * 算法从上下文中获取数据：
     * - 曲线数据：context->get<ThermalCurve*>("activeCurve")
     * - 参数：context->get<int>("param.windowSize")
     * - 选择的点：context->get<QVector<QPointF>>("selectedPoints")
     * - 参考曲线：context->get<ThermalCurve*>("referenceCurve")
     *
     * 返回值根据 outputType() 决定：
     * - OutputType::Curve: QVector<ThermalDataPoint>
     * - OutputType::Area: QVariantMap {"area": double, "series": QAreaSeries*}
     * - OutputType::Intersection: QVector<QPointF>
     * - OutputType::Annotation: QVariantList (标注信息)
     * - OutputType::MultipleCurves: QVariantList (多条曲线)
     *
     * @param context 算法上下文，包含所有运行时状态。
     * @return 算法执行结果。
     */
    virtual QVariant executeWithContext(AlgorithmContext* context) = 0;
};

#endif // ITHERMALALGORITHM_H

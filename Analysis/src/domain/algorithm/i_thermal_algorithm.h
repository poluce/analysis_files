#ifndef ITHERMALALGORITHM_H
#define ITHERMALALGORITHM_H

#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/algorithm/algorithm_result.h"
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

    // ==================== 曲线属性声明接口 ====================

    /**
     * @brief 声明算法生成的曲线是否为辅助曲线。
     *
     * **辅助曲线** vs **独立曲线**：
     * - 辅助曲线：如基线、平滑曲线，应继承父曲线的Y轴，保持视觉一致性
     * - 独立曲线：如微分、积分，数据类型改变，应创建新的Y轴
     *
     * 决定Y轴分配策略：
     * - true（辅助曲线） → 继承父曲线的Y轴
     * - false（独立曲线） → 根据SignalType创建新Y轴
     *
     * 示例：
     * - 基线算法：return true（辅助曲线，继承父曲线Y轴）
     * - 微分算法：return false（独立曲线，创建次Y轴）
     *
     * @return true=辅助曲线（继承父曲线Y轴），false=独立曲线（创建新Y轴）
     */
    virtual bool isAuxiliaryCurve() const = 0;

    // ==================== 上下文驱动执行接口 ====================

    /**
     * @brief 准备算法执行前的上下文并验证数据完整性（两阶段执行 - 阶段1）。
     *
     * **两阶段执行机制**：
     * - **阶段1（prepareContext）**：算法检查上下文中的数据是否完整，返回就绪状态
     * - **阶段2（executeWithContext）**：只在数据完整时执行计算
     *
     * 此方法的职责：
     * 1. 向上下文注入默认参数（如果缺失）
     * 2. **验证所需数据是否完整**（核心）
     * 3. 返回 true 表示数据就绪，可以执行；返回 false 表示需要等待用户交互
     *
     * 示例：
     * - 简单算法（微分、积分）：只需要 activeCurve，立即返回 true
     * - 交互算法（基线校正）：需要 selectedPoints，如果缺失返回 false
     *
     * 调用时机：在 executeWithContext() 之前调用。
     *
     * @param context 算法上下文。
     * @return true - 数据完整，算法就绪；false - 数据不完整，等待用户输入
     */
    virtual bool prepareContext(AlgorithmContext* context) {
        (void)context;
        return true;  // 默认实现：无需额外数据，立即就绪
    }

    /**
     * @brief 执行算法（上下文驱动，纯虚函数）。
     *
     * ✅ **核心执行接口**：算法从上下文中拉取所需数据，返回结构化的 AlgorithmResult。
     *
     * **输入**（从上下文拉取）：
     * - 曲线数据：context->get<ThermalCurve*>("activeCurve")
     * - 参数：context->get<int>("param.windowSize")
     * - 选择的点：context->get<QVector<QPointF>>("selectedPoints")
     * - 参考曲线：context->get<ThermalCurve*>("referenceCurve")
     *
     * **输出**（返回 AlgorithmResult 容器）：
     * - ResultType::Curve: 单曲线输出（如微分、积分）
     * - ResultType::Marker: 标注点输出（如峰值、外推点）
     * - ResultType::Region: 区域输出（如峰面积）
     * - ResultType::ScalarValue: 数值输出（如温度、斜率）
     * - ResultType::Composite: 混合输出（如峰面积=数值+曲线+区域）
     *
     * **示例**：
     * @code
     * // 简单曲线输出
     * AlgorithmResult result = AlgorithmResult::success("differentiation", curveId, ResultType::Curve);
     * result.setCurve(outputCurve);
     * result.setMeta("unit", "mg/min");
     * return result;
     *
     * // 混合输出
     * AlgorithmResult result = AlgorithmResult::success("peakArea", curveId, ResultType::Composite);
     * result.addCurve(baselineCurve);
     * result.setArea(area, "J/g");
     * result.addRegion(areaPolygon);
     * return result;
     * @endcode
     *
     * @param context 算法上下文，包含所有运行时输入数据。
     * @return AlgorithmResult - 结构化的算法执行结果（成功或失败）。
     */
    virtual AlgorithmResult executeWithContext(AlgorithmContext* context) = 0;
};

#endif // ITHERMALALGORITHM_H

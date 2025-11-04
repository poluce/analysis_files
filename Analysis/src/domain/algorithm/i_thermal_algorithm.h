#ifndef ITHERMALALGORITHM_H
#define ITHERMALALGORITHM_H

#include "domain/model/thermal_curve.h"
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

/*
 * ───────────────────────────────────────────────
 *  Algorithm Input Key Specification (v1.0)
 * ───────────────────────────────────────────────
 *  这些 key 用于在 Algorithm::execute(inputs) 的参数输入中传递数据。
 *  插件算法可复用这些标准字段，或在 "custom" map 中定义扩展字段。
 *
 *  基本输入：
 *   - "mainCurve"        当前选中曲线（Curve* 或 QLineSeries*）
 *   - "refCurve"         参考曲线（用于相交/积分类算法）
 *   - "points"           用户选中点集合 QVector<QPointF>
 *   - "x1", "x2"         选区范围边界
 *   - "region"           X区间范围 {x1, x2}
 *   - "baselineMode"     基线计算模式 ("linear", "poly")
 *   - "smoothWindow"     平滑算法窗口大小
 *   - "derivativeOrder"  微分阶数
 *   - "color"            新曲线颜色
 *   - "curveName"        生成曲线命名后缀
 *   - "annotationText"   标注内容
 *   - "meta"             附加元信息，如版本、来源
 *
 *  扩展输入：
 *   - "selectedCurves"   多曲线集合 QVector<Curve*>
 *   - "intersectionCurves" 相交曲线对
 *   - "highlightColor"   交互高亮颜色
 *   - "showAnnotation"   是否绘制文字标注
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

    /**
     * @brief 处理输入数据并返回结果。
     * @param inputData 要处理的数据点向量。
     * @return 处理后的数据点向量。
     */
    virtual QVector<ThermalDataPoint> process(const QVector<ThermalDataPoint>& inputData) = 0;

    /**
     * @brief 返回算法的唯一名称。
     * @return 算法名称。
     */
    virtual QString name() const = 0;

    /**
     * @brief 返回算法的中文显示名称。
     * @return 算法的中文名称。
     */
    virtual QString displayName() const = 0;

    /**
     * @brief 返回算法的分类（如 "预处理", "分析"）。
     * @return 算法分类。
     */
    virtual QString category() const = 0;

    /**
     * @brief 返回算法所需的参数定义。
     * @return 包含参数信息的QVariantMap。
     */
    virtual QVariantMap parameters() const = 0;

    /**
     * @brief 设置算法参数。
     * @param key 参数名称。
     * @param value 参数值。
     */
    virtual void setParameter(const QString& key, const QVariant& value) = 0;
    virtual void setParameter(const QVariantMap& key) = 0;

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

    // ==================== 新接口方法 (支持 A-E 类算法) ====================
    /**
     * @brief 配置算法参数的弹窗
     *
     * 用户在此配置弹窗上选择有关算法的配置信息
     *
     * @return 算法的输入类型。
     */
    virtual QVariantMap configure(QWidget* parent = nullptr) { return {}; }
    /**
     * @brief 返回算法的输入类型。
     *
     * 决定算法是否需要用户交互（选点、选择曲线等）。
     * 默认实现返回 InputType::None（A类算法）。
     *
     * @return 算法的输入类型。
     */
    virtual InputType inputType() const { return InputType::None; }

    /**
     * @brief 返回算法的输出类型。
     *
     * 决定算法输出结果的形式（新曲线、面积、标注等）。
     * 默认实现返回 OutputType::Curve（输出新曲线）。
     *
     * @return 算法的输出类型。
     */
    virtual OutputType outputType() const { return OutputType::Curve; }

    /**
     * @brief 执行算法（新接口，支持灵活输入/输出）。
     *
     * 通过 QVariantMap 传递输入数据，支持复杂场景：
     * - "mainCurve": ThermalCurve* (主曲线)
     * - "points": QVector<QPointF> (选择的点)
     * - "referenceCurve": ThermalCurve* (参考曲线，用于双曲线算法)
     * - 其他自定义参数
     *
     * 返回值根据 outputType() 决定：
     * - OutputType::Curve: QVector<ThermalDataPoint>
     * - OutputType::Area: QVariantMap {"area": double, "series": QAreaSeries*}
     * - OutputType::Intersection: QVector<QPointF>
     * - OutputType::Annotation: QVariantList (标注信息)
     * - OutputType::MultipleCurves: QVariantList (多条曲线)
     *
     * 默认实现调用旧的 process() 方法以保持向后兼容。
     *
     * @param inputs 输入数据映射。
     * @return 算法执行结果。
     */
    virtual QVariant execute(const QVariantMap& inputs)
    {
        // 默认实现：从 inputs 中提取主曲线，调用旧的 process() 方法
        if (inputs.contains("mainCurve")) {
            auto curve = inputs["mainCurve"].value<ThermalCurve*>();
            if (curve) {
                auto result = process(curve->getProcessedData());
                return QVariant::fromValue(result);
            }
        }
        return QVariant();
    }

    /**
     * @brief 返回用户交互提示信息。
     *
     * 当算法需要用户交互时（inputType() != None），
     * 返回给用户的提示信息（如 "请选择两个点定义基线范围"）。
     *
     * 默认返回空字符串。
     *
     * @return 交互提示信息。
     */
    virtual QString userPrompt() const { return QString(); }
};

#endif // ITHERMALALGORITHM_H

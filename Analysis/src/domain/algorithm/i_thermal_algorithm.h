#ifndef ITHERMALALGORITHM_H
#define ITHERMALALGORITHM_H

#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/algorithm/algorithm_result.h"
#include "domain/algorithm/i_progress_reporter.h"  // 完整定义（替换前向声明）
#include "domain/model/thermal_curve.h"
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

// 前置声明
class AlgorithmContext;

/*
 * ───────────────────────────────────────────────
 *  Algorithm Context Keys Specification (v2.0)
 * ───────────────────────────────────────────────
 *  算法通过 AlgorithmContext 从上下文中拉取数据。
 *  所有标准键名定义在 ContextKeys 命名空间中（algorithm_context.h）。
 *
 *  核心曲线数据：
 *   - ContextKeys::ActiveCurve      当前活动曲线 (ThermalCurve*)
 *   - ContextKeys::InputCurve       输入曲线 (ThermalCurve*)
 *   - ContextKeys::BaselineCurves   基线曲线列表 (QVector<ThermalCurve*>)
 *
 *  用户交互数据：
 *   - ContextKeys::SelectedPoints   用户选择的点 (QVector<ThermalDataPoint>)
 *
 *  通用参数：
 *   - ContextKeys::ParamWindow      窗口大小 (int)
 *   - ContextKeys::ParamHalfWin     半窗口大小 (int)
 *   - ContextKeys::ParamThreshold   阈值 (double)
 *
 *  详见：application/algorithm/algorithm_context.h 中的 ContextKeys 命名空间
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

    /**
     * @brief 声明算法生成的曲线是否为强绑定曲线。
     *
     * **强绑定曲线** vs **非强绑定曲线**：
     * - 强绑定曲线：与父曲线紧密关联，不应独立显示
     *   - 不在 ProjectExplorer 树中显示为独立节点
     *   - 父曲线隐藏时，强绑定的子曲线也自动隐藏
     *   - 例如：基线曲线（仅作为父曲线的辅助参考）
     *
     * - 非强绑定曲线：可以独立显示和操作
     *   - 在 ProjectExplorer 树中显示为父曲线的子节点
     *   - 父曲线隐藏时，子曲线可以独立控制显示/隐藏
     *   - 例如：滤波曲线（可独立作为分析对象）
     *
     * 决定曲线的显示策略：
     * - true（强绑定） → 不在树中独立显示，随父曲线隐藏
     * - false（非强绑定） → 在树中显示为子节点，可独立控制
     *
     * 示例：
     * - 基线算法：return true（强绑定，不独立显示）
     * - 滤波算法：return false（非强绑定，独立显示）
     * - 微分算法：return false（非强绑定，独立显示）
     *
     * @return true=强绑定（不独立显示），false=非强绑定（独立显示）
     */
    virtual bool isStronglyBound() const = 0;

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
     * @note 使用 ContextKeys 常量需要包含：#include "application/algorithm/algorithm_context.h"
     *
     * **输入**（从上下文拉取）：
     * - 曲线数据：context->get<ThermalCurve*>(ContextKeys::ActiveCurve)
     * - 参数：context->get<int>(ContextKeys::ParamWindow)
     * - 选择的点：context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints)
     * - 参考曲线：context->get<ThermalCurve*>(ContextKeys::InputCurve)
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

    // ==================== 进度报告接口（用于异步执行）====================

    /**
     * @brief 设置进度报告器
     *
     * 在工作线程中执行算法前，AlgorithmWorker 调用此方法设置进度报告器。
     * 算法通过 reportProgress() 和 shouldCancel() 与外界通信。
     *
     * @param reporter 进度报告器指针（nullptr 表示清理）
     */
    virtual void setProgressReporter(IProgressReporter* reporter) {
        m_progressReporter = reporter;
    }

protected:
    /**
     * @brief 报告当前进度
     *
     * 算法在执行过程中定期调用此方法报告进度。
     * 建议每处理 10% 数据时调用一次，避免过于频繁。
     *
     * @param percentage 进度百分比 (0-100)
     * @param message 可选的状态消息
     */
    void reportProgress(int percentage, const QString& message = QString()) const {
        if (m_progressReporter) {
            m_progressReporter->reportProgress(percentage, message);
        }
    }

    /**
     * @brief 检查是否应该取消执行
     *
     * 算法应定期（如每 100 次迭代）调用此方法检查取消标志。
     * 如果返回 true，应立即清理资源并返回空结果。
     *
     * @return true 表示应该尽快停止执行
     */
    bool shouldCancel() const {
        if (m_progressReporter) {
            return m_progressReporter->shouldCancel();
        }
        return false;
    }

private:
    mutable IProgressReporter* m_progressReporter = nullptr;  ///< 进度报告器（由 Worker 设置，mutable 允许在 const 方法中调用）
};

#endif // ITHERMALALGORITHM_H

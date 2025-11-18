#ifndef DOMAIN_ALGORITHM_DESCRIPTOR_H
#define DOMAIN_ALGORITHM_DESCRIPTOR_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief 算法参数定义（增强版 - Phase 2）
 *
 * 描述单个参数的键名、类型、默认值和校验信息。
 * 支持动态参数对话框生成（QDialog + QFormLayout）。
 */
struct AlgorithmParameterDefinition {
    QString key;                 //!< 参数键名（用于 AlgorithmContext 存储）
    QString label;               //!< UI 展示名称
    QString description;         //!< 参数描述（可用于 tooltip）
    QVariant::Type valueType = QVariant::Invalid;  //!< 参数类型
    QVariant defaultValue;       //!< 默认值
    bool required = true;        //!< 是否必填
    QVariantMap constraints;     //!< 约束信息（如 {"min":1, "max":100, "step":1, "unit":"mg"} ）
};

/**
 * @brief 算法描述信息（增强版 - Phase 2）
 *
 * 算法通过 IThermalAlgorithm::descriptor() 返回此结构，实现完全自描述：
 * - 是否需要交互（参数对话框、点选、曲线选择）
 * - 交互顺序
 * - 参数定义
 *
 * AlgorithmCoordinator 根据此描述自动编排执行流程，无需判断逻辑。
 */
struct AlgorithmDescriptor {
    // ==================== 基本信息 ====================
    QString name;               //!< 算法名称（如 "differentiation"）
    QString displayName;        //!< 显示名称（如 "微分"）
    QString category;           //!< 分类（如 "Analysis", "Smoothing"）

    // ==================== 交互需求（算法自描述）====================

    /**
     * @brief 是否需要参数对话框
     */
    bool needsParameters = false;

    /**
     * @brief 参数定义列表（如果 needsParameters = true）
     */
    QList<AlgorithmParameterDefinition> parameters;

    /**
     * @brief 是否需要点选交互
     */
    bool needsPointSelection = false;

    /**
     * @brief 所需点数（如果 needsPointSelection = true）
     */
    int requiredPointCount = 0;

    /**
     * @brief 点选提示信息
     */
    QString pointSelectionHint;

    /**
     * @brief 是否需要选择第二条曲线（未来扩展）
     */
    bool needsCurveSelection = false;

    /**
     * @brief 交互顺序（如果同时需要多种交互）
     *
     * 例如：
     * - ["parameters", "points"] - 先参数对话框，再点选
     * - ["points", "parameters"] - 先点选，再参数对话框
     * - ["points"] - 只需要点选
     *
     * 如果为空，则按默认顺序：parameters → points → curve
     */
    QStringList interactionOrder;

    // ==================== 依赖和输出（用于 AlgorithmContext）====================

    /**
     * @brief 执行前所需的上下文键（AlgorithmContext 内）
     *
     * 例如：["activeCurve", "param.windowSize"]
     */
    QStringList prerequisites;

    /**
     * @brief 执行完成后写回的上下文键
     *
     * 例如：["outputCurve", "result.peakArea"]
     */
    QStringList produces;
};

Q_DECLARE_METATYPE(AlgorithmDescriptor)
Q_DECLARE_METATYPE(AlgorithmParameterDefinition)

#endif // DOMAIN_ALGORITHM_DESCRIPTOR_H

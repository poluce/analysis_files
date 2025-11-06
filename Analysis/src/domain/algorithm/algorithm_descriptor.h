#ifndef DOMAIN_ALGORITHM_DESCRIPTOR_H
#define DOMAIN_ALGORITHM_DESCRIPTOR_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/**
 * @brief 定义算法交互类型
 *
 * 描述算法在执行前需要用户提供的额外输入：
 * - None             : 无需额外交互，直接执行。
 * - ParameterDialog  : 需要填写参数表单。
 * - PointSelection   : 需要在曲线上拾取固定数量的点。
 * - ParameterThenPoint : 先填写参数，再进行点选（适合依赖参数配置的拾取场景）。
 */
enum class AlgorithmInteraction {
    None,
    ParameterDialog,
    PointSelection,
    ParameterThenPoint
};

/**
 * @brief 算法参数定义
 *
 * 描述单个参数的键名、类型、默认值和校验信息。
 */
struct AlgorithmParameterDefinition {
    QString key;                 //!< 参数键名，需与 IThermalAlgorithm::setParameter 一致
    QString label;               //!< UI 展示名称
    QVariant::Type valueType = QVariant::Invalid;
    QVariant defaultValue;       //!< 默认值（用于自动执行或预填）
    bool required = true;        //!< 是否必填
    QVariantMap constraints;     //!< 约束信息（如 { "min":1, "max":100 } ）
};

/**
 * @brief 算法描述信息
 *
 * 注册时用于声明算法的交互方式、依赖条件以及输出约定。
 */
struct AlgorithmDescriptor {
    QString name;
    AlgorithmInteraction interaction = AlgorithmInteraction::None;
    QList<AlgorithmParameterDefinition> parameters;
    int requiredPointCount = 0;
    QString pointSelectionHint;
    QStringList prerequisites; //!< 执行前所需的上下文键（AlgorithmContext 内）
    QStringList produces;      //!< 执行完成后写回的上下文键
};

Q_DECLARE_METATYPE(AlgorithmDescriptor)
Q_DECLARE_METATYPE(AlgorithmParameterDefinition)

#endif // DOMAIN_ALGORITHM_DESCRIPTOR_H

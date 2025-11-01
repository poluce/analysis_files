#ifndef ITHERMALALGORITHM_H
#define ITHERMALALGORITHM_H

#include <QVector>
#include <QString>
#include <QVariantMap>
#include "domain/model/ThermalCurve.h"

// 前置声明
class ThermalDataPoint;

/**
 * @brief IThermalAlgorithm 接口为所有热分析算法定义了标准。
 *
 * 这个接口遵循策略模式，允许算法作为插件被执行和配置。
 */
class IThermalAlgorithm
{
public:
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

    /**
     * @brief 根据输入曲线类型获取输出曲线类型。
     * @param inputType 输入曲线的类型。
     * @return 输出曲线的类型。
     */
    virtual CurveType getOutputCurveType(CurveType inputType) const = 0;
};

#endif // ITHERMALALGORITHM_H

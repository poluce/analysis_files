#include "i_thermal_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include <QDebug>
#include <QPointF>

/**
 * @brief 默认实现：从上下文提取常用键，构建 QVariantMap，调用旧接口。
 *
 * 此默认实现提供向后兼容性，允许现有算法无需修改即可使用上下文接口。
 * 推荐算法重写此方法，直接从上下文拉取数据，避免中间的 QVariantMap 转换。
 */
QVariant IThermalAlgorithm::executeWithContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "executeWithContext: 上下文为空！";
        return QVariant();
    }

    // 从上下文提取常用键，构建 QVariantMap
    QVariantMap inputs;

    // 1. 提取主曲线（activeCurve 或 mainCurve）
    auto activeCurve = context->get<ThermalCurve*>("activeCurve");
    if (activeCurve.has_value()) {
        inputs["mainCurve"] = QVariant::fromValue(activeCurve.value());
    } else {
        auto mainCurve = context->get<ThermalCurve*>("mainCurve");
        if (mainCurve.has_value()) {
            inputs["mainCurve"] = QVariant::fromValue(mainCurve.value());
        }
    }

    // 2. 提取参考曲线（如果有）
    auto refCurve = context->get<ThermalCurve*>("referenceCurve");
    if (refCurve.has_value()) {
        inputs["referenceCurve"] = QVariant::fromValue(refCurve.value());
    }

    // 3. 提取选择的点（如果有）
    auto points = context->get<QVector<QPointF>>("selectedPoints");
    if (points.has_value()) {
        inputs["points"] = QVariant::fromValue(points.value());
    }

    // 4. 提取参数前缀的所有键（param.*）
    QStringList paramKeys = context->keys("param.");
    for (const QString& key : paramKeys) {
        // 去掉 "param." 前缀，直接作为参数名
        QString paramName = key.mid(6); // "param." 长度为 6
        inputs[paramName] = context->value(key);
    }

    // 5. 提取其他自定义参数（custom.*）
    QVariantMap customParams = context->values("custom.");
    if (!customParams.isEmpty()) {
        inputs["custom"] = customParams;
    }

    // 调用旧的 execute(QVariantMap) 接口
    return execute(inputs);
}

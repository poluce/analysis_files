#ifndef ALGORITHMRESULT_H
#define ALGORITHMRESULT_H

#include <QString>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QVariantMap>
#include <QVariant>
#include <QDateTime>
#include "domain/model/thermal_curve.h"

/**
 * @brief 算法输出结果类型
 */
enum class ResultType {
    Curve,          // 输出曲线（如微分、积分、滤波后的曲线）
    Marker,         // 标注点（如峰值点、外推点、拐点）
    Region,         // 区域（如峰面积、积分区域）
    ScalarValue,    // 单个数值（如外推温度、峰高、斜率）
    Composite       // 混合结果（包含多种类型）
};

/**
 * @brief 算法执行结果的统一容器
 *
 * 封装所有类型的算法输出，提供类型安全的访问接口。
 * 支持单一输出和混合输出。
 *
 * 示例用法：
 * @code
 * // 简单曲线输出（微分）
 * AlgorithmResult result = AlgorithmResult::success("differentiation", curveId, ResultType::Curve);
 * result.setCurve(outputCurve);
 * result.setMeta("unit", "mg/min");
 * return result;
 *
 * // 混合输出（峰面积）
 * AlgorithmResult result = AlgorithmResult::success("peakArea", curveId, ResultType::Composite);
 * result.addCurve(baselineCurve);
 * result.setArea(area, "J/g");
 * result.addRegion(areaPolygon, "峰面积");
 * result.addMarker(startPoint, "起点");
 * return result;
 * @endcode
 */
class AlgorithmResult {
public:
    // ==================== 构造函数 ====================

    /**
     * @brief 默认构造函数（表示失败或空结果）
     */
    AlgorithmResult()
        : m_success(false)
        , m_type(ResultType::ScalarValue)
        , m_timestamp(QDateTime::currentDateTime())
    {}

    /**
     * @brief 创建成功的结果
     */
    static AlgorithmResult success(
        const QString& algorithmKey,
        const QString& parentCurveId,
        ResultType type)
    {
        AlgorithmResult result;
        result.m_success = true;
        result.m_algorithmKey = algorithmKey;
        result.m_parentCurveId = parentCurveId;
        result.m_type = type;
        result.m_timestamp = QDateTime::currentDateTime();
        return result;
    }

    /**
     * @brief 创建失败的结果
     */
    static AlgorithmResult failure(
        const QString& algorithmKey,
        const QString& errorMessage)
    {
        AlgorithmResult result;
        result.m_success = false;
        result.m_algorithmKey = algorithmKey;
        result.m_errorMessage = errorMessage;
        result.m_timestamp = QDateTime::currentDateTime();
        return result;
    }

    // ==================== 状态查询 ====================

    bool isSuccess() const { return m_success; }
    bool hasError() const { return !m_success; }
    QString errorMessage() const { return m_errorMessage; }

    // ==================== 基本信息 ====================

    QString algorithmKey() const { return m_algorithmKey; }
    QString parentCurveId() const { return m_parentCurveId; }
    ResultType type() const { return m_type; }
    QDateTime timestamp() const { return m_timestamp; }

    // ==================== 曲线输出 ====================

    /**
     * @brief 添加输出曲线
     */
    void addCurve(const ThermalCurve& curve) {
        m_curves.append(curve);
    }

    /**
     * @brief 设置单条输出曲线（便捷方法）
     */
    void setCurve(const ThermalCurve& curve) {
        m_curves.clear();
        m_curves.append(curve);
    }

    /**
     * @brief 获取所有输出曲线
     */
    QList<ThermalCurve> curves() const { return m_curves; }

    /**
     * @brief 获取第一条曲线（用于单曲线输出）
     */
    ThermalCurve primaryCurve() const {
        return m_curves.isEmpty() ? ThermalCurve() : m_curves.first();
    }

    bool hasCurves() const { return !m_curves.isEmpty(); }
    int curveCount() const { return m_curves.size(); }

    // ==================== 标注点输出 ====================

    void addMarker(const QPointF& point, const QString& label = QString()) {
        m_markers.append(point);
        if (!label.isEmpty()) {
            m_meta[QString("marker.%1.label").arg(m_markers.size() - 1)] = label;
        }
    }

    void setMarkers(const QList<QPointF>& markers) { m_markers = markers; }
    QList<QPointF> markers() const { return m_markers; }
    bool hasMarkers() const { return !m_markers.isEmpty(); }
    int markerCount() const { return m_markers.size(); }

    // ==================== 区域输出 ====================

    void addRegion(const QPolygonF& region, const QString& label = QString()) {
        m_regions.append(region);
        if (!label.isEmpty()) {
            m_meta[QString("region.%1.label").arg(m_regions.size() - 1)] = label;
        }
    }

    void setRegions(const QList<QPolygonF>& regions) { m_regions = regions; }
    QList<QPolygonF> regions() const { return m_regions; }
    bool hasRegions() const { return !m_regions.isEmpty(); }
    int regionCount() const { return m_regions.size(); }

    // ==================== 元数据访问 ====================

    /**
     * @brief 设置元数据（键值对）
     *
     * 常用键名：
     * - "area"              : 峰面积 (double)
     * - "peakHeight"        : 峰高 (double)
     * - "peakPosition"      : 峰位置 (QPointF)
     * - "slope"             : 斜率 (double)
     * - "intercept"         : 截距 (double)
     * - "onset"             : 起始温度 (double)
     * - "endset"            : 结束温度 (double)
     * - "unit"              : 单位 (QString)
     * - "label"             : 标签 (QString)
     * - "color"             : 显示颜色 (QColor)
     * - "signalType"        : 信号类型 (SignalType)
     */
    void setMeta(const QString& key, const QVariant& value) {
        m_meta[key] = value;
    }

    QVariant meta(const QString& key, const QVariant& defaultValue = QVariant()) const {
        return m_meta.value(key, defaultValue);
    }

    template<typename T>
    T metaValue(const QString& key, const T& defaultValue = T()) const {
        return m_meta.value(key, QVariant::fromValue(defaultValue)).template value<T>();
    }

    QVariantMap allMeta() const { return m_meta; }
    bool hasMeta(const QString& key) const { return m_meta.contains(key); }

    // ==================== 便捷方法（常用标量值）====================

    void setArea(double area, const QString& unit = "J/g") {
        m_meta["area"] = area;
        m_meta["unit"] = unit;
    }

    double area() const { return metaValue<double>("area", 0.0); }

    void setPeakPosition(const QPointF& pos) {
        m_meta["peakPosition"] = pos;
    }

    QPointF peakPosition() const {
        return metaValue<QPointF>("peakPosition", QPointF());
    }

    void setSignalType(SignalType type) {
        m_meta["signalType"] = QVariant::fromValue(type);
    }

    SignalType signalType() const {
        return metaValue<SignalType>("signalType", SignalType::Raw);
    }

    // ==================== 调试输出 ====================

    QString toString() const {
        QString str = QString("[AlgorithmResult]\n");
        str += QString("  Algorithm: %1\n").arg(m_algorithmKey);
        str += QString("  Success: %1\n").arg(m_success ? "Yes" : "No");
        if (!m_success) {
            str += QString("  Error: %1\n").arg(m_errorMessage);
        }
        str += QString("  Type: %1\n").arg(resultTypeToString(m_type));
        str += QString("  Curves: %1\n").arg(m_curves.size());
        str += QString("  Markers: %1\n").arg(m_markers.size());
        str += QString("  Regions: %1\n").arg(m_regions.size());
        str += QString("  Meta: %1 items\n").arg(m_meta.size());
        str += QString("  Timestamp: %1\n").arg(m_timestamp.toString(Qt::ISODate));
        return str;
    }

private:
    // 执行状态
    bool m_success;
    QString m_errorMessage;

    // 基本信息
    QString m_algorithmKey;         // 算法标识（如 "differentiation"）
    QString m_parentCurveId;        // 来源曲线ID
    ResultType m_type;              // 输出类型
    QDateTime m_timestamp;          // 生成时间

    // 输出数据
    QList<ThermalCurve> m_curves;   // 曲线型输出
    QList<QPointF> m_markers;       // 点标注输出（如峰值、外推点）
    QList<QPolygonF> m_regions;     // 区域输出（如峰面积）
    QVariantMap m_meta;             // 元数据（数值、单位、标签等）

    static QString resultTypeToString(ResultType type) {
        switch (type) {
        case ResultType::Curve: return "Curve";
        case ResultType::Marker: return "Marker";
        case ResultType::Region: return "Region";
        case ResultType::ScalarValue: return "ScalarValue";
        case ResultType::Composite: return "Composite";
        default: return "Unknown";
        }
    }
};

#endif // ALGORITHMRESULT_H

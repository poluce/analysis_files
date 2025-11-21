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
    Curve,          // 输出曲线
    Marker,         // 标注点
    Region,         // 区域
    ScalarValue,    // 数值
    Composite       // 混合结果
};

/**
 * @brief 算法输出上下文键名（用于 AlgorithmContext 存储）
 */
namespace OutputKeys {
    inline constexpr const char* ResultPrefix = "result/";
    inline constexpr const char* ByTaskInfix = "/byTask/";
    inline constexpr const char* ByCurveInfix = "/byCurve/";
    inline constexpr const char* LatestTaskIdSuffix = "/latestTaskId";
    inline constexpr const char* HistoryTaskIdsSuffix = "/historyTaskIds";

    /// 生成按任务ID存储的键名: "result/{algorithm}/byTask/{taskId}"
    inline QString byTask(const QString& algorithm, const QString& taskId) {
        return QString(ResultPrefix) + algorithm + QString(ByTaskInfix) + taskId;
    }

    /// 生成曲线最新任务ID键名: "result/{algorithm}/byCurve/{curveId}/latestTaskId"
    inline QString latestTaskId(const QString& algorithm, const QString& curveId) {
        return QString(ResultPrefix) + algorithm + QString(ByCurveInfix) + curveId + QString(LatestTaskIdSuffix);
    }

    /// 生成曲线历史任务ID列表键名: "result/{algorithm}/byCurve/{curveId}/historyTaskIds"
    inline QString historyTaskIds(const QString& algorithm, const QString& curveId) {
        return QString(ResultPrefix) + algorithm + QString(ByCurveInfix) + curveId + QString(HistoryTaskIdsSuffix);
    }
}

/**
 * @brief 算法输出声明键名（用于 AlgorithmDescriptor.produces）
 */
namespace ProducesKeys {
    inline constexpr const char* Curve = "curve";
    inline constexpr const char* Curves = "curves";
    inline constexpr const char* Markers = "markers";
    inline constexpr const char* Scalar = "scalar";
}

/**
 * @brief 算法结果元数据键名（用于 AlgorithmResult.meta）
 */
namespace MetaKeys {
    // 数值
    inline constexpr const char* Area = "area";                  // double
    inline constexpr const char* Slope = "slope";                // double
    inline constexpr const char* Intercept = "intercept";        // double

    // 温度外推
    inline constexpr const char* ExtrapolatedTemperature = "extrapolatedTemperature";  // double
    inline constexpr const char* Confidence = "confidence";                    // double
    inline constexpr const char* Warning = "warning";                          // QString

    // 基线拟合
    inline constexpr const char* BaselineSlope = "baseline.slope";             // double
    inline constexpr const char* BaselineIntercept = "baseline.intercept";     // double
    inline constexpr const char* BaselineR2 = "baseline.r2";                   // double
    inline constexpr const char* BaselineSlopeNormalized = "baseline.slopeNormalized";  // double
    inline constexpr const char* BaselineMethod = "baseline.method";           // QString

    // 拐点
    inline constexpr const char* InflectionTemperature = "inflection.temperature";  // double
    inline constexpr const char* InflectionValue = "inflection.value";              // double
    inline constexpr const char* InflectionSlope = "inflection.slope";              // double

    // 算法参数
    inline constexpr const char* Method = "method";              // QString
    inline constexpr const char* WindowSize = "windowSize";      // int
    inline constexpr const char* HalfWin = "halfWin";            // int
    inline constexpr const char* Dt = "dt";                      // double
    inline constexpr const char* InstrumentType = "instrumentType";  // int
    inline constexpr const char* CorrectionType = "correctionType";  // QString
    inline constexpr const char* BaselinePointCount = "baselinePointCount";  // int
    inline constexpr const char* TemperatureRange = "temperatureRange";      // QString

    // 显示
    inline constexpr const char* Unit = "unit";                  // QString
    inline constexpr const char* Label = "label";                // QString
    inline constexpr const char* MarkerColor = "markerColor";    // QColor
    inline constexpr const char* SignalType = "signalType";      // SignalType

    /// 生成标注点标签键名: "marker.{index}.label"
    inline QString markerLabel(int index) {
        return QString("marker.%1.label").arg(index);
    }

    /// 生成区域标签键名: "region.{index}.label"
    inline QString regionLabel(int index) {
        return QString("region.%1.label").arg(index);
    }
}

/**
 * @brief 算法执行结果的统一容器
 *
 * 封装所有类型的算法输出，支持曲线、标注点、区域和元数据。
 */
class AlgorithmResult {
public:
    // ==================== 构造 ====================

    /// 默认构造（失败状态）
    AlgorithmResult()
        : m_success(false)
        , m_type(ResultType::ScalarValue)
        , m_timestamp(QDateTime::currentDateTime())
    {}

    /// 创建成功结果
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

    /// 创建失败结果
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

    // ==================== 状态 ====================

    bool isSuccess() const { return m_success; }
    bool hasError() const { return !m_success; }
    QString errorMessage() const { return m_errorMessage; }

    // ==================== 基本信息 ====================

    QString algorithmKey() const { return m_algorithmKey; }
    QString parentCurveId() const { return m_parentCurveId; }
    ResultType type() const { return m_type; }
    QDateTime timestamp() const { return m_timestamp; }

    // ==================== 曲线 ====================

    void setCurve(const ThermalCurve& curve) {
        m_curves.clear();
        m_curves.append(curve);
    }

    void addCurve(const ThermalCurve& curve) {
        m_curves.append(curve);
    }

    QList<ThermalCurve> curves() const { return m_curves; }
    bool hasCurves() const { return !m_curves.isEmpty(); }
    int curveCount() const { return m_curves.size(); }

    // ==================== 标注点 ====================

    void addMarker(const QPointF& point, const QString& label = QString()) {
        m_markers.append(point);
        if (!label.isEmpty()) {
            m_meta[MetaKeys::markerLabel(m_markers.size() - 1)] = label;
        }
    }

    QList<QPointF> markers() const { return m_markers; }
    bool hasMarkers() const { return !m_markers.isEmpty(); }
    int markerCount() const { return m_markers.size(); }

    // ==================== 区域 ====================

    void addRegion(const QPolygonF& region, const QString& label = QString()) {
        m_regions.append(region);
        if (!label.isEmpty()) {
            m_meta[MetaKeys::regionLabel(m_regions.size() - 1)] = label;
        }
    }

    bool hasRegions() const { return !m_regions.isEmpty(); }
    int regionCount() const { return m_regions.size(); }

    // ==================== 元数据 ====================

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

    // ==================== 便捷方法 ====================

    void setArea(double area, const QString& unit = "J/g") {
        m_meta[MetaKeys::Area] = area;
        m_meta[MetaKeys::Unit] = unit;
    }

    double area() const { return metaValue<double>(MetaKeys::Area, 0.0); }

    void setSignalType(SignalType type) {
        m_meta[MetaKeys::SignalType] = QVariant::fromValue(type);
    }

    // ==================== 调试 ====================

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
    bool m_success;
    QString m_errorMessage;

    QString m_algorithmKey;
    QString m_parentCurveId;
    ResultType m_type;
    QDateTime m_timestamp;

    QList<ThermalCurve> m_curves;
    QList<QPointF> m_markers;
    QList<QPolygonF> m_regions;
    QVariantMap m_meta;

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

Q_DECLARE_METATYPE(AlgorithmResult)
Q_DECLARE_METATYPE(ResultType)

#endif // ALGORITHMRESULT_H

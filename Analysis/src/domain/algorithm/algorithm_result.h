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
 * @brief 算法输出上下文键名
 *
 * 为算法结果在 AlgorithmContext 中的存储提供类型安全的键名常量。
 * 与输入键名（ContextKeys）分离，专门用于算法输出结果的存储和检索。
 *
 * @code
 * // 推荐使用（通过 AlgorithmContext 高层API）：
 * context->saveResult("task-001", "differentiation", "curve-001", result);
 * auto latest = context->latestResult("differentiation", "curve-001");
 *
 * // 或使用底层键名（仅用于特殊场景）：
 * QString key = OutputKeys::byTask("differentiation", "task-001");
 * context->setValue(key, QVariant::fromValue(result));
 * @endcode
 */
namespace OutputKeys {
    /** 结果键名前缀 */
    inline constexpr const char* ResultPrefix = "result/";

    /** 按任务ID索引中缀 */
    inline constexpr const char* ByTaskInfix = "/byTask/";

    /** 按曲线ID索引中缀 */
    inline constexpr const char* ByCurveInfix = "/byCurve/";

    /** 最新任务ID后缀 */
    inline constexpr const char* LatestTaskIdSuffix = "/latestTaskId";

    /** 历史任务ID后缀 */
    inline constexpr const char* HistoryTaskIdsSuffix = "/historyTaskIds";

    /**
     * @brief 生成按任务ID存储的结果键名（主存储）
     * @param algorithm 算法名称
     * @param taskId 任务ID
     * @return 完整键名 (格式: "result/{algorithm}/byTask/{taskId}")
     *
     * 使用示例:
     * @code
     * // 存储算法结果（不可覆盖）
     * QString key = OutputKeys::byTask("differentiation", "task-001");
     * context->setValue(key, QVariant::fromValue(result));
     *
     * // 读取特定任务的结果
     * auto result = context->get<AlgorithmResult>(key);
     * @endcode
     */
    inline QString byTask(const QString& algorithm, const QString& taskId) {
        return QString(ResultPrefix) + algorithm + QString(ByTaskInfix) + taskId;
    }

    /**
     * @brief 生成曲线最新任务ID键名（索引）
     * @param algorithm 算法名称
     * @param curveId 曲线ID
     * @return 完整键名 (格式: "result/{algorithm}/byCurve/{curveId}/latestTaskId")
     *
     * 使用示例:
     * @code
     * // 存储最新任务ID指针
     * QString key = OutputKeys::latestTaskId("differentiation", "curve-001");
     * context->setValue(key, "task-003");
     *
     * // 读取最新任务ID
     * auto latestTaskId = context->get<QString>(key);
     * @endcode
     */
    inline QString latestTaskId(const QString& algorithm, const QString& curveId) {
        return QString(ResultPrefix) + algorithm + QString(ByCurveInfix) + curveId + QString(LatestTaskIdSuffix);
    }

    /**
     * @brief 生成曲线历史任务ID列表键名（索引）
     * @param algorithm 算法名称
     * @param curveId 曲线ID
     * @return 完整键名 (格式: "result/{algorithm}/byCurve/{curveId}/historyTaskIds")
     *
     * 使用示例:
     * @code
     * // 存储历史任务ID列表
     * QString key = OutputKeys::historyTaskIds("differentiation", "curve-001");
     * context->setValue(key, QStringList{"task-003", "task-002", "task-001"});
     *
     * // 读取历史任务ID列表
     * auto historyIds = context->get<QStringList>(key);
     * @endcode
     */
    inline QString historyTaskIds(const QString& algorithm, const QString& curveId) {
        return QString(ResultPrefix) + algorithm + QString(ByCurveInfix) + curveId + QString(HistoryTaskIdsSuffix);
    }

}

/**
 * @brief 算法输出声明键名
 *
 * 为 AlgorithmDescriptor 的 produces 字段提供类型安全的键名常量。
 * 统一管理算法输出类型声明，避免字符串拼写错误。
 *
 * @code
 * // 推荐使用：
 * desc.produces.append(ProducesKeys::Curve);
 * desc.produces.append(ProducesKeys::Markers);
 *
 * // 避免使用：
 * desc.produces.append("curve");  // 容易拼写错误
 * @endcode
 */
namespace ProducesKeys {
    inline constexpr const char* Curve = "curve";       // 单条曲线输出
    inline constexpr const char* Curves = "curves";     // 多条曲线输出
    inline constexpr const char* Markers = "markers";   // 标注点输出
    inline constexpr const char* Scalar = "scalar";     // 数值输出
    inline constexpr const char* Region = "region";     // 区域输出
}

/**
 * @brief 算法结果元数据键名
 *
 * 为 AlgorithmResult 的元数据字段提供类型安全的键名常量。
 * 统一管理所有元数据的键名，避免字符串拼写错误。
 *
 * @code
 * // 推荐使用：
 * result.setMeta(MetaKeys::Area, 123.45);
 * result.setMeta(MetaKeys::Unit, "J/g");
 *
 * // 避免使用：
 * result.setMeta("area", 123.45);  // 容易拼写错误
 * @endcode
 */
namespace MetaKeys {
    // 峰值分析相关
    inline constexpr const char* Area = "area";                  // 峰面积 (double)
    inline constexpr const char* PeakHeight = "peakHeight";      // 峰高 (double)
    inline constexpr const char* PeakPosition = "peakPosition";  // 峰位置 (QPointF)

    // 线性拟合相关
    inline constexpr const char* Slope = "slope";                // 斜率 (double)
    inline constexpr const char* Intercept = "intercept";        // 截距 (double)

    // 温度外推相关
    inline constexpr const char* Onset = "onset";                       // 起始温度 (double)
    inline constexpr const char* Endset = "endset";                     // 结束温度 (double)
    inline constexpr const char* ExtrapolatedTemperature = "extrapolatedTemperature";  // 外推温度 (double)
    inline constexpr const char* Confidence = "confidence";             // 置信度 (double)
    inline constexpr const char* Warning = "warning";                   // 警告信息 (QString)

    // 基线拟合相关
    inline constexpr const char* BaselineSlope = "baseline.slope";                 // 基线斜率 (double)
    inline constexpr const char* BaselineIntercept = "baseline.intercept";         // 基线截距 (double)
    inline constexpr const char* BaselineR2 = "baseline.r2";                       // 基线 R² (double)
    inline constexpr const char* BaselineSlopeNormalized = "baseline.slopeNormalized";  // 归一化斜率 (double)
    inline constexpr const char* BaselineMethod = "baseline.method";               // 基线拟合方法 (QString)

    // 拐点相关
    inline constexpr const char* InflectionTemperature = "inflection.temperature"; // 拐点温度 (double)
    inline constexpr const char* InflectionValue = "inflection.value";             // 拐点值 (double)
    inline constexpr const char* InflectionSlope = "inflection.slope";             // 拐点斜率 (double)

    // 算法参数
    inline constexpr const char* Method = "method";                     // 算法方法 (QString)
    inline constexpr const char* WindowSize = "windowSize";             // 窗口大小 (int)

    // 关联曲线信息
    inline constexpr const char* BaselineCurveId = "baselineCurveId";       // 基线曲线ID (QString)
    inline constexpr const char* BaselineCurveName = "baselineCurveName";   // 基线曲线名称 (QString)
    inline constexpr const char* InstrumentType = "instrumentType";         // 仪器类型 (int)

    // 显示相关
    inline constexpr const char* Unit = "unit";                  // 单位 (QString)
    inline constexpr const char* Label = "label";                // 标签 (QString)
    inline constexpr const char* Color = "color";                // 显示颜色 (QColor)
    inline constexpr const char* MarkerColor = "markerColor";    // 标注点颜色 (QColor)

    // 曲线类型
    inline constexpr const char* SignalType = "signalType";      // 信号类型 (SignalType)

    /**
     * @brief 生成标注点标签键名
     * @param index 标注点索引
     * @return 完整键名 (格式: "marker.{index}.label")
     */
    inline QString markerLabel(int index) {
        return QString("marker.%1.label").arg(index);
    }

    /**
     * @brief 生成区域标签键名
     * @param index 区域索引
     * @return 完整键名 (格式: "region.{index}.label")
     */
    inline QString regionLabel(int index) {
        return QString("region.%1.label").arg(index);
    }
}

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
     * @brief 设置单条输出曲线（便捷方法）
     */
    void setCurve(const ThermalCurve& curve) {
        m_curves.clear();
        m_curves.append(curve);
    }

    /**
     * @brief 添加一条输出曲线
     */
    void addCurve(const ThermalCurve& curve) {
        m_curves.append(curve);
    }

    /**
     * @brief 获取所有输出曲线
     */
    QList<ThermalCurve> curves() const { return m_curves; }

    bool hasCurves() const { return !m_curves.isEmpty(); }
    int curveCount() const { return m_curves.size(); }

    // ==================== 标注点输出 ====================

    void addMarker(const QPointF& point, const QString& label = QString()) {
        m_markers.append(point);
        if (!label.isEmpty()) {
            m_meta[MetaKeys::markerLabel(m_markers.size() - 1)] = label;
        }
    }

    QList<QPointF> markers() const { return m_markers; }
    bool hasMarkers() const { return !m_markers.isEmpty(); }
    int markerCount() const { return m_markers.size(); }

    // ==================== 区域输出 ====================

    void addRegion(const QPolygonF& region, const QString& label = QString()) {
        m_regions.append(region);
        if (!label.isEmpty()) {
            m_meta[MetaKeys::regionLabel(m_regions.size() - 1)] = label;
        }
    }

    bool hasRegions() const { return !m_regions.isEmpty(); }
    int regionCount() const { return m_regions.size(); }

    // ==================== 元数据访问 ====================

    /**
     * @brief 设置元数据（键值对）
     *
     * 推荐使用 MetaKeys 命名空间中的常量作为键名：
     * - MetaKeys::Area              : 峰面积 (double)
     * - MetaKeys::PeakHeight        : 峰高 (double)
     * - MetaKeys::PeakPosition      : 峰位置 (QPointF)
     * - MetaKeys::Slope             : 斜率 (double)
     * - MetaKeys::Intercept         : 截距 (double)
     * - MetaKeys::Onset             : 起始温度 (double)
     * - MetaKeys::Endset            : 结束温度 (double)
     * - MetaKeys::Unit              : 单位 (QString)
     * - MetaKeys::Label             : 标签 (QString)
     * - MetaKeys::Color             : 显示颜色 (QColor)
     * - MetaKeys::SignalType        : 信号类型 (SignalType)
     *
     * 使用示例:
     * @code
     * result.setMeta(MetaKeys::Area, 123.45);
     * result.setMeta(MetaKeys::Unit, "J/g");
     * @endcode
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
        m_meta[MetaKeys::Area] = area;
        m_meta[MetaKeys::Unit] = unit;
    }

    double area() const { return metaValue<double>(MetaKeys::Area, 0.0); }

    void setSignalType(SignalType type) {
        m_meta[MetaKeys::SignalType] = QVariant::fromValue(type);
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

Q_DECLARE_METATYPE(AlgorithmResult)
Q_DECLARE_METATYPE(ResultType)

#endif // ALGORITHMRESULT_H

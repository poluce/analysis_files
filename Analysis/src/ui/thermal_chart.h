#ifndef THERMAL_CHART_H
#define THERMAL_CHART_H

#include <QChart>
#include <QHash>
#include <QMap>
#include <QVector>
#include <QPointF>
#include <QColor>

QT_CHARTS_USE_NAMESPACE

class QValueAxis;
class QLineSeries;
class QScatterSeries;
class QGraphicsLineItem;
class QGraphicsObject;
class ThermalCurve;
class CurveManager;
class FloatingLabel;

/**
 * @brief 定义横轴显示模式
 */
enum class XAxisMode {
    Temperature, // 温度为横轴（默认）
    Time         // 时间为横轴
};

/**
 * @brief ThermalChart - 自定义图表类
 *
 * 职责：
 * 1. 管理曲线系列（增删改查）
 * 2. 管理坐标轴（X/Y 轴，缩放）
 * 3. 管理叠加物（浮动标签、标注点、测量工具、注释线）
 * 4. 提供数据查询接口
 *
 * 设计原则：
 * - 只管理数据和图元，不处理用户交互
 * - 不依赖 QChartView，所有操作基于 QChart API
 * - 通过信号通知状态变化，让关注者自行响应
 */
class ThermalChart : public QChart {
    Q_OBJECT

public:
    explicit ThermalChart(QGraphicsItem* parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags());
    ~ThermalChart();

    // ==================== 外部依赖注入 ====================
    void setCurveManager(CurveManager* manager);

    // ==================== 曲线管理 ====================
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setCurveVisible(const QString& curveId, bool visible);
    void highlightCurve(const QString& curveId);

    // ==================== 坐标轴管理 ====================
    void rescaleAxes();
    void setXAxisMode(XAxisMode mode);
    XAxisMode xAxisMode() const { return m_xAxisMode; }

    // 查询接口（供 ThermalChartView 和叠加物使用）
    QValueAxis* axisX() const { return m_axisX; }
    QValueAxis* primaryAxisY() const { return m_axisY_primary; }
    QValueAxis* secondaryAxisY() const { return m_axisY_secondary; }
    QValueAxis* yAxisForCurve(const QString& curveId);

    // ==================== 系列查询 ====================
    QLineSeries* seriesForCurve(const QString& curveId) const;
    QString curveIdForSeries(QLineSeries* series) const;
    QColor getCurveColor(const QString& curveId) const;

    // ==================== 十字线管理（仅图元接口）====================
    void setCrosshairEnabled(bool vertical, bool horizontal);
    void updateCrosshairAtChartPos(const QPointF& chartPos);
    void clearCrosshair();

    // ==================== 浮动标签管理 ====================
    FloatingLabel* addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId);
    FloatingLabel* addFloatingLabelHUD(const QString& text, const QPointF& viewPos);
    void removeFloatingLabel(FloatingLabel* label);
    void clearFloatingLabels();
    const QVector<FloatingLabel*>& floatingLabels() const { return m_floatingLabels; }

    // ==================== 标注点（Markers）管理 ====================
    void addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                         const QColor& color = Qt::red, qreal size = 12.0);
    void removeCurveMarkers(const QString& curveId);
    void clearAllMarkers();

    // ==================== 测量工具管理 ====================
    void addMassLossTool(const struct ThermalDataPoint& point1,
                         const struct ThermalDataPoint& point2,
                         const QString& curveId);
    void removeMassLossTool(QGraphicsObject* tool);
    void clearAllMassLossTools();

    // ==================== 注释线管理 ====================
    struct AnnotationLine {
        QString id;
        QString curveId;
        QPointF start;
        QPointF end;
        QPen pen;
    };

    void addAnnotationLine(const QString& id, const QString& curveId,
                           const QPointF& start, const QPointF& end,
                           const QPen& pen = QPen(Qt::red, 2));
    void removeAnnotation(const QString& id);
    void clearAllAnnotations();

    // ==================== 选中点管理（用于算法交互）====================
    void setupSelectedPointsSeries(QValueAxis* targetYAxis);
    void addSelectedPoint(const QPointF& point);
    void clearSelectedPoints();

signals:
    /**
     * @brief 横轴模式改变信号
     *
     * 当横轴模式切换时发出，通知叠加物更新显示位置
     */
    void xAxisModeChanged(XAxisMode mode);

    /**
     * @brief 曲线可见性改变信号
     */
    void curveVisibilityChanged(const QString& curveId, bool visible);

private:
    // ==================== 系列管理辅助函数 ====================
    QLineSeries* createSeriesForCurve(const ThermalCurve& curve) const;
    void populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const;
    void attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY);
    void detachSeriesFromAxes(QLineSeries* series);
    void registerSeriesMapping(QLineSeries* series, const QString& curveId);
    void unregisterSeriesMapping(const QString& curveId);
    void updateSeriesStyle(QLineSeries* series, bool selected);
    void updateLegendVisibility(QLineSeries* series, bool visible) const;

    // ==================== 坐标轴管理辅助函数 ====================
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);
    void rescaleAxis(QValueAxis* axis) const;
    void resetAxesToDefault();

    // ==================== 数据查询辅助函数 ====================
    struct ThermalDataPoint findNearestDataPoint(const QVector<struct ThermalDataPoint>& curveData,
                                                   double temperature) const;

private:
    // ==================== 坐标轴 ====================
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_primary = nullptr;
    QValueAxis* m_axisY_secondary = nullptr;
    XAxisMode m_xAxisMode = XAxisMode::Temperature;

    // ==================== 曲线系列管理 ====================
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;
    QLineSeries* m_selectedSeries = nullptr;

    // ==================== 十字线 ====================
    QGraphicsLineItem* m_verticalCrosshairLine = nullptr;
    QGraphicsLineItem* m_horizontalCrosshairLine = nullptr;
    bool m_verticalCrosshairEnabled = false;
    bool m_horizontalCrosshairEnabled = false;

    // ==================== 选中点高亮系列（算法交互）====================
    QScatterSeries* m_selectedPointsSeries = nullptr;

    // ==================== 标注点（Markers）====================
    struct CurveMarkerData {
        QScatterSeries* series;
        QVector<struct ThermalDataPoint> dataPoints;
    };
    QMap<QString, CurveMarkerData> m_curveMarkers;

    // ==================== 浮动标签 ====================
    QVector<FloatingLabel*> m_floatingLabels;

    // ==================== 测量工具 ====================
    QVector<QGraphicsObject*> m_massLossTools;

    // ==================== 注释线 ====================
    QVector<AnnotationLine> m_annotations;

    // ==================== 外部依赖 ====================
    CurveManager* m_curveManager = nullptr;
};

#endif // THERMAL_CHART_H

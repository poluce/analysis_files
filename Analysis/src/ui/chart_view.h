#ifndef CHARTVIEW_H
#define CHARTVIEW_H

#include <QHash>
#include <QMouseEvent>
#include <QPen>
#include <QPointF>
#include <QVector>
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class ThermalCurve;
class QPainter;
class QGraphicsLineItem;

QT_CHARTS_USE_NAMESPACE

/**
 * @brief 定义图表的交互模式
 *
 * 控制用户当前与图表的交互方式：
 * - View: 普通视图操作（缩放、平移、区域选择）
 * - Pick: 拾取或测量模式（点击、标注、分析）
 */
enum class InteractionMode {
    View, // 视图模式：缩放、平移、放大镜、框选
    Pick  // 拾取模式：选择数据点、测量、显示坐标
};

class ChartView : public QWidget {
    Q_OBJECT
public:
    explicit ChartView(QWidget* parent = nullptr);

    void setHitTestBasePixelThreshold(qreal px);
    qreal hitTestBasePixelThreshold() const;

    void setHitTestIncludePenWidth(bool enabled);
    void setPickCount(int count);
    bool hitTestIncludePenWidth() const;
    void setInteractionMode(InteractionMode type);

    void addAnnotationLine(
        const QString& id, const QString& curveId, const QPointF& start, const QPointF& end, const QPen& pen = QPen(Qt::red, 2));
    void removeAnnotation(const QString& id);
    void clearAllAnnotations();

public slots:
    void addCurve(const ThermalCurve& curve);
    void updateCurve(const ThermalCurve& curve);
    void removeCurve(const QString& curveId);
    void clearCurves();
    void setVerticalCrosshairEnabled(bool enabled);
    void setHorizontalCrosshairEnabled(bool enabled);
    void rescaleAxes();
    void setCurveVisible(const QString& curveId, bool visible);

public:
    bool verticalCrosshairEnabled() const { return m_verticalCrosshairEnabled; }
    bool horizontalCrosshairEnabled() const { return m_horizontalCrosshairEnabled; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void curveSelected(const QString& curveId);
    /**
     * @brief 在拾取点的模式中当用户拾取够了设置的点数时排序后发出。
     * @param outputs 包含拾取点信息横轴位置：
     *  - "x1" point1
     *  - "x2" point2
     *  - "x3" point3
     */
    void pickPoints(const QVector<float>& outputs);

private:
    void updateSeriesStyle(QLineSeries* series, bool selected);
    QValueAxis* ensureYAxisForCurve(const ThermalCurve& curve);

    void drawAnnotationLines(QPainter& painter);
    void handleCurveSelectionClick(const QPointF& chartPos);
    QPointF mapToChartCoordinates(const QPoint& widgetPos) const;
    QLineSeries* findSeriesNearPoint(const QPointF& chartPos, qreal& outDistance) const;
    // --- 曲线系列管理：封装 QLineSeries 的创建、数据填充与映射维护 ---
    QLineSeries* seriesForCurve(const QString& curveId) const;
    // 根据 ThermalCurve 构建新的系列实例并写入基础属性
    QLineSeries* createSeriesForCurve(const ThermalCurve& curve) const;
    // 将 ThermalCurve 数据点批量写入指定系列
    void populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const;
    // 将系列附着到 X 轴与目标 Y 轴
    void attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY);
    // 将系列从所有已附着的坐标轴解绑
    void detachSeriesFromAxes(QLineSeries* series);
    // 记录系列与曲线 ID 的双向映射关系
    void registerSeriesMapping(QLineSeries* series, const QString& curveId);
    // 移除系列与曲线 ID 的双向映射关系
    void unregisterSeriesMapping(const QString& curveId);
    // 同步图例项的可见性
    void updateLegendVisibility(QLineSeries* series, bool visible) const;
    // --- 坐标轴管理：根据现有数据动态调整尺度并在清空时恢复默认 ---
    void rescaleAxis(QValueAxis* axis) const;
    void resetAxesToDefault();
    // --- 十字线管理：维护鼠标位置与十字线可见性 ---
    void updateCrosshairVisibility();
    void updateCrosshairPosition(const QPointF& viewportPos);
    void clearCrosshair();

private:
    QChartView* m_chartView;
    QLineSeries* m_selectedSeries;
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;

    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY_primary = nullptr;
    QValueAxis* m_axisY_secondary = nullptr;

    qreal m_hitTestBasePx = 8.0;
    bool m_hitTestIncludePen = true;
    bool m_verticalCrosshairEnabled = false;
    bool m_horizontalCrosshairEnabled = false;
    bool m_hasMousePosition = false;
    QGraphicsLineItem* m_verticalCrosshairLine = nullptr;
    QGraphicsLineItem* m_horizontalCrosshairLine = nullptr;

    struct AnnotationLine {
        QString id;
        QString curveId;
        QPointF start;
        QPointF end;
        QPen pen;
    };
    InteractionMode m_mode; // 切换视图和拾取点的模式
    int m_pickCount;
    QVector<float> m_pickPoints;

    QVector<AnnotationLine> m_annotations;
};

#endif // CHARTVIEW_H

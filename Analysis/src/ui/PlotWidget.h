#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QMouseEvent>
#include <QPen>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QHash>
#include <QVector>
#include <QPair>

class ThermalCurve;
class CurveManager;

QT_CHARTS_USE_NAMESPACE

// 交互模式枚举
enum class InteractionMode {
    Normal,      // 正常模式：点击选择曲线
    PickPoints   // 拾取点模式：点击获取坐标
};

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(CurveManager* curveManager, QWidget *parent = nullptr);

    // 配置接口 —— 点击命中阈值（像素）
    // 基础像素阈值，默认 8px（会再乘以屏幕 devicePixelRatio）。
    void setHitTestBasePixelThreshold(qreal px);
    qreal hitTestBasePixelThreshold() const;

    // 是否把线条笔宽纳入命中阈值（提高粗线的可点中性），默认 true。
    void setHitTestIncludePenWidth(bool enabled);
    bool hitTestIncludePenWidth() const;

    // 点拾取模式接口
    void startPointPicking(int numPoints);
    void cancelPointPicking();
    InteractionMode interactionMode() const;

    // 注释线管理
    void addAnnotationLine(const QString& id,
                          const QString& curveId,
                          const QPointF& start,
                          const QPointF& end,
                          const QPen& pen = QPen(Qt::red, 2));
    void removeAnnotation(const QString& id);
    void clearAllAnnotations();

public slots:
    void addCurve(const ThermalCurve& curve);
    void rescaleAxes(); // 重新缩放所有坐标轴以适应当前可见曲线
    void setCurveVisible(const QString& curveId, bool visible);

private slots:
    // 响应 CurveManager 的信号
    void onCurveAdded(const QString& curveId);
    void onCurveDataChanged(const QString& curveId);
    void onCurveRemoved(const QString& curveId);
    void onCurvesCleared();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void curveSelected(const QString& curveId);

    // 点拾取相关信号
    void pointsPickedOnCurve(const QString& curveId,
                             const QVector<QPointF>& points);
    void pointPickingProgress(int picked, int total);
    void pointPickingCancelled();

private:
    void updateSeriesStyle(QLineSeries *series, bool selected);
    void setupConnections();

    // 点拾取辅助方法
    QPair<QString, QPointF> findNearestPointOnCurves(const QPointF& screenPos);
    void drawPointMarkers(QPainter& painter);
    void drawAnnotationLines(QPainter& painter);

    CurveManager* m_curveManager;  // 非拥有指针
    QChartView *m_chartView;
    QLineSeries *m_selectedSeries;
    QHash<QLineSeries*, QString> m_seriesToId;
    QHash<QString, QLineSeries*> m_idToSeries;

    // 坐标轴
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY_primary = nullptr;
    QValueAxis *m_axisY_secondary = nullptr;

    // 命中判定相关配置
    qreal m_hitTestBasePx = 8.0;
    bool m_hitTestIncludePen = true;

    // 点拾取状态
    InteractionMode m_interactionMode = InteractionMode::Normal;
    bool m_isPickingPoints = false;
    int m_numPointsNeeded = 0;
    QVector<QPointF> m_pickedPoints;
    QString m_targetCurveId;  // 在哪条曲线上拾取

    // 注释线数据结构
    struct AnnotationLine {
        QString id;
        QString curveId;  // 关联的曲线ID，用于确定坐标系
        QPointF start;    // 数据坐标
        QPointF end;      // 数据坐标
        QPen pen;
    };
    QVector<AnnotationLine> m_annotations;
};

#endif // PLOTWIDGET_H

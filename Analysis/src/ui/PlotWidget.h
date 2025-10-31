#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>
#include <QMouseEvent>
#include <QPen>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QHash>

class ThermalCurve;
class CurveManager;

QT_CHARTS_USE_NAMESPACE

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

public slots:
    void addCurve(const ThermalCurve& curve);
    void rescaleAxes(); // 重新缩放所有坐标轴以适应当前可见曲线

private slots:
    // 响应 CurveManager 的信号
    void onCurveAdded(const QString& curveId);
    void onCurveDataChanged(const QString& curveId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void curveSelected(const QString& curveId);

private:
    void updateSeriesStyle(QLineSeries *series, bool selected);
    void setupConnections();

    CurveManager* m_curveManager;  // 非拥有指针
    QChartView *m_chartView;
    QLineSeries *m_selectedSeries;
    QHash<QLineSeries*, QString> m_seriesToId;

    // 坐标轴
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY_primary = nullptr;
    QValueAxis *m_axisY_secondary = nullptr;

    // 命中判定相关配置
    qreal m_hitTestBasePx = 8.0;
    bool m_hitTestIncludePen = true;
};

#endif // PLOTWIDGET_H

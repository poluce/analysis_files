#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QtCharts/QChartView>

class ThermalCurve;

QT_CHARTS_USE_NAMESPACE

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);

public slots:
    void addCurve(const ThermalCurve& curve);

private:
    QChartView *m_chartView;
};

#endif // PLOTWIDGET_H

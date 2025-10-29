#include "PlotWidget.h"
#include <QDebug>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QVBoxLayout>

QT_CHARTS_USE_NAMESPACE

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent)
{
    qDebug() << "PlotWidget constructor called.";

    // 1. Create Chart and ChartView
    QChart *chart = new QChart();
    chart->setTitle("Simple Line Chart Example");

    m_chartView = new QChartView(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 2. Create a demo series
    QLineSeries *series = new QLineSeries();
    series->setName("Demo Series");
    series->append(0, 10);
    series->append(1, 5);
    series->append(2, 8);
    series->append(3, 12);
    series->append(4, 7);

    // 3. Add series to chart and create axes
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // 4. Set layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);
}

#include "PlotWidget.h"
#include "domain/model/ThermalCurve.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent)
{
    m_chartView = new QChartView(this);
    QChart *chart = new QChart();
    chart->setTitle(tr("热分析曲线"));
    m_chartView->setChart(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 使用布局来确保 QChartView 能够缩放
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);
}

void PlotWidget::addCurve(const ThermalCurve &curve)
{
    QLineSeries *series = new QLineSeries();
    series->setName(curve.name());

    const auto& data = curve.getRawData(); // 或者 getProcessedData()
    for (const auto& point : data)
    {
        series->append(point.temperature, point.value);
    }

    QChart *chart = m_chartView->chart();
    chart->addSeries(series);

    // 如果是第一次添加曲线，则创建坐标轴
    if (chart->axes().isEmpty())
    {
        QValueAxis *axisX = new QValueAxis;
        axisX->setTitleText(tr("温度 (°C)"));
        chart->addAxis(axisX, Qt::AlignBottom);

        QValueAxis *axisY = new QValueAxis;
        axisY->setTitleText(tr("信号"));
        chart->addAxis(axisY, Qt::AlignLeft);
    }

    // 将 series 附加到坐标轴
    series->attachAxis(chart->axes(Qt::Horizontal).first());
    series->attachAxis(chart->axes(Qt::Vertical).first());

    // 刷新图表
    chart->createDefaultAxes(); // 自动调整坐标轴范围
}
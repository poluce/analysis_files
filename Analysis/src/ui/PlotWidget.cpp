#include "PlotWidget.h"
#include <QtCharts/QValueAxis>
#include "domain/model/ThermalCurve.h"
#include "application/curve/CurveManager.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QDebug>
#include <QtMath>
#include <limits>

QT_CHARTS_USE_NAMESPACE

PlotWidget::PlotWidget(CurveManager* curveManager, QWidget *parent)
    : QWidget(parent),
      m_curveManager(curveManager),
      m_selectedSeries(nullptr)
{
    m_chartView = new QChartView(this);
    QChart *chart = new QChart();
    chart->setTitle(tr("热分析曲线"));
    m_chartView->setChart(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 创建并设置主坐标轴
    m_axisX = new QValueAxis();
    m_axisX->setTitleText(tr("温度 (°C)"));
    chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY_primary = new QValueAxis();
    m_axisY_primary->setTitleText(tr("质量 (mg)"));
    chart->addAxis(m_axisY_primary, Qt::AlignLeft);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);

    // 建立信号连接
    setupConnections();
}

void PlotWidget::addCurve(const ThermalCurve &curve)
{
    QLineSeries *series = new QLineSeries();
    series->setName(curve.name());

    const auto& data = curve.getProcessedData();
    for (const auto& point : data)
        series->append(point.temperature, point.value);

    QChart *chart = m_chartView->chart();
    chart->addSeries(series);
    m_seriesToId.insert(series, curve.id());

    // --- 多Y轴逻辑 ---
    QValueAxis* axisY_target = nullptr;
    if (curve.type() == CurveType::DTG) {
        if (!m_axisY_secondary) {
            m_axisY_secondary = new QValueAxis();
            m_axisY_secondary->setTitleText(tr("质量变化率 (%/°C)"));
            // 可以为次坐标轴设置不同颜色以区分
            QPen pen = m_axisY_secondary->linePen();
            pen.setColor(Qt::red);
            m_axisY_secondary->setLinePen(pen);
            m_axisY_secondary->setLabelsColor(Qt::red);
            m_axisY_secondary->setTitleBrush(QBrush(Qt::red));

            chart->addAxis(m_axisY_secondary, Qt::AlignRight);
        }
        axisY_target = m_axisY_secondary;
    } else {
        axisY_target = m_axisY_primary;

        // 如果是TGA曲线，根据初始质量判断是百分比还是绝对质量
        if (curve.type() == CurveType::TGA) {
            const auto& metadata = curve.getMetadata();
            if (metadata.sampleMass > 0.0) {
                // 有初始质量，说明已转换为百分比
                m_axisY_primary->setTitleText(tr("质量 (%)"));
            } else {
                // 没有初始质量，使用绝对质量
                m_axisY_primary->setTitleText(tr("质量 (mg)"));
            }
        }
    }

    series->attachAxis(m_axisX);
    series->attachAxis(axisY_target);

    // 自动重新缩放坐标轴
    rescaleAxes();
}

void PlotWidget::rescaleAxes()
{
    QChart *chart = m_chartView->chart();

    // --- 独立调整每个坐标轴的范围 ---
    QList<QValueAxis*> axes_to_rescale;
    if(m_axisX) axes_to_rescale.append(m_axisX);
    if(m_axisY_primary) axes_to_rescale.append(m_axisY_primary);
    if(m_axisY_secondary) axes_to_rescale.append(m_axisY_secondary);

    for (QValueAxis* axis : axes_to_rescale) {
        qreal minVal = std::numeric_limits<qreal>::max();
        qreal maxVal = std::numeric_limits<qreal>::lowest();
        bool found_series = false;

        // 遍历图表中的所有系列，检查是否附加到当前坐标轴
        for (QAbstractSeries* s : chart->series()) {
            auto lineSeries = qobject_cast<QLineSeries*>(s);
            if (lineSeries && lineSeries->isVisible()) {
                // 检查这个系列是否附加到当前坐标轴
                bool is_attached = false;
                for (QAbstractAxis* attached_axis : lineSeries->attachedAxes()) {
                    if (attached_axis == axis) {
                        is_attached = true;
                        break;
                    }
                }

                if (is_attached) {
                    found_series = true;
                    for (const QPointF &p : lineSeries->points()) {
                        if (axis->orientation() == Qt::Horizontal) {
                            if (p.x() < minVal) minVal = p.x();
                            if (p.x() > maxVal) maxVal = p.x();
                        } else {
                            if (p.y() < minVal) minVal = p.y();
                            if (p.y() > maxVal) maxVal = p.y();
                        }
                    }
                }
            }
        }

        if (found_series) {
            // 直接使用数据的最小值和最大值，不留边距
            qreal range = maxVal - minVal;
            if (qFuzzyIsNull(range)) {
                // 如果数据是常数，设置一个默认范围
                range = qAbs(minVal) * 0.1;
                if (qFuzzyIsNull(range)) range = 1.0;
                axis->setRange(minVal - range/2, maxVal + range/2);
            } else {
                axis->setRange(minVal, maxVal);
            }
            axis->applyNiceNumbers();
        }
    }
}

void PlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    QChart *chart = m_chartView->chart();

    const QPointF mousePos = m_chartView->mapFromGlobal(mapToGlobal(event->pos()));

    auto pointToSegDist = [](const QPointF& p, const QPointF& a, const QPointF& b) -> qreal {
        const qreal vx = b.x() - a.x();
        const qreal vy = b.y() - a.y();
        const qreal wx = p.x() - a.x();
        const qreal wy = p.y() - a.y();
        const qreal vv = vx*vx + vy*vy;
        qreal t = vv > 0 ? (wx*vx + wy*vy) / vv : 0.0;
        if (t < 0) t = 0; else if (t > 1) t = 1;
        const QPointF proj(a.x() + t*vx, a.y() + t*vy);
        const qreal dx = p.x() - proj.x();
        const qreal dy = p.y() - proj.y();
        return qSqrt(dx*dx + dy*dy);
    };

    QLineSeries *clickedSeries = nullptr;
    qreal bestDist = 1e18;

    for (QAbstractSeries *abstractSeries : chart->series()) {
        QLineSeries *lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) continue;

        const auto pts = lineSeries->pointsVector();
        if (pts.size() < 2) continue;

        QPointF prevPos = chart->mapToPosition(pts[0], lineSeries);
        for (int i = 1; i < pts.size(); ++i) {
            const QPointF currPos = chart->mapToPosition(pts[i], lineSeries);
            const qreal d = pointToSegDist(mousePos, prevPos, currPos);
            if (d < bestDist) {
                bestDist = d;
                clickedSeries = lineSeries;
            }
            prevPos = currPos;
        }
    }

    const qreal deviceRatio = m_chartView->devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio; // 稍微扩大命中范围
    if (clickedSeries && m_hitTestIncludePen) {
        const qreal penWidth = clickedSeries->pen().widthF();
        baseThreshold += penWidth * 0.5;
    }

    if (clickedSeries && bestDist <= baseThreshold) {
        if (m_selectedSeries)
            updateSeriesStyle(m_selectedSeries, false);
        m_selectedSeries = clickedSeries;
        updateSeriesStyle(m_selectedSeries, true);
        emit curveSelected(m_seriesToId.value(m_selectedSeries));
    } else {
        if (m_selectedSeries) {
            updateSeriesStyle(m_selectedSeries, false);
            m_selectedSeries = nullptr;
        }
        emit curveSelected("");
    }

    QWidget::mousePressEvent(event);
}

void PlotWidget::updateSeriesStyle(QLineSeries *series, bool selected)
{
    if (!series) return;
    QPen pen = series->pen();
    pen.setWidth(selected ? 3 : 1);
    series->setPen(pen);
}

// —— 命中阈值配置 ——
void PlotWidget::setHitTestBasePixelThreshold(qreal px)
{
    // 限制最小值，避免被设置为 0 导致很难命中
    m_hitTestBasePx = (px <= 0) ? 1.0 : px;
}

qreal PlotWidget::hitTestBasePixelThreshold() const
{
    return m_hitTestBasePx;
}

void PlotWidget::setHitTestIncludePenWidth(bool enabled)
{
    m_hitTestIncludePen = enabled;
}

bool PlotWidget::hitTestIncludePenWidth() const
{
    return m_hitTestIncludePen;
}

// ========== 信号连接设置 ==========
void PlotWidget::setupConnections()
{
    if (m_curveManager) {
        // 监听 CurveManager 的信号（通知路径：Service → UI）
        connect(m_curveManager, &CurveManager::curveAdded,
                this, &PlotWidget::onCurveAdded);
        connect(m_curveManager, &CurveManager::curveDataChanged,
                this, &PlotWidget::onCurveDataChanged);
    }
}

// ========== 响应 CurveManager 信号的槽函数 ==========
void PlotWidget::onCurveAdded(const QString& curveId)
{
    // 当新曲线被添加时，自动显示
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (curve) {
        qDebug() << "PlotWidget: 自动添加曲线" << curve->name();
        addCurve(*curve);
    }
}

void PlotWidget::onCurveDataChanged(const QString& curveId)
{
    // 当曲线数据被修改时，刷新显示
    // 注意：目前的架构中算法会创建新曲线，所以这个信号暂时不会被频繁使用
    // 但保留此接口以支持未来可能的就地修改场景
    qDebug() << "PlotWidget: 曲线数据已变化" << curveId;

    // TODO: 实现曲线数据的更新逻辑
    // 可以找到对应的 series 并更新其数据点
}

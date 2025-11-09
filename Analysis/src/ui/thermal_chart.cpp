#include "thermal_chart.h"
#include "floating_label.h"
#include "trapezoid_measure_tool.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include "application/curve/curve_manager.h"

#include <QDebug>
#include <QGraphicsLineItem>
#include <QSignalBlocker>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QtMath>
#include <limits>

ThermalChart::ThermalChart(QGraphicsItem* parent, Qt::WindowFlags wFlags)
    : QChart(QChart::ChartTypeCartesian, parent, wFlags)
{
    qDebug() << "构造: ThermalChart";

    // 设置图表标题和图例
    setTitle(tr("热分析曲线"));
    legend()->setVisible(false);

    // 创建并设置主坐标轴
    m_axisX = new QValueAxis(this);
    m_axisX->setTitleText(tr("温度 (°C)"));
    addAxis(m_axisX, Qt::AlignBottom);

    m_axisY_primary = new QValueAxis(this);
    m_axisY_primary->setTitleText(tr("质量 (mg)"));
    addAxis(m_axisY_primary, Qt::AlignLeft);

    // 创建十字线图元
    QPen crosshairPen(Qt::gray, 0.0, Qt::DashLine);

    m_verticalCrosshairLine = new QGraphicsLineItem(this);
    m_verticalCrosshairLine->setPen(crosshairPen);
    m_verticalCrosshairLine->setVisible(false);
    m_verticalCrosshairLine->setZValue(zValue() + 10.0);

    m_horizontalCrosshairLine = new QGraphicsLineItem(this);
    m_horizontalCrosshairLine->setPen(crosshairPen);
    m_horizontalCrosshairLine->setVisible(false);
    m_horizontalCrosshairLine->setZValue(zValue() + 10.0);

    // 创建选中点高亮系列（用于算法交互）
    m_selectedPointsSeries = new QScatterSeries(this);
    m_selectedPointsSeries->setName(tr("选中的点"));
    m_selectedPointsSeries->setMarkerSize(12.0);
    m_selectedPointsSeries->setColor(Qt::red);
    m_selectedPointsSeries->setBorderColor(Qt::darkRed);
    m_selectedPointsSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    // 初始时不添加到 chart，在需要时添加
}

ThermalChart::~ThermalChart()
{
    qDebug() << "析构: ThermalChart";
}

void ThermalChart::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;
}

// ==================== 系列查询接口 ====================

QLineSeries* ThermalChart::seriesForCurve(const QString& curveId) const
{
    auto it = m_idToSeries.constFind(curveId);
    return (it == m_idToSeries.constEnd()) ? nullptr : it.value();
}

QString ThermalChart::curveIdForSeries(QLineSeries* series) const
{
    auto it = m_seriesToId.constFind(series);
    return (it == m_seriesToId.constEnd()) ? QString() : it.value();
}

QColor ThermalChart::getCurveColor(const QString& curveId) const
{
    QLineSeries* series = seriesForCurve(curveId);
    if (series) {
        return series->color();
    }
    return Qt::black;
}

// ==================== 坐标轴查询接口 ====================

QValueAxis* ThermalChart::yAxisForCurve(const QString& curveId)
{
    // 早期返回：曲线ID为空
    if (curveId.isEmpty()) {
        return m_axisY_primary;
    }

    // 早期返回：未找到曲线系列
    QLineSeries* curveSeries = seriesForCurve(curveId);
    if (!curveSeries) {
        qWarning() << "ThermalChart::yAxisForCurve - 未找到曲线" << curveId;
        return m_axisY_primary;
    }

    // 查找该曲线附着的Y轴（非X轴）
    const auto attachedAxes = curveSeries->attachedAxes();
    for (QAbstractAxis* axis : attachedAxes) {
        QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
        if (valueAxis && valueAxis != m_axisX) {
            return valueAxis;
        }
    }

    // 默认返回主Y轴
    return m_axisY_primary;
}

// ==================== 十字线管理（仅图元接口）====================

void ThermalChart::setCrosshairEnabled(bool vertical, bool horizontal)
{
    m_verticalCrosshairEnabled = vertical;
    m_horizontalCrosshairEnabled = horizontal;

    // 如果都禁用，清除十字线
    if (!vertical && !horizontal) {
        clearCrosshair();
    }
}

void ThermalChart::updateCrosshairAtChartPos(const QPointF& chartPos)
{
    const QRectF plotRect = plotArea();

    if (!plotRect.isValid() || !plotRect.contains(chartPos)) {
        // 鼠标不在绘图区域内，隐藏十字线
        if (m_verticalCrosshairLine) {
            m_verticalCrosshairLine->setVisible(false);
        }
        if (m_horizontalCrosshairLine) {
            m_horizontalCrosshairLine->setVisible(false);
        }
        return;
    }

    // 更新垂直十字线
    if (m_verticalCrosshairEnabled && m_verticalCrosshairLine) {
        m_verticalCrosshairLine->setLine(chartPos.x(), plotRect.top(),
                                         chartPos.x(), plotRect.bottom());
        m_verticalCrosshairLine->setVisible(true);
    } else if (m_verticalCrosshairLine) {
        m_verticalCrosshairLine->setVisible(false);
    }

    // 更新水平十字线
    if (m_horizontalCrosshairEnabled && m_horizontalCrosshairLine) {
        m_horizontalCrosshairLine->setLine(plotRect.left(), chartPos.y(),
                                           plotRect.right(), chartPos.y());
        m_horizontalCrosshairLine->setVisible(true);
    } else if (m_horizontalCrosshairLine) {
        m_horizontalCrosshairLine->setVisible(false);
    }
}

void ThermalChart::clearCrosshair()
{
    if (m_verticalCrosshairLine) {
        m_verticalCrosshairLine->setVisible(false);
    }
    if (m_horizontalCrosshairLine) {
        m_horizontalCrosshairLine->setVisible(false);
    }
}

// ==================== 选中点管理（用于算法交互）====================

void ThermalChart::setupSelectedPointsSeries(QValueAxis* targetYAxis)
{
    if (!m_selectedPointsSeries) {
        return;
    }

    m_selectedPointsSeries->clear();

    QString axisDebugName = (targetYAxis == m_axisY_primary ? "主轴(左)" : "次轴(右)");

    // 如果还未添加到 chart，则添加
    if (!series().contains(m_selectedPointsSeries)) {
        addSeries(m_selectedPointsSeries);
        m_selectedPointsSeries->attachAxis(m_axisX);
        m_selectedPointsSeries->attachAxis(targetYAxis);
        qDebug() << "ThermalChart::setupSelectedPointsSeries - 添加选中点系列，Y轴:" << axisDebugName;
        return;
    }

    // 如果已存在，则更新其轴关联
    const auto existingAxes = m_selectedPointsSeries->attachedAxes();
    for (QAbstractAxis* axis : existingAxes) {
        m_selectedPointsSeries->detachAxis(axis);
    }
    m_selectedPointsSeries->attachAxis(m_axisX);
    m_selectedPointsSeries->attachAxis(targetYAxis);
    qDebug() << "ThermalChart::setupSelectedPointsSeries - 更新选中点系列的轴，Y轴:" << axisDebugName;
}

void ThermalChart::addSelectedPoint(const QPointF& point)
{
    if (m_selectedPointsSeries) {
        m_selectedPointsSeries->append(point);
    }
}

void ThermalChart::clearSelectedPoints()
{
    if (m_selectedPointsSeries) {
        m_selectedPointsSeries->clear();
    }
}

// ==================== 系列管理辅助函数（占位符）====================

QLineSeries* ThermalChart::createSeriesForCurve(const ThermalCurve& curve) const
{
    auto* series = new QLineSeries();
    series->setName(curve.name());
    populateSeriesWithCurveData(series, curve);
    return series;
}

void ThermalChart::populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const
{
    if (!series) {
        return;
    }

    QSignalBlocker blocker(series);
    series->clear();

    const auto& data = curve.getProcessedData();

    // 根据横轴模式选择 X 轴数据
    if (m_xAxisMode == XAxisMode::Temperature) {
        for (const auto& point : data) {
            series->append(point.temperature, point.value);
        }
    } else {
        for (const auto& point : data) {
            series->append(point.time, point.value);
        }
    }
}

void ThermalChart::attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY)
{
    if (!series || !axisY || !m_axisX) {
        return;
    }

    series->attachAxis(m_axisX);
    series->attachAxis(axisY);
}

void ThermalChart::detachSeriesFromAxes(QLineSeries* series)
{
    if (!series) {
        return;
    }

    const auto axes = series->attachedAxes();
    for (QAbstractAxis* axis : axes) {
        series->detachAxis(axis);
    }
}

void ThermalChart::registerSeriesMapping(QLineSeries* series, const QString& curveId)
{
    if (!series) {
        return;
    }

    m_seriesToId.insert(series, curveId);
    m_idToSeries.insert(curveId, series);
}

void ThermalChart::unregisterSeriesMapping(const QString& curveId)
{
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        m_idToSeries.remove(curveId);
        return;
    }

    m_seriesToId.remove(series);
    m_idToSeries.remove(curveId);
}

void ThermalChart::updateSeriesStyle(QLineSeries* series, bool selected)
{
    if (!series) {
        return;
    }

    QPen pen = series->pen();
    pen.setWidth(selected ? 3 : 1);
    series->setPen(pen);
}

void ThermalChart::updateLegendVisibility(QLineSeries* series, bool visible) const
{
    if (!series || !legend()) {
        return;
    }

    const auto markers = legend()->markers(series);
    for (QLegendMarker* marker : markers) {
        marker->setVisible(visible);
    }
}

// ==================== 坐标轴管理辅助函数（占位符）====================

void ThermalChart::rescaleAxis(QValueAxis* axis) const
{
    if (!axis) {
        return;
    }

    qreal minVal = std::numeric_limits<qreal>::max();
    qreal maxVal = std::numeric_limits<qreal>::lowest();
    bool hasData = false;

    const auto seriesList = series();
    for (QAbstractSeries* abstractSeries : seriesList) {
        auto* lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) {
            continue;
        }

        if (!lineSeries->attachedAxes().contains(axis)) {
            continue;
        }

        hasData = true;
        const auto points = lineSeries->pointsVector();
        for (const QPointF& point : points) {
            if (axis->orientation() == Qt::Horizontal) {
                minVal = qMin(minVal, point.x());
                maxVal = qMax(maxVal, point.x());
            } else {
                minVal = qMin(minVal, point.y());
                maxVal = qMax(maxVal, point.y());
            }
        }
    }

    if (!hasData) {
        return;
    }

    qreal range = maxVal - minVal;
    if (qFuzzyIsNull(range)) {
        range = qAbs(minVal) * 0.1;
        if (qFuzzyIsNull(range)) {
            range = 1.0;
        }
        axis->setRange(minVal - range / 2, maxVal + range / 2);
    } else {
        axis->setRange(minVal, maxVal);
    }
    axis->applyNiceNumbers();
}

void ThermalChart::resetAxesToDefault()
{
    if (m_axisY_secondary) {
        removeAxis(m_axisY_secondary);
        m_axisY_secondary->deleteLater();
        m_axisY_secondary = nullptr;
    }

    if (m_axisY_primary) {
        m_axisY_primary->setTitleText(tr("质量 (mg)"));
        m_axisY_primary->setRange(0.0, 1.0);
    }

    if (m_axisX) {
        m_axisX->setRange(0.0, 1.0);
    }
}

QValueAxis* ThermalChart::ensureYAxisForCurve(const ThermalCurve& curve)
{
    // ==================== 优先级1：SignalType 强制规则 ====================
    if (curve.signalType() == SignalType::Derivative) {
        if (!m_axisY_secondary) {
            m_axisY_secondary = new QValueAxis(this);
            QPen pen = m_axisY_secondary->linePen();
            pen.setColor(Qt::red);
            m_axisY_secondary->setLinePen(pen);
            m_axisY_secondary->setLabelsColor(Qt::red);
            m_axisY_secondary->setTitleBrush(QBrush(Qt::red));
            addAxis(m_axisY_secondary, Qt::AlignRight);
        }
        m_axisY_secondary->setTitleText(curve.getYAxisLabel());
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用次 Y 轴（Derivative 强制规则）";
        return m_axisY_secondary;
    }

    // ==================== 优先级2：辅助曲线继承父曲线的 Y 轴 ====================
    if (!curve.isAuxiliaryCurve() || curve.parentId().isEmpty()) {
        if (!m_axisY_primary) {
            m_axisY_primary = new QValueAxis(this);
            addAxis(m_axisY_primary, Qt::AlignLeft);
        }
        m_axisY_primary->setTitleText(curve.getYAxisLabel());
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用主 Y 轴（默认）";
        return m_axisY_primary;
    }

    // 尝试查找父曲线的系列
    QLineSeries* parentSeries = seriesForCurve(curve.parentId());
    if (!parentSeries || parentSeries->attachedAxes().isEmpty()) {
        if (!m_axisY_primary) {
            m_axisY_primary = new QValueAxis(this);
            addAxis(m_axisY_primary, Qt::AlignLeft);
        }
        m_axisY_primary->setTitleText(curve.getYAxisLabel());
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用主 Y 轴（默认）";
        return m_axisY_primary;
    }

    // 查找父曲线的 Y 轴（非 X 轴）
    for (QAbstractAxis* axis : parentSeries->attachedAxes()) {
        QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
        if (valueAxis && valueAxis != m_axisX) {
            qDebug() << "ThermalChart: 辅助曲线" << curve.name() << "继承父曲线的 Y 轴";
            valueAxis->setTitleText(curve.getYAxisLabel());
            return valueAxis;
        }
    }

    // ==================== 优先级3：默认分配主 Y 轴 ====================
    if (!m_axisY_primary) {
        m_axisY_primary = new QValueAxis(this);
        addAxis(m_axisY_primary, Qt::AlignLeft);
    }
    m_axisY_primary->setTitleText(curve.getYAxisLabel());
    qDebug() << "ThermalChart: 曲线" << curve.name() << "使用主 Y 轴（默认）";
    return m_axisY_primary;
}

// ==================== 数据查询辅助函数 ====================

ThermalDataPoint ThermalChart::findNearestDataPoint(const QVector<ThermalDataPoint>& curveData,
                                                      double temperature) const
{
    if (curveData.isEmpty()) {
        return ThermalDataPoint();
    }

    int nearestIdx = 0;
    double minDist = qAbs(curveData[0].temperature - temperature);

    for (int i = 1; i < curveData.size(); ++i) {
        double dist = qAbs(curveData[i].temperature - temperature);
        if (dist < minDist) {
            minDist = dist;
            nearestIdx = i;
        }
    }

    return curveData[nearestIdx];
}

// ==================== 占位符方法（Phase 2-3 实现）====================

void ThermalChart::addCurve(const ThermalCurve& curve)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::addCurve - 占位符";
}

void ThermalChart::updateCurve(const ThermalCurve& curve)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::updateCurve - 占位符";
}

void ThermalChart::removeCurve(const QString& curveId)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::removeCurve - 占位符";
}

void ThermalChart::clearCurves()
{
    // Phase 2 实现
    qDebug() << "ThermalChart::clearCurves - 占位符";
}

void ThermalChart::setCurveVisible(const QString& curveId, bool visible)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::setCurveVisible - 占位符";
}

void ThermalChart::highlightCurve(const QString& curveId)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::highlightCurve - 占位符";
}

void ThermalChart::rescaleAxes()
{
    rescaleAxis(m_axisX);
    rescaleAxis(m_axisY_primary);
    rescaleAxis(m_axisY_secondary);
}

void ThermalChart::setXAxisMode(XAxisMode mode)
{
    // Phase 2 实现
    qDebug() << "ThermalChart::setXAxisMode - 占位符";
}

FloatingLabel* ThermalChart::addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::addFloatingLabel - 占位符";
    return nullptr;
}

FloatingLabel* ThermalChart::addFloatingLabelHUD(const QString& text, const QPointF& viewPos)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::addFloatingLabelHUD - 占位符";
    return nullptr;
}

void ThermalChart::removeFloatingLabel(FloatingLabel* label)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::removeFloatingLabel - 占位符";
}

void ThermalChart::clearFloatingLabels()
{
    // Phase 3 实现
    qDebug() << "ThermalChart::clearFloatingLabels - 占位符";
}

void ThermalChart::addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                                    const QColor& color, qreal size)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::addCurveMarkers - 占位符";
}

void ThermalChart::removeCurveMarkers(const QString& curveId)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::removeCurveMarkers - 占位符";
}

void ThermalChart::clearAllMarkers()
{
    // Phase 3 实现
    qDebug() << "ThermalChart::clearAllMarkers - 占位符";
}

void ThermalChart::addMassLossTool(const ThermalDataPoint& point1,
                                     const ThermalDataPoint& point2,
                                     const QString& curveId)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::addMassLossTool - 占位符";
}

void ThermalChart::removeMassLossTool(QGraphicsObject* tool)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::removeMassLossTool - 占位符";
}

void ThermalChart::clearAllMassLossTools()
{
    // Phase 3 实现
    qDebug() << "ThermalChart::clearAllMassLossTools - 占位符";
}

void ThermalChart::addAnnotationLine(const QString& id, const QString& curveId,
                                       const QPointF& start, const QPointF& end,
                                       const QPen& pen)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::addAnnotationLine - 占位符";
}

void ThermalChart::removeAnnotation(const QString& id)
{
    // Phase 3 实现
    qDebug() << "ThermalChart::removeAnnotation - 占位符";
}

void ThermalChart::clearAllAnnotations()
{
    // Phase 3 实现
    qDebug() << "ThermalChart::clearAllAnnotations - 占位符";
}

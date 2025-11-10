#include "thermal_chart.h"
#include "floating_label.h"
#include "trapezoid_measure_tool.h"
#include "peak_area_tool.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include "application/curve/curve_manager.h"

#include <QDebug>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
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

void ThermalChart::setVerticalCrosshairEnabled(bool enabled)
{
    setCrosshairEnabled(enabled, m_horizontalCrosshairEnabled);
}

void ThermalChart::setHorizontalCrosshairEnabled(bool enabled)
{
    setCrosshairEnabled(m_verticalCrosshairEnabled, enabled);
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
        // 检查自定义标题优先级
        if (!m_customYAxisTitlePrimary.isEmpty()) {
            m_axisY_primary->setTitleText(m_customYAxisTitlePrimary);
        } else {
            m_axisY_primary->setTitleText(tr("质量 (mg)"));
        }
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
        // 检查自定义标题优先级
        if (!m_customYAxisTitleSecondary.isEmpty()) {
            m_axisY_secondary->setTitleText(m_customYAxisTitleSecondary);
        } else {
            m_axisY_secondary->setTitleText(curve.getYAxisLabel());
        }
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用次 Y 轴（Derivative 强制规则）";
        return m_axisY_secondary;
    }

    // ==================== 优先级2：辅助曲线继承父曲线的 Y 轴 ====================
    if (!curve.isAuxiliaryCurve() || curve.parentId().isEmpty()) {
        if (!m_axisY_primary) {
            m_axisY_primary = new QValueAxis(this);
            addAxis(m_axisY_primary, Qt::AlignLeft);
        }
        // 检查自定义标题优先级
        if (!m_customYAxisTitlePrimary.isEmpty()) {
            m_axisY_primary->setTitleText(m_customYAxisTitlePrimary);
        } else {
            m_axisY_primary->setTitleText(curve.getYAxisLabel());
        }
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
        // 检查自定义标题优先级
        if (!m_customYAxisTitlePrimary.isEmpty()) {
            m_axisY_primary->setTitleText(m_customYAxisTitlePrimary);
        } else {
            m_axisY_primary->setTitleText(curve.getYAxisLabel());
        }
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用主 Y 轴（默认）";
        return m_axisY_primary;
    }

    // 查找父曲线的 Y 轴（非 X 轴）
    for (QAbstractAxis* axis : parentSeries->attachedAxes()) {
        QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
        if (valueAxis && valueAxis != m_axisX) {
            qDebug() << "ThermalChart: 辅助曲线" << curve.name() << "继承父曲线的 Y 轴";
            // 辅助曲线继承父曲线Y轴时，检查自定义标题优先级
            if (valueAxis == m_axisY_primary && !m_customYAxisTitlePrimary.isEmpty()) {
                valueAxis->setTitleText(m_customYAxisTitlePrimary);
            } else if (valueAxis == m_axisY_secondary && !m_customYAxisTitleSecondary.isEmpty()) {
                valueAxis->setTitleText(m_customYAxisTitleSecondary);
            } else {
                valueAxis->setTitleText(curve.getYAxisLabel());
            }
            return valueAxis;
        }
    }

    // ==================== 优先级3：默认分配主 Y 轴 ====================
    if (!m_axisY_primary) {
        m_axisY_primary = new QValueAxis(this);
        addAxis(m_axisY_primary, Qt::AlignLeft);
    }
    // 检查自定义标题优先级
    if (!m_customYAxisTitlePrimary.isEmpty()) {
        m_axisY_primary->setTitleText(m_customYAxisTitlePrimary);
    } else {
        m_axisY_primary->setTitleText(curve.getYAxisLabel());
    }
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

// ==================== Phase 2: 曲线管理实现 ====================

void ThermalChart::addCurve(const ThermalCurve& curve)
{
    QLineSeries* series = createSeriesForCurve(curve);
    if (!series) {
        return;
    }

    addSeries(series);
    registerSeriesMapping(series, curve.id());

    QValueAxis* axisY_target = ensureYAxisForCurve(curve);
    attachSeriesToAxes(series, axisY_target);

    rescaleAxes();
}

void ThermalChart::updateCurve(const ThermalCurve& curve)
{
    QLineSeries* series = seriesForCurve(curve.id());
    if (!series) {
        return;
    }

    populateSeriesWithCurveData(series, curve);
    detachSeriesFromAxes(series);
    QValueAxis* axisY_target = ensureYAxisForCurve(curve);
    attachSeriesToAxes(series, axisY_target);

    rescaleAxes();
}

void ThermalChart::removeCurve(const QString& curveId)
{
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        return;
    }

    removeSeries(series);
    if (m_selectedSeries == series) {
        m_selectedSeries = nullptr;
    }

    // 清除该曲线的标注点（如果有）
    removeCurveMarkers(curveId);

    unregisterSeriesMapping(curveId);
    series->deleteLater();

    rescaleAxes();
}

void ThermalChart::clearCurves()
{
    const auto currentSeries = series();
    for (QAbstractSeries* series : currentSeries) {
        removeSeries(series);
        if (auto* lineSeries = qobject_cast<QLineSeries*>(series)) {
            const QString curveId = m_seriesToId.take(lineSeries);
            if (!curveId.isEmpty()) {
                m_idToSeries.remove(curveId);
            }
        }
        series->deleteLater();
    }

    m_seriesToId.clear();
    m_idToSeries.clear();
    m_selectedSeries = nullptr;
    resetAxesToDefault();
    clearCrosshair();
    clearFloatingLabels();
    clearAllMarkers();
}

void ThermalChart::setCurveVisible(const QString& curveId, bool visible)
{
    // 早期返回：检查曲线是否存在
    QLineSeries* series = seriesForCurve(curveId);
    if (!series || series->isVisible() == visible) {
        return;
    }

    // 设置当前曲线的可见性
    series->setVisible(visible);
    updateLegendVisibility(series, visible);

    // ==================== 级联处理标注点（Markers）====================
    if (m_curveMarkers.contains(curveId)) {
        CurveMarkerData& markerData = m_curveMarkers[curveId];
        if (markerData.series) {
            markerData.series->setVisible(visible);
            qDebug() << "ThermalChart::setCurveVisible - 同步标注点可见性:" << curveId << visible;
        }
    }

    // ==================== 级联处理子曲线 ====================
    if (m_curveManager) {
        QVector<ThermalCurve*> children = m_curveManager->getChildren(curveId);
        for (ThermalCurve* child : children) {
            if (!child) continue;

            // 只级联处理强绑定的子曲线（如基线）
            if (child->isStronglyBound()) {
                QLineSeries* childSeries = seriesForCurve(child->id());
                if (childSeries) {
                    childSeries->setVisible(visible);
                    updateLegendVisibility(childSeries, visible);

                    // 递归处理子曲线的标注点
                    if (m_curveMarkers.contains(child->id())) {
                        CurveMarkerData& childMarkerData = m_curveMarkers[child->id()];
                        if (childMarkerData.series) {
                            childMarkerData.series->setVisible(visible);
                        }
                    }

                    qDebug() << "ThermalChart::setCurveVisible - 级联设置子曲线可见性:" << child->name() << visible;
                }
            }
        }
    }

    rescaleAxes();
    emit curveVisibilityChanged(curveId, visible);
}

void ThermalChart::highlightCurve(const QString& curveId)
{
    // 查找对应的系列
    QLineSeries* targetSeries = seriesForCurve(curveId);

    if (!targetSeries) {
        // 如果找不到系列，清除当前高亮
        if (m_selectedSeries) {
            updateSeriesStyle(m_selectedSeries, false);
            m_selectedSeries = nullptr;
        }
        return;
    }

    // 如果已经是当前选中的曲线，不需要重复操作
    if (m_selectedSeries == targetSeries) {
        return;
    }

    // 取消之前选中曲线的高亮
    if (m_selectedSeries) {
        updateSeriesStyle(m_selectedSeries, false);
    }

    // 高亮新选中的曲线
    m_selectedSeries = targetSeries;
    updateSeriesStyle(m_selectedSeries, true);
}

void ThermalChart::rescaleAxes()
{
    rescaleAxis(m_axisX);
    rescaleAxis(m_axisY_primary);
    rescaleAxis(m_axisY_secondary);
}

void ThermalChart::setXAxisMode(XAxisMode mode)
{
    if (m_xAxisMode == mode) {
        return;
    }

    // 切换模式
    m_xAxisMode = mode;

    // 更新X轴标题（检查自定义标题优先级）
    if (!m_customXAxisTitle.isEmpty()) {
        // 使用自定义标题（不随模式切换而改变）
        m_axisX->setTitleText(m_customXAxisTitle);
        qDebug() << "ThermalChart::setXAxisMode - 使用自定义X轴标题:" << m_customXAxisTitle;
    } else {
        // 使用默认标题（根据横轴模式自动切换）
        if (m_xAxisMode == XAxisMode::Temperature) {
            m_axisX->setTitleText(tr("温度 (°C)"));
            qDebug() << "ThermalChart::setXAxisMode - 切换到温度横轴";
        } else {
            m_axisX->setTitleText(tr("时间 (s)"));
            qDebug() << "ThermalChart::setXAxisMode - 切换到时间横轴";
        }
    }

    // 通知所有测量工具更新横轴模式
    bool useTimeAxis = (m_xAxisMode == XAxisMode::Time);
    for (QGraphicsObject* obj : m_massLossTools) {
        TrapezoidMeasureTool* tool = qobject_cast<TrapezoidMeasureTool*>(obj);
        if (tool) {
            tool->setXAxisMode(useTimeAxis);
        }
    }
    qDebug() << "ThermalChart::setXAxisMode - 已通知" << m_massLossTools.size() << "个测量工具更新横轴模式";

    // 通知所有峰面积工具更新横轴模式
    for (QGraphicsObject* obj : m_peakAreaTools) {
        PeakAreaTool* tool = qobject_cast<PeakAreaTool*>(obj);
        if (tool) {
            tool->setXAxisMode(useTimeAxis);
        }
    }
    qDebug() << "ThermalChart::setXAxisMode - 已通知" << m_peakAreaTools.size() << "个峰面积工具更新横轴模式";

    // 重新加载所有曲线数据
    if (!m_curveManager) {
        qWarning() << "ThermalChart::setXAxisMode - CurveManager 未设置";
        emit xAxisModeChanged(m_xAxisMode);
        rescaleAxes();
        return;
    }

    const auto& allCurves = m_curveManager->getAllCurves();
    for (const ThermalCurve& curve : allCurves) {
        QLineSeries* series = seriesForCurve(curve.id());
        if (series) {
            populateSeriesWithCurveData(series, curve);
        }
    }

    // 重新缩放坐标轴以适应新数据范围
    rescaleAxes();

    // 更新所有标注点（Markers）的显示位置
    for (auto it = m_curveMarkers.begin(); it != m_curveMarkers.end(); ++it) {
        CurveMarkerData& markerData = it.value();
        if (markerData.series) {
            markerData.series->clear();
            for (const ThermalDataPoint& dataPoint : markerData.dataPoints) {
                QPointF displayPoint;
                if (m_xAxisMode == XAxisMode::Temperature) {
                    displayPoint.setX(dataPoint.temperature);
                } else {
                    displayPoint.setX(dataPoint.time);
                }
                displayPoint.setY(dataPoint.value);
                markerData.series->append(displayPoint);
            }
        }
    }

    qDebug() << "ThermalChart::setXAxisMode - 已完成横轴切换和曲线重绘";

    // 发出信号通知浮动标签更新（FloatingLabel 会监听此信号）
    emit xAxisModeChanged(m_xAxisMode);
}

void ThermalChart::toggleXAxisMode()
{
    XAxisMode newMode = (m_xAxisMode == XAxisMode::Temperature) ? XAxisMode::Time : XAxisMode::Temperature;
    setXAxisMode(newMode);
}

// ==================== Phase 3: 浮动标签管理实现 ====================

FloatingLabel* ThermalChart::addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId)
{
    // 查找对应的系列
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        qWarning() << "ThermalChart::addFloatingLabel - 未找到曲线" << curveId;
        return nullptr;
    }

    // 创建浮动标签（数据锚定模式）
    auto* label = new FloatingLabel(this);
    label->setMode(FloatingLabel::Mode::DataAnchored);
    label->setText(text);
    label->setAnchorValue(dataPos, series);

    // 添加到场景
    scene()->addItem(label);

    // 保存到列表
    m_floatingLabels.append(label);

    // 连接关闭信号
    connect(label, &FloatingLabel::closeRequested, this, [this, label]() {
        removeFloatingLabel(label);
    });

    // 连接 plotAreaChanged 信号，确保标签跟随坐标轴变化
    connect(this, &QChart::plotAreaChanged, label, &FloatingLabel::updateGeometry);

    // 连接 xAxisModeChanged 信号，确保标签在横轴切换时更新位置
    connect(this, &ThermalChart::xAxisModeChanged, label, &FloatingLabel::updateGeometry);

    qDebug() << "ThermalChart::addFloatingLabel - 添加浮动标签（数据锚定）：" << text
             << "，位置：" << dataPos << "，曲线：" << curveId;

    return label;
}

FloatingLabel* ThermalChart::addFloatingLabelHUD(const QString& text, const QPointF& viewPos)
{
    // 创建浮动标签（视图锚定模式）
    auto* label = new FloatingLabel(this);
    label->setMode(FloatingLabel::Mode::ViewAnchored);
    label->setText(text);

    // 计算绝对位置（相对于 plotArea）
    QRectF area = plotArea();
    QPointF absolutePos = area.topLeft() + viewPos;
    label->setPos(absolutePos);

    // 添加到场景
    scene()->addItem(label);

    // 保存到列表
    m_floatingLabels.append(label);

    // 连接关闭信号
    connect(label, &FloatingLabel::closeRequested, this, [this, label]() {
        removeFloatingLabel(label);
    });

    qDebug() << "ThermalChart::addFloatingLabelHUD - 添加浮动标签（视图锚定）：" << text
             << "，位置：" << viewPos;

    return label;
}

void ThermalChart::removeFloatingLabel(FloatingLabel* label)
{
    if (!label) {
        return;
    }

    // 从列表中移除
    int index = m_floatingLabels.indexOf(label);
    if (index >= 0) {
        m_floatingLabels.remove(index);
    }

    // 从场景中移除并删除
    if (scene()) {
        scene()->removeItem(label);
    }

    label->deleteLater();

    qDebug() << "ThermalChart::removeFloatingLabel - 移除浮动标签";
}

void ThermalChart::clearFloatingLabels()
{
    // 移除所有浮动标签
    for (FloatingLabel* label : m_floatingLabels) {
        if (label) {
            if (scene()) {
                scene()->removeItem(label);
            }
            label->deleteLater();
        }
    }

    m_floatingLabels.clear();

    qDebug() << "ThermalChart::clearFloatingLabels - 清空所有浮动标签";
}

// ==================== Phase 3: 标注点（Markers）管理实现 ====================

void ThermalChart::addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                                    const QColor& color, qreal size)
{
    // 早期返回：检查前置条件
    if (curveId.isEmpty() || markers.isEmpty()) {
        return;
    }

    // 如果已存在标注点系列，先移除
    removeCurveMarkers(curveId);

    // 查找曲线对应的系列（用于确定Y轴）
    QLineSeries* curveSeries = seriesForCurve(curveId);
    if (!curveSeries) {
        qWarning() << "ThermalChart::addCurveMarkers - 未找到曲线" << curveId;
        return;
    }

    // 从 CurveManager 获取曲线数据，将 QPointF 转换为 ThermalDataPoint
    QVector<ThermalDataPoint> dataPoints;
    if (m_curveManager) {
        ThermalCurve* curve = m_curveManager->getCurve(curveId);
        if (curve) {
            const auto& curveData = curve->getProcessedData();
            for (const QPointF& marker : markers) {
                double targetTemp = marker.x();
                ThermalDataPoint foundPoint = findNearestDataPoint(curveData, targetTemp);
                dataPoints.append(foundPoint);
            }
        }
    }

    // 如果无法获取完整数据，使用 QPointF 创建临时 ThermalDataPoint
    if (dataPoints.isEmpty()) {
        for (const QPointF& marker : markers) {
            ThermalDataPoint point;
            point.temperature = marker.x();
            point.time = 0.0;
            point.value = marker.y();
            dataPoints.append(point);
        }
    }

    // 创建标注点系列
    auto* markerSeries = new QScatterSeries();
    markerSeries->setName(QString("标注点 (%1)").arg(curveId));
    markerSeries->setMarkerSize(size);
    markerSeries->setColor(color);
    markerSeries->setBorderColor(color.darker(120));
    markerSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);

    // 添加点（根据当前横轴模式选择X坐标）
    for (const ThermalDataPoint& dataPoint : dataPoints) {
        QPointF displayPoint;
        if (m_xAxisMode == XAxisMode::Temperature) {
            displayPoint.setX(dataPoint.temperature);
        } else {
            displayPoint.setX(dataPoint.time);
        }
        displayPoint.setY(dataPoint.value);
        markerSeries->append(displayPoint);
    }

    // 添加到图表
    addSeries(markerSeries);

    // 附着到与曲线相同的轴
    const auto axes = curveSeries->attachedAxes();
    for (QAbstractAxis* axis : axes) {
        markerSeries->attachAxis(axis);
    }

    // 保存映射关系和原始数据
    CurveMarkerData markerData;
    markerData.series = markerSeries;
    markerData.dataPoints = dataPoints;
    m_curveMarkers[curveId] = markerData;

    qDebug() << "ThermalChart::addCurveMarkers - 为曲线" << curveId << "添加了" << markers.size() << "个标注点";
}

void ThermalChart::removeCurveMarkers(const QString& curveId)
{
    // 早期返回：检查是否存在
    if (!m_curveMarkers.contains(curveId)) {
        return;
    }

    CurveMarkerData markerData = m_curveMarkers.take(curveId);
    if (!markerData.series) {
        return;
    }

    // 从图表中移除
    removeSeries(markerData.series);
    markerData.series->deleteLater();

    qDebug() << "ThermalChart::removeCurveMarkers - 移除曲线" << curveId << "的标注点";
}

void ThermalChart::clearAllMarkers()
{
    // 移除所有标注点系列
    for (auto it = m_curveMarkers.begin(); it != m_curveMarkers.end(); ++it) {
        CurveMarkerData& markerData = it.value();
        if (markerData.series) {
            removeSeries(markerData.series);
            markerData.series->deleteLater();
        }
    }

    m_curveMarkers.clear();

    qDebug() << "ThermalChart::clearAllMarkers - 清空所有标注点";
}

// ==================== Phase 3: 测量工具管理实现 ====================

void ThermalChart::addMassLossTool(const ThermalDataPoint& point1,
                                     const ThermalDataPoint& point2,
                                     const QString& curveId)
{
    // 创建测量工具
    auto* tool = new TrapezoidMeasureTool(this);
    tool->setCurveManager(m_curveManager);

    // 设置坐标轴和系列（用于正确的坐标转换）
    if (m_curveManager && !curveId.isEmpty()) {
        QValueAxis* yAxis = yAxisForCurve(curveId);
        QLineSeries* series = seriesForCurve(curveId);
        tool->setAxes(curveId, m_axisX, yAxis, series);
    }

    // 设置当前横轴模式
    tool->setXAxisMode(m_xAxisMode == XAxisMode::Time);

    // 设置测量点（传递完整的 ThermalDataPoint）
    tool->setMeasurePoints(point1, point2);

    // 连接删除信号
    connect(tool, &TrapezoidMeasureTool::removeRequested, this, [this, tool]() {
        removeMassLossTool(tool);
    });

    // 添加到场景
    scene()->addItem(tool);
    m_massLossTools.append(tool);

    qDebug() << "ThermalChart::addMassLossTool - 添加测量工具，测量值:" << tool->measureValue();
}

void ThermalChart::removeMassLossTool(QGraphicsObject* tool)
{
    if (!tool) {
        return;
    }

    m_massLossTools.removeOne(tool);

    // 从场景中移除
    if (scene()) {
        scene()->removeItem(tool);
    }

    tool->deleteLater();

    qDebug() << "ThermalChart::removeMassLossTool - 移除测量工具";
}

void ThermalChart::clearAllMassLossTools()
{
    for (QGraphicsObject* tool : m_massLossTools) {
        if (tool) {
            if (scene()) {
                scene()->removeItem(tool);
            }
            tool->deleteLater();
        }
    }

    m_massLossTools.clear();

    qDebug() << "ThermalChart::clearAllMassLossTools - 清空所有测量工具";
}

// ==================== 峰面积工具实现 ====================

PeakAreaTool* ThermalChart::addPeakAreaTool(const ThermalDataPoint& point1,
                                             const ThermalDataPoint& point2,
                                             const QString& curveId)
{
    // 创建峰面积工具
    auto* tool = new PeakAreaTool(this);
    tool->setCurveManager(m_curveManager);

    // 设置坐标轴和系列（用于正确的坐标转换）
    if (m_curveManager && !curveId.isEmpty()) {
        QValueAxis* yAxis = yAxisForCurve(curveId);
        QLineSeries* series = seriesForCurve(curveId);
        tool->setAxes(curveId, m_axisX, yAxis, series);
    }

    // 设置当前横轴模式
    tool->setXAxisMode(m_xAxisMode == XAxisMode::Time);

    // 设置测量点（传递完整的 ThermalDataPoint）
    tool->setMeasurePoints(point1, point2);

    // 连接删除信号
    connect(tool, &PeakAreaTool::removeRequested, this, [this, tool]() {
        removePeakAreaTool(tool);
    });

    // 连接面积变化信号（用于实时更新 FloatingLabel 等）
    connect(tool, &PeakAreaTool::areaChanged, this, [tool](qreal newArea) {
        qDebug() << "峰面积已更新:" << newArea;
        // 未来可以在这里更新 FloatingLabel
    });

    // 连接坐标轴变化信号，确保缩放/平移后刷新阴影区域
    connect(this, &QChart::plotAreaChanged, tool, &PeakAreaTool::updateCache);

    // 连接横轴模式变化信号，确保切换温度/时间轴后同步更新
    connect(this, &ThermalChart::xAxisModeChanged, tool, [tool](XAxisMode mode) {
        tool->setXAxisMode(mode == XAxisMode::Time);
    });

    // 添加到场景
    scene()->addItem(tool);
    m_peakAreaTools.append(tool);

    qDebug() << "ThermalChart::addPeakAreaTool - 添加峰面积工具，面积:" << tool->peakArea();

    // 返回工具指针，供调用者进一步配置（如设置基线模式）
    return tool;
}

void ThermalChart::removePeakAreaTool(QGraphicsObject* tool)
{
    if (!tool) {
        return;
    }

    m_peakAreaTools.removeOne(tool);

    if (scene()) {
        scene()->removeItem(tool);
    }

    tool->deleteLater();

    qDebug() << "ThermalChart::removePeakAreaTool - 移除峰面积工具";
}

void ThermalChart::clearAllPeakAreaTools()
{
    for (QGraphicsObject* tool : m_peakAreaTools) {
        if (tool) {
            if (scene()) {
                scene()->removeItem(tool);
            }
            tool->deleteLater();
        }
    }

    m_peakAreaTools.clear();

    qDebug() << "ThermalChart::clearAllPeakAreaTools - 清空所有峰面积工具";
}

// ==================== 标题配置（自定义标题）====================

void ThermalChart::setCustomChartTitle(const QString& title)
{
    m_customChartTitle = title;

    // 立即应用标题
    if (m_customChartTitle.isEmpty()) {
        setTitle(tr("热分析曲线"));  // 恢复默认
    } else {
        setTitle(m_customChartTitle);   // 使用自定义
    }

    qDebug() << "ThermalChart::setCustomChartTitle - 设置图表标题:" << (title.isEmpty() ? tr("热分析曲线") : title);
}

void ThermalChart::setCustomXAxisTitle(const QString& title)
{
    m_customXAxisTitle = title;

    // 立即应用标题（需要根据当前横轴模式判断）
    if (m_customXAxisTitle.isEmpty()) {
        // 恢复默认：根据横轴模式自动切换
        if (m_xAxisMode == XAxisMode::Temperature) {
            m_axisX->setTitleText(tr("温度 (°C)"));
        } else {
            m_axisX->setTitleText(tr("时间 (s)"));
        }
    } else {
        m_axisX->setTitleText(m_customXAxisTitle);  // 使用自定义
    }

    qDebug() << "ThermalChart::setCustomXAxisTitle - 设置X轴标题:" << (title.isEmpty() ? "自动" : title);
}

void ThermalChart::setCustomYAxisTitlePrimary(const QString& title)
{
    m_customYAxisTitlePrimary = title;

    // 立即应用标题
    if (m_customYAxisTitlePrimary.isEmpty()) {
        // 恢复默认：使用当前主轴标题（不改变）
        // 实际应该在下次添加曲线时由 ensureYAxisForCurve 自动更新
    } else {
        m_axisY_primary->setTitleText(m_customYAxisTitlePrimary);
    }

    qDebug() << "ThermalChart::setCustomYAxisTitlePrimary - 设置主Y轴标题:" << (title.isEmpty() ? "自动" : title);
}

void ThermalChart::setCustomYAxisTitleSecondary(const QString& title)
{
    m_customYAxisTitleSecondary = title;

    // 立即应用标题（仅当次Y轴已存在时）
    if (m_axisY_secondary) {
        if (m_customYAxisTitleSecondary.isEmpty()) {
            // 恢复默认：保持当前标题（不改变）
            // 实际应该在下次添加微分曲线时由 ensureYAxisForCurve 自动更新
        } else {
            m_axisY_secondary->setTitleText(m_customYAxisTitleSecondary);
        }
    }

    qDebug() << "ThermalChart::setCustomYAxisTitleSecondary - 设置次Y轴标题:" << (title.isEmpty() ? "自动" : title);
}

void ThermalChart::setCustomTitles(const QString& chartTitle,
                                   const QString& xAxisTitle,
                                   const QString& yAxisTitlePrimary,
                                   const QString& yAxisTitleSecondary)
{
    setCustomChartTitle(chartTitle);
    setCustomXAxisTitle(xAxisTitle);
    setCustomYAxisTitlePrimary(yAxisTitlePrimary);
    setCustomYAxisTitleSecondary(yAxisTitleSecondary);

    qDebug() << "ThermalChart::setCustomTitles - 批量设置所有标题";
}

void ThermalChart::clearCustomTitles()
{
    m_customChartTitle.clear();
    m_customXAxisTitle.clear();
    m_customYAxisTitlePrimary.clear();
    m_customYAxisTitleSecondary.clear();

    // 立即应用默认标题
    setTitle(tr("热分析曲线"));

    if (m_xAxisMode == XAxisMode::Temperature) {
        m_axisX->setTitleText(tr("温度 (°C)"));
    } else {
        m_axisX->setTitleText(tr("时间 (s)"));
    }

    // Y轴标题保持当前值（会在下次添加曲线时自动更新）

    qDebug() << "ThermalChart::clearCustomTitles - 清除所有自定义标题";
}

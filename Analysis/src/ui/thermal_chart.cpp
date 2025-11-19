#include "thermal_chart.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include "peak_area_tool.h"
#include "trapezoid_measure_tool.h"

#include <QDebug>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPointer>  // Phase 3: QPointer for lambda capture
#include <QSignalBlocker>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
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

    m_axisY_mass = new QValueAxis(this);
    m_axisY_mass->setTitleText(tr("质量 (mg)"));
    addAxis(m_axisY_mass, Qt::AlignLeft);

    m_axisY_diff = new QValueAxis(this);
    QPen pen = m_axisY_diff->linePen();
    pen.setColor(Qt::red);
    m_axisY_diff->setLinePen(pen);
    m_axisY_diff->setLabelsColor(Qt::red);
    m_axisY_diff->setTitleBrush(QBrush(Qt::red));
    addAxis(m_axisY_diff, Qt::AlignRight);

    // 创建十字线图元
    QPen crosshairPen(Qt::gray, 0.0, Qt::DashLine);

    m_verticalCrosshairLine = new QGraphicsLineItem(this);
    m_verticalCrosshairLine->setPen(crosshairPen);
    m_verticalCrosshairLine->setZValue(zValue() + 10.0);

    m_horizontalCrosshairLine = new QGraphicsLineItem(this);
    m_horizontalCrosshairLine->setPen(crosshairPen);
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

ThermalChart::~ThermalChart() { qDebug() << "析构: ThermalChart"; }

void ThermalChart::setCurveManager(CurveManager* manager) { m_curveManager = manager; }

void ThermalChart::initialize()
{
    // 断言 Setter 注入的必需依赖
    Q_ASSERT(m_curveManager != nullptr);

    // 标记为已初始化状态
    m_initialized = true;

    qDebug() << "ThermalChart 初始化完成，所有依赖已就绪";
}

// ==================== 系列查询接口 ====================
// 通过UUID 查询曲线系列
QLineSeries* ThermalChart::seriesForCurveId(const QString& curveId) const
{
    auto it = m_idToSeries.constFind(curveId);

    if (it == m_idToSeries.constEnd()) {
        qDebug() << "ThermalChart::seriesForCurveId 检查UUID:" << curveId << "为空";
        return nullptr;
    } else {
        return it.value();
    }
}
// 通过曲线系类 查询UUID
QString ThermalChart::curveIdForSeries(QLineSeries* series) const
{
    auto it = m_seriesToId.constFind(series);
    return (it == m_seriesToId.constEnd()) ? QString() : it.value();
}
// 获取曲线的颜色
QColor ThermalChart::getCurveColor(const QString& curveId) const
{
    QLineSeries* series = seriesForCurveId(curveId);
    if (series) {
        return series->color();
    }
    return Qt::black;
}

// ==================== 坐标轴查询接口 ====================
// 通过UUID 获取曲线的Y轴
QValueAxis* ThermalChart::yAxisForCurveId(const QString& curveId)
{
    // 早期返回：未找到曲线系列
    QLineSeries* curveSeries = seriesForCurveId(curveId);
    if (!curveSeries) {
        qWarning() << "ThermalChart::yAxisForCurveId - 未找到曲线" << curveId;
        return m_axisY_mass;
    }

    // 查找该曲线附着的Y轴
    const auto attachedAxes = curveSeries->attachedAxes();
    for (QAbstractAxis* axis : attachedAxes) {
        QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
        if (valueAxis != m_axisX) { // 排除X轴
            return valueAxis;
        }
    }

    // 默认返回主Y轴
    return m_axisY_mass;
}

// ==================== 十字线管理（仅图元接口）====================
// 设置十字线的显示和隐藏
void ThermalChart::setCrosshairEnabled(bool vertical, bool horizontal)
{
    // 如果是开启那么就在事件处理中，获取位置后开启
    m_verticalCrosshairEnabled = vertical;
    m_horizontalCrosshairEnabled = horizontal;

    // 如果是关闭，那么就立马关闭显示
    if (!vertical && !horizontal) {
        clearCrosshair();
    }
}
// 更新十字线的位置信息
void ThermalChart::updateCrosshairAtChartPos(const QPointF& chartPos)
{
    const QRectF plotRect = plotArea();
    if (!plotRect.isValid() || !plotRect.contains(chartPos)) {
        // 鼠标不在绘图区域内，隐藏十字线
        clearCrosshair();
        return;
    }
    // 更新十字线位置
    m_verticalCrosshairLine->setLine(chartPos.x(), plotRect.top(), chartPos.x(), plotRect.bottom());
    m_horizontalCrosshairLine->setLine(plotRect.left(), chartPos.y(), plotRect.right(), chartPos.y());

    // 更新垂直十字线显示
    m_verticalCrosshairLine->setVisible(m_verticalCrosshairEnabled);
    m_horizontalCrosshairLine->setVisible(m_horizontalCrosshairEnabled);
}
// 隐藏十字线
void ThermalChart::clearCrosshair()
{
    m_verticalCrosshairLine->setVisible(false);
    m_horizontalCrosshairLine->setVisible(false);
}
// 单独设置 垂直十字线
void ThermalChart::setVerticalCrosshairEnabled(bool enabled) { setCrosshairEnabled(enabled, m_horizontalCrosshairEnabled); }
// 单独设置 水平十字线
void ThermalChart::setHorizontalCrosshairEnabled(bool enabled) { setCrosshairEnabled(m_verticalCrosshairEnabled, enabled); }

// ==================== 选中点管理（用于算法交互）====================
// 在指定的Y轴上重新挂载散点图
void ThermalChart::rebindSelectedPointsSeries(QValueAxis* targetYAxis)
{
    m_selectedPointsSeries->clear();
    QString axisDebugName = (targetYAxis == m_axisY_mass ? "主轴(左)" : "次轴(右)");

    // 如果还未添加到 chart，则添加
    if (!series().contains(m_selectedPointsSeries)) {
        addSeries(m_selectedPointsSeries);
        attachSeriesToAxes(m_selectedPointsSeries, targetYAxis);
        qDebug() << "ThermalChart::rebindSelectedPointsSeries - 添加选中点系列，Y轴:" << axisDebugName;
        return;
    }

    // 删除已绑定的轴然后，重新绑定
    detachSeriesFromAxes(m_selectedPointsSeries);
    attachSeriesToAxes(m_selectedPointsSeries, targetYAxis);
    qDebug() << "ThermalChart::rebindSelectedPointsSeries - 更新选中点系列的轴，Y轴:" << axisDebugName;
}
// 为散点图添加用户选点的数据
void ThermalChart::addSelectedPoint(const QPointF& point) { m_selectedPointsSeries->append(point); }
// 清空散点图的数据
void ThermalChart::clearSelectedPoints() { m_selectedPointsSeries->clear(); }

// ==================== 系列管理辅助函数（占位符）====================
// 根据热分析数据创建曲线系列
QLineSeries* ThermalChart::createSeriesForThermalCurve(const ThermalCurve& curve) const
{
    auto* series = new QLineSeries();
    series->setName(curve.name());
    QSignalBlocker blocker(series);
    series->replace(buildSeriesPoints(curve));
    return series;
}
// 根据显示模式实时的构建数据
QList<QPointF> ThermalChart::buildSeriesPoints(const ThermalCurve& curve) const
{
    QList<QPointF> points;
    const auto& data = curve.getProcessedData();
    points.reserve(data.size());

    // 根据横轴模式选择 X 轴数据
    if (m_xAxisMode == XAxisMode::Temperature) {
        for (const auto& point : data) {
            points.append(QPointF(point.temperature, point.value));
        }
    } else {
        for (const auto& point : data) {
            points.append(QPointF(point.time, point.value));
        }
    }
    return points;
}
// 绑定到指定的轴上
void ThermalChart::attachSeriesToAxes(QXYSeries* series, QValueAxis* axisY)
{
    series->attachAxis(m_axisX);
    series->attachAxis(axisY);
}
// 从剥离绑定的轴
void ThermalChart::detachSeriesFromAxes(QXYSeries* series)
{
    const auto axes = series->attachedAxes();
    for (QAbstractAxis* axis : axes) {
        series->detachAxis(axis);
    }
}
// 注册键值对 key Value
void ThermalChart::registerSeriesMapping(QLineSeries* series, const QString& curveId)
{
    m_seriesToId.insert(series, curveId);
    m_idToSeries.insert(curveId, series);
}
// 删除键值对
void ThermalChart::unregisterSeriesMapping(const QString& curveId)
{
    QLineSeries* series = seriesForCurveId(curveId);
    if (!series) {
        m_idToSeries.remove(curveId);
        return;
    }

    m_seriesToId.remove(series);
    m_idToSeries.remove(curveId);
}
// 更新选中曲线的样式
void ThermalChart::updateSeriesStyle(QLineSeries* series, bool selected)
{
    QPen pen = series->pen();
    pen.setWidth(selected ? 3 : 1);
    series->setPen(pen);
}

// ==================== 坐标轴管理辅助函数（占位符）====================
// 重新调节指定轴的缩放（只对可见曲线进行调整）
void ThermalChart::updateAxisRangeForAttachedSeries(QValueAxis* axis) const
{
    if (!axis) {
        return;
    }

    qreal minVal = std::numeric_limits<qreal>::max();
    qreal maxVal = std::numeric_limits<qreal>::lowest();

    const QList<QLineSeries*> attachedSeries = lineSeriesAttachedToAxis(axis);

    if (attachedSeries.isEmpty()) {
        return;
    }

    for (auto lineSeries : attachedSeries) {
        // 跳过不可见的系列（只对可见曲线进行适应视图）
        if (!lineSeries->isVisible()) {
            continue;
        }

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
// 获取指定轴的所有 liseseries
QList<QLineSeries*> ThermalChart::lineSeriesAttachedToAxis(QAbstractAxis* axis) const
{
    QList<QLineSeries*> attachedSeries;
    // 遍历当前图表中的所有曲线
    for (QAbstractSeries* s : series()) {
        if (!s) {
            continue;
        }

        // 如果这个曲线绑定了传入的 axis，则加入结果
        if (s->attachedAxes().contains(axis)) {
            // 确认是 QLineSeries 类型（你也可以返回 QAbstractSeries*）
            if (auto* lineSeries = qobject_cast<QLineSeries*>(s)) {
                attachedSeries.append(lineSeries);
            }
        }
    }

    return attachedSeries;
}
// 重置所有轴的状态
void ThermalChart::resetAxesToDefault()
{
    // 检查自定义标题优先级
    if (!m_customYAxisTitlePrimary.isEmpty()) {
        m_axisY_mass->setTitleText(m_customYAxisTitlePrimary);
    } else {
        m_axisY_mass->setTitleText(tr("质量 (mg)"));
    }
    m_axisY_diff->setRange(0.0, 1.0);
    m_axisY_mass->setRange(0.0, 1.0);
    m_axisX->setRange(0.0, 1.0);
}

QValueAxis* ThermalChart::ensureYAxisForCurve(const ThermalCurve& curve)
{
    // ==================== 优先级1：SignalType 强制规则 ====================
    if (curve.signalType() == SignalType::Derivative) {
        // 检查自定义标题优先级
        if (!m_customYAxisTitleDiff.isEmpty()) {
            m_axisY_diff->setTitleText(m_customYAxisTitleDiff);
        } else {
            m_axisY_diff->setTitleText(curve.getYAxisLabel());
        }
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用微分 Y 轴（Derivative 强制规则）";
        return m_axisY_diff;
    }
    // yAxisForCurveId

    // ==================== 优先级2：辅助曲线继承父曲线的 Y 轴 ====================
    if (!curve.isAuxiliaryCurve() || curve.parentId().isEmpty()) {
        qDebug() << "ThermalChart: 曲线" << curve.name() << "使用主 Y 轴（默认）";
        return m_axisY_mass;
    }

    QValueAxis* yAx = yAxisForCurveId(curve.parentId());
    return yAx;
}

// ==================== 数据查询辅助函数 ====================

ThermalDataPoint ThermalChart::findNearestDataPoint(const QVector<ThermalDataPoint>& curveData, double xValue) const
{
    if (curveData.isEmpty()) {
        return ThermalDataPoint();
    }

    // 根据当前横轴模式选择比较字段
    bool useTimeAxis = (m_xAxisMode == XAxisMode::Time);

    int nearestIdx = 0;
    double minDist = useTimeAxis ? qAbs(curveData[0].time - xValue) : qAbs(curveData[0].temperature - xValue);

    for (int i = 1; i < curveData.size(); ++i) {
        double dist = useTimeAxis ? qAbs(curveData[i].time - xValue) : qAbs(curveData[i].temperature - xValue);
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
    QLineSeries* series = createSeriesForThermalCurve(curve);
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
    QLineSeries* series = seriesForCurveId(curve.id());
    if (!series) {
        return;
    }

    QSignalBlocker blocker(series);
    series->replace(buildSeriesPoints(curve));
    detachSeriesFromAxes(series);
    QValueAxis* axisY_target = ensureYAxisForCurve(curve);
    attachSeriesToAxes(series, axisY_target);

    rescaleAxes();
}

void ThermalChart::removeCurve(const QString& curveId)
{
    QLineSeries* series = seriesForCurveId(curveId);
    if (!series) {
        return;
    }

    removeSeries(series);
    if (m_selectedSeries == series) {
        m_selectedSeries = nullptr;
    }

    // 清除该曲线的标注点（如果有）
    removeCurveMarkers(curveId);

    // 清除该曲线关联的测量工具（防止悬空指针导致崩溃）
    removeCurveMassLossTools(curveId);

    // 清除该曲线关联的峰面积工具（防止悬空指针导致崩溃）
    removeCurvePeakAreaTools(curveId);

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
    clearAllMarkers();
    clearAllMassLossTools(); // 清空所有测量工具（TrapezoidMeasureTool）
    clearAllPeakAreaTools(); // 清空所有峰面积工具
}

void ThermalChart::setCurveVisible(const QString& curveId, bool visible)
{
    Q_ASSERT(m_initialized); // 确保依赖完整

    // 早期返回：检查曲线是否存在
    QLineSeries* series = seriesForCurveId(curveId);
    if (!series || series->isVisible() == visible) {
        return;
    }

    // 设置当前曲线的可见性
    series->setVisible(visible);

    // ==================== 级联处理标注点（Markers）====================
    if (m_curveMarkers.contains(curveId)) {
        CurveMarkerData& markerData = m_curveMarkers[curveId];
        if (markerData.series) {
            markerData.series->setVisible(visible);
            qDebug() << "ThermalChart::setCurveVisible - 同步标注点可见性:" << curveId << visible;
        }
    }

    // ==================== 级联处理子曲线 ====================
    QVector<ThermalCurve*> children = m_curveManager->getChildren(curveId);
    for (ThermalCurve* child : children) {
        if (!child)
            continue;

        // 只级联处理强绑定的子曲线（如基线）
        if (child->isStronglyBound()) {
            QLineSeries* childSeries = seriesForCurveId(child->id());
            if (childSeries) {
                childSeries->setVisible(visible);

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

    rescaleAxes();
    emit curveVisibilityChanged(curveId, visible);
}

void ThermalChart::highlightCurve(const QString& curveId)
{
    // 查找对应的系列
    QLineSeries* targetSeries = seriesForCurveId(curveId);

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
    updateAxisRangeForAttachedSeries(m_axisX);
    updateAxisRangeForAttachedSeries(m_axisY_mass);
    updateAxisRangeForAttachedSeries(m_axisY_diff);

    // 坐标轴变化后，通知所有工具更新（避免工具位置显示错误）
    updateAllTools();
}

void ThermalChart::setXAxisMode(XAxisMode mode)
{
    Q_ASSERT(m_initialized); // 确保依赖完整

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
    const auto& allCurves = m_curveManager->getAllCurves();
    for (const ThermalCurve& curve : allCurves) {
        QLineSeries* series = seriesForCurveId(curve.id());
        if (series) {
            QSignalBlocker blocker(series);
            series->replace(buildSeriesPoints(curve));
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

    // 发出信号通知叠加物更新
    emit xAxisModeChanged(m_xAxisMode);
}

// ==================== Phase 3: 标注点（Markers）管理实现 ====================

void ThermalChart::addCurveMarkers(const QString& curveId, const QList<QPointF>& markers, const QColor& color, qreal size)
{
    Q_ASSERT(m_initialized); // 确保依赖完整

    // 早期返回：检查前置条件
    if (curveId.isEmpty() || markers.isEmpty()) {
        return;
    }

    // 如果已存在标注点系列，先移除
    removeCurveMarkers(curveId);

    // 查找曲线对应的系列（用于确定Y轴）
    QLineSeries* curveSeries = seriesForCurveId(curveId);
    if (!curveSeries) {
        qWarning() << "ThermalChart::addCurveMarkers - 未找到曲线" << curveId;
        return;
    }

    // 从 CurveManager 获取曲线数据，将 QPointF 转换为 ThermalDataPoint
    QVector<ThermalDataPoint> dataPoints;
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (curve) {
        const auto& curveData = curve->getProcessedData();
        for (const QPointF& marker : markers) {
            double targetX = marker.x(); // 根据横轴模式，可能是 temperature 或 time
            ThermalDataPoint foundPoint = findNearestDataPoint(curveData, targetX);
            dataPoints.append(foundPoint);
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

QGraphicsObject* ThermalChart::addMassLossTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)
{
    Q_ASSERT(m_initialized); // 确保依赖完整

    // 创建测量工具
    auto* tool = new TrapezoidMeasureTool(this);
    tool->setCurveManager(m_curveManager);

    // 设置坐标轴和系列（用于正确的坐标转换）
    if (!curveId.isEmpty()) {
        QValueAxis* yAxis = yAxisForCurveId(curveId);
        QLineSeries* series = seriesForCurveId(curveId);
        tool->setAxes(curveId, m_axisX, yAxis, series);
    }

    // 设置当前横轴模式
    tool->setXAxisMode(m_xAxisMode == XAxisMode::Time);

    // 设置测量点（传递完整的 ThermalDataPoint）
    tool->setMeasurePoints(point1, point2);

    // 连接删除信号（使用 QPointer 防止访问已删除对象）
    connect(tool, &TrapezoidMeasureTool::removeRequested, this, [this, toolPtr = QPointer<QGraphicsObject>(tool)]() {
        if (!toolPtr.isNull()) {
            emit massLossToolRemoveRequested(toolPtr.data());
        }
    });

    // 添加到场景
    scene()->addItem(tool);
    m_massLossTools.append(tool);

    qDebug() << "ThermalChart::addMassLossTool - 添加测量工具，测量值:" << tool->measureValue();

    return tool;  // 返回工具指针
}

void ThermalChart::removeMassLossTool(QGraphicsObject* tool)
{
    m_massLossTools.removeOne(tool);

    // 从场景中移除
    scene()->removeItem(tool);

    // 阻止信号发射（删除已由命令模式触发，无需再发出 removeRequested）
    tool->blockSignals(true);
    tool->deleteLater();

    qDebug() << "ThermalChart::removeMassLossTool - 移除测量工具";
}

void ThermalChart::clearAllMassLossTools()
{
    for (QGraphicsObject* tool : m_massLossTools) {
        if (tool) {
            scene()->removeItem(tool);
            tool->deleteLater();
        }
    }

    m_massLossTools.clear();

    qDebug() << "ThermalChart::clearAllMassLossTools - 清空所有测量工具";
}

void ThermalChart::removeCurveMassLossTools(const QString& curveId)
{
    if (curveId.isEmpty()) {
        return;
    }

    // 逆序遍历，避免删除时索引错乱
    for (int i = m_massLossTools.size() - 1; i >= 0; --i) {
        QGraphicsObject* obj = m_massLossTools[i];
        if (!obj) {
            continue;
        }

        // 尝试转换为 TrapezoidMeasureTool 并检查 curveId
        TrapezoidMeasureTool* tool = qobject_cast<TrapezoidMeasureTool*>(obj);
        if (tool && tool->curveId() == curveId) {
            qDebug() << "ThermalChart::removeCurveMassLossTools - 删除曲线" << curveId << "的测量工具";
            m_massLossTools.removeAt(i);
            scene()->removeItem(obj);

            // 阻止信号发射，防止删除时触发 removeRequested 信号导致 "pure virtual method called" 崩溃
            obj->blockSignals(true);
            obj->deleteLater();
        }
    }
}

// ==================== 峰面积工具实现 ====================

PeakAreaTool*
ThermalChart::addPeakAreaTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)
{
    Q_ASSERT(m_initialized); // 确保依赖完整

    // 创建峰面积工具
    auto* tool = new PeakAreaTool(this);
    tool->setCurveManager(m_curveManager);

    // 设置坐标轴和系列（用于正确的坐标转换）
    if (!curveId.isEmpty()) {
        QValueAxis* yAxis = yAxisForCurveId(curveId);
        QLineSeries* series = seriesForCurveId(curveId);
        tool->setAxes(curveId, m_axisX, yAxis, series);
    }

    // 设置当前横轴模式
    tool->setXAxisMode(m_xAxisMode == XAxisMode::Time);

    // 设置测量点（传递完整的 ThermalDataPoint）
    tool->setMeasurePoints(point1, point2);

    // 连接删除信号（使用 QPointer 防止访问已删除对象）
    connect(tool, &PeakAreaTool::removeRequested, this, [this, toolPtr = QPointer<PeakAreaTool>(tool)]() {
        if (!toolPtr.isNull()) {
            emit peakAreaToolRemoveRequested(toolPtr.data());
        }
    });

    // 连接面积变化信号
    connect(tool, &PeakAreaTool::areaChanged, this, [](qreal newArea) { qDebug() << "峰面积已更新:" << newArea; });

    // 连接坐标轴变化信号，确保缩放/平移后刷新阴影区域
    connect(this, &QChart::plotAreaChanged, tool, &PeakAreaTool::updateCache);

    // 连接横轴模式变化信号，确保切换温度/时间轴后同步更新
    connect(this, &ThermalChart::xAxisModeChanged, tool, [tool](XAxisMode mode) { tool->setXAxisMode(mode == XAxisMode::Time); });

    // 添加到场景
    scene()->addItem(tool);
    m_peakAreaTools.append(tool);

    qDebug() << "ThermalChart::addPeakAreaTool - 添加峰面积工具，面积:" << tool->peakArea();

    // 返回工具指针，供调用者进一步配置（如设置基线模式）
    return tool;
}

void ThermalChart::removePeakAreaTool(QGraphicsObject* tool)
{
    m_peakAreaTools.removeOne(tool);

    scene()->removeItem(tool);

    // 阻止信号发射（删除已由命令模式触发，无需再发出 removeRequested）
    tool->blockSignals(true);
    tool->deleteLater();

    qDebug() << "ThermalChart::removePeakAreaTool - 移除峰面积工具";
}

void ThermalChart::clearAllPeakAreaTools()
{
    for (QGraphicsObject* tool : m_peakAreaTools) {
        if (tool) {
            scene()->removeItem(tool);
            tool->deleteLater();
        }
    }

    m_peakAreaTools.clear();

    qDebug() << "ThermalChart::clearAllPeakAreaTools - 清空所有峰面积工具";
}

void ThermalChart::updateAllTools()
{
    // 更新所有测量工具
    for (QGraphicsObject* tool : m_massLossTools) {
        if (tool) {
            tool->update();
        }
    }

    // 更新所有峰面积工具
    for (QGraphicsObject* tool : m_peakAreaTools) {
        if (tool) {
            tool->update();
        }
    }
}

void ThermalChart::removeCurvePeakAreaTools(const QString& curveId)
{
    if (curveId.isEmpty()) {
        return;
    }

    // 逆序遍历，避免删除时索引错乱
    for (int i = m_peakAreaTools.size() - 1; i >= 0; --i) {
        QGraphicsObject* obj = m_peakAreaTools[i];
        if (!obj) {
            continue;
        }

        // 尝试转换为 PeakAreaTool 并检查 curveId
        PeakAreaTool* tool = qobject_cast<PeakAreaTool*>(obj);
        if (tool && tool->curveId() == curveId) {
            qDebug() << "ThermalChart::removeCurvePeakAreaTools - 删除曲线" << curveId << "的峰面积工具";
            m_peakAreaTools.removeAt(i);
            scene()->removeItem(obj);

            // 阻止信号发射，防止删除时触发 removeRequested 信号导致 "pure virtual method called" 崩溃
            obj->blockSignals(true);
            obj->deleteLater();
        }
    }
}

// ==================== 标题配置（自定义标题）====================

void ThermalChart::setCustomChartTitle(const QString& title)
{
    m_customChartTitle = title;

    // 立即应用标题
    if (m_customChartTitle.isEmpty()) {
        setTitle(tr("热分析曲线")); // 恢复默认
    } else {
        setTitle(m_customChartTitle); // 使用自定义
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
        m_axisX->setTitleText(m_customXAxisTitle); // 使用自定义
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
        m_axisY_mass->setTitleText(m_customYAxisTitlePrimary);
    }

    qDebug() << "ThermalChart::setCustomYAxisTitlePrimary - 设置主Y轴标题:" << (title.isEmpty() ? "自动" : title);
}

void ThermalChart::setCustomYAxisTitleSecondary(const QString& title)
{
    m_customYAxisTitleDiff = title;

    // 立即应用标题（仅当次Y轴已存在时）
    if (m_axisY_diff) {
        if (m_customYAxisTitleDiff.isEmpty()) {
            // 恢复默认：保持当前标题（不改变）
            // 实际应该在下次添加微分曲线时由 ensureYAxisForCurve 自动更新
        } else {
            m_axisY_diff->setTitleText(m_customYAxisTitleDiff);
        }
    }

    qDebug() << "ThermalChart::setCustomYAxisTitleSecondary - 设置次Y轴标题:" << (title.isEmpty() ? "自动" : title);
}

void ThermalChart::setCustomTitles(
    const QString& chartTitle, const QString& xAxisTitle, const QString& yAxisTitlePrimary, const QString& yAxisTitleSecondary)
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
    m_customYAxisTitleDiff.clear();

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

// ==================== 框选缩放辅助函数实现 ====================

bool ThermalChart::isSeriesAttachedToYAxis(QLineSeries* series, QValueAxis* yAxis) const
{
    if (!series || !yAxis) {
        return false;
    }

    QList<QAbstractAxis*> yAxes = series->attachedAxes();
    for (QAbstractAxis* axis : yAxes) {
        if (axis == yAxis) {
            return true;
        }
    }

    return false;
}

bool ThermalChart::calculateYRangeInXRange(QLineSeries* series, qreal xMin, qreal xMax, qreal& outYMin, qreal& outYMax) const
{
    if (!series) {
        return false;
    }

    bool hasData = false;
    qreal yMin = std::numeric_limits<qreal>::max();
    qreal yMax = std::numeric_limits<qreal>::lowest();

    // 遍历系列的数据点，找出在 [xMin, xMax] 范围内的 Y 值范围
    const QVector<QPointF>& points = series->pointsVector();
    for (const QPointF& point : points) {
        qreal x = point.x();
        qreal y = point.y();

        // 只考虑在 X 范围内的点
        if (x >= xMin && x <= xMax) {
            yMin = qMin(yMin, y);
            yMax = qMax(yMax, y);
            hasData = true;
        }
    }

    if (hasData) {
        outYMin = yMin;
        outYMax = yMax;
    }

    return hasData;
}


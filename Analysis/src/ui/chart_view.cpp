#include "chart_view.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>
#include <QEvent>
#include <QGraphicsLineItem>
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QAbstractSeries>
#include <QtCharts/QChart>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtGlobal>
#include <QtMath>
#include <limits>

QT_CHARTS_USE_NAMESPACE

ChartView::ChartView(QWidget* parent)
    : QWidget(parent)
    , m_chartView(nullptr)
    , m_selectedSeries(nullptr)
{
    qDebug() << "构造:  ChartView";
    m_chartView = new QChartView(this);
    QChart* chart = new QChart();
    chart->setTitle(tr("热分析曲线"));
    m_chartView->setChart(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 安装事件过滤器以在 chartView 上绘制
    m_chartView->viewport()->installEventFilter(this);

    // 创建并设置主坐标轴
    m_axisX = new QValueAxis();
    m_axisX->setTitleText(tr("温度 (°C)"));
    chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY_primary = new QValueAxis();
    m_axisY_primary->setTitleText(tr("质量 (mg)"));
    chart->addAxis(m_axisY_primary, Qt::AlignLeft);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);

    m_chartView->setMouseTracking(true);
    m_chartView->viewport()->setMouseTracking(true);

    QPen crosshairPen(Qt::gray, 0.0, Qt::DashLine);
    m_verticalCrosshairLine = new QGraphicsLineItem(chart);
    m_verticalCrosshairLine->setPen(crosshairPen);
    m_verticalCrosshairLine->setVisible(false);
    m_verticalCrosshairLine->setZValue(chart->zValue() + 10.0);

    m_horizontalCrosshairLine = new QGraphicsLineItem(chart);
    m_horizontalCrosshairLine->setPen(crosshairPen);
    m_horizontalCrosshairLine->setVisible(false);
    m_horizontalCrosshairLine->setZValue(chart->zValue() + 10.0);

    updateCrosshairVisibility();
}

void ChartView::addCurve(const ThermalCurve& curve)
{
    QLineSeries* series = createSeriesForCurve(curve);
    if (!series) {
        return;
    }

    QChart* chart = m_chartView->chart();
    chart->addSeries(series);
    registerSeriesMapping(series, curve.id());

    QValueAxis* axisY_target = ensureYAxisForCurve(curve);
    attachSeriesToAxes(series, axisY_target);

    rescaleAxes();
}

void ChartView::rescaleAxes()
{
    rescaleAxis(m_axisX);
    rescaleAxis(m_axisY_primary);
    rescaleAxis(m_axisY_secondary);
}

void ChartView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF chartViewPos = mapToChartCoordinates(event->pos());
    if (m_mode == InteractionMode::Pick) {
        handlePointSelectionClick(chartViewPos);
    } else {
        handleCurveSelectionClick(chartViewPos);
    }

    QWidget::mousePressEvent(event);
}

void ChartView::handleCurveSelectionClick(const QPointF& chartPos)
{
    qreal bestDist = std::numeric_limits<qreal>::max();
    QLineSeries* clickedSeries = findSeriesNearPoint(chartPos, bestDist);

    const qreal deviceRatio = m_chartView->devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio;
    if (clickedSeries && m_hitTestIncludePen) {
        const qreal penWidth = clickedSeries->pen().widthF();
        baseThreshold += penWidth * 0.5;
    }

    if (clickedSeries && bestDist <= baseThreshold) {
        if (m_selectedSeries) {
            updateSeriesStyle(m_selectedSeries, false);
        }
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
}

void ChartView::handlePointSelectionClick(const QPointF& chartViewPos)
{
    if (!m_chartView || !m_chartView->chart()) {
        return;
    }

    const QPointF valuePoint = m_chartView->chart()->mapToValue(chartViewPos);

    // ==================== 新架构：检查活动算法状态 ====================
    if (m_activeAlgorithm.isValid() && m_interactionState == InteractionState::WaitingForPoints) {
        // 添加选点到活动算法的选点列表
        m_selectedPoints.append(valuePoint);
        qDebug() << "ChartView: 算法" << m_activeAlgorithm.displayName
                 << "选点进度:" << m_selectedPoints.size() << "/" << m_activeAlgorithm.requiredPointCount
                 << ", 点:" << valuePoint;

        // 检查是否已收集足够的点
        if (m_selectedPoints.size() >= m_activeAlgorithm.requiredPointCount) {
            // 状态转换: WaitingForPoints → PointsCompleted
            m_interactionState = InteractionState::PointsCompleted;
            emit interactionStateChanged(m_interactionState);

            qDebug() << "ChartView: 算法" << m_activeAlgorithm.displayName
                     << "交互完成，发送信号触发执行";

            // 发出算法交互完成信号，触发算法执行
            emit algorithmInteractionCompleted(m_activeAlgorithm.name, m_selectedPoints);

            // 切换回视图模式
            setInteractionMode(InteractionMode::View);
        }
        return;
    }

    // ==================== 旧架构兼容：使用 pickCount 模式 ====================
    if (m_pickCount <= 0) {
        return;
    }

    m_pickPoints.append(valuePoint);

    if (m_pickPoints.size() >= m_pickCount) {
        emit pickPoints(m_pickPoints);
        qDebug() << "发送用户选取的点坐标:" << m_pickPoints;
        m_pickCount = 0;
        setInteractionMode(InteractionMode::View);
    }
}

QPointF ChartView::mapToChartCoordinates(const QPoint& widgetPos) const
{
    // 从窗口内的坐标 到全局坐标 再到想要的窗口坐标
    return m_chartView->mapFromGlobal(mapToGlobal(widgetPos));
}

QLineSeries* ChartView::findSeriesNearPoint(const QPointF& chartPos, qreal& outDistance) const
{
    QChart* chart = m_chartView->chart();
    if (!chart) {
        outDistance = std::numeric_limits<qreal>::max();
        return nullptr;
    }

    auto pointToSegDist = [](const QPointF& p, const QPointF& a, const QPointF& b) -> qreal {
        const qreal vx = b.x() - a.x();
        const qreal vy = b.y() - a.y();
        const qreal wx = p.x() - a.x();
        const qreal wy = p.y() - a.y();
        const qreal vv = vx * vx + vy * vy;
        qreal t = vv > 0 ? (wx * vx + wy * vy) / vv : 0.0;
        if (t < 0) {
            t = 0;
        } else if (t > 1) {
            t = 1;
        }
        const QPointF proj(a.x() + t * vx, a.y() + t * vy);
        const qreal dx = p.x() - proj.x();
        const qreal dy = p.y() - proj.y();
        return qSqrt(dx * dx + dy * dy);
    };

    QLineSeries* closestSeries = nullptr;
    qreal bestDist = std::numeric_limits<qreal>::max();

    for (QAbstractSeries* abstractSeries : chart->series()) {
        QLineSeries* lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) {
            continue;
        }

        const auto points = lineSeries->pointsVector();
        if (points.size() < 2) {
            continue;
        }

        QPointF previous = chart->mapToPosition(points[0], lineSeries);
        for (int i = 1; i < points.size(); ++i) {
            const QPointF current = chart->mapToPosition(points[i], lineSeries);
            const qreal dist = pointToSegDist(chartPos, previous, current);
            if (dist < bestDist) {
                bestDist = dist;
                closestSeries = lineSeries;
            }
            previous = current;
        }
    }

    outDistance = bestDist;
    return closestSeries;
}
void ChartView::updateSeriesStyle(QLineSeries* series, bool selected)
{
    if (!series)
        return;
    QPen pen = series->pen();
    pen.setWidth(selected ? 3 : 1);
    series->setPen(pen);
}

// —— 命中阈值配置 ——
void ChartView::setHitTestBasePixelThreshold(qreal px)
{
    // 限制最小值，避免被设置为 0 导致很难命中
    m_hitTestBasePx = (px <= 0) ? 1.0 : px;
}

qreal ChartView::hitTestBasePixelThreshold() const { return m_hitTestBasePx; }

void ChartView::setHitTestIncludePenWidth(bool enabled) { m_hitTestIncludePen = enabled; }

bool ChartView::hitTestIncludePenWidth() const { return m_hitTestIncludePen; }

void ChartView::setPickCount(int count)
{
    m_pickCount = qMax(0, count);
    m_pickPoints.clear();
}

void ChartView::setInteractionMode(InteractionMode type)
{
    if (m_mode == type) {
        return;
    }

    m_mode = type;
    m_pickPoints.clear();

    if (m_mode == InteractionMode::Pick) {
        qDebug() << "视图进入选点模式";
        if (m_chartView) {
            m_chartView->setRubberBand(QChartView::NoRubberBand);
        }
        setCursor(Qt::CrossCursor);
    } else {
        if (m_chartView) {
            m_chartView->setRubberBand(QChartView::RectangleRubberBand);
        }
        unsetCursor();
    }
}

// ==================== 活动算法状态机实现 ====================

void ChartView::startAlgorithmInteraction(const QString& algorithmName, const QString& displayName,
                                          int requiredPoints, const QString& hint)
{
    qDebug() << "ChartView::startAlgorithmInteraction - 启动算法交互";
    qDebug() << "  算法名称:" << algorithmName;
    qDebug() << "  显示名称:" << displayName;
    qDebug() << "  需要点数:" << requiredPoints;
    qDebug() << "  提示信息:" << hint;

    // 清空之前的状态
    m_selectedPoints.clear();

    // 设置活动算法信息
    m_activeAlgorithm.name = algorithmName;
    m_activeAlgorithm.displayName = displayName;
    m_activeAlgorithm.requiredPointCount = requiredPoints;
    m_activeAlgorithm.hint = hint;

    // 状态转换: Idle → WaitingForPoints
    m_interactionState = InteractionState::WaitingForPoints;
    emit interactionStateChanged(m_interactionState);

    // 切换到选点模式
    setInteractionMode(InteractionMode::Pick);
    setPickCount(requiredPoints);  // 兼容旧架构的可视化反馈

    qDebug() << "ChartView: 算法" << displayName << "已进入等待用户选点状态";
}

void ChartView::cancelAlgorithmInteraction()
{
    if (!m_activeAlgorithm.isValid()) {
        qDebug() << "ChartView::cancelAlgorithmInteraction - 没有活动算法，无需取消";
        return;
    }

    qDebug() << "ChartView::cancelAlgorithmInteraction - 取消算法交互:" << m_activeAlgorithm.displayName;

    // 清空活动算法信息
    m_activeAlgorithm.clear();
    m_selectedPoints.clear();

    // 状态转换: 任意状态 → Idle
    m_interactionState = InteractionState::Idle;
    emit interactionStateChanged(m_interactionState);

    // 切换回视图模式
    setInteractionMode(InteractionMode::View);

    qDebug() << "ChartView: 算法交互已取消，回到空闲状态";
}

void ChartView::setCurveVisible(const QString& curveId, bool visible)
{
    QLineSeries* series = seriesForCurve(curveId);
    if (!series || series->isVisible() == visible) {
        return;
    }

    series->setVisible(visible);
    updateLegendVisibility(series, visible);
    rescaleAxes();
}

void ChartView::setVerticalCrosshairEnabled(bool enabled)
{
    if (m_verticalCrosshairEnabled == enabled) {
        return;
    }

    m_verticalCrosshairEnabled = enabled;
    if (!m_verticalCrosshairEnabled && !m_horizontalCrosshairEnabled) {
        clearCrosshair();
    } else {
        updateCrosshairVisibility();
    }
}

void ChartView::setHorizontalCrosshairEnabled(bool enabled)
{
    if (m_horizontalCrosshairEnabled == enabled) {
        return;
    }

    m_horizontalCrosshairEnabled = enabled;
    if (!m_verticalCrosshairEnabled && !m_horizontalCrosshairEnabled) {
        clearCrosshair();
    } else {
        updateCrosshairVisibility();
    }
}

void ChartView::updateCurve(const ThermalCurve& curve)
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

void ChartView::removeCurve(const QString& curveId)
{
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        return;
    }

    QChart* chart = m_chartView->chart();
    chart->removeSeries(series);
    if (m_selectedSeries == series) {
        m_selectedSeries = nullptr;
    }

    unregisterSeriesMapping(curveId);
    series->deleteLater();

    rescaleAxes();
}

void ChartView::clearCurves()
{
    QChart* chart = m_chartView->chart();
    const auto currentSeries = chart->series();
    for (QAbstractSeries* series : currentSeries) {
        chart->removeSeries(series);
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
    emit curveSelected(QString());
}

bool ChartView::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_chartView->viewport()) {
        switch (event->type()) {
        case QEvent::Paint: {
            QPainter painter(m_chartView->viewport());
            painter.setRenderHint(QPainter::Antialiasing);
            drawAnnotationLines(painter);
            return false;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            updateCrosshairPosition(mouseEvent->pos());
            break;
        }
        case QEvent::Leave:
        case QEvent::HoverLeave:
            clearCrosshair();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ChartView::drawAnnotationLines(QPainter& painter)
{
    if (m_annotations.isEmpty()) {
        return;
    }

    qDebug() << "ChartView::drawAnnotationLines - 绘制" << m_annotations.size() << "条注释线";
    QChart* chart = m_chartView->chart();

    int lineIndex = 0;
    for (const auto& annotation : m_annotations) {
        qDebug() << "  注释线" << lineIndex << ": ID=" << annotation.id << ", 曲线ID=" << annotation.curveId
                 << ", 起点=" << annotation.start << ", 终点=" << annotation.end;

        QLineSeries* series = m_idToSeries.value(annotation.curveId);
        if (!series) {
            qWarning() << "    错误: 无法找到曲线" << annotation.curveId << "的系列对象";
            lineIndex++;
            continue;
        }

        // 将数据坐标转换为场景坐标
        QPointF sceneStart = chart->mapToPosition(annotation.start, series);
        QPointF sceneEnd = chart->mapToPosition(annotation.end, series);

        // 将场景坐标转换为视口坐标
        QPointF viewportStart = m_chartView->mapFromScene(sceneStart);
        QPointF viewportEnd = m_chartView->mapFromScene(sceneEnd);

        qDebug() << "    数据坐标" << annotation.start << annotation.end << "-> 场景坐标" << sceneStart << sceneEnd
                 << "-> 视口坐标" << viewportStart << viewportEnd;

        painter.setPen(annotation.pen);
        painter.drawLine(viewportStart, viewportEnd);

        qDebug() << "    线条绘制完成";
        lineIndex++;
    }

    qDebug() << "  注释线绘制完成";
}

// ========== 注释线管理方法 ==========
void ChartView::addAnnotationLine(
    const QString& id, const QString& curveId, const QPointF& start, const QPointF& end, const QPen& pen)
{
    m_annotations.append({ id, curveId, start, end, pen });
    qDebug() << "ChartView: 添加注释线" << id << "在曲线" << curveId << "数据点:" << start << end;
    m_chartView->viewport()->update(); // 触发 viewport 重绘
}

void ChartView::removeAnnotation(const QString& id)
{
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (m_annotations[i].id == id) {
            m_annotations.removeAt(i);
            qDebug() << "ChartView: 移除注释线" << id;
            m_chartView->viewport()->update(); // 触发 viewport 重绘
            return;
        }
    }
}

void ChartView::clearAllAnnotations()
{
    if (!m_annotations.isEmpty()) {
        m_annotations.clear();
        qDebug() << "ChartView: 清除所有注释线";
        m_chartView->viewport()->update(); // 触发 viewport 重绘
    }
}

void ChartView::updateCrosshairPosition(const QPointF& viewportPos)
{
    if (!m_verticalCrosshairEnabled && !m_horizontalCrosshairEnabled) {
        return;
    }

    if (!m_chartView || !m_chartView->chart()) {
        return;
    }

    QChart* chart = m_chartView->chart();
    const QRectF plotRect = chart->plotArea();
    if (!plotRect.isValid()) {
        clearCrosshair();
        return;
    }

    QPointF scenePos = m_chartView->mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart->mapFromScene(scenePos);

    if (!plotRect.contains(chartPos)) {
        if (m_hasMousePosition) {
            clearCrosshair();
        }
        return;
    }

    m_hasMousePosition = true;

    if (m_verticalCrosshairLine) {
        if (m_verticalCrosshairEnabled) {
            m_verticalCrosshairLine->setLine(QLineF(chartPos.x(), plotRect.top(), chartPos.x(), plotRect.bottom()));
            m_verticalCrosshairLine->setVisible(true);
        } else {
            m_verticalCrosshairLine->setVisible(false);
        }
    }

    if (m_horizontalCrosshairLine) {
        if (m_horizontalCrosshairEnabled) {
            m_horizontalCrosshairLine->setLine(QLineF(plotRect.left(), chartPos.y(), plotRect.right(), chartPos.y()));
            m_horizontalCrosshairLine->setVisible(true);
        } else {
            m_horizontalCrosshairLine->setVisible(false);
        }
    }

    updateCrosshairVisibility();
}

void ChartView::clearCrosshair()
{
    if (!m_hasMousePosition) {
        updateCrosshairVisibility();
        return;
    }

    m_hasMousePosition = false;
    updateCrosshairVisibility();
}

QLineSeries* ChartView::seriesForCurve(const QString& curveId) const
{
    auto it = m_idToSeries.constFind(curveId);
    return (it == m_idToSeries.constEnd()) ? nullptr : it.value();
}

QLineSeries* ChartView::createSeriesForCurve(const ThermalCurve& curve) const
{
    auto* series = new QLineSeries();
    series->setName(curve.name());
    populateSeriesWithCurveData(series, curve);
    return series;
}

void ChartView::populateSeriesWithCurveData(QLineSeries* series, const ThermalCurve& curve) const
{
    if (!series) {
        return;
    }

    QSignalBlocker blocker(series);
    series->clear();

    const auto& data = curve.getProcessedData();
    for (const auto& point : data) {
        series->append(point.temperature, point.value);
    }
}

void ChartView::attachSeriesToAxes(QLineSeries* series, QValueAxis* axisY)
{
    if (!series || !axisY || !m_axisX) {
        return;
    }

    series->attachAxis(m_axisX);
    series->attachAxis(axisY);
}

void ChartView::detachSeriesFromAxes(QLineSeries* series)
{
    if (!series) {
        return;
    }

    const auto axes = series->attachedAxes();
    for (QAbstractAxis* axis : axes) {
        series->detachAxis(axis);
    }
}

void ChartView::registerSeriesMapping(QLineSeries* series, const QString& curveId)
{
    if (!series) {
        return;
    }

    m_seriesToId.insert(series, curveId);
    m_idToSeries.insert(curveId, series);
}

void ChartView::unregisterSeriesMapping(const QString& curveId)
{
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        m_idToSeries.remove(curveId);
        return;
    }

    m_seriesToId.remove(series);
    m_idToSeries.remove(curveId);
}

void ChartView::updateLegendVisibility(QLineSeries* series, bool visible) const
{
    if (!series || !m_chartView || !m_chartView->chart()) {
        return;
    }

    if (QLegend* legend = m_chartView->chart()->legend()) {
        const auto markers = legend->markers(series);
        for (QLegendMarker* marker : markers) {
            marker->setVisible(visible);
        }
    }
}

void ChartView::rescaleAxis(QValueAxis* axis) const
{
    if (!axis || !m_chartView || !m_chartView->chart()) {
        return;
    }

    QChart* chart = m_chartView->chart();

    qreal minVal = std::numeric_limits<qreal>::max();
    qreal maxVal = std::numeric_limits<qreal>::lowest();
    bool hasData = false;

    const auto seriesList = chart->series();
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

void ChartView::resetAxesToDefault()
{
    if (!m_chartView || !m_chartView->chart()) {
        return;
    }

    QChart* chart = m_chartView->chart();

    if (m_axisY_secondary) {
        chart->removeAxis(m_axisY_secondary);
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

void ChartView::updateCrosshairVisibility()
{
    const bool showVertical = m_verticalCrosshairEnabled && m_hasMousePosition;
    const bool showHorizontal = m_horizontalCrosshairEnabled && m_hasMousePosition;

    if (m_verticalCrosshairLine) {
        m_verticalCrosshairLine->setVisible(showVertical);
    }
    if (m_horizontalCrosshairLine) {
        m_horizontalCrosshairLine->setVisible(showHorizontal);
    }
}

QValueAxis* ChartView::ensureYAxisForCurve(const ThermalCurve& curve)
{
    QChart* chart = m_chartView->chart();
    if (curve.signalType() == SignalType::Derivative) {
        if (!m_axisY_secondary) {
            m_axisY_secondary = new QValueAxis();
            QPen pen = m_axisY_secondary->linePen();
            pen.setColor(Qt::red);
            m_axisY_secondary->setLinePen(pen);
            m_axisY_secondary->setLabelsColor(Qt::red);
            m_axisY_secondary->setTitleBrush(QBrush(Qt::red));
            chart->addAxis(m_axisY_secondary, Qt::AlignRight);
        }
        m_axisY_secondary->setTitleText(curve.getYAxisLabel());
        return m_axisY_secondary;
    }

    if (!m_axisY_primary) {
        m_axisY_primary = new QValueAxis();
        chart->addAxis(m_axisY_primary, Qt::AlignLeft);
    }
    m_axisY_primary->setTitleText(curve.getYAxisLabel());
    return m_axisY_primary;
}

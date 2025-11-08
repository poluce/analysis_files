#include "chart_view.h"
#include "floating_label.h"
#include "domain/model/thermal_curve.h"
#include "application/curve/curve_manager.h"
#include <QDebug>
#include <QEvent>
#include <QGraphicsLineItem>
#include <QCursor>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
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

    // ==================== 创建选中点高亮系列 ====================
    // 用于在算法交互模式下显示用户选择的点（红色散点）
    m_selectedPointsSeries = new QScatterSeries();
    m_selectedPointsSeries->setName(tr("选中的点"));
    m_selectedPointsSeries->setMarkerSize(12.0);  // 点的大小
    m_selectedPointsSeries->setColor(Qt::red);     // 红色高亮
    m_selectedPointsSeries->setBorderColor(Qt::darkRed);  // 深红色边框
    m_selectedPointsSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    // 初始时不添加到 chart，只在需要时添加
    // chart->addSeries(m_selectedPointsSeries);  // 将在 startAlgorithmInteraction 时添加

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

void ChartView::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;
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

void ChartView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    // 添加横轴切换菜单项
    QString currentMode = (m_xAxisMode == XAxisMode::Temperature) ? tr("温度") : tr("时间");
    QString targetMode = (m_xAxisMode == XAxisMode::Temperature) ? tr("时间") : tr("温度");
    QAction* toggleAction = menu.addAction(tr("切换到以%1为横轴").arg(targetMode));
    connect(toggleAction, &QAction::triggered, this, &ChartView::toggleXAxisMode);

    // 显示菜单
    menu.exec(event->globalPos());
}

void ChartView::toggleXAxisMode()
{
    // 切换模式
    if (m_xAxisMode == XAxisMode::Temperature) {
        m_xAxisMode = XAxisMode::Time;
        m_axisX->setTitleText(tr("时间 (s)"));
        qDebug() << "ChartView::toggleXAxisMode - 切换到时间横轴";
    } else {
        m_xAxisMode = XAxisMode::Temperature;
        m_axisX->setTitleText(tr("温度 (°C)"));
        qDebug() << "ChartView::toggleXAxisMode - 切换到温度横轴";
    }

    // 重新加载所有曲线数据
    if (!m_curveManager) {
        qWarning() << "ChartView::toggleXAxisMode - CurveManager 未设置";
        return;
    }

    const auto& allCurves = m_curveManager->getAllCurves();
    for (const ThermalCurve& curve : allCurves) {
        QLineSeries* series = seriesForCurve(curve.id());
        if (series) {
            // 重新填充数据（populateSeriesWithCurveData 会根据 m_xAxisMode 选择数据）
            populateSeriesWithCurveData(series, curve);
        }
    }

    // 重新缩放坐标轴以适应新数据范围
    rescaleAxes();

    // 更新选中点的显示位置（如果有选中的点）
    updateSelectedPointsDisplay();

    qDebug() << "ChartView::toggleXAxisMode - 已完成横轴切换和曲线重绘";
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

    // 检查是否有活动算法等待交互
    if (!m_activeAlgorithm.isValid() || m_interactionState != InteractionState::WaitingForPoints) {
        qWarning() << "ChartView::handlePointSelectionClick - 没有活动算法等待选点，忽略点击";
        return;
    }

    // 转换鼠标坐标到图表坐标
    QPointF rawValuePoint = convertToValueCoordinates(chartViewPos);

    // ==================== 从目标曲线数据中查找最接近的点 ====================
    ThermalDataPoint selectedDataPoint;  // 保存完整的数据点
    QPointF displayPoint = rawValuePoint;  // 用于显示的坐标（根据横轴模式变化）

    if (m_curveManager && !m_activeAlgorithm.targetCurveId.isEmpty()) {
        ThermalCurve* targetCurve = m_curveManager->getCurve(m_activeAlgorithm.targetCurveId);
        if (targetCurve) {
            const auto& curveData = targetCurve->getProcessedData();
            if (!curveData.isEmpty()) {
                // 根据当前横轴模式查找最接近的点
                double targetXValue = rawValuePoint.x();
                int closestIndex = 0;
                double minDist;

                if (m_xAxisMode == XAxisMode::Temperature) {
                    // 温度模式：根据温度查找
                    minDist = qAbs(curveData[0].temperature - targetXValue);
                    for (int i = 1; i < curveData.size(); ++i) {
                        double dist = qAbs(curveData[i].temperature - targetXValue);
                        if (dist < minDist) {
                            minDist = dist;
                            closestIndex = i;
                        }
                    }
                } else {
                    // 时间模式：根据时间查找
                    minDist = qAbs(curveData[0].time - targetXValue);
                    for (int i = 1; i < curveData.size(); ++i) {
                        double dist = qAbs(curveData[i].time - targetXValue);
                        if (dist < minDist) {
                            minDist = dist;
                            closestIndex = i;
                        }
                    }
                }

                // 保存完整的数据点（包含温度、时间、值）
                selectedDataPoint = curveData[closestIndex];

                // 根据当前横轴模式设置显示坐标
                if (m_xAxisMode == XAxisMode::Temperature) {
                    displayPoint.setX(selectedDataPoint.temperature);
                } else {
                    displayPoint.setX(selectedDataPoint.time);
                }
                displayPoint.setY(selectedDataPoint.value);

                // 记录选中点所属的曲线ID（第一次选点时记录）
                if (m_selectedPoints.isEmpty()) {
                    m_selectedPointsCurveId = m_activeAlgorithm.targetCurveId;
                }

                qDebug() << "ChartView: 从目标曲线" << targetCurve->name()
                         << "找到最接近点 - 原始选点:" << rawValuePoint
                         << "-> 实际数据点(T=" << selectedDataPoint.temperature
                         << ", t=" << selectedDataPoint.time
                         << ", v=" << selectedDataPoint.value << ")";
            }
        } else {
            qWarning() << "ChartView: 无法获取目标曲线" << m_activeAlgorithm.targetCurveId;
            return;  // 无法获取曲线，直接返回
        }
    } else {
        qWarning() << "ChartView: 没有目标曲线或 CurveManager 未设置";
        return;
    }

    // 添加完整数据点到选点列表
    m_selectedPoints.append(selectedDataPoint);

    // 在图表上显示选中的点（红色高亮）
    if (m_selectedPointsSeries) {
        m_selectedPointsSeries->append(displayPoint);
        qDebug() << "ChartView: 添加红色高亮点到图表:" << displayPoint;
    } else {
        qWarning() << "ChartView: 选点系列为空，无法显示高亮点";
    }

    qDebug() << "ChartView: 算法" << m_activeAlgorithm.displayName
             << "选点进度:" << m_selectedPoints.size() << "/" << m_activeAlgorithm.requiredPointCount
             << ", 数据点(T=" << selectedDataPoint.temperature << ", t=" << selectedDataPoint.time << ", v=" << selectedDataPoint.value << ")";

    // 检查是否已收集足够的点
    if (m_selectedPoints.size() >= m_activeAlgorithm.requiredPointCount) {
        completePointSelection();
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


void ChartView::setInteractionMode(InteractionMode type)
{
    if (m_mode == type) {
        return;
    }

    m_mode = type;

    if (m_mode == InteractionMode::Pick) {
        qDebug() << "ChartView: 进入选点模式（Pick）";
        if (m_chartView) {
            m_chartView->setRubberBand(QChartView::NoRubberBand);
        }
        setCursor(Qt::CrossCursor);
    } else {
        qDebug() << "ChartView: 进入视图模式（View）";
        if (m_chartView) {
            // 禁用 RubberBand，避免单击时触发框选缩放
            // 用户可以通过菜单或快捷键手动触发缩放功能
            m_chartView->setRubberBand(QChartView::NoRubberBand);
        }
        unsetCursor();
    }
}

// ==================== 活动算法状态机实现 ====================

void ChartView::startAlgorithmInteraction(const QString& algorithmName, const QString& displayName,
                                          int requiredPoints, const QString& hint, const QString& curveId)
{
    qDebug() << "ChartView::startAlgorithmInteraction - 启动算法交互";
    qDebug() << "  算法:" << displayName << ", 需要点数:" << requiredPoints << ", 目标曲线:" << curveId;

    // 清空之前的状态
    clearInteractionState();

    // 查找目标曲线的Y轴
    QValueAxis* targetYAxis = findYAxisForCurve(curveId);

    // 配置选中点高亮系列
    setupSelectedPointsSeries(targetYAxis);

    // 设置活动算法信息
    m_activeAlgorithm.name = algorithmName;
    m_activeAlgorithm.displayName = displayName;
    m_activeAlgorithm.requiredPointCount = requiredPoints;
    m_activeAlgorithm.hint = hint;
    m_activeAlgorithm.targetCurveId = curveId;  // 保存目标曲线ID

    // 状态转换: Idle → WaitingForPoints
    transitionToState(InteractionState::WaitingForPoints);

    // 切换到选点模式
    setInteractionMode(InteractionMode::Pick);

    qDebug() << "ChartView: 算法" << displayName << "已进入等待用户选点状态";
}

void ChartView::cancelAlgorithmInteraction()
{
    if (!m_activeAlgorithm.isValid()) {
        qDebug() << "ChartView::cancelAlgorithmInteraction - 没有活动算法，无需取消";
        return;
    }

    qDebug() << "ChartView::cancelAlgorithmInteraction - 取消算法交互:" << m_activeAlgorithm.displayName;

    // 清空活动算法信息和交互状态
    m_activeAlgorithm.clear();
    clearInteractionState();

    // 状态转换: 任意状态 → Idle
    transitionToState(InteractionState::Idle);

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

void ChartView::highlightCurve(const QString& curveId)
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

    // 如果删除的曲线是选中点所属的曲线，清除选中的点
    if (!m_selectedPointsCurveId.isEmpty() && m_selectedPointsCurveId == curveId) {
        qDebug() << "ChartView::removeCurve - 删除的曲线是选中点所属的曲线，清除选中点:" << curveId;
        clearInteractionState();
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
    clearFloatingLabels();  // 清空所有浮动标签
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

// ==================== 算法交互辅助函数实现 ====================

void ChartView::clearInteractionState()
{
    m_selectedPoints.clear();
    m_selectedPointsCurveId.clear();  // 清除选中点所属的曲线ID
    if (m_selectedPointsSeries) {
        m_selectedPointsSeries->clear();
    }
}

QValueAxis* ChartView::findYAxisForCurve(const QString& curveId)
{
    if (curveId.isEmpty()) {
        return m_axisY_primary;
    }

    QLineSeries* curveSeries = seriesForCurve(curveId);
    if (!curveSeries) {
        qWarning() << "ChartView::findYAxisForCurve - 未找到曲线" << curveId << "，使用默认主Y轴";
        return m_axisY_primary;
    }

    // 获取该曲线附着的所有轴
    const auto attachedAxes = curveSeries->attachedAxes();
    for (QAbstractAxis* axis : attachedAxes) {
        QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
        if (valueAxis && valueAxis != m_axisX) {
            qDebug() << "ChartView::findYAxisForCurve - 找到曲线" << curveId << "的Y轴";
            return valueAxis;
        }
    }

    return m_axisY_primary;
}

void ChartView::setupSelectedPointsSeries(QValueAxis* targetYAxis)
{
    if (!m_selectedPointsSeries || !m_chartView || !m_chartView->chart()) {
        return;
    }

    QChart* chart = m_chartView->chart();
    m_selectedPointsSeries->clear();

    // 如果还未添加到 chart，则添加
    if (!chart->series().contains(m_selectedPointsSeries)) {
        chart->addSeries(m_selectedPointsSeries);
        m_selectedPointsSeries->attachAxis(m_axisX);
        m_selectedPointsSeries->attachAxis(targetYAxis);
        qDebug() << "ChartView::setupSelectedPointsSeries - 添加选中点系列，Y轴:"
                 << (targetYAxis == m_axisY_primary ? "主轴(左)" : "次轴(右)");
    } else {
        // 如果已存在，则更新其轴关联
        const auto existingAxes = m_selectedPointsSeries->attachedAxes();
        for (QAbstractAxis* axis : existingAxes) {
            m_selectedPointsSeries->detachAxis(axis);
        }
        m_selectedPointsSeries->attachAxis(m_axisX);
        m_selectedPointsSeries->attachAxis(targetYAxis);
        qDebug() << "ChartView::setupSelectedPointsSeries - 更新选中点系列的轴，Y轴:"
                 << (targetYAxis == m_axisY_primary ? "主轴(左)" : "次轴(右)");
    }
}

void ChartView::transitionToState(InteractionState newState)
{
    if (m_interactionState == newState) {
        return;
    }

    m_interactionState = newState;
    emit interactionStateChanged(static_cast<int>(m_interactionState));
}

QPointF ChartView::convertToValueCoordinates(const QPointF& chartViewPos)
{
    if (!m_chartView || !m_chartView->chart()) {
        return QPointF();
    }

    // 使用 m_selectedPointsSeries 作为参考系列，确保使用与选点系列相同的Y轴
    if (m_selectedPointsSeries && !m_selectedPointsSeries->attachedAxes().isEmpty()) {
        QPointF valuePoint = m_chartView->chart()->mapToValue(chartViewPos, m_selectedPointsSeries);
        qDebug() << "ChartView::convertToValueCoordinates - 使用选点系列的Y轴，点:" << valuePoint;
        return valuePoint;
    }

    // 回退到默认转换
    qWarning() << "ChartView::convertToValueCoordinates - 选点系列未正确附着到轴，使用默认转换";
    return m_chartView->chart()->mapToValue(chartViewPos);
}

void ChartView::completePointSelection()
{
    transitionToState(InteractionState::PointsCompleted);

    qDebug() << "ChartView::completePointSelection - 算法" << m_activeAlgorithm.displayName
             << "交互完成，发送信号触发执行";

    // 发出算法交互完成信号，触发算法执行
    QString completedAlgorithmName = m_activeAlgorithm.name;
    QVector<ThermalDataPoint> completedPoints = m_selectedPoints;
    emit algorithmInteractionCompleted(completedAlgorithmName, completedPoints);

    // 清空活动算法信息（算法执行完成后不需要保留交互状态）
    m_activeAlgorithm.clear();

    // 状态转换: PointsCompleted → Idle
    transitionToState(InteractionState::Idle);

    // 切换回视图模式
    setInteractionMode(InteractionMode::View);

    qDebug() << "ChartView::completePointSelection - 算法交互状态已清理，回到空闲状态";
}

void ChartView::updateSelectedPointsDisplay()
{
    // 如果没有选中的点或没有选中点系列，直接返回
    if (m_selectedPoints.isEmpty() || !m_selectedPointsSeries) {
        return;
    }

    qDebug() << "ChartView::updateSelectedPointsDisplay - 更新" << m_selectedPoints.size() << "个选中点的显示位置";

    // 清空旧的显示点
    m_selectedPointsSeries->clear();

    // 根据当前横轴模式重新计算显示坐标
    for (const ThermalDataPoint& dataPoint : m_selectedPoints) {
        QPointF displayPoint;

        // 根据横轴模式选择 X 坐标
        if (m_xAxisMode == XAxisMode::Temperature) {
            displayPoint.setX(dataPoint.temperature);
        } else {
            displayPoint.setX(dataPoint.time);
        }

        // Y 坐标始终是值
        displayPoint.setY(dataPoint.value);

        // 添加到显示系列
        m_selectedPointsSeries->append(displayPoint);
    }

    qDebug() << "ChartView::updateSelectedPointsDisplay - 已更新选中点显示";
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

    // 根据横轴模式选择 X 轴数据
    if (m_xAxisMode == XAxisMode::Temperature) {
        // 温度为横轴
        for (const auto& point : data) {
            series->append(point.temperature, point.value);
        }
    } else {
        // 时间为横轴
        for (const auto& point : data) {
            series->append(point.time, point.value);
        }
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

    // ==================== 优先级1：SignalType 强制规则（最高优先级）====================
    // 微分曲线必须使用次 Y 轴，无论是否是辅助曲线或有无父曲线
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
        qDebug() << "ChartView: 曲线" << curve.name() << "使用次 Y 轴（Derivative 强制规则）";
        return m_axisY_secondary;
    }

    // ==================== 优先级2：辅助曲线继承父曲线的 Y 轴 ====================
    // 只有辅助曲线（如基线、滤波）才继承父曲线的 Y 轴
    // 独立曲线（如积分）即使有父曲线也应该创建新的 Y 轴
    if (curve.isAuxiliaryCurve() && !curve.parentId().isEmpty()) {
        QLineSeries* parentSeries = seriesForCurve(curve.parentId());
        if (parentSeries && !parentSeries->attachedAxes().isEmpty()) {
            // 查找父曲线的 Y 轴（非 X 轴）
            for (QAbstractAxis* axis : parentSeries->attachedAxes()) {
                QValueAxis* valueAxis = qobject_cast<QValueAxis*>(axis);
                if (valueAxis && valueAxis != m_axisX) {
                    qDebug() << "ChartView: 辅助曲线" << curve.name() << "继承父曲线的 Y 轴";
                    valueAxis->setTitleText(curve.getYAxisLabel());
                    return valueAxis;
                }
            }
        }
    }

    // ==================== 优先级3：默认分配主 Y 轴 ====================
    // 独立曲线（如积分）或无父曲线的曲线使用主 Y 轴
    if (!m_axisY_primary) {
        m_axisY_primary = new QValueAxis();
        chart->addAxis(m_axisY_primary, Qt::AlignLeft);
    }
    m_axisY_primary->setTitleText(curve.getYAxisLabel());
    qDebug() << "ChartView: 曲线" << curve.name() << "使用主 Y 轴（默认）";
    return m_axisY_primary;
}

// ==================== 浮动标签管理接口实现 ====================

FloatingLabel* ChartView::addFloatingLabel(const QString& text, const QPointF& dataPos, const QString& curveId)
{
    if (!m_chartView || !m_chartView->chart()) {
        qWarning() << "ChartView::addFloatingLabel - 图表未初始化";
        return nullptr;
    }

    QChart* chart = m_chartView->chart();

    // 查找对应的系列
    QLineSeries* series = seriesForCurve(curveId);
    if (!series) {
        qWarning() << "ChartView::addFloatingLabel - 未找到曲线" << curveId;
        return nullptr;
    }

    // 创建浮动标签（数据锚定模式）
    auto* label = new FloatingLabel(chart);
    label->setMode(FloatingLabel::Mode::DataAnchored);
    label->setText(text);
    label->setAnchorValue(dataPos, series);

    // 添加到场景
    chart->scene()->addItem(label);

    // 保存到列表
    m_floatingLabels.append(label);

    // 连接关闭信号
    connect(label, &FloatingLabel::closeRequested, this, [this, label]() {
        removeFloatingLabel(label);
    });

    // 连接 plotAreaChanged 信号，确保标签跟随坐标轴变化
    connect(chart, &QChart::plotAreaChanged, label, &FloatingLabel::updateGeometry);

    qDebug() << "ChartView::addFloatingLabel - 添加浮动标签（数据锚定）：" << text
             << "，位置：" << dataPos << "，曲线：" << curveId;

    return label;
}

FloatingLabel* ChartView::addFloatingLabelHUD(const QString& text, const QPointF& viewPos)
{
    if (!m_chartView || !m_chartView->chart()) {
        qWarning() << "ChartView::addFloatingLabelHUD - 图表未初始化";
        return nullptr;
    }

    QChart* chart = m_chartView->chart();

    // 创建浮动标签（视图锚定模式）
    auto* label = new FloatingLabel(chart);
    label->setMode(FloatingLabel::Mode::ViewAnchored);
    label->setText(text);

    // 计算绝对位置（相对于 plotArea）
    QRectF plotArea = chart->plotArea();
    QPointF absolutePos = plotArea.topLeft() + viewPos;
    label->setPos(absolutePos);

    // 添加到场景
    chart->scene()->addItem(label);

    // 保存到列表
    m_floatingLabels.append(label);

    // 连接关闭信号
    connect(label, &FloatingLabel::closeRequested, this, [this, label]() {
        removeFloatingLabel(label);
    });

    qDebug() << "ChartView::addFloatingLabelHUD - 添加浮动标签（视图锚定）：" << text
             << "，位置：" << viewPos;

    return label;
}

void ChartView::removeFloatingLabel(FloatingLabel* label)
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
    if (m_chartView && m_chartView->chart() && m_chartView->chart()->scene()) {
        m_chartView->chart()->scene()->removeItem(label);
    }

    label->deleteLater();

    qDebug() << "ChartView::removeFloatingLabel - 移除浮动标签";
}

void ChartView::clearFloatingLabels()
{
    // 移除所有浮动标签
    for (FloatingLabel* label : m_floatingLabels) {
        if (label) {
            if (m_chartView && m_chartView->chart() && m_chartView->chart()->scene()) {
                m_chartView->chart()->scene()->removeItem(label);
            }
            label->deleteLater();
        }
    }

    m_floatingLabels.clear();

    qDebug() << "ChartView::clearFloatingLabels - 清空所有浮动标签";
}

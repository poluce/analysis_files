#include "thermal_chart_view.h"
#include "thermal_chart.h"
#include "peak_area_tool.h"
#include "application/curve/curve_manager.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtMath>

ThermalChartView::ThermalChartView(ThermalChart* chart, QWidget* parent)
    : QChartView(chart, parent)
    , m_thermalChart(chart)
{
    qDebug() << "构造: ThermalChartView";

    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    // 安装事件过滤器（用于右键拖动等）
    viewport()->installEventFilter(this);
}

ThermalChartView::~ThermalChartView()
{
    qDebug() << "析构: ThermalChartView";
}

void ThermalChartView::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;

    // 关键修复：将 CurveManager 传递给 ThermalChart
    // ThermalChart 需要 CurveManager 来执行以下功能：
    // 1. 横轴切换时重新加载所有曲线数据 (setXAxisMode)
    // 2. 级联删除子曲线 (removeCurve)
    // 3. 测量工具访问曲线数据 (addMassLossTool)
    if (m_thermalChart) {
        m_thermalChart->setCurveManager(manager);
    }
}

// ==================== 交互模式 ====================

void ThermalChartView::setInteractionMode(InteractionMode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;

    if (m_mode == InteractionMode::Pick) {
        qDebug() << "ThermalChartView: 进入选点模式（Pick）";
        setRubberBand(QChartView::NoRubberBand);
        setCursor(Qt::CrossCursor);
    } else {
        qDebug() << "ThermalChartView: 进入视图模式（View）";
        setRubberBand(QChartView::NoRubberBand);
        unsetCursor();
    }
}

// ==================== 碰撞检测配置 ====================

void ThermalChartView::setHitTestBasePixelThreshold(qreal px)
{
    m_hitTestBasePx = (px <= 0) ? 1.0 : px;
}

void ThermalChartView::setHitTestIncludePenWidth(bool enabled)
{
    m_hitTestIncludePen = enabled;
}

// ==================== 测量工具 ====================

void ThermalChartView::startMassLossTool()
{
    qDebug() << "ThermalChartView::startMassLossTool - 启动质量损失测量工具";
    setInteractionMode(InteractionMode::Pick);
    m_massLossToolActive = true;
}

void ThermalChartView::startPeakAreaTool(const QString& curveId, bool useLinearBaseline, const QString& referenceCurveId)
{
    qDebug() << "ThermalChartView::startPeakAreaTool - 启动峰面积测量工具";
    qDebug() << "  计算曲线:" << curveId;
    qDebug() << "  基线类型:" << (useLinearBaseline ? "直线基线" : "参考曲线基线");
    if (!referenceCurveId.isEmpty()) {
        qDebug() << "  参考曲线:" << referenceCurveId;
    }

    // 存储用户选择的参数
    m_peakAreaCurveId = curveId;
    m_peakAreaUseLinearBaseline = useLinearBaseline;
    m_peakAreaReferenceCurveId = referenceCurveId;

    setInteractionMode(InteractionMode::Pick);
    m_peakAreaToolActive = true;
}

// ==================== Phase 4: 事件处理实现 ====================

void ThermalChartView::mousePressEvent(QMouseEvent* event)
{
    // 右键拖动已在 eventFilter 中处理
    if (event->button() != Qt::LeftButton) {
        QChartView::mousePressEvent(event);
        return;
    }

    const QPointF viewportPos = event->pos();

    // 测量工具逻辑已移至 eventFilter
    // 只处理普通的点选和曲线选择
    if (m_mode == InteractionMode::Pick) {
        handleValueClick(viewportPos);
    } else {
        handleCurveSelectionClick(viewportPos);
    }

    QChartView::mousePressEvent(event);
}

void ThermalChartView::mouseMoveEvent(QMouseEvent* event)
{
    // 更新十字线位置（坐标转换：viewport → scene → chart）
    if (m_thermalChart) {
        QPointF scenePos = mapToScene(event->pos());
        QPointF chartPos = chart()->mapFromScene(scenePos);
        m_thermalChart->updateCrosshairAtChartPos(chartPos);
    }

    // 发出悬停信号（用于未来的悬停提示）
    if (m_thermalChart) {
        QPointF valuePos = chartToValue(sceneToChart(viewportToScene(event->pos())));
        emit hoverMoved(event->pos(), valuePos);
    }

    QChartView::mouseMoveEvent(event);
}

void ThermalChartView::mouseReleaseEvent(QMouseEvent* event)
{
    // 右键拖动和测量工具逻辑已移至 eventFilter
    // 这个方法保留用于未来可能的扩展
    QChartView::mouseReleaseEvent(event);
}

void ThermalChartView::wheelEvent(QWheelEvent* event)
{
    // Ctrl+滚轮：缩放图表
    if (event->modifiers() & Qt::ControlModifier) {
        if (!m_thermalChart) {
            event->ignore();
            return;
        }

        // 获取鼠标在图表中的位置（数据坐标）
        QPointF chartPos = mapToScene(event->pos());
        QPointF valuePos = chart()->mapToValue(chartPos);

        // 缩放因子
        qreal factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;

        // 缩放 X 轴
        QValueAxis* axisX = m_thermalChart->axisX();
        if (axisX) {
            qreal xMin = axisX->min();
            qreal xMax = axisX->max();
            qreal xRange = xMax - xMin;
            qreal xCenter = valuePos.x();

            // 以鼠标位置为中心缩放
            qreal leftRatio = (xCenter - xMin) / xRange;
            qreal rightRatio = (xMax - xCenter) / xRange;

            qreal newRange = xRange * factor;
            qreal newMin = xCenter - newRange * leftRatio;
            qreal newMax = xCenter + newRange * rightRatio;

            axisX->setRange(newMin, newMax);
        }

        // 缩放所有 Y 轴（每个轴以自身中心点缩放）
        for (QAbstractAxis* axis : chart()->axes(Qt::Vertical)) {
            QValueAxis* yAxis = qobject_cast<QValueAxis*>(axis);
            if (yAxis) {
                qreal yMin = yAxis->min();
                qreal yMax = yAxis->max();
                qreal yCenter = (yMin + yMax) / 2.0;
                qreal yRange = yMax - yMin;

                qreal newRange = yRange * factor;
                qreal newMin = yCenter - newRange / 2.0;
                qreal newMax = yCenter + newRange / 2.0;

                yAxis->setRange(newMin, newMax);
            }
        }

        event->accept();
    } else {
        QChartView::wheelEvent(event);
    }
}

void ThermalChartView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    if (m_thermalChart) {
        QString currentMode = (m_thermalChart->xAxisMode() == XAxisMode::Temperature) ? tr("温度") : tr("时间");
        QString targetMode = (m_thermalChart->xAxisMode() == XAxisMode::Temperature) ? tr("时间") : tr("温度");
        QAction* toggleAction = menu.addAction(tr("切换到以%1为横轴").arg(targetMode));
        connect(toggleAction, &QAction::triggered, this, [this]() {
            if (m_thermalChart) {
                XAxisMode currentMode = m_thermalChart->xAxisMode();
                XAxisMode newMode = (currentMode == XAxisMode::Temperature) ? XAxisMode::Time : XAxisMode::Temperature;
                m_thermalChart->setXAxisMode(newMode);
            }
        });
    }

    menu.exec(event->globalPos());
}

// ==================== EventFilter 事件处理辅助函数实现 ====================

bool ThermalChartView::handleRightButtonPress(QMouseEvent* mouseEvent)
{
    if (mouseEvent->button() != Qt::RightButton) {
        return false;
    }

    m_isRightDragging = true;
    m_rightDragStartPos = mouseEvent->pos();
    setCursor(Qt::ClosedHandCursor);
    return false;  // 让事件继续传递
}

bool ThermalChartView::handleRightButtonRelease(QMouseEvent* mouseEvent)
{
    if (mouseEvent->button() != Qt::RightButton || !m_isRightDragging) {
        return false;
    }

    m_isRightDragging = false;
    unsetCursor();
    return false;  // 让事件继续传递
}

bool ThermalChartView::handleRightButtonDrag(QMouseEvent* mouseEvent)
{
    if (!m_isRightDragging || !m_thermalChart) {
        return false;
    }

    handleRightDrag(mouseEvent->pos());
    return false;  // 让事件继续传递
}

void ThermalChartView::handleMouseLeave()
{
    if (m_thermalChart) {
        m_thermalChart->clearCrosshair();
    }
}

bool ThermalChartView::handleMassLossToolClick(const QPointF& viewportPos)
{
    qDebug() << "ThermalChartView::handleMassLossToolClick - 单次点击创建质量损失测量工具，点击位置:" << viewportPos;

    // 使用当前活动曲线进行测量
    if (!m_curveManager) {
        qWarning() << "ThermalChartView::handleMassLossToolClick - CurveManager 未设置";
        return false;
    }

    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve || !m_thermalChart) {
        qWarning() << "ThermalChartView::handleMassLossToolClick - 没有活动曲线或 ThermalChart 未设置";
        return false;
    }

    qDebug() << "ThermalChartView::handleMassLossToolClick - 使用活动曲线:" << activeCurve->name();

    // 获取活动曲线的系列（用于正确的坐标转换）
    QLineSeries* activeSeries = m_thermalChart->seriesForCurve(activeCurve->id());
    if (!activeSeries) {
        qWarning() << "ThermalChartView::handleMassLossToolClick - 无法获取活动曲线的系列";
        return false;
    }

    // 使用活动曲线的坐标系进行转换
    QPointF scenePos = mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart()->mapFromScene(scenePos);
    QPointF dataClick = chart()->mapToValue(chartPos, activeSeries);

    qDebug() << "ThermalChartView::handleMassLossToolClick - 点击位置数据坐标:" << dataClick;

    const auto& data = activeCurve->getProcessedData();
    if (data.isEmpty()) {
        qWarning() << "ThermalChartView::handleMassLossToolClick - 活动曲线数据为空";
        return false;
    }

    // 自动在点击位置左右延伸范围（默认左右各延伸15°C）
    qreal rangeExtension = 15.0;
    qreal startX = dataClick.x() - rangeExtension;
    qreal endX = dataClick.x() + rangeExtension;

    // 查找最接近的点
    ThermalDataPoint point1 = findNearestDataPoint(data, startX);
    ThermalDataPoint point2 = findNearestDataPoint(data, endX);

    qDebug() << "ThermalChartView::handleMassLossToolClick - 自动延伸范围: ±" << rangeExtension;

    // 创建测量工具（委托给 ThermalChart）
    m_thermalChart->addMassLossTool(point1, point2, activeCurve->id());

    // 重置状态
    m_massLossToolActive = false;
    setInteractionMode(InteractionMode::View);

    return false;
}

bool ThermalChartView::handlePeakAreaToolClick(const QPointF& viewportPos)
{
    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 单次点击创建峰面积测量工具，点击位置:" << viewportPos;

    // 使用用户选择的计算曲线（不再依赖活动曲线）
    if (!m_curveManager || m_peakAreaCurveId.isEmpty()) {
        qWarning() << "ThermalChartView::handlePeakAreaToolClick - CurveManager 未设置或未指定目标曲线";
        return false;
    }

    ThermalCurve* targetCurve = m_curveManager->getCurve(m_peakAreaCurveId);
    if (!targetCurve || !m_thermalChart) {
        qWarning() << "ThermalChartView::handlePeakAreaToolClick - 无法获取目标曲线或 ThermalChart 未设置";
        return false;
    }

    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 使用用户选择的曲线:" << targetCurve->name();

    // 获取目标曲线的系列（用于正确的坐标转换）
    QLineSeries* targetSeries = m_thermalChart->seriesForCurve(targetCurve->id());
    if (!targetSeries) {
        qWarning() << "ThermalChartView::handlePeakAreaToolClick - 无法获取目标曲线的系列";
        return false;
    }

    // 检查点击位置是否在绘图区域内
    QPointF scenePos = mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart()->mapFromScene(scenePos);
    if (!chart()->plotArea().contains(chartPos)) {
        qDebug() << "ThermalChartView::handlePeakAreaToolClick - 点击位置不在绘图区内，忽略";
        // 重置状态
        m_peakAreaToolActive = false;
        m_peakAreaCurveId.clear();
        m_peakAreaReferenceCurveId.clear();
        setInteractionMode(InteractionMode::View);
        return false;
    }

    // 使用目标曲线的坐标系进行转换
    QPointF dataClick = chart()->mapToValue(chartPos, targetSeries);

    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 点击位置数据坐标:" << dataClick;

    const auto& data = targetCurve->getProcessedData();
    if (data.isEmpty()) {
        qWarning() << "ThermalChartView::handlePeakAreaToolClick - 目标曲线数据为空";
        return false;
    }

    // 自动在点击位置左右延伸范围（根据当前可见轴范围的 5% 动态计算）
    qreal rangeExtension = 20.0; // 默认值
    QAbstractAxis* axisX = m_thermalChart->axisX();
    if (axisX) {
        QValueAxis* valueAxisX = qobject_cast<QValueAxis*>(axisX);
        if (valueAxisX) {
            qreal axisRange = valueAxisX->max() - valueAxisX->min();
            rangeExtension = axisRange * 0.05; // 5% 的轴范围
        }
    }

    qreal startX = dataClick.x() - rangeExtension;
    qreal endX = dataClick.x() + rangeExtension;

    // 查找最接近的点
    ThermalDataPoint point1 = findNearestDataPoint(data, startX);
    ThermalDataPoint point2 = findNearestDataPoint(data, endX);

    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 自动延伸范围: ±" << rangeExtension;

    // 创建峰面积测量工具（委托给 ThermalChart）
    PeakAreaTool* tool = m_thermalChart->addPeakAreaTool(point1, point2, targetCurve->id());

    // 应用基线模式（直线基线 或 参考曲线基线）
    if (tool) {
        if (m_peakAreaUseLinearBaseline) {
            // 直线基线（默认）
            tool->setBaselineMode(PeakAreaTool::BaselineMode::Linear);
            qDebug() << "ThermalChartView::handlePeakAreaToolClick - 应用直线基线模式";
        } else if (!m_peakAreaReferenceCurveId.isEmpty()) {
            // 参考曲线基线
            tool->setBaselineMode(PeakAreaTool::BaselineMode::ReferenceCurve);
            tool->setReferenceCurve(m_peakAreaReferenceCurveId);
            qDebug() << "ThermalChartView::handlePeakAreaToolClick - 应用参考曲线基线:" << m_peakAreaReferenceCurveId;
        }
    }

    // 重置状态
    m_peakAreaToolActive = false;
    m_peakAreaCurveId.clear();
    m_peakAreaReferenceCurveId.clear();
    setInteractionMode(InteractionMode::View);

    return false;
}

bool ThermalChartView::eventFilter(QObject* watched, QEvent* event)
{
    // 只处理 viewport 的事件
    if (watched != viewport()) {
        return QChartView::eventFilter(watched, event);
    }

    // 事件分发：将不同类型的事件委托给专门的处理函数
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        return handleRightButtonPress(mouseEvent);
    }

    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);

        // 处理右键释放
        if (handleRightButtonRelease(mouseEvent)) {
            return true;
        }

        // 处理质量损失测量工具单次点击创建
        if (mouseEvent->button() == Qt::LeftButton && m_massLossToolActive && m_mode == InteractionMode::Pick) {
            return handleMassLossToolClick(mouseEvent->pos());
        }

        // 处理峰面积测量工具单次点击创建
        if (mouseEvent->button() == Qt::LeftButton && m_peakAreaToolActive && m_mode == InteractionMode::Pick) {
            return handlePeakAreaToolClick(mouseEvent->pos());
        }
        break;
    }

    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        return handleRightButtonDrag(mouseEvent);
    }

    case QEvent::Leave:
    case QEvent::HoverLeave:
        handleMouseLeave();
        break;

    default:
        break;
    }

    return QChartView::eventFilter(watched, event);
}

// ==================== Phase 4: 交互辅助函数实现 ====================

void ThermalChartView::handleCurveSelectionClick(const QPointF& viewportPos)
{
    if (!m_thermalChart) {
        return;
    }

    qreal bestDist = std::numeric_limits<qreal>::max();
    QLineSeries* clickedSeries = findSeriesNearPoint(viewportPos, bestDist);

    const qreal deviceRatio = devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio;
    if (clickedSeries && m_hitTestIncludePen) {
        const qreal penWidth = clickedSeries->pen().widthF();
        baseThreshold += penWidth * 0.5;
    }

    if (clickedSeries && bestDist <= baseThreshold) {
        // 高亮选中的曲线
        QString curveId = m_thermalChart->curveIdForSeries(clickedSeries);
        m_thermalChart->highlightCurve(curveId);
        emit curveHit(curveId);
    } else {
        // 清除高亮
        m_thermalChart->highlightCurve("");
        emit curveHit("");
    }
}

void ThermalChartView::handleValueClick(const QPointF& viewportPos)
{
    if (!m_thermalChart) {
        return;
    }

    // 转换鼠标坐标到数据坐标
    QPointF scenePos = mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart()->mapFromScene(scenePos);

    // 查找最接近的系列
    qreal bestDist = std::numeric_limits<qreal>::max();
    QLineSeries* clickedSeries = findSeriesNearPoint(viewportPos, bestDist);

    if (clickedSeries) {
        QPointF valuePos = chart()->mapToValue(chartPos, clickedSeries);
        emit valueClicked(valuePos, clickedSeries);
    }
}

QLineSeries* ThermalChartView::findSeriesNearPoint(const QPointF& viewportPos, qreal& outDistance) const
{
    if (!chart()) {
        outDistance = std::numeric_limits<qreal>::max();
        return nullptr;
    }

    // 点到线段距离计算函数
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

    for (QAbstractSeries* abstractSeries : chart()->series()) {
        QLineSeries* lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) {
            continue;
        }

        const auto points = lineSeries->pointsVector();
        if (points.size() < 2) {
            continue;
        }

        QPointF previous = chart()->mapToPosition(points[0], lineSeries);
        for (int i = 1; i < points.size(); ++i) {
            const QPointF current = chart()->mapToPosition(points[i], lineSeries);
            const qreal dist = pointToSegDist(viewportPos, previous, current);
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

qreal ThermalChartView::hitThreshold() const
{
    const qreal deviceRatio = devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio;
    return baseThreshold;
}

void ThermalChartView::handleRightDrag(const QPointF& currentPos)
{
    if (!m_thermalChart || !chart()) {
        return;
    }

    // 计算像素偏移
    QPointF pixelDelta = m_rightDragStartPos - currentPos;

    // 移动 X 轴（使用数据坐标偏移）
    QValueAxis* axisX = m_thermalChart->axisX();
    if (axisX) {
        // 正确的坐标转换链路：viewport → scene → chart → value
        QPointF startScene = mapToScene(m_rightDragStartPos.toPoint());
        QPointF currentScene = mapToScene(currentPos.toPoint());
        QPointF startChart = chart()->mapFromScene(startScene);
        QPointF currentChart = chart()->mapFromScene(currentScene);
        QPointF startValue = chart()->mapToValue(startChart);
        QPointF currentValue = chart()->mapToValue(currentChart);
        qreal xDataDelta = startValue.x() - currentValue.x();

        qreal xMin = axisX->min();
        qreal xMax = axisX->max();
        axisX->setRange(xMin + xDataDelta, xMax + xDataDelta);
    }

    // 移动所有 Y 轴（每个轴根据自己的范围独立计算偏移）
    QRectF plotArea = chart()->plotArea();
    qreal plotHeight = plotArea.height();

    for (QAbstractAxis* axis : chart()->axes(Qt::Vertical)) {
        QValueAxis* yAxis = qobject_cast<QValueAxis*>(axis);
        if (yAxis && plotHeight > 0) {
            qreal yMin = yAxis->min();
            qreal yMax = yAxis->max();
            qreal yRange = yMax - yMin;

            // 像素偏移转换为该轴的数据偏移（像素比例 × 轴范围）
            // 注意：屏幕Y轴向下为正，需要取负以符合直觉（向上拖→图表向上移）
            qreal yDataDelta = -(pixelDelta.y() / plotHeight) * yRange;

            yAxis->setRange(yMin + yDataDelta, yMax + yDataDelta);
        }
    }

    // 更新起始位置
    m_rightDragStartPos = currentPos;
}

// ==================== 数据查询辅助函数 ====================

ThermalDataPoint ThermalChartView::findNearestDataPoint(const QVector<ThermalDataPoint>& curveData,
                                                          double xValue) const
{
    if (curveData.isEmpty()) {
        return ThermalDataPoint();
    }

    // 根据当前横轴模式选择比较字段
    bool useTimeAxis = (m_thermalChart && m_thermalChart->xAxisMode() == XAxisMode::Time);

    int nearestIdx = 0;
    double minDist = useTimeAxis
                     ? qAbs(curveData[0].time - xValue)
                     : qAbs(curveData[0].temperature - xValue);

    for (int i = 1; i < curveData.size(); ++i) {
        double dist = useTimeAxis
                      ? qAbs(curveData[i].time - xValue)
                      : qAbs(curveData[i].temperature - xValue);
        if (dist < minDist) {
            minDist = dist;
            nearestIdx = i;
        }
    }

    return curveData[nearestIdx];
}

// ==================== 坐标转换 ====================

QPointF ThermalChartView::viewportToScene(const QPointF& viewportPos) const
{
    return mapToScene(viewportPos.toPoint());
}

QPointF ThermalChartView::sceneToChart(const QPointF& scenePos) const
{
    if (!chart()) {
        return QPointF();
    }
    return chart()->mapFromScene(scenePos);
}

QPointF ThermalChartView::chartToValue(const QPointF& chartPos) const
{
    if (!chart()) {
        return QPointF();
    }
    return chart()->mapToValue(chartPos);
}

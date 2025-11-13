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
        // View 模式：启动框选缩放
        m_isBoxSelecting = true;
        m_boxSelectStart = viewportPos;
        m_boxSelectEnd = viewportPos;

        qDebug() << "ThermalChartView::mousePressEvent - 开始框选，起点:" << viewportPos;
    }

    QChartView::mousePressEvent(event);
}

void ThermalChartView::mouseMoveEvent(QMouseEvent* event)
{
    // 如果正在框选，更新选框矩形
    if (m_isBoxSelecting) {
        m_boxSelectEnd = event->pos();
        updateSelectionBoxDisplay();

        // 不调用父类方法，避免触发默认的缩放行为
        return;
    }

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
    // 处理框选缩放
    if (m_isBoxSelecting && event->button() == Qt::LeftButton) {
        m_boxSelectEnd = event->pos();
        finalizeBoxSelection();

        // 不调用父类方法，避免触发默认行为
        return;
    }

    // 右键拖动和测量工具逻辑已移至 eventFilter
    // 这个方法保留用于未来可能的扩展
    QChartView::mouseReleaseEvent(event);
}

// ==================== 缩放辅助函数实现 ====================

void ThermalChartView::zoomXAxisAtPoint(const QPointF& chartPos, qreal factor)
{
    QValueAxis* axisX = m_thermalChart->axisX();
    if (!axisX) {
        return;
    }

    // 将鼠标位置转换为 X 轴的数据坐标
    QPointF valuePos = chart()->mapToValue(chartPos);
    qreal xCenter = valuePos.x();

    qreal xMin = axisX->min();
    qreal xMax = axisX->max();
    qreal xRange = xMax - xMin;

    // 以鼠标位置为中心缩放
    qreal leftRatio = (xCenter - xMin) / xRange;
    qreal rightRatio = (xMax - xCenter) / xRange;

    qreal newRange = xRange * factor;
    qreal newMin = xCenter - newRange * leftRatio;
    qreal newMax = xCenter + newRange * rightRatio;

    axisX->setRange(newMin, newMax);
}

void ThermalChartView::zoomYAxisAtPoint(QValueAxis* yAxis, const QPointF& chartPos, qreal factor)
{
    if (!yAxis) {
        return;
    }

    // 获取鼠标在当前 Y 轴坐标系中的位置
    QAbstractSeries* firstSeries = findFirstSeriesForYAxis(yAxis);

    qreal yCenter;
    if (firstSeries) {
        // 使用系列进行精确的坐标转换
        QPointF valuePos = chart()->mapToValue(chartPos, firstSeries);
        yCenter = valuePos.y();
    } else {
        // 回退：使用轴的中心点
        qreal yMin = yAxis->min();
        qreal yMax = yAxis->max();
        yCenter = (yMin + yMax) / 2.0;
    }

    qreal yMin = yAxis->min();
    qreal yMax = yAxis->max();
    qreal yRange = yMax - yMin;

    // 以鼠标位置为中心缩放
    qreal bottomRatio = (yCenter - yMin) / yRange;
    qreal topRatio = (yMax - yCenter) / yRange;

    qreal newRange = yRange * factor;
    qreal newMin = yCenter - newRange * bottomRatio;
    qreal newMax = yCenter + newRange * topRatio;

    yAxis->setRange(newMin, newMax);
}

void ThermalChartView::zoomAllYAxesAtPoint(const QPointF& chartPos, qreal factor)
{
    for (QAbstractAxis* axis : chart()->axes(Qt::Vertical)) {
        QValueAxis* yAxis = qobject_cast<QValueAxis*>(axis);
        if (yAxis) {
            zoomYAxisAtPoint(yAxis, chartPos, factor);
        }
    }
}

QAbstractSeries* ThermalChartView::findFirstSeriesForYAxis(QValueAxis* yAxis) const
{
    if (!yAxis) {
        return nullptr;
    }

    for (QAbstractSeries* series : chart()->series()) {
        if (series->attachedAxes().contains(yAxis)) {
            return series;
        }
    }

    return nullptr;
}

// ==================== 框选缩放辅助函数实现 ====================

QRectF ThermalChartView::convertViewportRectToChartRect(const QPointF& viewportStart, const QPointF& viewportEnd) const
{
    // 转换为图表坐标系（mapToScene 需要 QPoint，所以转换 QPointF → QPoint）
    QPointF sceneStart = mapToScene(viewportStart.toPoint());
    QPointF sceneEnd = mapToScene(viewportEnd.toPoint());
    QPointF chartStart = chart()->mapFromScene(sceneStart);
    QPointF chartEnd = chart()->mapFromScene(sceneEnd);

    return QRectF(chartStart, chartEnd);
}

void ThermalChartView::updateSelectionBoxDisplay()
{
    if (!m_thermalChart) {
        return;
    }

    // 构造矩形（图表坐标系）
    QRectF chartRect = convertViewportRectToChartRect(m_boxSelectStart, m_boxSelectEnd);

    // 显示选框
    m_thermalChart->showSelectionBox(chartRect);
}

void ThermalChartView::finalizeBoxSelection()
{
    // 计算框选区域大小（视口坐标系）
    QPointF delta = m_boxSelectEnd - m_boxSelectStart;
    qreal width = qAbs(delta.x());
    qreal height = qAbs(delta.y());

    qDebug() << "ThermalChartView::finalizeBoxSelection - 框选结束，区域大小:" << width << "x" << height;

    // 边界检查：框选区域太小（<10像素）时忽略，避免误触
    const qreal minBoxSize = 10.0;
    if (width > minBoxSize && height > minBoxSize) {
        // 转换为图表坐标系
        QRectF chartRect = convertViewportRectToChartRect(m_boxSelectStart, m_boxSelectEnd);

        // 执行缩放（方案 A：X轴精确，Y轴自适应）
        if (m_thermalChart) {
            m_thermalChart->zoomToRect(chartRect);
            qDebug() << "ThermalChartView::finalizeBoxSelection - 执行框选缩放";
        }
    } else {
        // 区域太小，隐藏选框
        if (m_thermalChart) {
            m_thermalChart->hideSelectionBox();
        }
        qDebug() << "ThermalChartView::finalizeBoxSelection - 框选区域过小，忽略";
    }

    // 重置框选状态
    m_isBoxSelecting = false;
    m_boxSelectStart = QPointF();
    m_boxSelectEnd = QPointF();
}

void ThermalChartView::wheelEvent(QWheelEvent* event)
{
    // Ctrl+滚轮：缩放图表
    if (event->modifiers() & Qt::ControlModifier) {
        if (!m_thermalChart) {
            event->ignore();
            return;
        }

        // 正确的坐标转换链路：viewport → scene → chart → value
        QPointF viewportPos = event->pos();
        QPointF scenePos = mapToScene(viewportPos.toPoint());
        QPointF chartPos = chart()->mapFromScene(scenePos);

        // 缩放因子（向上滚动=放大，向下滚动=缩小）
        qreal factor = event->angleDelta().y() > 0 ? 0.8 : 1.25;

        // 以鼠标位置为中心缩放 X 轴和所有 Y 轴
        zoomXAxisAtPoint(chartPos, factor);
        zoomAllYAxesAtPoint(chartPos, factor);

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

// ==================== 质量损失工具辅助函数实现 ====================

bool ThermalChartView::validateMassLossToolPreconditions(ThermalCurve** outCurve, QLineSeries** outSeries)
{
    // 验证 CurveManager
    if (!m_curveManager) {
        qWarning() << "ThermalChartView::validateMassLossToolPreconditions - CurveManager 未设置";
        return false;
    }

    // 获取活动曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve || !m_thermalChart) {
        qWarning() << "ThermalChartView::validateMassLossToolPreconditions - 没有活动曲线或 ThermalChart 未设置";
        return false;
    }

    // 获取活动曲线的系列（用于正确的坐标转换）
    QLineSeries* activeSeries = m_thermalChart->seriesForCurve(activeCurve->id());
    if (!activeSeries) {
        qWarning() << "ThermalChartView::validateMassLossToolPreconditions - 无法获取活动曲线的系列";
        return false;
    }

    qDebug() << "ThermalChartView::validateMassLossToolPreconditions - 使用活动曲线:" << activeCurve->name();

    // 输出结果
    if (outCurve) *outCurve = activeCurve;
    if (outSeries) *outSeries = activeSeries;

    return true;
}

void ThermalChartView::resetMassLossToolState()
{
    m_massLossToolActive = false;
    setInteractionMode(InteractionMode::View);
}

bool ThermalChartView::handleMassLossToolClick(const QPointF& viewportPos)
{
    qDebug() << "ThermalChartView::handleMassLossToolClick - 单次点击创建质量损失测量工具，点击位置:" << viewportPos;

    // 1. 验证前置条件并获取活动曲线和系列
    ThermalCurve* activeCurve = nullptr;
    QLineSeries* activeSeries = nullptr;
    if (!validateMassLossToolPreconditions(&activeCurve, &activeSeries)) {
        return false;
    }

    // 2. 进行坐标转换
    QPointF scenePos = mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart()->mapFromScene(scenePos);
    QPointF dataClick = chart()->mapToValue(chartPos, activeSeries);

    qDebug() << "ThermalChartView::handleMassLossToolClick - 点击位置数据坐标:" << dataClick;

    // 3. 获取曲线数据
    const auto& data = activeCurve->getProcessedData();
    if (data.isEmpty()) {
        qWarning() << "ThermalChartView::handleMassLossToolClick - 活动曲线数据为空";
        return false;
    }

    // 4. 计算延伸范围（固定 15.0°C）
    qreal rangeExtension = 15.0;
    qreal startX = dataClick.x() - rangeExtension;
    qreal endX = dataClick.x() + rangeExtension;

    // 5. 查找最接近的点
    ThermalDataPoint point1 = findNearestDataPoint(data, startX);
    ThermalDataPoint point2 = findNearestDataPoint(data, endX);

    qDebug() << "ThermalChartView::handleMassLossToolClick - 自动延伸范围: ±" << rangeExtension;

    // 6. 创建测量工具（委托给 ThermalChart）
    m_thermalChart->addMassLossTool(point1, point2, activeCurve->id());

    // 7. 重置状态
    resetMassLossToolState();

    return false;
}

// ==================== 峰面积工具辅助函数实现 ====================

bool ThermalChartView::validatePeakAreaToolPreconditions(ThermalCurve** outCurve, QLineSeries** outSeries)
{
    // 验证 CurveManager 和目标曲线 ID
    if (!m_curveManager || m_peakAreaCurveId.isEmpty()) {
        qWarning() << "ThermalChartView::validatePeakAreaToolPreconditions - CurveManager 未设置或未指定目标曲线";
        return false;
    }

    // 获取目标曲线
    ThermalCurve* targetCurve = m_curveManager->getCurve(m_peakAreaCurveId);
    if (!targetCurve || !m_thermalChart) {
        qWarning() << "ThermalChartView::validatePeakAreaToolPreconditions - 无法获取目标曲线或 ThermalChart 未设置";
        return false;
    }

    // 获取目标曲线的系列（用于正确的坐标转换）
    QLineSeries* targetSeries = m_thermalChart->seriesForCurve(targetCurve->id());
    if (!targetSeries) {
        qWarning() << "ThermalChartView::validatePeakAreaToolPreconditions - 无法获取目标曲线的系列";
        return false;
    }

    qDebug() << "ThermalChartView::validatePeakAreaToolPreconditions - 使用用户选择的曲线:" << targetCurve->name();

    // 输出结果
    if (outCurve) *outCurve = targetCurve;
    if (outSeries) *outSeries = targetSeries;

    return true;
}

bool ThermalChartView::isClickInsidePlotArea(const QPointF& viewportPos, QPointF* outChartPos)
{
    QPointF scenePos = mapToScene(viewportPos.toPoint());
    QPointF chartPos = chart()->mapFromScene(scenePos);

    if (outChartPos) {
        *outChartPos = chartPos;
    }

    if (!chart()->plotArea().contains(chartPos)) {
        qDebug() << "ThermalChartView::isClickInsidePlotArea - 点击位置不在绘图区内，忽略";
        resetPeakAreaToolState();
        return false;
    }

    return true;
}

qreal ThermalChartView::calculateDynamicRangeExtension(qreal percentage)
{
    qreal rangeExtension = 20.0; // 默认值

    if (!m_thermalChart) {
        return rangeExtension;
    }

    QAbstractAxis* axisX = m_thermalChart->axisX();
    if (axisX) {
        QValueAxis* valueAxisX = qobject_cast<QValueAxis*>(axisX);
        if (valueAxisX) {
            qreal axisRange = valueAxisX->max() - valueAxisX->min();
            rangeExtension = axisRange * percentage;
        }
    }

    return rangeExtension;
}

void ThermalChartView::applyPeakAreaBaseline(PeakAreaTool* tool)
{
    if (!tool) {
        return;
    }

    if (m_peakAreaUseLinearBaseline) {
        // 直线基线（默认）
        tool->setBaselineMode(PeakAreaTool::BaselineMode::Linear);
        qDebug() << "ThermalChartView::applyPeakAreaBaseline - 应用直线基线模式";
    } else if (!m_peakAreaReferenceCurveId.isEmpty()) {
        // 参考曲线基线
        tool->setBaselineMode(PeakAreaTool::BaselineMode::ReferenceCurve);
        tool->setReferenceCurve(m_peakAreaReferenceCurveId);
        qDebug() << "ThermalChartView::applyPeakAreaBaseline - 应用参考曲线基线:" << m_peakAreaReferenceCurveId;
    }
}

void ThermalChartView::resetPeakAreaToolState()
{
    m_peakAreaToolActive = false;
    m_peakAreaCurveId.clear();
    m_peakAreaReferenceCurveId.clear();
    setInteractionMode(InteractionMode::View);
}

bool ThermalChartView::handlePeakAreaToolClick(const QPointF& viewportPos)
{
    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 单次点击创建峰面积测量工具，点击位置:" << viewportPos;

    // 1. 验证前置条件并获取目标曲线和系列
    ThermalCurve* targetCurve = nullptr;
    QLineSeries* targetSeries = nullptr;
    if (!validatePeakAreaToolPreconditions(&targetCurve, &targetSeries)) {
        return false;
    }

    // 2. 检查点击位置是否在绘图区域内
    QPointF chartPos;
    if (!isClickInsidePlotArea(viewportPos, &chartPos)) {
        return false;
    }

    // 3. 进行坐标转换
    QPointF dataClick = chart()->mapToValue(chartPos, targetSeries);
    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 点击位置数据坐标:" << dataClick;

    // 4. 获取曲线数据
    const auto& data = targetCurve->getProcessedData();
    if (data.isEmpty()) {
        qWarning() << "ThermalChartView::handlePeakAreaToolClick - 目标曲线数据为空";
        return false;
    }

    // 5. 计算动态延伸范围（5% 的轴范围）
    qreal rangeExtension = calculateDynamicRangeExtension(0.05);
    qreal startX = dataClick.x() - rangeExtension;
    qreal endX = dataClick.x() + rangeExtension;

    // 6. 查找最接近的点
    ThermalDataPoint point1 = findNearestDataPoint(data, startX);
    ThermalDataPoint point2 = findNearestDataPoint(data, endX);

    qDebug() << "ThermalChartView::handlePeakAreaToolClick - 自动延伸范围: ±" << rangeExtension;

    // 7. 创建峰面积测量工具
    PeakAreaTool* tool = m_thermalChart->addPeakAreaTool(point1, point2, targetCurve->id());

    // 8. 应用基线模式
    applyPeakAreaBaseline(tool);

    // 9. 重置状态
    resetPeakAreaToolState();

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

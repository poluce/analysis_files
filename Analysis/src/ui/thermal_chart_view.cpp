#include "thermal_chart_view.h"
#include "thermal_chart.h"
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

qreal ThermalChartView::hitTestBasePixelThreshold() const
{
    return m_hitTestBasePx;
}

void ThermalChartView::setHitTestIncludePenWidth(bool enabled)
{
    m_hitTestIncludePen = enabled;
}

bool ThermalChartView::hitTestIncludePenWidth() const
{
    return m_hitTestIncludePen;
}

// ==================== 测量工具 ====================

void ThermalChartView::startMassLossTool()
{
    qDebug() << "ThermalChartView::startMassLossTool - 启动质量损失测量工具";
    setInteractionMode(InteractionMode::Pick);
    m_massLossToolActive = true;
}

// ==================== 事件处理（占位符）====================

void ThermalChartView::mousePressEvent(QMouseEvent* event)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::mousePressEvent - 占位符";
    QChartView::mousePressEvent(event);
}

void ThermalChartView::mouseMoveEvent(QMouseEvent* event)
{
    // Phase 4 实现
    // 更新十字线位置
    if (m_thermalChart) {
        QPointF scenePos = mapToScene(event->pos());
        QPointF chartPos = chart()->mapFromScene(scenePos);
        m_thermalChart->updateCrosshairAtChartPos(chartPos);
    }

    QChartView::mouseMoveEvent(event);
}

void ThermalChartView::mouseReleaseEvent(QMouseEvent* event)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::mouseReleaseEvent - 占位符";
    QChartView::mouseReleaseEvent(event);
}

void ThermalChartView::wheelEvent(QWheelEvent* event)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::wheelEvent - 占位符";
    QChartView::wheelEvent(event);
}

void ThermalChartView::contextMenuEvent(QContextMenuEvent* event)
{
    // Phase 4 实现
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

bool ThermalChartView::eventFilter(QObject* watched, QEvent* event)
{
    // Phase 4 实现（右键拖动等）
    return QChartView::eventFilter(watched, event);
}

// ==================== 交互辅助函数（占位符）====================

void ThermalChartView::handleCurveSelectionClick(const QPointF& viewportPos)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::handleCurveSelectionClick - 占位符";
}

void ThermalChartView::handleValueClick(const QPointF& viewportPos)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::handleValueClick - 占位符";
}

QLineSeries* ThermalChartView::findSeriesNearPoint(const QPointF& viewportPos, qreal& outDistance) const
{
    // Phase 4 实现
    outDistance = 0.0;
    return nullptr;
}

qreal ThermalChartView::hitThreshold() const
{
    const qreal deviceRatio = devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio;
    return baseThreshold;
}

void ThermalChartView::handleRightDrag(const QPointF& currentPos)
{
    // Phase 4 实现
    qDebug() << "ThermalChartView::handleRightDrag - 占位符";
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

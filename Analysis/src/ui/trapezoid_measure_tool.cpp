#include "trapezoid_measure_tool.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include <QBrush>
#include <QDebug>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <qmath.h>

TrapezoidMeasureTool::TrapezoidMeasureTool(QChart* chart, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_chart(chart)
    , m_curveManager(nullptr)
    , m_xAxis(nullptr)
    , m_yAxis(nullptr)
    , m_dragState(None)
    , m_hoveredHandle(0)
    , m_closeButtonHovered(false)
    , m_handleRadius(8.0)
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);
    setZValue(1000.0); // 确保在所有曲线之上

    // 设置线条样式
    m_linePen = QPen(QColor(255, 100, 100), 2.0, Qt::SolidLine);

    qDebug() << "TrapezoidMeasureTool: 创建测量工具";
}

QRectF TrapezoidMeasureTool::boundingRect() const
{
    if (!m_chart || !m_xAxis || !m_yAxis) {
        return QRectF();
    }

    // 计算包围矩形（包含两个端点和关闭按钮）
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    qreal minX = qMin(scene1.x(), scene2.x()) - m_handleRadius - 10;
    qreal maxX = qMax(scene1.x(), scene2.x()) + m_handleRadius + 100; // 留出文本空间
    qreal minY = qMin(scene1.y(), scene2.y()) - m_handleRadius - 40; // 留出关闭按钮和文本空间
    qreal maxY = qMax(scene1.y(), scene2.y()) + m_handleRadius + 10;

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void TrapezoidMeasureTool::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (!m_chart || !m_xAxis || !m_yAxis) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // 转换为场景坐标
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    // 绘制直角梯形的三条边
    painter->setPen(m_linePen);

    // 1. 第一条水平线（从第一个点延伸到竖直线）
    QPointF horizontal1End(scene2.x(), scene1.y());
    painter->drawLine(scene1, horizontal1End);

    // 2. 竖直线（连接两条水平线）
    painter->drawLine(horizontal1End, scene2);

    // 3. 第二条水平线（从竖直线延伸到第二个点）
    QPointF horizontal2Start(scene1.x(), scene2.y());
    painter->drawLine(horizontal2Start, scene2);

    // 绘制拖动手柄
    paintHandle(painter, scene1, m_hoveredHandle == 1);
    paintHandle(painter, scene2, m_hoveredHandle == 2);

    // 绘制测量值文本
    paintMeasureText(painter);

    // 绘制关闭按钮
    paintCloseButton(painter);
}

void TrapezoidMeasureTool::setMeasurePoints(const QPointF& point1, const QPointF& point2)
{
    prepareGeometryChange();
    m_point1 = point1;
    m_point2 = point2;
    update();

    qDebug() << "TrapezoidMeasureTool: 设置测量点" << point1 << point2 << "测量值:" << measureValue();
}

void TrapezoidMeasureTool::setAxes(const QString& curveId, QValueAxis* xAxis, QValueAxis* yAxis)
{
    m_curveId = curveId;
    m_xAxis = xAxis;
    m_yAxis = yAxis;
}

void TrapezoidMeasureTool::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;
}

qreal TrapezoidMeasureTool::measureValue() const
{
    return qAbs(m_point2.y() - m_point1.y());
}

void TrapezoidMeasureTool::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    QPointF pos = event->pos();

    // 检查是否点击关闭按钮
    if (isPointInCloseButton(pos)) {
        emit removeRequested();
        event->accept();
        return;
    }

    // 检查是否点击拖动手柄
    int handle = getHandleAtPosition(pos);
    if (handle == 1) {
        m_dragState = DraggingHandle1;
        event->accept();
        return;
    }
    if (handle == 2) {
        m_dragState = DraggingHandle2;
        event->accept();
        return;
    }

    QGraphicsObject::mousePressEvent(event);
}

void TrapezoidMeasureTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragState == None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    // 转换为数据坐标
    QPointF dataPos = sceneToData(event->scenePos());

    // 吸附到曲线
    QPointF snappedPoint = findNearestPointOnCurve(dataPos.x());

    // 更新对应的测量点
    prepareGeometryChange();
    if (m_dragState == DraggingHandle1) {
        m_point1 = snappedPoint;
    } else if (m_dragState == DraggingHandle2) {
        m_point2 = snappedPoint;
    }
    update();

    event->accept();
}

void TrapezoidMeasureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragState = None;
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);
}

void TrapezoidMeasureTool::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF pos = event->pos();

    // 检查关闭按钮悬停
    bool wasHovered = m_closeButtonHovered;
    m_closeButtonHovered = isPointInCloseButton(pos);
    if (wasHovered != m_closeButtonHovered) {
        update();
    }

    // 检查手柄悬停
    int oldHoveredHandle = m_hoveredHandle;
    m_hoveredHandle = getHandleAtPosition(pos);
    if (oldHoveredHandle != m_hoveredHandle) {
        update();
        // 更新鼠标光标
        if (m_hoveredHandle > 0) {
            setCursor(Qt::SizeAllCursor);
        } else if (m_closeButtonHovered) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    QGraphicsObject::hoverMoveEvent(event);
}

QPointF TrapezoidMeasureTool::dataToScene(const QPointF& dataPoint) const
{
    if (!m_chart || !m_xAxis || !m_yAxis) {
        return QPointF();
    }

    // 将数据坐标转换为图表坐标
    QPointF chartPos = m_chart->mapToPosition(dataPoint);
    // 转换为场景坐标
    return m_chart->mapToScene(chartPos);
}

QPointF TrapezoidMeasureTool::sceneToData(const QPointF& scenePoint) const
{
    if (!m_chart || !m_xAxis || !m_yAxis) {
        return QPointF();
    }

    // 转换为图表坐标
    QPointF chartPos = m_chart->mapFromScene(scenePoint);
    // 转换为数据坐标
    return m_chart->mapToValue(chartPos);
}

QPointF TrapezoidMeasureTool::findNearestPointOnCurve(qreal xValue)
{
    if (!m_curveManager || m_curveId.isEmpty()) {
        return QPointF(xValue, 0.0);
    }

    ThermalCurve* curve = m_curveManager->getCurve(m_curveId);
    if (!curve) {
        return QPointF(xValue, 0.0);
    }

    const auto& data = curve->getProcessedData();
    if (data.isEmpty()) {
        return QPointF(xValue, 0.0);
    }

    // 查找最接近的点（根据当前横轴模式）
    // 注意：这里假设是温度横轴，如果需要支持时间横轴，需要从 ChartView 获取当前模式
    int nearestIdx = 0;
    qreal minDist = qAbs(data[0].temperature - xValue);

    for (int i = 1; i < data.size(); ++i) {
        qreal dist = qAbs(data[i].temperature - xValue);
        if (dist < minDist) {
            minDist = dist;
            nearestIdx = i;
        }
    }

    return QPointF(data[nearestIdx].temperature, data[nearestIdx].value);
}

void TrapezoidMeasureTool::paintCloseButton(QPainter* painter)
{
    // 关闭按钮位置：在上方中心
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);
    qreal centerX = (scene1.x() + scene2.x()) / 2.0;
    qreal topY = qMin(scene1.y(), scene2.y()) - 30;

    m_closeButtonRect = QRectF(centerX - 10, topY, 20, 20);

    // 绘制关闭按钮背景
    if (m_closeButtonHovered) {
        painter->setBrush(QBrush(QColor(255, 100, 100, 200)));
    } else {
        painter->setBrush(QBrush(QColor(200, 200, 200, 150)));
    }
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(m_closeButtonRect);

    // 绘制 X 符号
    painter->setPen(QPen(Qt::white, 2.0));
    qreal margin = 5;
    painter->drawLine(m_closeButtonRect.left() + margin, m_closeButtonRect.top() + margin,
                      m_closeButtonRect.right() - margin, m_closeButtonRect.bottom() - margin);
    painter->drawLine(m_closeButtonRect.right() - margin, m_closeButtonRect.top() + margin,
                      m_closeButtonRect.left() + margin, m_closeButtonRect.bottom() - margin);
}

void TrapezoidMeasureTool::paintHandle(QPainter* painter, const QPointF& scenePos, bool isHovered)
{
    // 绘制拖动手柄
    if (isHovered) {
        painter->setBrush(QBrush(QColor(255, 150, 150, 220)));
    } else {
        painter->setBrush(QBrush(QColor(255, 100, 100, 180)));
    }
    painter->setPen(QPen(Qt::white, 2.0));
    painter->drawEllipse(scenePos, m_handleRadius, m_handleRadius);
}

void TrapezoidMeasureTool::paintMeasureText(QPainter* painter)
{
    // 测量值文本位置：在右侧中心
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);
    qreal rightX = qMax(scene1.x(), scene2.x()) + 15;
    qreal centerY = (scene1.y() + scene2.y()) / 2.0;

    // 设置文本样式
    QFont font;
    font.setPointSize(12);
    font.setBold(true);
    painter->setFont(font);

    // 绘制文本背景
    QString text = QString("Δm = %1 mg").arg(measureValue(), 0, 'f', 3);
    QRectF textRect = painter->fontMetrics().boundingRect(text);
    textRect.moveCenter(QPointF(rightX + textRect.width() / 2, centerY));
    textRect.adjust(-5, -3, 5, 3);

    painter->setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter->setPen(QPen(QColor(100, 100, 100), 1.0));
    painter->drawRoundedRect(textRect, 3, 3);

    // 绘制文本
    painter->setPen(QPen(Qt::black));
    painter->drawText(textRect, Qt::AlignCenter, text);
}

void TrapezoidMeasureTool::updatePosition()
{
    prepareGeometryChange();
    update();
}

bool TrapezoidMeasureTool::isPointInCloseButton(const QPointF& pos) const
{
    return m_closeButtonRect.contains(pos);
}

int TrapezoidMeasureTool::getHandleAtPosition(const QPointF& pos) const
{
    if (!m_xAxis || !m_yAxis) {
        return 0;
    }

    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    qreal threshold = m_handleRadius + 5.0;

    // 检查第一个手柄
    QPointF diff1 = pos - scene1;
    if (qSqrt(diff1.x() * diff1.x() + diff1.y() * diff1.y()) <= threshold) {
        return 1;
    }

    // 检查第二个手柄
    QPointF diff2 = pos - scene2;
    if (qSqrt(diff2.x() * diff2.x() + diff2.y() * diff2.y()) <= threshold) {
        return 2;
    }

    return 0;
}

#include "peak_area_tool.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QCursor>
#include <QDebug>
#include <QtMath>
#include <algorithm>

PeakAreaTool::PeakAreaTool(QChart* chart, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_chart(chart)
    , m_curveManager(nullptr)
    , m_xAxis(nullptr)
    , m_yAxis(nullptr)
    , m_series(nullptr)
    , m_useTimeAxis(false)
    , m_baselineMode(BaselineMode::Zero)
    , m_cachedArea(0.0)
    , m_dragState(None)
    , m_hoveredHandle(0)
    , m_closeButtonHovered(false)
    , m_handleRadius(6.0)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setZValue(10.0);  // 在曲线上方，但在十字线下方

    // 样式设置
    m_boundaryPen = QPen(QColor(0, 120, 215), 2.0);  // 蓝色边界线
    m_fillBrush = QBrush(QColor(0, 120, 215, 60));    // 半透明蓝色填充（约25%透明度）

    qDebug() << "构造: PeakAreaTool";
}

QRectF PeakAreaTool::boundingRect() const
{
    if (!m_chart || !m_xAxis || !m_yAxis) {
        return QRectF();
    }

    // 返回覆盖整个绘图区域的矩形
    return m_chart->plotArea();
}

void PeakAreaTool::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (!m_chart || !m_xAxis || !m_yAxis) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // 1. 绘制阴影填充区域
    paintAreaFill(painter);

    // 2. 绘制边界线
    paintBoundary(painter);

    // 3. 绘制拖动手柄
    paintHandles(painter);

    // 4. 绘制面积文本
    paintMeasureText(painter);

    // 5. 绘制关闭按钮
    paintCloseButton(painter);
}

void PeakAreaTool::setMeasurePoints(const ThermalDataPoint& point1, const ThermalDataPoint& point2)
{
    m_point1 = point1;
    m_point2 = point2;

    updateCache();
    update();

    qDebug() << "PeakAreaTool::setMeasurePoints - 点1:" << point1.temperature << point1.value
             << ", 点2:" << point2.temperature << point2.value
             << ", 面积:" << m_cachedArea;
}

void PeakAreaTool::setAxes(const QString& curveId, QValueAxis* xAxis, QValueAxis* yAxis, QAbstractSeries* series)
{
    m_curveId = curveId;
    m_xAxis = xAxis;
    m_yAxis = yAxis;
    m_series = series;

    updateCache();
    update();
}

void PeakAreaTool::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;
}

void PeakAreaTool::setXAxisMode(bool useTimeAxis)
{
    if (m_useTimeAxis == useTimeAxis) {
        return;
    }

    m_useTimeAxis = useTimeAxis;
    updateCache();
    update();

    qDebug() << "PeakAreaTool::setXAxisMode - 切换到" << (useTimeAxis ? "时间" : "温度") << "轴";
}

void PeakAreaTool::setBaselineMode(BaselineMode mode)
{
    if (m_baselineMode == mode) {
        return;
    }

    m_baselineMode = mode;
    updateCache();
    update();

    qDebug() << "PeakAreaTool::setBaselineMode - 切换到基线模式" << static_cast<int>(mode);
}

void PeakAreaTool::setReferenceCurve(const QString& curveId)
{
    m_baselineCurveId = curveId;

    if (m_baselineMode == BaselineMode::ReferenceCurve) {
        updateCache();
        update();
    }
}

QString PeakAreaTool::peakAreaText() const
{
    // TODO: 根据仪器类型确定单位
    return QString("峰面积 = %1").arg(QString::number(qAbs(m_cachedArea), 'f', 3));
}

// ==================== 绘制函数实现 ====================

void PeakAreaTool::paintAreaFill(QPainter* painter)
{
    if (m_cachedPolygon.isEmpty()) {
        return;
    }

    painter->setBrush(m_fillBrush);
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(m_cachedPolygon);
}

void PeakAreaTool::paintBoundary(QPainter* painter)
{
    if (m_cachedPolygon.size() < 2) {
        return;
    }

    painter->setPen(m_boundaryPen);
    painter->setBrush(Qt::NoBrush);

    // 只绘制上边界（曲线部分）
    // 多边形的前半部分是曲线点，后半部分是基线点
    int curvePointCount = m_cachedPolygon.size() / 2;

    for (int i = 0; i < curvePointCount - 1; ++i) {
        painter->drawLine(m_cachedPolygon[i], m_cachedPolygon[i + 1]);
    }
}

void PeakAreaTool::paintHandles(QPainter* painter)
{
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    paintHandle(painter, scene1, m_hoveredHandle == 1);
    paintHandle(painter, scene2, m_hoveredHandle == 2);
}

void PeakAreaTool::paintHandle(QPainter* painter, const QPointF& scenePos, bool isHovered)
{
    if (isHovered) {
        // 悬停时：半透明蓝色填充
        painter->setBrush(QColor(0, 120, 215, 180));
        painter->setPen(QPen(Qt::blue, 2.0));
    } else {
        // 正常状态：白色填充，蓝色边框
        painter->setBrush(Qt::white);
        painter->setPen(QPen(Qt::blue, 1.5));
    }

    painter->drawEllipse(scenePos, m_handleRadius, m_handleRadius);
}

void PeakAreaTool::paintMeasureText(QPainter* painter)
{
    QString text = peakAreaText();

    // 设置字体
    QFont font("Arial", 10);
    painter->setFont(font);

    // 计算文本位置（两个端点中间，稍微向上偏移）
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);
    QPointF textPos = (scene1 + scene2) / 2.0;
    textPos.setY(textPos.y() - 20);  // 向上偏移20像素

    // 计算文本矩形
    QFontMetrics fm(font);
    QRectF textRect = fm.boundingRect(text);
    textRect.moveCenter(textPos);
    textRect.adjust(-5, -3, 5, 3);  // 添加边距

    // 绘制半透明白色背景
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 200));
    painter->drawRoundedRect(textRect, 3, 3);

    // 绘制文本
    painter->setPen(Qt::black);
    painter->drawText(textRect, Qt::AlignCenter, text);
}

void PeakAreaTool::paintCloseButton(QPainter* painter)
{
    // 关闭按钮位于右上角
    QRectF plotArea = m_chart->plotArea();
    qreal btnSize = 20.0;
    m_closeButtonRect = QRectF(plotArea.right() - btnSize - 10,
                                plotArea.top() + 10,
                                btnSize, btnSize);

    // 绘制背景
    if (m_closeButtonHovered) {
        painter->setBrush(QColor(255, 100, 100, 200));  // 红色悬停
    } else {
        painter->setBrush(QColor(200, 200, 200, 150));  // 灰色正常
    }
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(m_closeButtonRect, 3, 3);

    // 绘制 X 符号
    painter->setPen(QPen(Qt::white, 2.0));
    qreal margin = 5.0;
    painter->drawLine(m_closeButtonRect.topLeft() + QPointF(margin, margin),
                      m_closeButtonRect.bottomRight() - QPointF(margin, margin));
    painter->drawLine(m_closeButtonRect.topRight() + QPointF(-margin, margin),
                      m_closeButtonRect.bottomLeft() + QPointF(margin, -margin));
}

// ==================== 计算函数实现 ====================

qreal PeakAreaTool::calculateArea()
{
    if (!m_curveManager || m_curveId.isEmpty()) {
        return 0.0;
    }

    ThermalCurve* curve = m_curveManager->getCurve(m_curveId);
    if (!curve) {
        return 0.0;
    }

    const auto& data = curve->getProcessedData();
    if (data.isEmpty()) {
        return 0.0;
    }

    // 确定X范围
    double x1 = m_useTimeAxis ? m_point1.time : m_point1.temperature;
    double x2 = m_useTimeAxis ? m_point2.time : m_point2.temperature;
    if (x1 > x2) {
        std::swap(x1, x2);
    }

    // 梯形积分法计算面积
    double area = 0.0;

    for (int i = 0; i < data.size() - 1; ++i) {
        double xi = m_useTimeAxis ? data[i].time : data[i].temperature;
        double xi1 = m_useTimeAxis ? data[i + 1].time : data[i + 1].temperature;

        // 检查是否在积分范围内
        if (xi1 < x1 || xi > x2) {
            continue;
        }

        // 裁剪到积分范围
        double effectiveX1 = qMax(xi, x1);
        double effectiveX2 = qMin(xi1, x2);

        // 计算有效的Y值（曲线值 - 基线值）
        double yi = data[i].value - getBaselineValue(xi);
        double yi1 = data[i + 1].value - getBaselineValue(xi1);

        // 梯形面积
        double dx = effectiveX2 - effectiveX1;
        double avgY = (yi + yi1) / 2.0;
        area += avgY * dx;
    }

    return area;
}

QPolygonF PeakAreaTool::buildAreaPolygon()
{
    QPolygonF polygon;

    if (!m_curveManager || m_curveId.isEmpty()) {
        return polygon;
    }

    ThermalCurve* curve = m_curveManager->getCurve(m_curveId);
    if (!curve) {
        return polygon;
    }

    const auto& data = curve->getProcessedData();
    if (data.isEmpty()) {
        return polygon;
    }

    // 确定X范围
    double x1 = m_useTimeAxis ? m_point1.time : m_point1.temperature;
    double x2 = m_useTimeAxis ? m_point2.time : m_point2.temperature;
    if (x1 > x2) {
        std::swap(x1, x2);
    }

    // 第一步：从左到右添加曲线上的点（上边界）
    for (const auto& pt : data) {
        double x = m_useTimeAxis ? pt.time : pt.temperature;
        if (x >= x1 && x <= x2) {
            QPointF scenePos = dataToScene(pt);
            polygon.append(scenePos);
        }
    }

    // 第二步：从右到左添加基线点（下边界）
    QVector<ThermalDataPoint> baselinePoints;
    for (int i = polygon.size() - 1; i >= 0; --i) {
        // 获取对应的数据点
        int dataIndex = -1;
        for (int j = 0; j < data.size(); ++j) {
            double x = m_useTimeAxis ? data[j].time : data[j].temperature;
            if (x >= x1 && x <= x2) {
                if (baselinePoints.size() == polygon.size() - 1 - i) {
                    dataIndex = j;
                    break;
                }
            }
        }

        if (dataIndex >= 0 && dataIndex < data.size()) {
            ThermalDataPoint baselinePt = data[dataIndex];
            double x = m_useTimeAxis ? baselinePt.time : baselinePt.temperature;
            baselinePt.value = getBaselineValue(x);  // 替换为基线值
            QPointF baselineScenePos = dataToScene(baselinePt);
            polygon.append(baselineScenePos);
        }
    }

    return polygon;
}

qreal PeakAreaTool::getBaselineValue(qreal xValue)
{
    switch (m_baselineMode) {
    case BaselineMode::Zero:
        return 0.0;

    case BaselineMode::Linear: {
        // 直线基线：两端点连线
        double x1 = m_useTimeAxis ? m_point1.time : m_point1.temperature;
        double x2 = m_useTimeAxis ? m_point2.time : m_point2.temperature;
        double y1 = m_point1.value;
        double y2 = m_point2.value;

        if (qAbs(x2 - x1) < 1e-9) {
            return y1;  // 避免除零
        }

        // 线性插值
        double ratio = (xValue - x1) / (x2 - x1);
        return y1 + ratio * (y2 - y1);
    }

    case BaselineMode::ReferenceCurve: {
        // 参考曲线基线：从基线曲线获取
        if (!m_curveManager || m_baselineCurveId.isEmpty()) {
            return 0.0;
        }

        ThermalCurve* baseline = m_curveManager->getCurve(m_baselineCurveId);
        if (!baseline) {
            return 0.0;
        }

        const auto& data = baseline->getProcessedData();
        if (data.isEmpty()) {
            return 0.0;
        }

        // 查找最接近xValue的点（线性查找）
        int closestIndex = 0;
        double minDist = qAbs((m_useTimeAxis ? data[0].time : data[0].temperature) - xValue);

        for (int i = 1; i < data.size(); ++i) {
            double x = m_useTimeAxis ? data[i].time : data[i].temperature;
            double dist = qAbs(x - xValue);
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }

        return data[closestIndex].value;
    }
    }

    return 0.0;
}

// ==================== 鼠标事件实现 ====================

void PeakAreaTool::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
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
            setCursor(Qt::ClosedHandCursor);
            event->accept();
        } else if (handle == 2) {
            m_dragState = DraggingHandle2;
            setCursor(Qt::ClosedHandCursor);
            event->accept();
        }
    }

    QGraphicsObject::mousePressEvent(event);
}

void PeakAreaTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragState == DraggingHandle1 || m_dragState == DraggingHandle2) {
        // 转换坐标
        QPointF scenePos = event->scenePos();
        QPointF dataPos = sceneToData(scenePos);

        // 吸附到曲线
        double xValue = m_useTimeAxis ? dataPos.y() : dataPos.x();  // 注意：sceneToData返回的是图表坐标
        ThermalDataPoint snappedPoint = findNearestPointOnCurve(xValue);

        // 更新端点
        if (m_dragState == DraggingHandle1) {
            m_point1 = snappedPoint;
        } else {
            m_point2 = snappedPoint;
        }

        // 重新计算面积和多边形
        qreal newArea = calculateArea();
        if (qAbs(newArea - m_cachedArea) > 0.001) {
            m_cachedArea = newArea;
            emit areaChanged(newArea);
        }

        m_cachedPolygon = buildAreaPolygon();
        update();

        event->accept();
        return;
    }

    QGraphicsObject::mouseMoveEvent(event);
}

void PeakAreaTool::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragState != None) {
        m_dragState = None;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsObject::mouseReleaseEvent(event);
}

void PeakAreaTool::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QPointF pos = event->pos();

    // 检查关闭按钮悬停
    bool wasCloseHovered = m_closeButtonHovered;
    m_closeButtonHovered = isPointInCloseButton(pos);

    // 检查手柄悬停
    int oldHoveredHandle = m_hoveredHandle;
    m_hoveredHandle = getHandleAtPosition(pos);

    // 设置光标
    if (m_closeButtonHovered || m_hoveredHandle > 0) {
        setCursor(Qt::PointingHandCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }

    // 如果悬停状态改变，重绘
    if (wasCloseHovered != m_closeButtonHovered || oldHoveredHandle != m_hoveredHandle) {
        update();
    }

    QGraphicsObject::hoverMoveEvent(event);
}

void PeakAreaTool::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu menu;

    QAction* zeroBaseline = menu.addAction("零基线 (Y=0)");
    QAction* linearBaseline = menu.addAction("直线基线（两端点连线）");

    // TODO: 参考曲线基线模式需要在 ThermalCurve 中添加 baselineCurveId 追踪功能
    // 暂时禁用此功能，只保留零基线和直线基线模式
    // QAction* refCurveBaseline = nullptr;
    // if (m_curveManager && !m_curveId.isEmpty()) {
    //     // 需要查找所有 signalType == SignalType::Baseline 的子曲线
    //     refCurveBaseline = menu.addAction("参考曲线基线");
    // }

    // 标记当前选中的模式
    if (m_baselineMode == BaselineMode::Zero) {
        zeroBaseline->setCheckable(true);
        zeroBaseline->setChecked(true);
    } else if (m_baselineMode == BaselineMode::Linear) {
        linearBaseline->setCheckable(true);
        linearBaseline->setChecked(true);
    }
    // else if (m_baselineMode == BaselineMode::ReferenceCurve && refCurveBaseline) {
    //     refCurveBaseline->setCheckable(true);
    //     refCurveBaseline->setChecked(true);
    // }

    QAction* selected = menu.exec(event->screenPos());

    if (selected == zeroBaseline) {
        setBaselineMode(BaselineMode::Zero);
    } else if (selected == linearBaseline) {
        setBaselineMode(BaselineMode::Linear);
    }
    // else if (selected == refCurveBaseline) {
    //     ThermalCurve* parentCurve = m_curveManager->getCurve(m_curveId);
    //     if (parentCurve) {
    //         setBaselineMode(BaselineMode::ReferenceCurve);
    //         // setReferenceCurve(parentCurve->baselineCurveId());
    //     }
    // }

    event->accept();
}

// ==================== 辅助函数实现 ====================

QPointF PeakAreaTool::dataToScene(const ThermalDataPoint& dataPoint) const
{
    if (!m_chart || !m_series) {
        return QPointF();
    }

    double x = m_useTimeAxis ? dataPoint.time : dataPoint.temperature;
    double y = dataPoint.value;

    return m_chart->mapToPosition(QPointF(x, y), m_series);
}

QPointF PeakAreaTool::sceneToData(const QPointF& scenePoint) const
{
    if (!m_chart || !m_series) {
        return QPointF();
    }

    return m_chart->mapToValue(scenePoint, m_series);
}

ThermalDataPoint PeakAreaTool::findNearestPointOnCurve(qreal xValue)
{
    ThermalDataPoint result;

    if (!m_curveManager || m_curveId.isEmpty()) {
        return result;
    }

    ThermalCurve* curve = m_curveManager->getCurve(m_curveId);
    if (!curve) {
        return result;
    }

    const auto& data = curve->getProcessedData();
    if (data.isEmpty()) {
        return result;
    }

    // 查找最接近xValue的点
    int closestIndex = 0;
    double minDist = qAbs((m_useTimeAxis ? data[0].time : data[0].temperature) - xValue);

    for (int i = 1; i < data.size(); ++i) {
        double x = m_useTimeAxis ? data[i].time : data[i].temperature;
        double dist = qAbs(x - xValue);
        if (dist < minDist) {
            minDist = dist;
            closestIndex = i;
        }
    }

    return data[closestIndex];
}

bool PeakAreaTool::isPointInCloseButton(const QPointF& pos) const
{
    return m_closeButtonRect.contains(pos);
}

int PeakAreaTool::getHandleAtPosition(const QPointF& pos) const
{
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    qreal threshold = m_handleRadius + 3.0;  // 增加一点容差

    if (QLineF(pos, scene1).length() < threshold) {
        return 1;
    }
    if (QLineF(pos, scene2).length() < threshold) {
        return 2;
    }

    return 0;
}

void PeakAreaTool::updateCache()
{
    m_cachedArea = calculateArea();
    m_cachedPolygon = buildAreaPolygon();

    qDebug() << "PeakAreaTool::updateCache - 面积:" << m_cachedArea;
}

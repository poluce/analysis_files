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

    // 将场景坐标多边形转换为本地坐标
    QPolygonF localPolygon;
    for (const QPointF& scenePoint : m_cachedPolygon) {
        localPolygon.append(mapFromScene(scenePoint));
    }

    painter->setBrush(m_fillBrush);
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(localPolygon);
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
        // 转换为本地坐标
        QPointF local1 = mapFromScene(m_cachedPolygon[i]);
        QPointF local2 = mapFromScene(m_cachedPolygon[i + 1]);
        painter->drawLine(local1, local2);
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
    // 将场景坐标转换为本地坐标（相对于 boundingRect）
    QPointF localPos = mapFromScene(scenePos);

    if (isHovered) {
        // 悬停时：半透明蓝色填充
        painter->setBrush(QColor(0, 120, 215, 180));
        painter->setPen(QPen(Qt::blue, 2.0));
    } else {
        // 正常状态：白色填充，蓝色边框
        painter->setBrush(Qt::white);
        painter->setPen(QPen(Qt::blue, 1.5));
    }

    painter->drawEllipse(localPos, m_handleRadius, m_handleRadius);
}

void PeakAreaTool::paintMeasureText(QPainter* painter)
{
    QString text = peakAreaText();

    // 设置字体（稍微加粗以提高可读性）
    QFont font;
    font.setPointSize(11);
    font.setBold(true);
    painter->setFont(font);

    // 计算文本位置：在上方中心（原始设计）
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);
    QPointF sceneTextPos = (scene1 + scene2) / 2.0;
    sceneTextPos.setY(sceneTextPos.y() - 20);  // 向上偏移20像素

    // 转换为本地坐标
    QPointF localTextPos = mapFromScene(sceneTextPos);

    // 计算文本矩形
    QFontMetrics fm(font);
    QRectF textRect = fm.boundingRect(text);
    textRect.moveCenter(localTextPos);
    textRect.adjust(-5, -3, 5, 3);  // 添加边距

    // 绘制半透明白色背景
    painter->setBrush(QBrush(QColor(255, 255, 255, 200)));
    painter->setPen(QPen(QColor(100, 100, 100), 1.0));
    painter->drawRoundedRect(textRect, 3, 3);

    // 绘制文本
    painter->setPen(QPen(Qt::black));
    painter->drawText(textRect, Qt::AlignCenter, text);
}

void PeakAreaTool::paintCloseButton(QPainter* painter)
{
    // 计算文本位置和大小（与 paintMeasureText 中的计算一致）
    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);
    QPointF sceneTextPos = (scene1 + scene2) / 2.0;
    sceneTextPos.setY(sceneTextPos.y() - 20);  // 文本向上偏移20像素

    // 转换为本地坐标
    QPointF localTextPos = mapFromScene(sceneTextPos);

    // 计算文本矩形（需要与 paintMeasureText 中的字体一致）
    QFont font;
    font.setPointSize(11);
    font.setBold(true);
    QFontMetrics fm(font);
    QString text = peakAreaText();
    QRectF textRect = fm.boundingRect(text);
    textRect.moveCenter(localTextPos);
    textRect.adjust(-5, -3, 5, 3);  // 添加边距（与 paintMeasureText 一致）

    // 关闭按钮位置：在文本矩形上方，留8像素间隙
    qreal btnSize = 20.0;
    qreal gap = 8.0;  // 间隙
    qreal buttonCenterX = textRect.center().x();
    qreal buttonCenterY = textRect.top() - gap - btnSize / 2;

    m_closeButtonRect = QRectF(buttonCenterX - btnSize / 2,
                                buttonCenterY - btnSize / 2,
                                btnSize,
                                btnSize);

    // 绘制关闭按钮背景（圆形）
    if (m_closeButtonHovered) {
        painter->setBrush(QColor(255, 100, 100, 200));  // 红色悬停
    } else {
        painter->setBrush(QColor(200, 200, 200, 150));  // 灰色正常
    }
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(m_closeButtonRect);  // 使用圆形

    // 绘制 X 符号
    painter->setPen(QPen(Qt::white, 2.0));
    qreal margin = 5.0;
    painter->drawLine(m_closeButtonRect.left() + margin, m_closeButtonRect.top() + margin,
                      m_closeButtonRect.right() - margin, m_closeButtonRect.bottom() - margin);
    painter->drawLine(m_closeButtonRect.right() - margin, m_closeButtonRect.top() + margin,
                      m_closeButtonRect.left() + margin, m_closeButtonRect.bottom() - margin);
}

// ==================== 计算函数实现 ====================

qreal PeakAreaTool::calculateArea()
{
    if (!m_curveManager || m_curveId.isEmpty()) {
        qWarning() << "PeakAreaTool::calculateArea - CurveManager 或 curveId 为空";
        return 0.0;
    }

    ThermalCurve* curve = m_curveManager->getCurve(m_curveId);
    if (!curve) {
        qWarning() << "PeakAreaTool::calculateArea - 无法获取曲线:" << m_curveId;
        return 0.0;
    }

    const auto& data = curve->getProcessedData();
    if (data.isEmpty()) {
        qWarning() << "PeakAreaTool::calculateArea - 曲线数据为空";
        return 0.0;
    }

    // 确定X范围
    double x1 = m_useTimeAxis ? m_point1.time : m_point1.temperature;
    double x2 = m_useTimeAxis ? m_point2.time : m_point2.temperature;

    qDebug() << "PeakAreaTool::calculateArea - 调试信息:";
    qDebug() << "  曲线ID:" << m_curveId;
    qDebug() << "  数据点数量:" << data.size();
    qDebug() << "  使用时间轴:" << m_useTimeAxis;
    qDebug() << "  点1 - temp:" << m_point1.temperature << ", time:" << m_point1.time << ", value:" << m_point1.value;
    qDebug() << "  点2 - temp:" << m_point2.temperature << ", time:" << m_point2.time << ", value:" << m_point2.value;
    qDebug() << "  X范围: [" << x1 << "," << x2 << "]";
    qDebug() << "  基线模式:" << static_cast<int>(m_baselineMode);

    if (x1 > x2) {
        std::swap(x1, x2);
        qDebug() << "  X范围交换后: [" << x1 << "," << x2 << "]";
    }

    // 梯形积分法计算面积
    double area = 0.0;
    int inRangeCount = 0;

    for (int i = 0; i < data.size() - 1; ++i) {
        double xi = m_useTimeAxis ? data[i].time : data[i].temperature;
        double xi1 = m_useTimeAxis ? data[i + 1].time : data[i + 1].temperature;

        // 检查是否在积分范围内
        if (xi1 < x1 || xi > x2) {
            continue;
        }

        inRangeCount++;

        // 裁剪到积分范围
        double effectiveX1 = qMax(xi, x1);
        double effectiveX2 = qMin(xi1, x2);

        // 🐛 BUG修复：使用 effectiveX1 和 effectiveX2 计算基线值（而不是 xi 和 xi1）
        double baselineY1 = getBaselineValue(effectiveX1);
        double baselineY2 = getBaselineValue(effectiveX2);

        // 插值计算边界点的曲线值
        double curveY1, curveY2;
        if (qAbs(xi1 - xi) > 1e-9) {
            // 线性插值计算 effectiveX1 处的 Y 值
            double ratio1 = (effectiveX1 - xi) / (xi1 - xi);
            curveY1 = data[i].value + ratio1 * (data[i + 1].value - data[i].value);

            // 线性插值计算 effectiveX2 处的 Y 值
            double ratio2 = (effectiveX2 - xi) / (xi1 - xi);
            curveY2 = data[i].value + ratio2 * (data[i + 1].value - data[i].value);
        } else {
            // 避免除零
            curveY1 = curveY2 = data[i].value;
        }

        // 计算有效的Y值（计算曲线 - 参考曲线）
        double yi = curveY1 - baselineY1;
        double yi1 = curveY2 - baselineY2;

        if (inRangeCount <= 3) {
            qDebug() << "  第" << inRangeCount << "个有效数据段:";
            qDebug() << "    X: [" << effectiveX1 << "," << effectiveX2 << "], dx =" << (effectiveX2 - effectiveX1);
            qDebug() << "    曲线Y: [" << curveY1 << "," << curveY2 << "]";
            qDebug() << "    基线Y: [" << baselineY1 << "," << baselineY2 << "]";
            qDebug() << "    净Y: [" << yi << "," << yi1 << "]";
        }

        // 梯形面积（使用绝对值，不关心曲线在基线上方还是下方）
        double dx = effectiveX2 - effectiveX1;
        double trapezoidArea = (yi + yi1) / 2.0 * dx;
        area += qAbs(trapezoidArea);
    }

    qDebug() << "  有效数据段数量:" << inRangeCount;
    qDebug() << "  计算得到的面积:" << area;

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

        qDebug() << "PeakAreaTool::mousePressEvent - 点击位置(本地):" << pos;

        // 检查是否点击关闭按钮
        if (isPointInCloseButton(pos)) {
            qDebug() << "PeakAreaTool::mousePressEvent - 点击关闭按钮";
            emit removeRequested();
            event->accept();
            return;
        }

        // 检查是否点击拖动手柄
        int handle = getHandleAtPosition(pos);
        if (handle == 1) {
            qDebug() << "PeakAreaTool::mousePressEvent - 开始拖动手柄1";
            m_dragState = DraggingHandle1;
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;  // 重要：立即返回，不要调用父类
        } else if (handle == 2) {
            qDebug() << "PeakAreaTool::mousePressEvent - 开始拖动手柄2";
            m_dragState = DraggingHandle2;
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;  // 重要：立即返回，不要调用父类
        }
    }

    QGraphicsObject::mousePressEvent(event);
}

void PeakAreaTool::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragState == DraggingHandle1 || m_dragState == DraggingHandle2) {
        // 转换坐标：场景坐标 → 数据坐标
        QPointF scenePos = event->scenePos();
        QPointF dataPos = sceneToData(scenePos);

        // 从数据坐标中提取 X 值（横轴的值，无论是温度还是时间）
        // mapToValue 返回的 QPointF.x() 就是横轴的值
        double xValue = dataPos.x();

        qDebug() << "PeakAreaTool::mouseMoveEvent - 场景坐标:" << scenePos
                 << ", 数据坐标:" << dataPos
                 << ", xValue:" << xValue;

        // 吸附到曲线
        ThermalDataPoint snappedPoint = findNearestPointOnCurve(xValue);

        // 更新端点
        if (m_dragState == DraggingHandle1) {
            m_point1 = snappedPoint;
            qDebug() << "PeakAreaTool::mouseMoveEvent - 更新端点1:" << snappedPoint.temperature << snappedPoint.time << snappedPoint.value;
        } else {
            m_point2 = snappedPoint;
            qDebug() << "PeakAreaTool::mouseMoveEvent - 更新端点2:" << snappedPoint.temperature << snappedPoint.time << snappedPoint.value;
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
    // pos 是本地坐标，m_closeButtonRect 是场景坐标
    // 需要将 pos 转换为场景坐标进行比较
    QPointF scenePos = mapToScene(pos);
    return m_closeButtonRect.contains(scenePos);
}

int PeakAreaTool::getHandleAtPosition(const QPointF& pos) const
{
    // pos 是本地坐标（相对于 plotArea），scene1/scene2 是场景坐标
    // 由于 boundingRect() 返回 plotArea()，本地坐标原点在 plotArea 左上角
    // 需要将 pos 转换为场景坐标进行比较
    QPointF scenePos = mapToScene(pos);

    QPointF scene1 = dataToScene(m_point1);
    QPointF scene2 = dataToScene(m_point2);

    qreal threshold = m_handleRadius + 3.0;  // 增加一点容差

    qreal dist1 = QLineF(scenePos, scene1).length();
    qreal dist2 = QLineF(scenePos, scene2).length();

    qDebug() << "PeakAreaTool::getHandleAtPosition - pos(本地):" << pos
             << ", scenePos:" << scenePos
             << ", scene1:" << scene1 << ", dist1:" << dist1
             << ", scene2:" << scene2 << ", dist2:" << dist2
             << ", threshold:" << threshold;

    if (dist1 < threshold) {
        qDebug() << "PeakAreaTool::getHandleAtPosition - 检测到手柄1";
        return 1;
    }
    if (dist2 < threshold) {
        qDebug() << "PeakAreaTool::getHandleAtPosition - 检测到手柄2";
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

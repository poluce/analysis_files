#include "PlotWidget.h"
#include <QtCharts/QValueAxis>
#include "domain/model/ThermalCurve.h"
#include "application/curve/CurveManager.h"

#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QDebug>
#include <QtMath>
#include <limits>
#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QPainter>
#include <QMessageBox>

QT_CHARTS_USE_NAMESPACE

PlotWidget::PlotWidget(CurveManager* curveManager, QWidget *parent)
    : QWidget(parent),
      m_curveManager(curveManager),
      m_selectedSeries(nullptr)
{
    m_chartView = new QChartView(this);
    QChart *chart = new QChart();
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

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);

    // 建立信号连接
    setupConnections();
}

void PlotWidget::addCurve(const ThermalCurve &curve)
{
    QLineSeries *series = new QLineSeries();
    series->setName(curve.name());

    const auto& data = curve.getProcessedData();
    for (const auto& point : data)
        series->append(point.temperature, point.value);

    QChart *chart = m_chartView->chart();
    chart->addSeries(series);
    m_seriesToId.insert(series, curve.id());
    m_idToSeries.insert(curve.id(), series);

    // --- 多Y轴逻辑 ---
    QValueAxis* axisY_target = nullptr;
    if (curve.signalType() == SignalType::Derivative) {
        // 微分信号使用次坐标轴（右侧，红色）
        if (!m_axisY_secondary) {
            m_axisY_secondary = new QValueAxis();
            // 可以为次坐标轴设置不同颜色以区分
            QPen pen = m_axisY_secondary->linePen();
            pen.setColor(Qt::red);
            m_axisY_secondary->setLinePen(pen);
            m_axisY_secondary->setLabelsColor(Qt::red);
            m_axisY_secondary->setTitleBrush(QBrush(Qt::red));

            chart->addAxis(m_axisY_secondary, Qt::AlignRight);
        }
        // 动态设置次坐标轴标签（根据仪器类型自动推断）
        m_axisY_secondary->setTitleText(curve.getYAxisLabel());
        axisY_target = m_axisY_secondary;
    } else {
        // 原始信号使用主坐标轴（左侧）
        axisY_target = m_axisY_primary;
        // 动态设置主坐标轴标签（根据仪器类型和初始质量自动推断）
        m_axisY_primary->setTitleText(curve.getYAxisLabel());
    }

    series->attachAxis(m_axisX);
    series->attachAxis(axisY_target);

    // 自动重新缩放坐标轴
    rescaleAxes();
}

void PlotWidget::rescaleAxes()
{
    QChart *chart = m_chartView->chart();

    // --- 独立调整每个坐标轴的范围 ---
    QList<QValueAxis*> axes_to_rescale;
    if(m_axisX) axes_to_rescale.append(m_axisX);
    if(m_axisY_primary) axes_to_rescale.append(m_axisY_primary);
    if(m_axisY_secondary) axes_to_rescale.append(m_axisY_secondary);

    for (QValueAxis* axis : axes_to_rescale) {
        qreal minVal = std::numeric_limits<qreal>::max();
        qreal maxVal = std::numeric_limits<qreal>::lowest();
        bool found_series = false;

        // 遍历图表中的所有系列，检查是否附加到当前坐标轴
        for (QAbstractSeries* s : chart->series()) {
            auto lineSeries = qobject_cast<QLineSeries*>(s);
            if (lineSeries && lineSeries->isVisible()) {
                // 检查这个系列是否附加到当前坐标轴
                bool is_attached = false;
                for (QAbstractAxis* attached_axis : lineSeries->attachedAxes()) {
                    if (attached_axis == axis) {
                        is_attached = true;
                        break;
                    }
                }

                if (is_attached) {
                    found_series = true;
                    for (const QPointF &p : lineSeries->points()) {
                        if (axis->orientation() == Qt::Horizontal) {
                            if (p.x() < minVal) minVal = p.x();
                            if (p.x() > maxVal) maxVal = p.x();
                        } else {
                            if (p.y() < minVal) minVal = p.y();
                            if (p.y() > maxVal) maxVal = p.y();
                        }
                    }
                }
            }
        }

        if (found_series) {
            // 直接使用数据的最小值和最大值，不留边距
            qreal range = maxVal - minVal;
            if (qFuzzyIsNull(range)) {
                // 如果数据是常数，设置一个默认范围
                range = qAbs(minVal) * 0.1;
                if (qFuzzyIsNull(range)) range = 1.0;
                axis->setRange(minVal - range/2, maxVal + range/2);
            } else {
                axis->setRange(minVal, maxVal);
            }
            axis->applyNiceNumbers();
        }
    }
}

void PlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // ========== 点拾取模式 ==========
    if (m_interactionMode == InteractionMode::PickPoints && m_isPickingPoints) {
        qDebug() << "PlotWidget::mousePressEvent - 点拾取模式，鼠标位置:" << event->pos();
        const QPointF mousePos = m_chartView->mapFromGlobal(mapToGlobal(event->pos()));
        qDebug() << "  转换后的视口坐标:" << mousePos;

        // 找到最近的曲线上的点
        auto [curveId, dataPoint] = findNearestPointOnCurves(mousePos);

        if (curveId.isEmpty()) {
            // 没有找到有效的点
            qWarning() << "  未找到有效的点（距离过远或无曲线）";
            QWidget::mousePressEvent(event);
            return;
        }

        qDebug() << "  找到点: 曲线ID=" << curveId << ", 数据坐标=" << dataPoint;

        // 确保在同一条曲线上拾取
        if (m_pickedPoints.isEmpty()) {
            m_targetCurveId = curveId;
            qDebug() << "  设置目标曲线为:" << m_targetCurveId;
        } else if (curveId != m_targetCurveId) {
            // 警告用户必须在同一条曲线上选点
            qWarning() << "  错误: 点击的曲线" << curveId << "与目标曲线" << m_targetCurveId << "不同";
            QMessageBox::warning(this, tr("提示"),
                tr("请在同一条曲线上选择点"));
            QWidget::mousePressEvent(event);
            return;
        }

        // 添加点
        m_pickedPoints.append(dataPoint);
        qDebug() << "  已添加点，当前已选" << m_pickedPoints.size() << "/" << m_numPointsNeeded << "个点";
        emit pointPickingProgress(m_pickedPoints.size(), m_numPointsNeeded);

        m_chartView->viewport()->update();  // 重绘以显示标记
        qDebug() << "  已触发 viewport 重绘";

        // 检查是否完成
        if (m_pickedPoints.size() >= m_numPointsNeeded) {
            // 完成拾取
            qDebug() << "  点拾取完成";

            // 如果有回调函数，使用回调；否则发射信号
            if (m_pickingCallback) {
                qDebug() << "  使用回调函数模式";
                m_pickingCallback(m_targetCurveId, m_pickedPoints);
            } else {
                qDebug() << "  使用信号模式，发射 pointsPickedOnCurve";
                emit pointsPickedOnCurve(m_targetCurveId, m_pickedPoints);
            }

            // 恢复正常模式
            cancelPointPicking();
        }

        QWidget::mousePressEvent(event);
        return;
    }

    // ========== 正常模式：曲线选择 ==========
    QChart *chart = m_chartView->chart();

    const QPointF mousePos = m_chartView->mapFromGlobal(mapToGlobal(event->pos()));

    auto pointToSegDist = [](const QPointF& p, const QPointF& a, const QPointF& b) -> qreal {
        const qreal vx = b.x() - a.x();
        const qreal vy = b.y() - a.y();
        const qreal wx = p.x() - a.x();
        const qreal wy = p.y() - a.y();
        const qreal vv = vx*vx + vy*vy;
        qreal t = vv > 0 ? (wx*vx + wy*vy) / vv : 0.0;
        if (t < 0) t = 0; else if (t > 1) t = 1;
        const QPointF proj(a.x() + t*vx, a.y() + t*vy);
        const qreal dx = p.x() - proj.x();
        const qreal dy = p.y() - proj.y();
        return qSqrt(dx*dx + dy*dy);
    };

    QLineSeries *clickedSeries = nullptr;
    qreal bestDist = 1e18;

    for (QAbstractSeries *abstractSeries : chart->series()) {
        QLineSeries *lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) continue;

        const auto pts = lineSeries->pointsVector();
        if (pts.size() < 2) continue;

        QPointF prevPos = chart->mapToPosition(pts[0], lineSeries);
        for (int i = 1; i < pts.size(); ++i) {
            const QPointF currPos = chart->mapToPosition(pts[i], lineSeries);
            const qreal d = pointToSegDist(mousePos, prevPos, currPos);
            if (d < bestDist) {
                bestDist = d;
                clickedSeries = lineSeries;
            }
            prevPos = currPos;
        }
    }

    const qreal deviceRatio = m_chartView->devicePixelRatioF();
    qreal baseThreshold = (m_hitTestBasePx + 3.0) * deviceRatio; // 稍微扩大命中范围
    if (clickedSeries && m_hitTestIncludePen) {
        const qreal penWidth = clickedSeries->pen().widthF();
        baseThreshold += penWidth * 0.5;
    }

    if (clickedSeries && bestDist <= baseThreshold) {
        if (m_selectedSeries)
            updateSeriesStyle(m_selectedSeries, false);
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

    QWidget::mousePressEvent(event);
}

void PlotWidget::updateSeriesStyle(QLineSeries *series, bool selected)
{
    if (!series) return;
    QPen pen = series->pen();
    pen.setWidth(selected ? 3 : 1);
    series->setPen(pen);
}

// —— 命中阈值配置 ——
void PlotWidget::setHitTestBasePixelThreshold(qreal px)
{
    // 限制最小值，避免被设置为 0 导致很难命中
    m_hitTestBasePx = (px <= 0) ? 1.0 : px;
}

qreal PlotWidget::hitTestBasePixelThreshold() const
{
    return m_hitTestBasePx;
}

void PlotWidget::setHitTestIncludePenWidth(bool enabled)
{
    m_hitTestIncludePen = enabled;
}

bool PlotWidget::hitTestIncludePenWidth() const
{
    return m_hitTestIncludePen;
}

// ========== 信号连接设置 ==========
void PlotWidget::setupConnections()
{
    if (m_curveManager) {
        // 监听 CurveManager 的信号（通知路径：Service → UI）
        connect(m_curveManager, &CurveManager::curveAdded,
                this, &PlotWidget::onCurveAdded);
        connect(m_curveManager, &CurveManager::curveDataChanged,
                this, &PlotWidget::onCurveDataChanged);
        connect(m_curveManager, &CurveManager::curvesCleared,
                this, &PlotWidget::onCurvesCleared);
        connect(m_curveManager, &CurveManager::curveRemoved,
                this, &PlotWidget::onCurveRemoved);
    }
}

void PlotWidget::setCurveVisible(const QString& curveId, bool visible)
{
    auto it = m_idToSeries.find(curveId);
    if (it == m_idToSeries.end()) {
        return;
    }

    QLineSeries *series = it.value();
    if (!series) {
        return;
    }

    if (series->isVisible() == visible) {
        return;
    }

    series->setVisible(visible);

    if (QLegend *legend = m_chartView->chart()->legend()) {
        const auto markers = legend->markers(series);
        for (QLegendMarker *marker : markers) {
            marker->setVisible(visible);
        }
    }

    rescaleAxes();
}

// ========== 响应 CurveManager 信号的槽函数 ==========
void PlotWidget::onCurveAdded(const QString& curveId)
{
    // 当新曲线被添加时，自动显示
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (curve) {
        qDebug() << "PlotWidget: 自动添加曲线" << curve->name();
        addCurve(*curve);
        // 曲线默认可见（默认勾选）
    }
}

void PlotWidget::onCurveDataChanged(const QString& curveId)
{
    // 当曲线数据被修改时，刷新显示
    // 注意：目前的架构中算法会创建新曲线，所以这个信号暂时不会被频繁使用
    // 但保留此接口以支持未来可能的就地修改场景
    qDebug() << "PlotWidget: 曲线数据已变化" << curveId;

    // TODO: 实现曲线数据的更新逻辑
    // 可以找到对应的 series 并更新其数据点
}

void PlotWidget::onCurveRemoved(const QString& curveId)
{
    auto it = m_idToSeries.find(curveId);
    if (it == m_idToSeries.end()) {
        return;
    }

    QLineSeries *series = it.value();
    if (!series) {
        m_idToSeries.remove(curveId);
        return;
    }

    QChart *chart = m_chartView->chart();
    chart->removeSeries(series);
    m_seriesToId.remove(series);
    m_idToSeries.remove(curveId);
    if (m_selectedSeries == series) {
        m_selectedSeries = nullptr;
    }
    series->deleteLater();

    rescaleAxes();
}

void PlotWidget::onCurvesCleared()
{
    QChart *chart = m_chartView->chart();

    const auto currentSeries = chart->series();
    for (QAbstractSeries *series : currentSeries) {
        chart->removeSeries(series);
        series->deleteLater();
    }

    m_seriesToId.clear();
    m_idToSeries.clear();
    m_selectedSeries = nullptr;

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

    emit curveSelected(QString());
}

// ========== 点拾取模式相关方法 ==========
void PlotWidget::startPointPicking(int numPoints, PointPickingCallback callback)
{
    // 使用回调函数的新版本
    m_interactionMode = InteractionMode::PickPoints;
    m_isPickingPoints = true;
    m_numPointsNeeded = numPoints;
    m_pickedPoints.clear();
    m_targetCurveId.clear();
    m_pickingCallback = callback;  // 保存回调函数

    // 改变鼠标光标为十字
    setCursor(Qt::CrossCursor);

    qDebug() << "PlotWidget: 开始点拾取模式（回调版本），需要" << numPoints << "个点";

    emit pointPickingProgress(0, numPoints);
}

void PlotWidget::startPointPicking(int numPoints)
{
    // 旧版本：使用信号（保留兼容性）
    m_interactionMode = InteractionMode::PickPoints;
    m_isPickingPoints = true;
    m_numPointsNeeded = numPoints;
    m_pickedPoints.clear();
    m_targetCurveId.clear();
    m_pickingCallback = nullptr;  // 清空回调函数，使用信号模式

    // 改变鼠标光标为十字
    setCursor(Qt::CrossCursor);

    qDebug() << "PlotWidget: 开始点拾取模式（信号版本），需要" << numPoints << "个点";

    emit pointPickingProgress(0, numPoints);
}

void PlotWidget::cancelPointPicking()
{
    m_interactionMode = InteractionMode::Normal;
    m_isPickingPoints = false;
    m_pickedPoints.clear();
    m_targetCurveId.clear();
    m_pickingCallback = nullptr;  // 清空回调函数

    // 恢复鼠标光标
    setCursor(Qt::ArrowCursor);

    qDebug() << "PlotWidget: 取消点拾取模式";

    m_chartView->viewport()->update();  // 重绘以清除标记

    emit pointPickingCancelled();
}

InteractionMode PlotWidget::interactionMode() const
{
    return m_interactionMode;
}

QPair<QString, QPointF> PlotWidget::findNearestPointOnCurves(const QPointF& screenPos)
{
    qDebug() << "PlotWidget::findNearestPointOnCurves - 查找最近点，屏幕坐标:" << screenPos;
    QChart *chart = m_chartView->chart();

    QString nearestCurveId;
    QPointF nearestDataPoint;
    qreal minDist = 1e18;

    int seriesCount = 0;
    for (QAbstractSeries *abstractSeries : chart->series()) {
        QLineSeries *lineSeries = qobject_cast<QLineSeries*>(abstractSeries);
        if (!lineSeries || !lineSeries->isVisible()) continue;

        seriesCount++;
        const auto pts = lineSeries->pointsVector();
        qDebug() << "  检查曲线:" << m_seriesToId.value(lineSeries) << ", 点数:" << pts.size();

        // 遍历所有数据点，找到屏幕距离最近的点
        for (const QPointF& dataPoint : pts) {
            QPointF screenPoint = chart->mapToPosition(dataPoint, lineSeries);
            qreal dist = QLineF(screenPos, screenPoint).length();

            if (dist < minDist) {
                minDist = dist;
                nearestDataPoint = dataPoint;
                nearestCurveId = m_seriesToId.value(lineSeries);
            }
        }
    }

    qDebug() << "  检查了" << seriesCount << "条曲线";
    qDebug() << "  最小距离:" << minDist << "像素";

    // 设置一个阈值（例如20像素）
    const qreal threshold = 20.0 * m_chartView->devicePixelRatioF();
    qDebug() << "  距离阈值:" << threshold << "像素 (devicePixelRatio=" << m_chartView->devicePixelRatioF() << ")";

    if (minDist > threshold) {
        qDebug() << "  距离超过阈值，返回空";
        return {QString(), QPointF()};
    }

    qDebug() << "  找到最近点: 曲线=" << nearestCurveId << ", 数据坐标=" << nearestDataPoint;
    return {nearestCurveId, nearestDataPoint};
}

bool PlotWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_chartView->viewport() && event->type() == QEvent::Paint) {
        // 先让 chartView 自己绘制
        // paintEvent 会在这里之后自动处理

        qDebug() << "PlotWidget::eventFilter - Paint 事件，开始绘制覆盖层";
        qDebug() << "  当前拾取点数:" << m_pickedPoints.size();
        qDebug() << "  当前注释线数:" << m_annotations.size();

        // 在 chartView 绘制完成后，在其上面绘制我们的内容
        QPainter painter(m_chartView->viewport());
        painter.setRenderHint(QPainter::Antialiasing);

        // 绘制注释线
        drawAnnotationLines(painter);

        // 在点拾取模式下绘制标记
        if (m_isPickingPoints && !m_pickedPoints.isEmpty()) {
            qDebug() << "  绘制点标记...";
            drawPointMarkers(painter);
        }

        qDebug() << "  绘制覆盖层完成";
        return false;  // 继续传递事件
    }
    return QWidget::eventFilter(watched, event);
}

void PlotWidget::drawPointMarkers(QPainter& painter)
{
    qDebug() << "PlotWidget::drawPointMarkers - 绘制" << m_pickedPoints.size() << "个点标记";
    QChart *chart = m_chartView->chart();
    QLineSeries *series = m_idToSeries.value(m_targetCurveId);
    if (!series) {
        qWarning() << "  错误: 无法找到目标曲线的系列对象，ID=" << m_targetCurveId;
        return;
    }

    qDebug() << "  目标曲线:" << m_targetCurveId;

    // 绘制已拾取的点
    int pointIndex = 0;
    for (const QPointF& dataPoint : m_pickedPoints) {
        // chart->mapToPosition 返回场景坐标
        QPointF scenePoint = chart->mapToPosition(dataPoint, series);

        // 将场景坐标转换为视口坐标
        QPointF viewportPoint = m_chartView->mapFromScene(scenePoint);

        qDebug() << "  点" << pointIndex << ": 数据坐标" << dataPoint
                 << "-> 场景坐标" << scenePoint
                 << "-> 视口坐标" << viewportPoint;

        // 绘制圆圈标记
        painter.setPen(QPen(Qt::red, 3));
        painter.setBrush(QBrush(QColor(255, 0, 0, 100)));  // 半透明红色
        painter.drawEllipse(viewportPoint, 8, 8);

        // 绘制十字
        painter.setPen(QPen(Qt::red, 2));
        painter.drawLine(viewportPoint - QPointF(10, 0),
                        viewportPoint + QPointF(10, 0));
        painter.drawLine(viewportPoint - QPointF(0, 10),
                        viewportPoint + QPointF(0, 10));

        pointIndex++;
    }

    qDebug() << "  点标记绘制完成";
}

void PlotWidget::drawAnnotationLines(QPainter& painter)
{
    if (m_annotations.isEmpty()) {
        return;
    }

    qDebug() << "PlotWidget::drawAnnotationLines - 绘制" << m_annotations.size() << "条注释线";
    QChart *chart = m_chartView->chart();

    int lineIndex = 0;
    for (const auto& annotation : m_annotations) {
        qDebug() << "  注释线" << lineIndex << ": ID=" << annotation.id
                 << ", 曲线ID=" << annotation.curveId
                 << ", 起点=" << annotation.start
                 << ", 终点=" << annotation.end;

        QLineSeries *series = m_idToSeries.value(annotation.curveId);
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

        qDebug() << "    数据坐标" << annotation.start << annotation.end
                 << "-> 场景坐标" << sceneStart << sceneEnd
                 << "-> 视口坐标" << viewportStart << viewportEnd;

        painter.setPen(annotation.pen);
        painter.drawLine(viewportStart, viewportEnd);

        qDebug() << "    线条绘制完成";
        lineIndex++;
    }

    qDebug() << "  注释线绘制完成";
}

// ========== 注释线管理方法 ==========
void PlotWidget::addAnnotationLine(const QString& id,
                                   const QString& curveId,
                                   const QPointF& start,
                                   const QPointF& end,
                                   const QPen& pen)
{
    m_annotations.append({id, curveId, start, end, pen});
    qDebug() << "PlotWidget: 添加注释线" << id << "在曲线" << curveId
             << "数据点:" << start << end;
    m_chartView->viewport()->update();  // 触发 viewport 重绘
}

void PlotWidget::removeAnnotation(const QString& id)
{
    for (int i = 0; i < m_annotations.size(); ++i) {
        if (m_annotations[i].id == id) {
            m_annotations.removeAt(i);
            qDebug() << "PlotWidget: 移除注释线" << id;
            m_chartView->viewport()->update();  // 触发 viewport 重绘
            return;
        }
    }
}

void PlotWidget::clearAllAnnotations()
{
    if (!m_annotations.isEmpty()) {
        m_annotations.clear();
        qDebug() << "PlotWidget: 清除所有注释线";
        m_chartView->viewport()->update();  // 触发 viewport 重绘
    }
}

#include "chart_view.h"
#include "thermal_chart.h"
#include "thermal_chart_view.h"
#include "domain/model/thermal_curve.h"
#include "application/curve/curve_manager.h"
#include <QDebug>
#include <QPen>
#include <QColor>
#include <QVBoxLayout>
#include <QAbstractSeries>

QT_CHARTS_USE_NAMESPACE

ChartView::ChartView(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "构造: ChartView (简化容器)";

    // 创建 ThermalChart（数据管理） - 生命周期与 ChartView 绑定
    m_chart = new ThermalChart();

    // 创建 ThermalChartView（交互处理） - 生命周期与 ChartView 绑定
    m_chartView = new ThermalChartView(m_chart, this);

    // 设置布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
    setLayout(layout);

    // ==================== 信号转发 ====================
    // ThermalChartView → ChartView
    connect(m_chartView, &ThermalChartView::curveHit, this, &ChartView::onCurveHit);
    connect(m_chartView, &ThermalChartView::valueClicked, this, &ChartView::onValueClicked);

    // ThermalChartView → ChartView（转发）
    connect(m_chartView, &ThermalChartView::curveHit, this, &ChartView::curveSelected);
}

// ==================== 依赖注入 ====================
void ChartView::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;

    // 设置 ThermalChart 的依赖并初始化
    m_chart->setCurveManager(manager);
    m_chart->initialize();  // 统一初始化（断言完整性）

    // 设置 ThermalChartView 的依赖并初始化
    m_chartView->setCurveManager(manager);
    m_chartView->initialize();  // 统一初始化（断言完整性）
}

// ==================== 交互配置（转发给 ThermalChartView）====================
void ChartView::setHitTestBasePixelThreshold(qreal px)
{
    m_chartView->setHitTestBasePixelThreshold(px);
}

qreal ChartView::hitTestBasePixelThreshold() const
{
    return m_chartView->hitTestBasePixelThreshold();
}

void ChartView::setHitTestIncludePenWidth(bool enabled)
{
    m_chartView->setHitTestIncludePenWidth(enabled);
}

bool ChartView::hitTestIncludePenWidth() const
{
    return m_chartView->hitTestIncludePenWidth();
}

void ChartView::setInteractionMode(int type)
{
    m_chartView->setInteractionMode(static_cast<InteractionMode>(type));
}

// ==================== 横轴模式（转发给 ThermalChart）====================
int ChartView::xAxisMode() const
{
    return static_cast<int>(m_chart->xAxisMode());
}

void ChartView::toggleXAxisMode()
{
    m_chart->toggleXAxisMode();
}

// ==================== 标题配置（转发给 ThermalChart）====================
void ChartView::setChartTitle(const QString& title)
{
    m_chart->setCustomChartTitle(title);
}

void ChartView::setXAxisTitle(const QString& title)
{
    m_chart->setCustomXAxisTitle(title);
}

void ChartView::setYAxisTitlePrimary(const QString& title)
{
    m_chart->setCustomYAxisTitlePrimary(title);
}

void ChartView::setYAxisTitleSecondary(const QString& title)
{
    m_chart->setCustomYAxisTitleSecondary(title);
}

void ChartView::setAllTitles(const QString& chartTitle,
                            const QString& xAxisTitle,
                            const QString& yAxisTitlePrimary,
                            const QString& yAxisTitleSecondary)
{
    m_chart->setCustomTitles(chartTitle, xAxisTitle,
                            yAxisTitlePrimary, yAxisTitleSecondary);
}

void ChartView::clearCustomTitles()
{
    m_chart->clearCustomTitles();
}

// ==================== 曲线管理（转发给 ThermalChart）====================
void ChartView::addCurve(const ThermalCurve& curve)
{
    m_chart->addCurve(curve);
}

void ChartView::updateCurve(const ThermalCurve& curve)
{
    m_chart->updateCurve(curve);
}

void ChartView::removeCurve(const QString& curveId)
{
    m_chart->removeCurve(curveId);

    // 如果删除的曲线是选中点所属的曲线，清除选中点
    if (m_selectedPointsCurveId == curveId) {
        qDebug() << "ChartView::removeCurve - 删除的曲线是选中点所属的曲线，清除选中点:" << curveId;
        m_selectedPoints.clear();
        m_selectedPointsCurveId.clear();
    }
}

void ChartView::clearCurves()
{
    m_chart->clearCurves();
}

void ChartView::setCurveVisible(const QString& curveId, bool visible)
{
    m_chart->setCurveVisible(curveId, visible);
}

void ChartView::highlightCurve(const QString& curveId)
{
    m_chart->highlightCurve(curveId);
}

// ==================== 坐标轴管理（转发给 ThermalChart）====================
void ChartView::rescaleAxes()
{
    m_chart->rescaleAxes();
}

// ==================== 十字线控制（转发给 ThermalChart）====================
void ChartView::setVerticalCrosshairEnabled(bool enabled)
{
    m_chart->setVerticalCrosshairEnabled(enabled);
}

void ChartView::setHorizontalCrosshairEnabled(bool enabled)
{
    m_chart->setHorizontalCrosshairEnabled(enabled);
}

bool ChartView::verticalCrosshairEnabled() const
{
    return m_chart->verticalCrosshairEnabled();
}

bool ChartView::horizontalCrosshairEnabled() const
{
    return m_chart->horizontalCrosshairEnabled();
}

// ==================== 叠加物管理（转发给 ThermalChart）====================

void ChartView::addCurveMarkers(const QString& curveId, const QList<QPointF>& markers,
                                 const QColor& color, qreal size)
{
    m_chart->addCurveMarkers(curveId, markers, color, size);
}

void ChartView::removeCurveMarkers(const QString& curveId)
{
    m_chart->removeCurveMarkers(curveId);
}

void ChartView::clearAllMarkers()
{
    m_chart->clearAllMarkers();
}

void ChartView::startMassLossTool()
{
    m_chartView->startMassLossTool();
}

void ChartView::addMassLossTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)
{
    m_chart->addMassLossTool(point1, point2, curveId);
}

void ChartView::removeMassLossTool(QGraphicsObject* tool)
{
    m_chart->removeMassLossTool(tool);
}

void ChartView::clearAllMassLossTools()
{
    m_chart->clearAllMassLossTools();
}

// ==================== 峰面积工具（转发给 ThermalChartView/ThermalChart）====================

void ChartView::startPeakAreaTool(const QString& curveId, bool useLinearBaseline, const QString& referenceCurveId)
{
    m_chartView->startPeakAreaTool(curveId, useLinearBaseline, referenceCurveId);
}

void ChartView::addPeakAreaTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)
{
    m_chart->addPeakAreaTool(point1, point2, curveId);
}

void ChartView::removePeakAreaTool(QGraphicsObject* tool)
{
    m_chart->removePeakAreaTool(tool);
}

void ChartView::clearAllPeakAreaTools()
{
    m_chart->clearAllPeakAreaTools();
}

QColor ChartView::getCurveColor(const QString& curveId) const
{
    return m_chart->getCurveColor(curveId);
}

// ==================== 算法交互状态机（ChartView 核心职责）====================

void ChartView::startAlgorithmInteraction(const QString& algorithmName, const QString& displayName,
                                          int requiredPoints, const QString& hint, const QString& curveId)
{
    qDebug() << "ChartView::startAlgorithmInteraction - 启动算法交互";
    qDebug() << "  算法:" << displayName << ", 需要点数:" << requiredPoints << ", 目标曲线:" << curveId;

    // 清空之前的选点
    m_selectedPoints.clear();
    m_selectedPointsCurveId.clear();

    // 设置活动算法信息
    m_activeAlgorithm.name = algorithmName;
    m_activeAlgorithm.displayName = displayName;
    m_activeAlgorithm.requiredPointCount = requiredPoints;
    m_activeAlgorithm.hint = hint;
    m_activeAlgorithm.targetCurveId = curveId;

    // 状态转换: Idle → WaitingForPoints
    transitionToState(InteractionState::WaitingForPoints);

    // 切换到选点模式
    setInteractionMode(static_cast<int>(InteractionMode::Pick));

    // 设置选点系列，附着到目标曲线的 Y 轴
    if (!curveId.isEmpty()) {
        QValueAxis* targetYAxis = m_chart->yAxisForCurveId(curveId);
        m_chart->rebindSelectedPointsSeries(targetYAxis);
    }

    qDebug() << "ChartView: 算法" << displayName << "已进入等待用户选点状态";
}

void ChartView::cancelAlgorithmInteraction()
{
    if (!m_activeAlgorithm.isValid()) {
        qDebug() << "ChartView::cancelAlgorithmInteraction - 没有活动算法，无需取消";
        return;
    }

    qDebug() << "ChartView::cancelAlgorithmInteraction - 取消算法交互:" << m_activeAlgorithm.displayName;

    // 清除选点系列
    m_chart->clearSelectedPoints();

    // 清空活动算法信息和交互状态
    m_activeAlgorithm.clear();
    m_selectedPoints.clear();
    m_selectedPointsCurveId.clear();

    // 状态转换: 任意状态 → Idle
    transitionToState(InteractionState::Idle);

    // 切换回视图模式
    setInteractionMode(static_cast<int>(InteractionMode::View));

    qDebug() << "ChartView: 算法交互已取消，回到空闲状态";
}

void ChartView::onValueClicked(const QPointF& value, QAbstractSeries* series)
{
    Q_UNUSED(series);

    // 检查是否有活动算法等待交互
    if (!m_activeAlgorithm.isValid() || m_interactionState != InteractionState::WaitingForPoints) {
        return;
    }

    handlePointSelection(value);
}

void ChartView::onCurveHit(const QString& curveId)
{
    Q_UNUSED(curveId);
    // 曲线命中信号已经通过 connect 转发给外部，这里不需要额外处理
}

void ChartView::handlePointSelection(const QPointF& value)
{
    // 检查前置条件
    if (!m_curveManager || m_activeAlgorithm.targetCurveId.isEmpty()) {
        qWarning() << "ChartView: 没有目标曲线或 CurveManager 未设置";
        return;
    }

    ThermalCurve* targetCurve = m_curveManager->getCurve(m_activeAlgorithm.targetCurveId);
    if (!targetCurve) {
        qWarning() << "ChartView: 无法获取目标曲线" << m_activeAlgorithm.targetCurveId;
        return;
    }

    const auto& curveData = targetCurve->getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "ChartView: 目标曲线数据为空";
        return;
    }

    // 从目标曲线数据中查找最接近的点
    double targetXValue = value.x();
    int closestIndex = 0;
    double minDist = qAbs(curveData[0].temperature - targetXValue);

    // 根据当前横轴模式选择比较方式
    bool useTemperature = (xAxisMode() == 0);  // 0 = Temperature

    if (useTemperature) {
        for (int i = 1; i < curveData.size(); ++i) {
            double dist = qAbs(curveData[i].temperature - targetXValue);
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }
    } else {
        for (int i = 1; i < curveData.size(); ++i) {
            double dist = qAbs(curveData[i].time - targetXValue);
            if (dist < minDist) {
                minDist = dist;
                closestIndex = i;
            }
        }
    }

    // 保存完整的数据点
    ThermalDataPoint selectedDataPoint = curveData[closestIndex];

    // 记录选中点所属的曲线ID（第一次选点时记录）
    if (m_selectedPoints.isEmpty()) {
        m_selectedPointsCurveId = m_activeAlgorithm.targetCurveId;
    }

    qDebug() << "ChartView: 从目标曲线" << targetCurve->name()
             << "找到最接近点 - T=" << selectedDataPoint.temperature
             << ", t=" << selectedDataPoint.time
             << ", v=" << selectedDataPoint.value;

    // 添加完整数据点到选点列表
    m_selectedPoints.append(selectedDataPoint);

    qDebug() << "ChartView: 算法" << m_activeAlgorithm.displayName
             << "选点进度:" << m_selectedPoints.size() << "/" << m_activeAlgorithm.requiredPointCount;

    // ==================== 立即显示选点标记 ====================
    // 每次选点后立即在选点系列中添加显示点（复用上面的 useTemperature 变量）
    qreal xValue = useTemperature ? selectedDataPoint.temperature : selectedDataPoint.time;
    QPointF displayPoint(xValue, selectedDataPoint.value);
    m_chart->addSelectedPoint(displayPoint);

    // 检查是否已收集足够的点
    if (m_selectedPoints.size() >= m_activeAlgorithm.requiredPointCount) {
        completePointSelection();
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

void ChartView::completePointSelection()
{
    transitionToState(InteractionState::PointsCompleted);

    qDebug() << "ChartView::completePointSelection - 算法" << m_activeAlgorithm.displayName
             << "交互完成，发送信号触发执行";

    // 发出算法交互完成信号，触发算法执行
    QString completedAlgorithmName = m_activeAlgorithm.name;
    QVector<ThermalDataPoint> completedPoints = m_selectedPoints;
    emit algorithmInteractionCompleted(completedAlgorithmName, completedPoints);

    // 清除选点系列
    m_chart->clearSelectedPoints();

    // 清空临时选择点
    m_selectedPoints.clear();
    m_selectedPointsCurveId.clear();

    // 清空活动算法信息
    m_activeAlgorithm.clear();

    // 状态转换: PointsCompleted → Idle
    transitionToState(InteractionState::Idle);

    // 切换回视图模式
    setInteractionMode(static_cast<int>(InteractionMode::View));

    qDebug() << "ChartView::completePointSelection - 算法交互状态已清理，回到空闲状态";
}

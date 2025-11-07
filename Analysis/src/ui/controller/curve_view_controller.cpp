#include "curve_view_controller.h"
#include "application/curve/curve_manager.h"
#include "application/project/project_tree_manager.h"
#include "domain/model/thermal_curve.h"
#include "ui/chart_view.h"
#include "ui/project_explorer_view.h"
#include <QAbstractItemModel>
#include <QDebug>
#include <QModelIndex>
#include <QTreeView>

CurveViewController::CurveViewController(
    CurveManager* curveManager, ChartView* plotWidget, ProjectTreeManager* treeManager, ProjectExplorerView* projectExplorer,
    QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_plotWidget(plotWidget)
    , m_treeManager(treeManager)
    , m_projectExplorer(projectExplorer)
{
    qDebug() << "构造:  CurveViewController";

    auto* model = m_treeManager->model();
    m_projectExplorer->setModel(model);

    QTreeView* tree = m_projectExplorer->treeView();
    tree->expandAll();
    connect(model, &QAbstractItemModel::rowsInserted, tree, [tree](const QModelIndex&, int, int) { tree->expandAll(); });

    // 连接 CurveManager 的信号
    connect(m_curveManager, &CurveManager::curveAdded, this, &CurveViewController::onCurveAdded);
    connect(m_curveManager, &CurveManager::curveRemoved, this, &CurveViewController::onCurveRemoved);
    connect(m_curveManager, &CurveManager::curveDataChanged, this, &CurveViewController::onCurveDataChanged);
    connect(m_curveManager, &CurveManager::activeCurveChanged, this, &CurveViewController::onActiveCurveChanged);
    connect(m_curveManager, &CurveManager::curvesCleared, this, &CurveViewController::onCurvesCleared);

    // 连接 ChartView 的信号
    connect(m_plotWidget, &ChartView::curveSelected, this, &CurveViewController::onCurveSelected);
    m_plotWidget->setHorizontalCrosshairEnabled(true);
    m_plotWidget->setVerticalCrosshairEnabled(true);

    // 注意：点选交互由 ChartView 状态机管理，不再通过 CurveViewController 转发

    // 连接 ProjectTreeManager 的信号
    connect(m_treeManager, &ProjectTreeManager::curveCheckStateChanged, this, &CurveViewController::onCurveCheckStateChanged);
    connect(m_treeManager, &ProjectTreeManager::curveItemClicked, this, &CurveViewController::onCurveItemClicked);
    connect(m_treeManager, &ProjectTreeManager::activeCurveIndexChanged, this, &CurveViewController::onActiveCurveIndexChanged);

    // 连接 ProjectExplorerView 的单击信号到 ProjectTreeManager
    connect(m_projectExplorer, &ProjectExplorerView::curveItemClicked, m_treeManager, &ProjectTreeManager::onCurveItemClicked);
}

// ========== 视图管理接口 ==========

void CurveViewController::setCurveVisible(const QString& curveId, bool visible)
{
    qDebug() << "CurveViewController::setCurveVisible - 曲线ID:" << curveId << ", 可见性:" << visible;

    m_plotWidget->setCurveVisible(curveId, visible);
}

void CurveViewController::highlightCurve(const QString& curveId)
{
    qDebug() << "CurveViewController::highlightCurve - 曲线ID:" << curveId;

    // 在图表视图中高亮曲线（加粗显示）
    if (m_plotWidget) {
        m_plotWidget->highlightCurve(curveId);
    }
}

void CurveViewController::updateAllCurves()
{
    qDebug() << "CurveViewController::updateAllCurves - 更新所有曲线";

    // 刷新树模型
    m_treeManager->refresh();
    m_projectExplorer->treeView()->expandAll();

    // 刷新图表（如果需要）
    // m_plotWidget->update();
}

// ========== 响应 CurveManager 信号 ==========

void CurveViewController::onCurveAdded(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveAdded - 曲线已添加:" << curveId;

    if (!m_curveManager || !m_plotWidget) {
        return;
    }

    if (ThermalCurve* curve = m_curveManager->getCurve(curveId)) {
        m_plotWidget->addCurve(*curve);
    } else {
        qWarning() << "CurveViewController::onCurveAdded - 未找到曲线数据，ID:" << curveId;
        return;
    }

    if (m_projectExplorer && m_projectExplorer->treeView()) {
        m_projectExplorer->treeView()->expandAll();
    }

    // 例如：自动选中新添加的曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (activeCurve && activeCurve->id() == curveId) {
        highlightCurve(curveId);
    }
}

void CurveViewController::onCurveRemoved(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveRemoved - 曲线已移除:" << curveId;

    if (!m_plotWidget) {
        return;
    }
    m_plotWidget->removeCurve(curveId);
}

void CurveViewController::onCurveDataChanged(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveDataChanged - 曲线数据已变化:" << curveId;

    if (!m_curveManager || !m_plotWidget) {
        return;
    }

    if (ThermalCurve* curve = m_curveManager->getCurve(curveId)) {
        m_plotWidget->updateCurve(*curve);
        return;
    }

    qWarning() << "CurveViewController::onCurveDataChanged - 未找到曲线数据，ID:" << curveId;
}

void CurveViewController::onActiveCurveChanged(const QString& curveId)
{
    qDebug() << "CurveViewController::onActiveCurveChanged - 活动曲线已变化:" << curveId;

    // 高亮活动曲线（在图表中加粗）
    if (!curveId.isEmpty()) {
        highlightCurve(curveId);

        // 在项目浏览器中高亮选中项
        if (m_treeManager) {
            m_treeManager->setActiveCurve(curveId);
        }
    }
}

void CurveViewController::onCurvesCleared()
{
    qDebug() << "CurveViewController::onCurvesCleared - 清空所有曲线";

    if (!m_plotWidget) {
        return;
    }
    m_plotWidget->clearCurves();
}

// ========== 响应 ChartView 信号 ==========

void CurveViewController::onCurveSelected(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveSelected - 用户选择了曲线:" << curveId;

    // 将选择同步到 CurveManager
    if (curveId.isEmpty()) {
        return;
    }
    m_curveManager->setActiveCurve(curveId);
}

// ========== 响应 ProjectTreeManager 信号 ==========

void CurveViewController::onCurveCheckStateChanged(const QString& curveId, bool checked)
{
    qDebug() << "CurveViewController::onCurveCheckStateChanged - 曲线:" << curveId << ", 勾选状态:" << checked;

    // 更新 ChartView 的曲线可见性
    setCurveVisible(curveId, checked);

    // ==================== 强绑定子曲线联动逻辑 ====================
    // 当父曲线被隐藏时，强绑定的子曲线也应该被隐藏
    // 当父曲线显示时，强绑定的子曲线也应该显示
    const auto& allCurves = m_curveManager->getAllCurves();
    for (const ThermalCurve& childCurve : allCurves) {
        // 查找所有强绑定到当前曲线的子曲线
        if (childCurve.isStronglyBound() && childCurve.parentId() == curveId) {
            qDebug() << "  联动强绑定子曲线:" << childCurve.name() << "(id:" << childCurve.id() << ") -> visible:" << checked;
            setCurveVisible(childCurve.id(), checked);
        }
    }
}

void CurveViewController::onCurveItemClicked(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveItemClicked - 曲线:" << curveId;

    // 设置为活动曲线
    if (!curveId.isEmpty()) {
        m_curveManager->setActiveCurve(curveId);
    }
}

void CurveViewController::onActiveCurveIndexChanged(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    // 在树视图中设置当前选中项（高亮显示）
    if (m_projectExplorer && m_projectExplorer->treeView()) {
        m_projectExplorer->treeView()->setCurrentIndex(index);
        qDebug() << "CurveViewController::onActiveCurveIndexChanged - 在项目浏览器中高亮显示 index:" << index;
    }
}

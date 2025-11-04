#include "curve_view_controller.h"
#include "application/curve/curve_manager.h"
#include "application/project/project_tree_manager.h"
#include "domain/model/thermal_curve.h"
#include "ui/project_explorer_view.h"
#include "ui/chart_view.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QDebug>
#include <QTreeView>

CurveViewController::CurveViewController(
    CurveManager* curveManager, ChartView* plotWidget, ProjectTreeManager* treeManager,
    ProjectExplorerView* projectExplorer, QObject* parent)
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
    connect(model, &QAbstractItemModel::rowsInserted, tree, [tree](const QModelIndex&, int, int) {
        tree->expandAll();
    });

    // 连接 CurveManager 的信号
    connect(m_curveManager, &CurveManager::curveAdded, this, &CurveViewController::onCurveAdded);
    connect(m_curveManager, &CurveManager::curveRemoved, this, &CurveViewController::onCurveRemoved);
    connect(m_curveManager, &CurveManager::curveDataChanged, this, &CurveViewController::onCurveDataChanged);
    connect(m_curveManager, &CurveManager::activeCurveChanged, this, &CurveViewController::onActiveCurveChanged);

    // 连接 ChartView 的信号
    connect(m_plotWidget, &ChartView::curveSelected, this, &CurveViewController::onCurveSelected);

    // 连接 ProjectTreeManager 的信号
    connect(m_treeManager, &ProjectTreeManager::curveCheckStateChanged, this, &CurveViewController::onCurveCheckStateChanged);
}

// ========== 点拾取接口 ==========

void CurveViewController::requestPointPicking(int numPoints, PointPickingCallback callback)
{
    qDebug() << "CurveViewController::requestPointPicking - 请求拾取" << numPoints << "个点";

    // 委托给 ChartView
    m_plotWidget->startPointPicking(numPoints, callback);
}

void CurveViewController::cancelPointPicking()
{
    qDebug() << "CurveViewController::cancelPointPicking - 取消点拾取";

    m_plotWidget->cancelPointPicking();
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

    // 可以在这里添加高亮逻辑
    // 例如：改变曲线线宽、颜色等
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

    // ChartView 和 ProjectTreeManager 已经直接监听 CurveManager 的信号
    // 这里可以添加额外的协调逻辑（如果需要）

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

    // ChartView 和 ProjectTreeManager 已经直接监听 CurveManager 的信号
    // 这里可以添加额外的协调逻辑（如果需要）
}

void CurveViewController::onCurveDataChanged(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveDataChanged - 曲线数据已变化:" << curveId;

    // ChartView 已经直接监听 CurveManager 的信号并更新显示
    // 这里可以添加额外的协调逻辑（如果需要）
}

void CurveViewController::onActiveCurveChanged(const QString& curveId)
{
    qDebug() << "CurveViewController::onActiveCurveChanged - 活动曲线已变化:" << curveId;

    // 高亮活动曲线
    if (!curveId.isEmpty()) {
        highlightCurve(curveId);
    }
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
}

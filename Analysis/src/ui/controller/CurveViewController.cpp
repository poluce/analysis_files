#include "CurveViewController.h"
#include "application/curve/CurveManager.h"
#include "ui/PlotWidget.h"
#include "ui/CurveTreeModel.h"
#include "domain/model/ThermalCurve.h"
#include <QDebug>

CurveViewController::CurveViewController(CurveManager* curveManager,
                                         PlotWidget* plotWidget,
                                         CurveTreeModel* treeModel,
                                         QObject* parent)
    : QObject(parent),
      m_curveManager(curveManager),
      m_plotWidget(plotWidget),
      m_treeModel(treeModel)
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_plotWidget);
    Q_ASSERT(m_treeModel);

    // 连接 CurveManager 的信号
    connect(m_curveManager, &CurveManager::curveAdded,
            this, &CurveViewController::onCurveAdded);
    connect(m_curveManager, &CurveManager::curveRemoved,
            this, &CurveViewController::onCurveRemoved);
    connect(m_curveManager, &CurveManager::curveDataChanged,
            this, &CurveViewController::onCurveDataChanged);
    connect(m_curveManager, &CurveManager::activeCurveChanged,
            this, &CurveViewController::onActiveCurveChanged);

    // 连接 PlotWidget 的信号
    connect(m_plotWidget, &PlotWidget::curveSelected,
            this, &CurveViewController::onCurveSelected);

    // 连接 CurveTreeModel 的信号
    connect(m_treeModel, &CurveTreeModel::curveCheckStateChanged,
            this, &CurveViewController::onCurveCheckStateChanged);

    qDebug() << "CurveViewController 初始化完成";
}

// ========== 点拾取接口 ==========

void CurveViewController::requestPointPicking(int numPoints, PointPickingCallback callback)
{
    qDebug() << "CurveViewController::requestPointPicking - 请求拾取" << numPoints << "个点";

    if (!m_plotWidget) {
        qWarning() << "CurveViewController::requestPointPicking - PlotWidget 为空";
        return;
    }

    // 委托给 PlotWidget
    m_plotWidget->startPointPicking(numPoints, callback);
}

void CurveViewController::cancelPointPicking()
{
    qDebug() << "CurveViewController::cancelPointPicking - 取消点拾取";

    if (m_plotWidget) {
        m_plotWidget->cancelPointPicking();
    }
}

// ========== 视图管理接口 ==========

void CurveViewController::setCurveVisible(const QString& curveId, bool visible)
{
    qDebug() << "CurveViewController::setCurveVisible - 曲线ID:" << curveId << ", 可见性:" << visible;

    if (m_plotWidget) {
        m_plotWidget->setCurveVisible(curveId, visible);
    }
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
    if (m_treeModel) {
        m_treeModel->refresh();
    }

    // 刷新图表（如果需要）
    // m_plotWidget->update();
}

// ========== 响应 CurveManager 信号 ==========

void CurveViewController::onCurveAdded(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveAdded - 曲线已添加:" << curveId;

    // PlotWidget 和 CurveTreeModel 已经直接监听 CurveManager 的信号
    // 这里可以添加额外的协调逻辑（如果需要）

    // 例如：自动选中新添加的曲线
    if (m_curveManager) {
        ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
        if (activeCurve && activeCurve->id() == curveId) {
            highlightCurve(curveId);
        }
    }
}

void CurveViewController::onCurveRemoved(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveRemoved - 曲线已移除:" << curveId;

    // PlotWidget 和 CurveTreeModel 已经直接监听 CurveManager 的信号
    // 这里可以添加额外的协调逻辑（如果需要）
}

void CurveViewController::onCurveDataChanged(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveDataChanged - 曲线数据已变化:" << curveId;

    // PlotWidget 已经直接监听 CurveManager 的信号并更新显示
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

// ========== 响应 PlotWidget 信号 ==========

void CurveViewController::onCurveSelected(const QString& curveId)
{
    qDebug() << "CurveViewController::onCurveSelected - 用户选择了曲线:" << curveId;

    // 将选择同步到 CurveManager
    if (m_curveManager && !curveId.isEmpty()) {
        m_curveManager->setActiveCurve(curveId);
    }
}

// ========== 响应 CurveTreeModel 信号 ==========

void CurveViewController::onCurveCheckStateChanged(const QString& curveId, bool checked)
{
    qDebug() << "CurveViewController::onCurveCheckStateChanged - 曲线:" << curveId << ", 勾选状态:" << checked;

    // 更新 PlotWidget 的曲线可见性
    setCurveVisible(curveId, checked);
}

#include "remove_curve_command.h"

#include "application/curve/curve_manager.h"
#include <QDebug>

RemoveCurveCommand::RemoveCurveCommand(CurveManager* manager,
                                       const QString& curveId,
                                       bool cascadeDelete,
                                       QString description)
    : m_curveManager(manager)
    , m_targetCurveId(curveId)
    , m_cascadeDelete(cascadeDelete)
    , m_description(std::move(description))
{
    if (m_description.isEmpty()) {
        if (m_cascadeDelete) {
            m_description = QObject::tr("删除曲线及其子曲线");
        } else {
            m_description = QObject::tr("删除曲线 %1").arg(m_targetCurveId);
        }
    }
}

bool RemoveCurveCommand::execute()
{
    if (!m_curveManager) {
        qWarning() << "RemoveCurveCommand::execute - CurveManager 为空";
        return false;
    }

    if (m_targetCurveId.isEmpty()) {
        qWarning() << "RemoveCurveCommand::execute - 曲线ID为空";
        return false;
    }

    // 检查曲线是否存在
    ThermalCurve* curve = m_curveManager->getCurve(m_targetCurveId);
    if (!curve) {
        qWarning() << "RemoveCurveCommand::execute - 曲线不存在:" << m_targetCurveId;
        return false;
    }

    // 首次执行时保存当前状态
    if (!m_hasExecuted) {
        // 保存活动曲线ID
        if (ThermalCurve* active = m_curveManager->getActiveCurve()) {
            m_previousActiveId = active->id();
        }

        // 收集要删除的曲线（按删除顺序：子曲线在前）
        m_deletedCurves.clear();
        if (m_cascadeDelete) {
            collectCurvesToDelete(m_targetCurveId, m_deletedCurves);
        } else {
            // 只删除单个曲线
            m_deletedCurves.append(*curve);
        }

        qDebug() << "RemoveCurveCommand: 已收集" << m_deletedCurves.size() << "条曲线待删除";
    }

    // 按顺序删除曲线
    for (const ThermalCurve& curveToDelete : m_deletedCurves) {
        if (!m_curveManager->removeCurve(curveToDelete.id())) {
            qWarning() << "RemoveCurveCommand::execute - 删除曲线失败:" << curveToDelete.id();
        }
    }

    m_hasExecuted = true;
    qDebug() << "RemoveCurveCommand: 已删除" << m_deletedCurves.size() << "条曲线";
    return true;
}

bool RemoveCurveCommand::undo()
{
    if (!m_curveManager) {
        qWarning() << "RemoveCurveCommand::undo - CurveManager 为空";
        return false;
    }

    if (!m_hasExecuted) {
        qWarning() << "RemoveCurveCommand::undo - 命令尚未执行";
        return false;
    }

    // 按相反顺序恢复曲线（父曲线在前，子曲线在后）
    for (int i = m_deletedCurves.size() - 1; i >= 0; --i) {
        m_curveManager->addCurve(m_deletedCurves[i]);
    }

    // 恢复活动曲线
    if (!m_previousActiveId.isEmpty()) {
        m_curveManager->setActiveCurve(m_previousActiveId);
    }

    m_hasExecuted = false;
    qDebug() << "RemoveCurveCommand: 已恢复" << m_deletedCurves.size() << "条曲线";
    return true;
}

bool RemoveCurveCommand::redo()
{
    // redo 与首次执行行为一致
    return execute();
}

QString RemoveCurveCommand::description() const
{
    return m_description;
}

void RemoveCurveCommand::collectCurvesToDelete(const QString& curveId, QVector<ThermalCurve>& outCurves)
{
    if (!m_curveManager) {
        return;
    }

    // 深度优先遍历：先收集所有子曲线，再收集当前曲线
    // 这确保删除顺序是：子曲线 -> 父曲线
    QVector<ThermalCurve*> children = m_curveManager->getChildren(curveId);
    for (ThermalCurve* child : children) {
        collectCurvesToDelete(child->id(), outCurves);  // 递归收集子曲线
    }

    // 收集当前曲线
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (curve) {
        outCurves.append(*curve);
    }
}

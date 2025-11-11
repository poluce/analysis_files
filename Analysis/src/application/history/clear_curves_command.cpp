#include "clear_curves_command.h"

#include "application/curve/curve_manager.h"
#include <QDebug>

ClearCurvesCommand::ClearCurvesCommand(CurveManager* manager, QString description)
    : m_curveManager(manager)
    , m_description(std::move(description))
{
    if (m_description.isEmpty()) {
        m_description = QObject::tr("清空所有曲线");
    }
}

bool ClearCurvesCommand::execute()
{
    if (!m_curveManager) {
        qWarning() << "ClearCurvesCommand::execute - CurveManager 为空";
        return false;
    }

    // 首次执行时保存当前状态
    if (!m_hasExecuted) {
        // 保存所有曲线
        m_savedCurves = m_curveManager->getAllCurves();

        // 保存活动曲线ID
        if (ThermalCurve* active = m_curveManager->getActiveCurve()) {
            m_savedActiveId = active->id();
        }

        qDebug() << "ClearCurvesCommand: 已保存" << m_savedCurves.size() << "条曲线";
    }

    // 清空所有曲线
    m_curveManager->clearCurves();
    m_hasExecuted = true;

    qDebug() << "ClearCurvesCommand: 已清空所有曲线";
    return true;
}

bool ClearCurvesCommand::undo()
{
    if (!m_curveManager) {
        qWarning() << "ClearCurvesCommand::undo - CurveManager 为空";
        return false;
    }

    if (!m_hasExecuted) {
        qWarning() << "ClearCurvesCommand::undo - 命令尚未执行";
        return false;
    }

    // 恢复所有曲线
    for (const ThermalCurve& curve : m_savedCurves) {
        m_curveManager->addCurve(curve);
    }

    // 恢复活动曲线
    if (!m_savedActiveId.isEmpty()) {
        m_curveManager->setActiveCurve(m_savedActiveId);
    }

    m_hasExecuted = false;
    qDebug() << "ClearCurvesCommand: 已恢复" << m_savedCurves.size() << "条曲线";
    return true;
}

bool ClearCurvesCommand::redo()
{
    // redo 与首次执行行为一致
    return execute();
}

QString ClearCurvesCommand::description() const
{
    return m_description;
}

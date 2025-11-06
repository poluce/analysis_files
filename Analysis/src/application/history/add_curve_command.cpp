#include "add_curve_command.h"

#include "application/curve/curve_manager.h"
#include <QDebug>

AddCurveCommand::AddCurveCommand(CurveManager* manager, const ThermalCurve& curveData, QString description)
    : m_curveManager(manager)
    , m_curveData(curveData)
    , m_description(std::move(description))
{
    if (m_description.isEmpty()) {
        m_description = QObject::tr("添加曲线 %1").arg(m_curveData.name());
    }
}

bool AddCurveCommand::execute()
{
    if (!m_curveManager) {
        qWarning() << "AddCurveCommand::execute - CurveManager 为空";
        return false;
    }

    if (m_curveData.id().isEmpty()) {
        qWarning() << "AddCurveCommand::execute - 曲线 ID 为空";
        return false;
    }

    if (m_previousActiveId.isEmpty()) {
        if (ThermalCurve* active = m_curveManager->getActiveCurve()) {
            m_previousActiveId = active->id();
        }
    }

    m_curveManager->addCurve(m_curveData);
    m_curveManager->setActiveCurve(m_curveData.id());
    m_hasExecuted = true;

    qDebug() << "AddCurveCommand: 已添加曲线" << m_curveData.name() << "ID:" << m_curveData.id();
    return true;
}

bool AddCurveCommand::undo()
{
    if (!m_curveManager) {
        qWarning() << "AddCurveCommand::undo - CurveManager 为空";
        return false;
    }

    if (!m_hasExecuted) {
        qWarning() << "AddCurveCommand::undo - 命令尚未执行";
        return false;
    }

    if (!m_curveManager->removeCurve(m_curveData.id())) {
        qWarning() << "AddCurveCommand::undo - 删除曲线失败，ID:" << m_curveData.id();
        return false;
    }

    if (!m_previousActiveId.isEmpty()) {
        m_curveManager->setActiveCurve(m_previousActiveId);
    }

    m_hasExecuted = false;
    qDebug() << "AddCurveCommand: 已撤销曲线" << m_curveData.name() << "ID:" << m_curveData.id();
    return true;
}

bool AddCurveCommand::redo()
{
    // redo 与首次执行行为一致，但不重置 m_previousActiveId
    return execute();
}

QString AddCurveCommand::description() const { return m_description; }

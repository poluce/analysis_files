#include "remove_mass_loss_tool_command.h"
#include "ui/thermal_chart.h"
#include "ui/trapezoid_measure_tool.h"
#include <QDebug>

RemoveMassLossToolCommand::RemoveMassLossToolCommand(ThermalChart* chart,
                                                      QGraphicsObject* tool,
                                                      QString description)
    : m_chart(chart)
    , m_toolPointer(tool)
    , m_description(description)
{
    // 保存工具的配置信息（用于 undo 时重建）
    if (auto* measureTool = qobject_cast<TrapezoidMeasureTool*>(tool)) {
        m_point1 = measureTool->point1();
        m_point2 = measureTool->point2();
        m_curveId = measureTool->curveId();
        qDebug() << "RemoveMassLossToolCommand - 保存工具状态:" << m_curveId;
    } else {
        qWarning() << "RemoveMassLossToolCommand - 工具类型转换失败";
    }
}

bool RemoveMassLossToolCommand::execute()
{
    if (!m_chart) {
        qWarning() << "RemoveMassLossToolCommand::execute - ThermalChart 为空";
        return false;
    }

    if (m_toolPointer.isNull()) {
        qWarning() << "RemoveMassLossToolCommand::execute - 工具已被删除";
        return false;
    }

    // 删除工具
    m_chart->removeMassLossTool(m_toolPointer.data());
    m_wasRemoved = true;

    qDebug() << "RemoveMassLossToolCommand::execute - 移除测量工具";
    return true;
}

bool RemoveMassLossToolCommand::undo()
{
    if (!m_chart) {
        qWarning() << "RemoveMassLossToolCommand::undo - ThermalChart 为空";
        return false;
    }

    if (!m_wasRemoved) {
        qWarning() << "RemoveMassLossToolCommand::undo - 工具未被删除，无法恢复";
        return false;
    }

    // 重新创建工具
    m_toolPointer = m_chart->addMassLossTool(m_point1, m_point2, m_curveId);

    if (m_toolPointer.isNull()) {
        qWarning() << "RemoveMassLossToolCommand::undo - 重新创建工具失败";
        return false;
    }

    m_wasRemoved = false;
    qDebug() << "RemoveMassLossToolCommand::undo - 恢复测量工具";
    return true;
}

bool RemoveMassLossToolCommand::redo()
{
    return execute();
}

QString RemoveMassLossToolCommand::description() const
{
    return m_description.isEmpty() ? "移除质量损失测量工具" : m_description;
}

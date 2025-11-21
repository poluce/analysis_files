#include "remove_mass_loss_tool_command.h"
#include "ui/chart_view.h"
#include "ui/trapezoid_measure_tool.h"
#include <QDebug>

RemoveMassLossToolCommand::RemoveMassLossToolCommand(ChartView* chartView,
                                                      QGraphicsObject* tool,
                                                      QString description)
    : m_chartView(chartView)
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
    if (!m_chartView) {
        qWarning() << "RemoveMassLossToolCommand::execute - ChartView 为空";
        return false;
    }

    if (m_toolPointer.isNull()) {
        qWarning() << "RemoveMassLossToolCommand::execute - 工具已被删除";
        return false;
    }

    // 删除工具
    m_chartView->removeMassLossTool(m_toolPointer.data());
    m_wasRemoved = true;

    qDebug() << "RemoveMassLossToolCommand::execute - 移除测量工具";
    return true;
}

bool RemoveMassLossToolCommand::undo()
{
    if (!m_chartView) {
        qWarning() << "RemoveMassLossToolCommand::undo - ChartView 为空";
        return false;
    }

    if (!m_wasRemoved) {
        qWarning() << "RemoveMassLossToolCommand::undo - 工具未被删除，无法恢复";
        return false;
    }

    // 重新创建工具
    m_toolPointer = m_chartView->addMassLossTool(m_point1, m_point2, m_curveId);

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

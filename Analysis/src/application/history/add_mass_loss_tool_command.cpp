#include "add_mass_loss_tool_command.h"
#include "ui/chart_view.h"
#include <QDebug>

AddMassLossToolCommand::AddMassLossToolCommand(ChartView* chartView,
                                                const ThermalDataPoint& point1,
                                                const ThermalDataPoint& point2,
                                                const QString& curveId,
                                                QString description)
    : m_chartView(chartView)
    , m_point1(point1)
    , m_point2(point2)
    , m_curveId(curveId)
    , m_description(description)
{
}

bool AddMassLossToolCommand::execute()
{
    if (!m_chartView) {
        qWarning() << "AddMassLossToolCommand::execute - ChartView 为空";
        return false;
    }

    // 创建测量工具并保存到 QPointer
    m_toolPointer = m_chartView->addMassLossTool(m_point1, m_point2, m_curveId);

    if (m_toolPointer.isNull()) {
        qWarning() << "AddMassLossToolCommand::execute - 创建测量工具失败";
        return false;
    }

    m_hasExecuted = true;
    qDebug() << "AddMassLossToolCommand::execute - 成功创建测量工具";
    return true;
}

bool AddMassLossToolCommand::undo()
{
    if (!m_chartView) {
        qWarning() << "AddMassLossToolCommand::undo - ChartView 为空";
        return false;
    }

    // 关键：检查工具是否已被外部删除
    if (m_toolPointer.isNull()) {
        qWarning() << "AddMassLossToolCommand::undo - 工具已被外部删除，撤销操作跳过";
        return true;  // 返回 true，因为已经处于"撤销后的状态"
    }

    // 正常删除工具
    m_chartView->removeMassLossTool(m_toolPointer.data());
    m_toolPointer.clear();

    qDebug() << "AddMassLossToolCommand::undo - 移除测量工具";
    return true;
}

bool AddMassLossToolCommand::redo()
{
    // 重新创建工具（因为 undo 删除了它）
    return execute();
}

QString AddMassLossToolCommand::description() const
{
    return m_description.isEmpty() ? "添加质量损失测量工具" : m_description;
}

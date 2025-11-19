#include "remove_peak_area_tool_command.h"
#include "ui/thermal_chart.h"
#include <QDebug>

RemovePeakAreaToolCommand::RemovePeakAreaToolCommand(ThermalChart* chart,
                                                      PeakAreaTool* tool,
                                                      QString description)
    : m_chart(chart)
    , m_toolPointer(tool)
    , m_description(description)
{
    // 保存工具的完整配置信息（用于 undo 时重建）
    if (tool) {
        m_point1 = tool->point1();
        m_point2 = tool->point2();
        m_curveId = tool->curveId();
        m_baselineMode = tool->baselineMode();
        m_referenceCurveId = tool->referenceCurveId();
        qDebug() << "RemovePeakAreaToolCommand - 保存工具状态:"
                 << "曲线ID=" << m_curveId
                 << "基线模式=" << static_cast<int>(m_baselineMode)
                 << "参考曲线=" << m_referenceCurveId;
    } else {
        qWarning() << "RemovePeakAreaToolCommand - 工具指针为空";
    }
}

bool RemovePeakAreaToolCommand::execute()
{
    if (!m_chart) {
        qWarning() << "RemovePeakAreaToolCommand::execute - ThermalChart 为空";
        return false;
    }

    if (m_toolPointer.isNull()) {
        qWarning() << "RemovePeakAreaToolCommand::execute - 工具已被删除";
        return false;
    }

    // 删除工具
    m_chart->removePeakAreaTool(m_toolPointer.data());
    m_wasRemoved = true;

    qDebug() << "RemovePeakAreaToolCommand::execute - 移除峰面积工具";
    return true;
}

bool RemovePeakAreaToolCommand::undo()
{
    if (!m_chart) {
        qWarning() << "RemovePeakAreaToolCommand::undo - ThermalChart 为空";
        return false;
    }

    if (!m_wasRemoved) {
        qWarning() << "RemovePeakAreaToolCommand::undo - 工具未被删除，无法恢复";
        return false;
    }

    // 重新创建工具
    PeakAreaTool* tool = m_chart->addPeakAreaTool(m_point1, m_point2, m_curveId);

    if (!tool) {
        qWarning() << "RemovePeakAreaToolCommand::undo - 重新创建工具失败";
        return false;
    }

    m_toolPointer = tool;

    // 恢复基线模式
    if (m_baselineMode == PeakAreaTool::BaselineMode::Linear) {
        tool->setBaselineMode(PeakAreaTool::BaselineMode::Linear);
        qDebug() << "RemovePeakAreaToolCommand::undo - 恢复直线基线模式";
    } else if (m_baselineMode == PeakAreaTool::BaselineMode::ReferenceCurve && !m_referenceCurveId.isEmpty()) {
        tool->setBaselineMode(PeakAreaTool::BaselineMode::ReferenceCurve);
        tool->setReferenceCurve(m_referenceCurveId);
        qDebug() << "RemovePeakAreaToolCommand::undo - 恢复参考曲线基线:" << m_referenceCurveId;
    }

    m_wasRemoved = false;
    qDebug() << "RemovePeakAreaToolCommand::undo - 恢复峰面积工具";
    return true;
}

bool RemovePeakAreaToolCommand::redo()
{
    return execute();
}

QString RemovePeakAreaToolCommand::description() const
{
    return m_description.isEmpty() ? "移除峰面积测量工具" : m_description;
}

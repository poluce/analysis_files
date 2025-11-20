#include "remove_peak_area_tool_command.h"
#include "ui/chart_view.h"
#include <QDebug>

RemovePeakAreaToolCommand::RemovePeakAreaToolCommand(ChartView* chartView,
                                                      PeakAreaTool* tool,
                                                      QString description)
    : m_chartView(chartView)
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
    if (!m_chartView) {
        qWarning() << "RemovePeakAreaToolCommand::execute - ChartView 为空";
        return false;
    }

    if (m_toolPointer.isNull()) {
        qWarning() << "RemovePeakAreaToolCommand::execute - 工具已被删除";
        return false;
    }

    // 删除工具
    m_chartView->removePeakAreaTool(m_toolPointer.data());
    m_wasRemoved = true;

    qDebug() << "RemovePeakAreaToolCommand::execute - 移除峰面积工具";
    return true;
}

bool RemovePeakAreaToolCommand::undo()
{
    if (!m_chartView) {
        qWarning() << "RemovePeakAreaToolCommand::undo - ChartView 为空";
        return false;
    }

    if (!m_wasRemoved) {
        qWarning() << "RemovePeakAreaToolCommand::undo - 工具未被删除，无法恢复";
        return false;
    }

    // 重新创建工具
    PeakAreaTool* tool = m_chartView->addPeakAreaTool(m_point1, m_point2, m_curveId);

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

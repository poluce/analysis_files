#include "add_peak_area_tool_command.h"
#include "ui/thermal_chart.h"
#include "ui/peak_area_tool.h"
#include <QDebug>

AddPeakAreaToolCommand::AddPeakAreaToolCommand(ThermalChart* chart,
                                                const ThermalDataPoint& point1,
                                                const ThermalDataPoint& point2,
                                                const QString& curveId,
                                                bool useLinearBaseline,
                                                const QString& referenceCurveId,
                                                QString description)
    : m_chart(chart)
    , m_point1(point1)
    , m_point2(point2)
    , m_curveId(curveId)
    , m_useLinearBaseline(useLinearBaseline)
    , m_referenceCurveId(referenceCurveId)
    , m_description(description)
{
}

bool AddPeakAreaToolCommand::execute()
{
    if (!m_chart) {
        qWarning() << "AddPeakAreaToolCommand::execute - ThermalChart 为空";
        return false;
    }

    // 1. 创建峰面积工具
    PeakAreaTool* tool = m_chart->addPeakAreaTool(m_point1, m_point2, m_curveId);
    if (!tool) {
        qWarning() << "AddPeakAreaToolCommand::execute - 创建峰面积工具失败";
        return false;
    }

    m_toolPointer = tool;

    // 2. 配置基线模式
    if (m_useLinearBaseline) {
        tool->setBaselineMode(PeakAreaTool::BaselineMode::Linear);
        qDebug() << "AddPeakAreaToolCommand::execute - 应用直线基线模式";
    } else if (!m_referenceCurveId.isEmpty()) {
        tool->setBaselineMode(PeakAreaTool::BaselineMode::ReferenceCurve);
        tool->setReferenceCurve(m_referenceCurveId);
        qDebug() << "AddPeakAreaToolCommand::execute - 应用参考曲线基线:" << m_referenceCurveId;
    }

    m_hasExecuted = true;
    qDebug() << "AddPeakAreaToolCommand::execute - 成功创建峰面积工具";
    return true;
}

bool AddPeakAreaToolCommand::undo()
{
    if (!m_chart) {
        qWarning() << "AddPeakAreaToolCommand::undo - ThermalChart 为空";
        return false;
    }

    // 关键：检查工具是否已被外部删除
    if (m_toolPointer.isNull()) {
        qWarning() << "AddPeakAreaToolCommand::undo - 工具已被外部删除，撤销操作跳过";
        return true;
    }

    // 正常删除工具
    m_chart->removePeakAreaTool(m_toolPointer.data());
    m_toolPointer.clear();

    qDebug() << "AddPeakAreaToolCommand::undo - 移除峰面积工具";
    return true;
}

bool AddPeakAreaToolCommand::redo()
{
    return execute();
}

QString AddPeakAreaToolCommand::description() const
{
    return m_description.isEmpty() ? "添加峰面积测量工具" : m_description;
}

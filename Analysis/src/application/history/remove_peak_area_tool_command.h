#ifndef REMOVEPEAKAREATOOLCOMMAND_H
#define REMOVEPEAKAREATOOLCOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include "ui/peak_area_tool.h"  // 需要 BaselineMode 枚举
#include <QPointer>
#include <QString>

class ChartView;

/**
 * @brief RemovePeakAreaToolCommand 将峰面积测量工具的删除操作封装为可撤销命令。
 *
 * execute(): 删除峰面积工具（保存其状态）
 * undo():    重新创建峰面积工具（恢复状态，包括基线模式）
 * redo():    再次删除峰面积工具
 *
 * 使用 QPointer 防止悬空指针
 */
class RemovePeakAreaToolCommand : public ICommand {
public:
    RemovePeakAreaToolCommand(ChartView* chartView,
                              PeakAreaTool* tool,
                              QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;
    QString description() const override;

private:
    ChartView* m_chartView = nullptr;
    QPointer<PeakAreaTool> m_toolPointer;  // 智能指针

    // 保存工具完整状态（用于 undo 时重建）
    ThermalDataPoint m_point1;
    ThermalDataPoint m_point2;
    QString m_curveId;
    PeakAreaTool::BaselineMode m_baselineMode;
    QString m_referenceCurveId;

    QString m_description;
    bool m_wasRemoved = false;
};

#endif // REMOVEPEAKAREATOOLCOMMAND_H

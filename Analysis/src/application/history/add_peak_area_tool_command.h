#ifndef ADDPEAKAREATOOLCOMMAND_H
#define ADDPEAKAREATOOLCOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QPointer>
#include <QString>

class ChartView;
class PeakAreaTool;

/**
 * @brief AddPeakAreaToolCommand 将峰面积测量工具的添加操作封装为可撤销命令。
 *
 * execute(): 创建峰面积工具并配置基线模式
 * undo():    删除峰面积工具
 * redo():    重新创建并配置峰面积工具
 *
 * 使用 QPointer 防止悬空指针（同 AddMassLossToolCommand）
 */
class AddPeakAreaToolCommand : public ICommand {
public:
    AddPeakAreaToolCommand(ChartView* chartView,
                           const ThermalDataPoint& point1,
                           const ThermalDataPoint& point2,
                           const QString& curveId,
                           bool useLinearBaseline,
                           const QString& referenceCurveId = QString(),
                           QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;
    QString description() const override;

private:
    ChartView* m_chartView = nullptr;
    ThermalDataPoint m_point1;
    ThermalDataPoint m_point2;
    QString m_curveId;
    bool m_useLinearBaseline = true;
    QString m_referenceCurveId;

    QPointer<PeakAreaTool> m_toolPointer;  // 智能指针，防止悬空指针

    QString m_description;
    bool m_hasExecuted = false;
};

#endif // ADDPEAKAREATOOLCOMMAND_H

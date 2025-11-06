#ifndef ADDCURVECOMMAND_H
#define ADDCURVECOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QString>

class CurveManager;

/**
 * @brief AddCurveCommand 将新曲线的添加操作封装为可撤销命令。
 *
 * execute()   : 将曲线添加到 CurveManager，并激活该曲线。
 * undo()      : 删除刚添加的曲线，并将活动曲线恢复为执行前的值。
 * redo()      : 重新添加曲线并激活。
 */
class AddCurveCommand : public ICommand {
public:
    AddCurveCommand(CurveManager* manager, const ThermalCurve& curveData, QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;

    QString description() const override;

private:
    CurveManager* m_curveManager = nullptr;
    ThermalCurve m_curveData;
    QString m_previousActiveId;
    QString m_description;
    bool m_hasExecuted = false;
};

#endif // ADDCURVECOMMAND_H

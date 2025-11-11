#ifndef CLEARCURVESCOMMAND_H
#define CLEARCURVESCOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QMap>
#include <QString>

class CurveManager;

/**
 * @brief ClearCurvesCommand 将清空所有曲线的操作封装为可撤销命令。
 *
 * execute()   : 清空所有曲线，记录被清空的曲线数据。
 * undo()      : 恢复所有被清空的曲线，并恢复活动曲线。
 * redo()      : 重新清空所有曲线。
 *
 * 使用场景：
 * - 数据导入前清空旧数据
 * - 用户手动清空项目
 */
class ClearCurvesCommand : public ICommand {
public:
    explicit ClearCurvesCommand(CurveManager* manager, QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;

    QString description() const override;

private:
    CurveManager* m_curveManager = nullptr;
    QMap<QString, ThermalCurve> m_savedCurves;  ///< 保存被清空的曲线
    QString m_savedActiveId;                     ///< 保存被清空前的活动曲线ID
    QString m_description;
    bool m_hasExecuted = false;
};

#endif // CLEARCURVESCOMMAND_H

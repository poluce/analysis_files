#ifndef ADDMASSLOSSTOOLCOMMAND_H
#define ADDMASSLOSSTOOLCOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QPointer>
#include <QString>

class ThermalChart;
class QGraphicsObject;

/**
 * @brief AddMassLossToolCommand 将质量损失测量工具的添加操作封装为可撤销命令。
 *
 * execute(): 创建测量工具并添加到图表
 * undo():    删除测量工具
 * redo():    重新创建测量工具
 *
 * 使用 QPointer 防止悬空指针：
 * - 如果工具被外部删除（用户点击关闭按钮），QPointer 自动变为 nullptr
 * - undo() 时检查 isNull()，避免访问野指针导致 Crash
 */
class AddMassLossToolCommand : public ICommand {
public:
    AddMassLossToolCommand(ThermalChart* chart,
                           const ThermalDataPoint& point1,
                           const ThermalDataPoint& point2,
                           const QString& curveId,
                           QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;
    QString description() const override;

private:
    ThermalChart* m_chart = nullptr;
    ThermalDataPoint m_point1;
    ThermalDataPoint m_point2;
    QString m_curveId;

    QPointer<QGraphicsObject> m_toolPointer;  // 智能指针，防止悬空指针

    QString m_description;
    bool m_hasExecuted = false;
};

#endif // ADDMASSLOSSTOOLCOMMAND_H

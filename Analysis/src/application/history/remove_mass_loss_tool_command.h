#ifndef REMOVEMASSLOSSTOOLCOMMAND_H
#define REMOVEMASSLOSSTOOLCOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QPointer>
#include <QString>

class ChartView;
class QGraphicsObject;

/**
 * @brief RemoveMassLossToolCommand 将质量损失测量工具的删除操作封装为可撤销命令。
 *
 * execute(): 删除测量工具（保存其状态）
 * undo():    重新创建测量工具（恢复状态）
 * redo():    再次删除测量工具
 *
 * 使用 QPointer 防止悬空指针
 */
class RemoveMassLossToolCommand : public ICommand {
public:
    RemoveMassLossToolCommand(ChartView* chartView,
                              QGraphicsObject* tool,
                              QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;
    QString description() const override;

private:
    ChartView* m_chartView = nullptr;
    QPointer<QGraphicsObject> m_toolPointer;  // 智能指针

    // 保存工具状态（用于 undo 时重建）
    ThermalDataPoint m_point1;
    ThermalDataPoint m_point2;
    QString m_curveId;

    QString m_description;
    bool m_wasRemoved = false;
};

#endif // REMOVEMASSLOSSTOOLCOMMAND_H

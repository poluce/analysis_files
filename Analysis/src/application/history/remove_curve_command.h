#ifndef REMOVECURVECOMMAND_H
#define REMOVECURVECOMMAND_H

#include "domain/algorithm/i_command.h"
#include "domain/model/thermal_curve.h"
#include <QVector>
#include <QString>

class CurveManager;

/**
 * @brief RemoveCurveCommand 将删除曲线的操作封装为可撤销命令。
 *
 * execute()   : 删除指定曲线（可选级联删除子曲线），记录被删除的曲线。
 * undo()      : 恢复所有被删除的曲线（按正确顺序）。
 * redo()      : 重新删除曲线。
 *
 * 支持两种删除模式：
 * - 单曲线删除（cascadeDelete = false）：只删除指定曲线
 * - 级联删除（cascadeDelete = true）：删除曲线及其所有子曲线
 *
 * 使用场景：
 * - 用户手动删除曲线
 * - 算法执行前清理旧结果
 */
class RemoveCurveCommand : public ICommand {
public:
    /**
     * @brief 构造删除曲线命令
     *
     * @param manager CurveManager 实例
     * @param curveId 要删除的曲线ID
     * @param cascadeDelete 是否级联删除子曲线（默认 false）
     * @param description 命令描述（可选）
     */
    RemoveCurveCommand(CurveManager* manager,
                       const QString& curveId,
                       bool cascadeDelete = false,
                       QString description = QString());

    bool execute() override;
    bool undo() override;
    bool redo() override;

    QString description() const override;

private:
    /**
     * @brief 递归收集要删除的曲线（深度优先，子曲线在前）
     *
     * 确保删除顺序：先删除子曲线，再删除父曲线
     * 撤销顺序相反：先恢复父曲线，再恢复子曲线
     *
     * @param curveId 当前曲线ID
     * @param outCurves 输出：按删除顺序排列的曲线列表
     */
    void collectCurvesToDelete(const QString& curveId, QVector<ThermalCurve>& outCurves);

private:
    CurveManager* m_curveManager = nullptr;
    QString m_targetCurveId;                     ///< 目标曲线ID
    bool m_cascadeDelete = false;                ///< 是否级联删除
    QVector<ThermalCurve> m_deletedCurves;       ///< 被删除的曲线列表（按删除顺序）
    QString m_previousActiveId;                  ///< 删除前的活动曲线ID
    QString m_description;
    bool m_hasExecuted = false;
};

#endif // REMOVECURVECOMMAND_H

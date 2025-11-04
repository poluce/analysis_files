#ifndef BASELINECOMMAND_H
#define BASELINECOMMAND_H

#include "domain/algorithm/ICommand.h"
#include <QString>
#include <QPointF>

// 前置声明
class ChartView;

/**
 * @brief BaselineCommand 将基线绘制封装为可撤销的命令。
 *
 * 此命令在图表上添加基线注释，支持撤销和重做操作。
 * execute() 在图表上绘制基线。
 * undo() 删除基线。
 * redo() 重新绘制基线（复用execute）。
 */
class BaselineCommand : public ICommand
{
public:
    /**
     * @brief 构造一个基线绘制命令。
     * @param plotWidget ChartView 指针（不获取所有权）。
     * @param curveId 曲线ID。
     * @param start 起点数据坐标。
     * @param end 终点数据坐标。
     */
    BaselineCommand(ChartView* plotWidget,
                    const QString& curveId,
                    const QPointF& start,
                    const QPointF& end);

    /**
     * @brief 析构函数。
     */
    ~BaselineCommand() override = default;

    /**
     * @brief 执行命令，在图表上绘制基线。
     * @return 如果执行成功返回 true，否则返回 false。
     */
    bool execute() override;

    /**
     * @brief 撤销命令，删除基线。
     * @return 如果撤销成功返回 true，否则返回 false。
     */
    bool undo() override;

    /**
     * @brief 获取命令的描述信息。
     * @return 命令描述，例如 "绘制基线"。
     */
    QString description() const override;

private:
    ChartView* m_plotWidget;  // ChartView 指针（不拥有）
    QString m_curveId;         // 曲线ID
    QPointF m_start;           // 起点数据坐标
    QPointF m_end;             // 终点数据坐标
    QString m_lineId;          // 生成的基线ID（UUID）
};

#endif // BASELINECOMMAND_H

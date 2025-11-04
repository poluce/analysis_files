#ifndef ALGORITHMCOMMAND_H
#define ALGORITHMCOMMAND_H

#include "domain/algorithm/ICommand.h"
#include "domain/algorithm/IThermalAlgorithm.h"
#include "domain/model/ThermalCurve.h"
#include <QString>
#include <QVector>

// 前置声明
class CurveManager;

/**
 * @brief AlgorithmCommand 将算法执行封装为可撤销的命令。
 *
 *
 * 此命令创建 <新曲线>作为算法执行的结果，支持撤销和重做操作。
 * execute() 执行算法并创建新曲线。
 * undo() 删除新创建的曲线。
 * redo() 重新添加曲线。
 */
class AlgorithmCommand : public ICommand {
public:
    /**
     * @brief 构造一个算法命令。
     * @param algorithm 要执行的算法（不获取所有权，由 AlgorithmManager 管理）。
     * @param inputCurve 输入曲线（不获取所有权，由 CurveManager 管理）。
     * @param curveManager CurveManager 指针（不获取所有权）。
     * @param algorithmName 算法名称（用于描述）。
     */
    AlgorithmCommand(IThermalAlgorithm* algorithm, ThermalCurve* inputCurve, CurveManager* curveManager, const QString& algorithmName);

    /**
     * @brief 析构函数。
     */
    ~AlgorithmCommand() override = default;

    /**
     * @brief 执行算法，处理曲线数据。
     * @return 如果执行成功返回 true，否则返回 false。
     */
    bool execute() override;

    /**
     * @brief 撤销算法执行，恢复到之前的数据。
     * @return 如果撤销成功返回 true，否则返回 false。
     */
    bool undo() override;

    /**
     * @brief 重做算法执行，应用缓存的结果数据。
     * @return 如果重做成功返回 true，否则返回 false。
     */
    bool redo() override;

    /**
     * @brief 获取命令的描述信息。
     * @return 命令描述，例如 "对曲线'TGA_001'执行微分算法"。
     */
    QString description() const override;

private:
    IThermalAlgorithm* m_algorithm; // 算法指针（不拥有）
    ThermalCurve* m_inputCurve;     // 输入曲线指针（不拥有）
    CurveManager* m_curveManager;   // CurveManager 指针（不拥有）
    QString m_algorithmName;        // 算法名称
    QString m_inputCurveName;       // 输入曲线名称（用于描述）

    QString m_newCurveId;        // 新创建的曲线ID
    ThermalCurve m_newCurveData; // 新曲线的完整数据（用于重做）

    bool m_executed; // 是否已执行
};

#endif // ALGORITHMCOMMAND_H

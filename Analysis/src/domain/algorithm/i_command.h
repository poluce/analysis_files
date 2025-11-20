#ifndef ICOMMAND_H
#define ICOMMAND_H

#include <QString>

/**
 * @brief ICommand 接口定义了命令模式的标准。
 *
 * 这个接口遵循命令模式（Command Pattern），用于封装操作为对象，
 * 从而支持撤销（undo）和重做（redo）功能。
 * 所有可撤销的操作都应该实现此接口。
 */
class ICommand {
public:
    // 接口必须有虚析构函数
    virtual ~ICommand() = default;

    /**
     * @brief 执行命令操作。
     * @return 如果执行成功返回 true，否则返回 false。
     */
    virtual bool execute() = 0;

    /**
     * @brief 撤销命令操作，恢复到执行前的状态。
     * @return 如果撤销成功返回 true，否则返回 false。
     */
    virtual bool undo() = 0;

    /**
     * @brief 重做命令操作（默认实现为再次执行）。
     * @return 如果重做成功返回 true，否则返回 false。
     */
    virtual bool redo()
    {
        return execute();
    }

    /**
     * @brief 返回命令的描述信息（用于历史记录显示）。
     * @return 命令的中文描述。
     */
    virtual QString description() const = 0;

    /**
     * @brief 判断此命令是否可以被撤销。
     * @return 如果可以撤销返回 true，否则返回 false。
     */
    virtual bool canUndo() const
    {
        return true;
    }
};

#endif // ICOMMAND_H

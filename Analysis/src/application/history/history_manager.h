#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include "domain/algorithm/i_command.h"
#include <QObject>
#include <memory>
#include <deque>

/**
 * @brief HistoryManager 管理命令的历史记录，支持撤销和重做。
 *
 * 这个类实现了命令历史管理，维护撤销栈和重做栈。
 *
 * 设计要点：
 * - 通过 ApplicationContext 管理生命周期（依赖注入）
 * - 使用 std::deque 实现 O(1) 的栈操作
 * - 支持历史深度限制（默认50步）
 */
class HistoryManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit HistoryManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~HistoryManager();

    /**
     * @brief 执行命令并将其添加到历史记录。
     * @param command 要执行的命令（使用智能指针管理）。
     * @return 如果执行成功返回 true，否则返回 false。
     */
    bool executeCommand(std::unique_ptr<ICommand> command);

    /**
     * @brief 撤销最近的命令。
     * @return 如果撤销成功返回 true，否则返回 false。
     */
    bool undo();

    /**
     * @brief 重做最近被撤销的命令。
     * @return 如果重做成功返回 true，否则返回 false。
     */
    bool redo();

    /**
     * @brief 检查是否可以执行撤销操作。
     * @return 如果可以撤销返回 true，否则返回 false。
     */
    bool canUndo() const;

    /**
     * @brief 检查是否可以执行重做操作。
     * @return 如果可以重做返回 true，否则返回 false。
     */
    bool canRedo() const;

    /**
     * @brief 获取撤销栈的当前大小。
     * @return 撤销栈中命令的数量。
     */
    int undoCount() const;

    /**
     * @brief 获取重做栈的当前大小。
     * @return 重做栈中命令的数量。
     */
    int redoCount() const;

    /**
     * @brief 清空所有历史记录。
     */
    void clear();

    /**
     * @brief 设置历史记录的最大深度。
     * @param limit 最大深度（默认为50）。
     */
    void setHistoryLimit(int limit);

    /**
     * @brief 获取当前历史记录的最大深度。
     * @return 历史记录的最大深度。
     */
    int historyLimit() const;

signals:
    /**
     * @brief 当历史记录状态改变时发射此信号。
     * 用于通知UI更新撤销/重做按钮的状态。
     */
    void historyChanged();

private:
    // 禁止拷贝构造和赋值
    HistoryManager(const HistoryManager&) = delete;
    HistoryManager& operator=(const HistoryManager&) = delete;

    // 限制历史栈的大小
    void enforceHistoryLimit();

    // 通用的栈操作模板方法（消除 undo/redo 重复代码）
    using CommandStack = std::deque<std::unique_ptr<ICommand>>;
    bool performStackOperation(
        CommandStack& sourceStack,
        CommandStack& targetStack,
        bool (ICommand::*operation)(),  // 指向成员函数的指针
        const QString& operationName);

    std::deque<std::unique_ptr<ICommand>> m_undoStack; // 撤销栈（使用 deque 实现 O(1) 操作）
    std::deque<std::unique_ptr<ICommand>> m_redoStack; // 重做栈（使用 deque 实现 O(1) 操作）
    int m_historyLimit;                                // 历史记录最大深度
};

#endif // HISTORYMANAGER_H

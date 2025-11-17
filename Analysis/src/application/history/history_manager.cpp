#include "history_manager.h"
#include <QDebug>

HistoryManager::HistoryManager(QObject* parent)
    : QObject(parent)
    , m_historyLimit(50) // 默认历史记录深度为50
{
    qDebug() << "构造:  HistoryManager";
}

HistoryManager::~HistoryManager() { clear(); }

bool HistoryManager::executeCommand(std::unique_ptr<ICommand> command)
{
    qDebug() << "检查命令";
    if (!command) {
        qWarning() << "HistoryManager::executeCommand: 尝试执行空命令";
        return false;
    }
    qDebug() << "开始执行命令";
    // 执行命令
    if (!command->execute()) {
        qWarning() << "HistoryManager::executeCommand: 命令执行失败:" << command->description();
        return false;
    }

    qDebug() << "HistoryManager: 执行命令 -" << command->description();

    // 将命令添加到撤销栈
    m_undoStack.push_back(std::move(command));

    // 清空重做栈（执行新命令后，之前的重做历史失效）
    m_redoStack.clear();

    // 限制历史栈大小
    enforceHistoryLimit();

    // 发射历史改变信号
    emit historyChanged();

    return true;
}

bool HistoryManager::undo()
{
    return performStackOperation(m_undoStack, m_redoStack, &ICommand::undo, "撤销");
}

bool HistoryManager::redo()
{
    return performStackOperation(m_redoStack, m_undoStack, &ICommand::redo, "重做");
}

bool HistoryManager::performStackOperation(
    CommandStack& sourceStack,
    CommandStack& targetStack,
    bool (ICommand::*operation)(),
    const QString& operationName)
{
    if (sourceStack.empty()) {
        qDebug() << "HistoryManager::" << operationName << ": 栈为空";
        return false;
    }

    // 从源栈弹出命令
    std::unique_ptr<ICommand> command = std::move(sourceStack.back());
    sourceStack.pop_back();

    // 执行操作（undo 或 redo）
    if (!(command.get()->*operation)()) {  // 调用成员函数指针
        qWarning() << "HistoryManager::" << operationName << "失败:" << command->description();
        // 操作失败，将命令放回源栈
        sourceStack.push_back(std::move(command));
        return false;
    }

    qDebug() << "HistoryManager:" << operationName << "命令 -" << command->description();

    // 将命令移到目标栈
    targetStack.push_back(std::move(command));

    // 发射历史改变信号
    emit historyChanged();

    return true;
}

bool HistoryManager::canUndo() const { return !m_undoStack.empty(); }

bool HistoryManager::canRedo() const { return !m_redoStack.empty(); }

void HistoryManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit historyChanged();
    qDebug() << "HistoryManager: 历史记录已清空";
}

void HistoryManager::setHistoryLimit(int limit)
{
    if (limit <= 0) {
        qWarning() << "HistoryManager::setHistoryLimit: 限制值必须大于0，使用默认值50";
        m_historyLimit = 50;
        return;
    }

    m_historyLimit = limit;
    enforceHistoryLimit();
    qDebug() << "HistoryManager: 历史记录限制设置为" << m_historyLimit;
}

int HistoryManager::historyLimit() const { return m_historyLimit; }

void HistoryManager::enforceHistoryLimit()
{
    // 如果撤销栈超过限制，移除最旧的命令（使用 deque 的 O(1) pop_front）
    while (static_cast<int>(m_undoStack.size()) > m_historyLimit) {
        m_undoStack.pop_front();
    }
}

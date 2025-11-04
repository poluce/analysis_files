#include "history_manager.h"
#include <QDebug>

HistoryManager::HistoryManager(QObject* parent)
    : QObject(parent)
    , m_historyLimit(50) // 默认历史记录深度为50
{
    qDebug() << "构造:  HistoryManager";
}

HistoryManager::~HistoryManager() { clear(); }

HistoryManager& HistoryManager::instance(QObject* parent)
{
    static HistoryManager instance(parent);
    return instance;
}

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
    if (!canUndo()) {
        qDebug() << "HistoryManager::undo: 撤销栈为空";
        return false;
    }

    // 从撤销栈弹出命令
    std::unique_ptr<ICommand> command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // 执行撤销操作
    if (!command->undo()) {
        qWarning() << "HistoryManager::undo: 撤销失败:" << command->description();
        // 撤销失败，将命令放回撤销栈
        m_undoStack.push_back(std::move(command));
        return false;
    }

    qDebug() << "HistoryManager: 撤销命令 -" << command->description();

    // 将命令移到重做栈
    m_redoStack.push_back(std::move(command));

    // 发射历史改变信号
    emit historyChanged();

    return true;
}

bool HistoryManager::redo()
{
    if (!canRedo()) {
        qDebug() << "HistoryManager::redo: 重做栈为空";
        return false;
    }

    // 从重做栈弹出命令
    std::unique_ptr<ICommand> command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // 执行重做操作
    if (!command->redo()) {
        qWarning() << "HistoryManager::redo: 重做失败:" << command->description();
        // 重做失败，将命令放回重做栈
        m_redoStack.push_back(std::move(command));
        return false;
    }

    qDebug() << "HistoryManager: 重做命令 -" << command->description();

    // 将命令移回撤销栈
    m_undoStack.push_back(std::move(command));

    // 发射历史改变信号
    emit historyChanged();

    return true;
}

bool HistoryManager::canUndo() const { return !m_undoStack.empty(); }

bool HistoryManager::canRedo() const { return !m_redoStack.empty(); }

int HistoryManager::undoCount() const { return m_undoStack.size(); }

int HistoryManager::redoCount() const { return m_redoStack.size(); }

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
    // 如果撤销栈超过限制，移除最旧的命令（从vector开头移除）
    while (static_cast<int>(m_undoStack.size()) > m_historyLimit) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

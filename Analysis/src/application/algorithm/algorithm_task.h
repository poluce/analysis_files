#ifndef ALGORITHM_TASK_H
#define ALGORITHM_TASK_H

#include <QString>
#include <QUuid>
#include <QDateTime>
#include <QSharedPointer>

class AlgorithmContext;

/**
 * @brief 算法执行任务封装
 *
 * 封装一次算法执行的所有信息，包括算法名称、上下文快照、任务ID等。
 * 使用 QSharedPointer 管理生命周期，避免双重释放问题。
 *
 * 设计要点：
 * - 任务独占上下文快照（m_contextSnapshot），析构时自动清理
 * - 使用 QUuid 生成全局唯一的任务ID
 * - 支持取消标志（m_isCancelled）
 * - 记录创建时间戳用于调试和监控
 */
class AlgorithmTask {
public:
    /**
     * @brief 构造函数
     * @param algorithmName 算法名称（如 "differentiation"）
     * @param contextSnapshot 上下文快照（任务独占所有权）
     */
    AlgorithmTask(const QString& algorithmName, AlgorithmContext* contextSnapshot);

    /**
     * @brief 析构函数 - 自动清理上下文快照
     */
    ~AlgorithmTask();

    // 禁用拷贝构造和赋值（使用 QSharedPointer 管理）
    AlgorithmTask(const AlgorithmTask&) = delete;
    AlgorithmTask& operator=(const AlgorithmTask&) = delete;

    /**
     * @brief 获取任务唯一标识符
     */
    QString taskId() const { return m_taskId; }

    /**
     * @brief 获取算法名称
     */
    QString algorithmName() const { return m_algorithmName; }

    /**
     * @brief 获取上下文快照
     * @return 上下文指针（只读访问，不要修改或删除）
     */
    AlgorithmContext* context() const { return m_contextSnapshot; }

    /**
     * @brief 检查任务是否被取消
     */
    bool isCancelled() const { return m_isCancelled; }

    /**
     * @brief 标记任务为已取消
     */
    void cancel() { m_isCancelled = true; }

    /**
     * @brief 获取任务创建时间
     */
    QDateTime createdAt() const { return m_createdAt; }

private:
    QString m_taskId;                    ///< 任务唯一ID（UUID）
    QString m_algorithmName;             ///< 算法名称
    AlgorithmContext* m_contextSnapshot; ///< 上下文快照（独占所有权）
    QDateTime m_createdAt;               ///< 创建时间戳
    bool m_isCancelled;                  ///< 取消标志
};

/// 智能指针类型别名
using AlgorithmTaskPtr = QSharedPointer<AlgorithmTask>;

#endif // ALGORITHM_TASK_H

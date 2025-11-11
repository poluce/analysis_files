#ifndef ALGORITHM_WORKER_H
#define ALGORITHM_WORKER_H

#include <QObject>
#include <QVariant>
#include <QElapsedTimer>
#include "algorithm_task.h"
#include "../../domain/algorithm/i_progress_reporter.h"

class IThermalAlgorithm;

/**
 * @brief 算法工作线程执行器
 *
 * 在独立线程中执行算法任务，实现 IProgressReporter 接口以支持进度报告和取消功能。
 *
 * 设计要点：
 * - 必须通过 moveToThread() 移动到工作线程
 * - 通过 QMetaObject::invokeMethod 异步调用 executeTask()
 * - 实现 IProgressReporter 接口，算法通过此接口报告进度
 * - 使用 QSharedPointer<AlgorithmTask> 管理任务生命周期
 * - 支持任务取消（通过 requestCancellation 槽函数）
 *
 * 信号流程：
 * 1. taskStarted(taskId, algorithmName) - 任务开始执行
 * 2. taskProgress(taskId, percentage, message) - 进度更新
 * 3. taskFinished(taskId, result, elapsedMs) - 任务成功完成
 * 4. taskFailed(taskId, errorMessage) - 任务失败
 */
class AlgorithmWorker : public QObject, public IProgressReporter {
    Q_OBJECT

public:
    explicit AlgorithmWorker(QObject* parent = nullptr);
    ~AlgorithmWorker() override;

    // IProgressReporter 接口实现
    void reportProgress(int percentage, const QString& message = QString()) override;
    bool shouldCancel() const override;

public slots:
    /**
     * @brief 执行算法任务（必须在工作线程中调用）
     * @param task 任务对象（使用 QSharedPointer）
     * @param algorithm 算法实例（注意：从主线程共享，只读访问）
     *
     * 执行流程：
     * 1. 验证参数有效性
     * 2. 设置 ProgressReporter（algorithm->setProgressReporter(this)）
     * 3. 调用 algorithm->executeWithContext(task->context())
     * 4. 清理 ProgressReporter
     * 5. 发出 taskFinished 或 taskFailed 信号
     *
     * 注意：不调用 prepareContext()，该方法已在主线程调用
     */
    void executeTask(AlgorithmTaskPtr task, IThermalAlgorithm* algorithm);

    /**
     * @brief 请求取消当前任务
     *
     * 设置取消标志，算法应通过 shouldCancel() 检查并尽快停止。
     */
    void requestCancellation();

signals:
    /**
     * @brief 任务开始执行
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     */
    void taskStarted(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 任务进度更新
     * @param taskId 任务ID
     * @param percentage 进度百分比 (0-100)
     * @param message 状态消息
     */
    void taskProgress(const QString& taskId, int percentage, const QString& message);

    /**
     * @brief 任务成功完成
     * @param taskId 任务ID
     * @param result 算法执行结果（QVariant 封装）
     * @param elapsedMs 执行耗时（毫秒）
     */
    void taskFinished(const QString& taskId, const QVariant& result, qint64 elapsedMs);

    /**
     * @brief 任务执行失败
     * @param taskId 任务ID
     * @param errorMessage 错误信息
     */
    void taskFailed(const QString& taskId, const QString& errorMessage);

private:
    AlgorithmTaskPtr m_currentTask;   ///< 当前执行的任务
    bool m_cancellationRequested;     ///< 取消标志
};

#endif // ALGORITHM_WORKER_H

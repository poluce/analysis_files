#include "algorithm_worker.h"
#include "../../domain/algorithm/i_thermal_algorithm.h"
#include <QDebug>
#include <QThread>
#include <exception>

AlgorithmWorker::AlgorithmWorker(QObject* parent)
    : QObject(parent)
    , m_currentTask(nullptr)
    , m_cancellationRequested(false)
{
    qDebug() << "[AlgorithmWorker] Created worker" << this;
}

AlgorithmWorker::~AlgorithmWorker()
{
    qDebug() << "[AlgorithmWorker] Destroying worker" << this;
}

void AlgorithmWorker::executeTask(AlgorithmTaskPtr task, IThermalAlgorithm* algorithm)
{
    // 1. 验证参数
    if (!task) {
        qWarning() << "[AlgorithmWorker] executeTask called with null task";
        emit taskFailed("", "Invalid task: null pointer");
        return;
    }

    if (!algorithm) {
        qWarning() << "[AlgorithmWorker] executeTask called with null algorithm";
        emit taskFailed(task->taskId(), "Invalid algorithm: null pointer");
        return;
    }

    // 2. 初始化任务状态
    m_currentTask = task;
    m_cancellationRequested = false;

    QString taskId = task->taskId();
    QString algorithmName = task->algorithmName();

    qDebug() << "[AlgorithmWorker] Starting task" << taskId
             << "algorithm:" << algorithmName
             << "thread:" << QThread::currentThread();

    emit taskStarted(taskId, algorithmName);

    // 3. 开始计时
    QElapsedTimer timer;
    timer.start();

    try {
        // 4. 设置进度报告器（关键步骤）
        algorithm->setProgressReporter(this);

        // 5. 执行算法（注意：不调用 prepareContext，已在主线程调用）
        AlgorithmResult result = algorithm->executeWithContext(task->context());

        // 6. 清理进度报告器
        algorithm->setProgressReporter(nullptr);

        // 7. 检查是否被取消
        if (shouldCancel()) {
            qWarning() << "[AlgorithmWorker] Task" << taskId << "was cancelled during execution";
            emit taskFailed(taskId, "Task cancelled during execution");
            m_currentTask.clear();
            return;
        }

        // 8. 报告成功（使用 QVariant::fromValue 包装结果）
        qint64 elapsed = timer.elapsed();
        qDebug() << "[AlgorithmWorker] Task" << taskId << "finished successfully in"
                 << elapsed << "ms";
        emit taskFinished(taskId, QVariant::fromValue(result), elapsed);

    } catch (const std::exception& e) {
        // 异常处理：清理进度报告器并报告失败
        algorithm->setProgressReporter(nullptr);

        QString errorMsg = QString("Exception during execution: %1").arg(e.what());
        qCritical() << "[AlgorithmWorker] Task" << taskId << "failed:" << errorMsg;
        emit taskFailed(taskId, errorMsg);

    } catch (...) {
        // 捕获所有未知异常
        algorithm->setProgressReporter(nullptr);

        QString errorMsg = "Unknown exception during execution";
        qCritical() << "[AlgorithmWorker] Task" << taskId << "failed:" << errorMsg;
        emit taskFailed(taskId, errorMsg);
    }

    // 9. 清理任务引用
    m_currentTask.clear();
}

void AlgorithmWorker::requestCancellation()
{
    m_cancellationRequested = true;

    if (m_currentTask) {
        qDebug() << "[AlgorithmWorker] Cancellation requested for task"
                 << m_currentTask->taskId();
        m_currentTask->cancel();
    } else {
        qDebug() << "[AlgorithmWorker] Cancellation requested but no active task";
    }
}

// IProgressReporter 接口实现

void AlgorithmWorker::reportProgress(int percentage, const QString& message)
{
    if (!m_currentTask) {
        qWarning() << "[AlgorithmWorker] reportProgress called without active task";
        return;
    }

    QString taskId = m_currentTask->taskId();

    // 发出进度信号（通过 Qt 信号槽机制线程安全传递到主线程）
    emit taskProgress(taskId, percentage, message);

    qDebug() << "[AlgorithmWorker] Task" << taskId << "progress:"
             << percentage << "%" << message;
}

bool AlgorithmWorker::shouldCancel() const
{
    // 检查取消标志或任务取消标志
    if (m_cancellationRequested) {
        return true;
    }

    if (m_currentTask && m_currentTask->isCancelled()) {
        return true;
    }

    return false;
}

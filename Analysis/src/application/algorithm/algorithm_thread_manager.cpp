#include "algorithm_thread_manager.h"
#include "algorithm_worker.h"
#include <QDebug>
#include <QThread>

// 单例实例
static AlgorithmThreadManager* s_instance = nullptr;

AlgorithmThreadManager* AlgorithmThreadManager::instance()
{
    if (!s_instance) {
        s_instance = new AlgorithmThreadManager();
    }
    return s_instance;
}

AlgorithmThreadManager::AlgorithmThreadManager(QObject* parent)
    : QObject(parent)
    , m_maxThreads(1)  // 默认单线程模式（v1.2 设计）
{
    qDebug() << "[ThreadManager] Initialized with maxThreads:" << m_maxThreads
             << "(single-threaded async mode, idealThreadCount:"
             << QThread::idealThreadCount() << ")";
}

AlgorithmThreadManager::~AlgorithmThreadManager()
{
    qDebug() << "[ThreadManager] Shutting down, cleaning up" << m_workers.size() << "threads";

    // 清理所有工作线程
    for (WorkerInfo& info : m_workers) {
        // 1. 停止线程事件循环
        info.thread->quit();

        // 2. 等待线程结束（最多 5 秒）
        if (!info.thread->wait(5000)) {
            qWarning() << "[ThreadManager] Thread" << info.thread << "failed to quit gracefully, terminating";
            info.thread->terminate();
            info.thread->wait();
        }

        // 3. 删除工作对象和线程对象
        delete info.worker;
        delete info.thread;
    }

    m_workers.clear();
    qDebug() << "[ThreadManager] Shutdown complete";
}

void AlgorithmThreadManager::setMaxThreads(int maxThreads)
{
    if (maxThreads < 1) {
        qWarning() << "[ThreadManager] Invalid maxThreads:" << maxThreads << ", using 1";
        maxThreads = 1;
    }

    if (!m_workers.isEmpty()) {
        qWarning() << "[ThreadManager] setMaxThreads called after threads created, will not take effect for existing threads";
    }

    m_maxThreads = maxThreads;
    qDebug() << "[ThreadManager] maxThreads set to" << m_maxThreads;
}

int AlgorithmThreadManager::activeThreadCount() const
{
    int count = 0;
    for (const WorkerInfo& info : m_workers) {
        if (info.isBusy) {
            ++count;
        }
    }
    return count;
}

QPair<AlgorithmWorker*, QThread*> AlgorithmThreadManager::acquireWorker()
{
    // 1. 查找空闲的工作线程
    for (WorkerInfo& info : m_workers) {
        if (!info.isBusy) {
            info.isBusy = true;
            qDebug() << "[ThreadManager] Acquired idle worker" << info.worker
                     << "thread:" << info.thread
                     << "active:" << activeThreadCount() << "/" << m_workers.size();
            return {info.worker, info.thread};
        }
    }

    // 2. 如果没有空闲线程，检查是否可以创建新线程
    if (m_workers.size() < m_maxThreads) {
        // 创建新线程
        QThread* thread = new QThread(this);
        AlgorithmWorker* worker = new AlgorithmWorker();

        // 将工作对象移动到新线程
        worker->moveToThread(thread);

        // 启动线程
        thread->start();

        // 添加到线程池
        WorkerInfo info{thread, worker, true};
        m_workers.append(info);

        qDebug() << "[ThreadManager] Created new worker" << worker
                 << "thread:" << thread
                 << "total threads:" << m_workers.size() << "/" << m_maxThreads;

        return {worker, thread};
    }

    // 3. 所有线程都忙且已达上限，返回 nullptr
    qDebug() << "[ThreadManager] All workers busy" << activeThreadCount() << "/" << m_workers.size()
             << ", task should be queued";
    return {nullptr, nullptr};
}

void AlgorithmThreadManager::releaseWorker(AlgorithmWorker* worker)
{
    if (!worker) {
        qWarning() << "[ThreadManager] releaseWorker called with null pointer";
        return;
    }

    // 查找并释放工作线程
    for (WorkerInfo& info : m_workers) {
        if (info.worker == worker) {
            if (!info.isBusy) {
                qWarning() << "[ThreadManager] Worker" << worker << "already idle, double release?";
                return;
            }

            info.isBusy = false;
            qDebug() << "[ThreadManager] Released worker" << worker
                     << "active:" << activeThreadCount() << "/" << m_workers.size();

            // 发出信号，通知可以处理队列中的任务
            emit workerReleased();
            return;
        }
    }

    qWarning() << "[ThreadManager] releaseWorker called with unknown worker" << worker;
}

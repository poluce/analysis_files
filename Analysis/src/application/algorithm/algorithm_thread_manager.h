#ifndef ALGORITHM_THREAD_MANAGER_H
#define ALGORITHM_THREAD_MANAGER_H

#include <QObject>
#include <QThread>
#include <QVector>

class AlgorithmWorker;

/**
 * @brief 算法线程池管理器
 *
 * 管理工作线程池，负责创建、分配和回收工作线程。
 *
 * 设计要点：
 * - 通过 ApplicationContext 管理生命周期（依赖注入）
 * - 默认单线程模式（maxThreads = 1），适用于用户交互驱动的低并发场景
 * - 支持配置最大线程数（通过 setMaxThreads）
 * - 工作线程按需创建，达到上限后等待空闲线程
 * - 析构时自动清理所有线程（quit + wait）
 *
 * 单线程模式优势：
 * - FIFO 执行顺序，简单直观
 * - 避免资源竞争和死锁
 * - 降低调试复杂度
 * - 适合峰面积计算等单个慢速算法场景
 */
class AlgorithmThreadManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit AlgorithmThreadManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 自动清理所有工作线程（quit + wait）
     */
    ~AlgorithmThreadManager() override;

    /**
     * @brief 设置最大线程数
     * @param maxThreads 最大线程数（默认 1）
     *
     * 注意：只能在启动时调用，运行时修改无效（已创建的线程不会被销毁）。
     */
    void setMaxThreads(int maxThreads);

    /**
     * @brief 获取当前最大线程数
     */
    int maxThreads() const { return m_maxThreads; }

    /**
     * @brief 获取当前活跃线程数
     */
    int activeThreadCount() const;

    /**
     * @brief 获取总线程数（包括空闲线程）
     */
    int totalThreadCount() const { return m_workers.size(); }

    /**
     * @brief 获取一个可用的工作线程
     * @return QPair<worker, thread>，如果所有线程都忙返回 {nullptr, nullptr}
     *
     * 分配策略：
     * 1. 优先返回空闲的已有线程
     * 2. 如果没有空闲线程且未达上限，创建新线程
     * 3. 如果已达上限，返回 nullptr（调用者应排队等待）
     */
    QPair<AlgorithmWorker*, QThread*> acquireWorker();

    /**
     * @brief 释放工作线程，标记为空闲状态
     * @param worker 要释放的工作线程
     */
    void releaseWorker(AlgorithmWorker* worker);

signals:
    /**
     * @brief 工作线程被释放，可以处理队列中的任务
     */
    void workerReleased();

private:

    // 禁用拷贝
    AlgorithmThreadManager(const AlgorithmThreadManager&) = delete;
    AlgorithmThreadManager& operator=(const AlgorithmThreadManager&) = delete;

    /**
     * @brief 工作线程信息
     */
    struct WorkerInfo {
        QThread* thread;           ///< 线程对象
        AlgorithmWorker* worker;   ///< 工作对象
        bool isBusy;               ///< 是否正在执行任务
    };

    QVector<WorkerInfo> m_workers;  ///< 工作线程池
    int m_maxThreads;               ///< 最大线程数
};

#endif // ALGORITHM_THREAD_MANAGER_H

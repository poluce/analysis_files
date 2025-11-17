#ifndef ALGORITHMANAGER_H
#define ALGORITHMANAGER_H

#include "domain/algorithm/i_thermal_algorithm.h"
#include "algorithm_task.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QQueue>
#include <QSet>

// 前置声明
class ThermalCurve;
class CurveManager;
class AlgorithmContext;
class AlgorithmWorker;
class AlgorithmThreadManager;

/**
 * @brief 算法服务类 - 管理算法注册和执行
 *
 * 纯上下文驱动设计：所有算法通过 executeWithContext() 执行。
 *
 * 设计要点：
 * - 通过 ApplicationContext 管理生命周期（依赖注入）
 * - 所有依赖（ThreadManager、HistoryManager）通过构造函数或 setter 注入
 * - 支持同步执行（executeWithContext）和异步执行（executeAsync）
 */
class AlgorithmManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造函数（依赖注入）
     * @param threadManager 线程管理器实例（必需）
     * @param parent 父对象
     */
    explicit AlgorithmManager(AlgorithmThreadManager* threadManager,
                             QObject* parent = nullptr);

    ~AlgorithmManager();

    void setCurveManager(CurveManager* manager);
    void registerAlgorithm(IThermalAlgorithm* algorithm);
    IThermalAlgorithm* getAlgorithm(const QString& name);

    /**
     * @brief 上下文驱动的算法执行（同步，在主线程执行）
     *
     * 算法从上下文中拉取所需数据，执行后返回结果。
     *
     * @param name 算法名称
     * @param context 算法上下文（包含曲线、参数、选择的点等）
     */
    void executeWithContext(const QString& name, AlgorithmContext* context);

    /**
     * @brief 异步执行算法（在工作线程中执行）
     *
     * 创建上下文快照，提交任务到线程池执行。
     *
     * 执行流程：
     * 1. 验证算法和上下文有效性
     * 2. 调用 prepareContext() 验证数据完整性
     * 3. 创建上下文快照（context->clone()）
     * 4. 创建任务并尝试分配工作线程
     * 5. 如果所有线程忙，则加入队列等待
     *
     * @param name 算法名称
     * @param context 算法上下文（将被克隆）
     * @return 任务ID（UUID），用于跟踪和取消任务；失败返回空字符串
     */
    QString executeAsync(const QString& name, AlgorithmContext* context);

    /**
     * @brief 取消正在执行或排队的任务
     *
     * @param taskId 任务ID
     * @return true 如果任务被成功取消，false 表示任务不存在
     */
    bool cancelTask(const QString& taskId);

signals:
    /**
     * @brief 算法执行完成信号
     *
     * @param algorithmName 算法名称
     * @param result 结构化的执行结果（AlgorithmResult）
     */
    void algorithmResultReady(const QString& algorithmName, const AlgorithmResult& result);

    /**
     * @brief 算法执行失败信号
     *
     * @param algorithmName 算法名称
     * @param errorMessage 错误信息
     */
    void algorithmExecutionFailed(const QString& algorithmName, const QString& errorMessage);

    /**
     * @brief 算法生成标注点信号
     *
     * @param curveId 关联的曲线ID
     * @param markers 标注点列表（数据坐标）
     * @param color 标注点颜色
     */
    void markersGenerated(const QString& curveId, const QList<QPointF>& markers, const QColor& color);

    // ==================== 异步执行信号 ====================

    /**
     * @brief 任务已加入队列
     *
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     */
    void algorithmQueued(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 任务开始执行
     *
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     */
    void algorithmStarted(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 任务进度更新
     *
     * @param taskId 任务ID
     * @param percentage 进度百分比 (0-100)
     * @param message 状态消息
     */
    void algorithmProgress(const QString& taskId, int percentage, const QString& message);

    /**
     * @brief 任务执行完成
     *
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     * @param result 执行结果
     * @param elapsedMs 执行耗时（毫秒）
     */
    void algorithmFinished(const QString& taskId, const QString& algorithmName,
                          const AlgorithmResult& result, qint64 elapsedMs);

    /**
     * @brief 任务执行失败
     *
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     * @param errorMessage 错误信息
     */
    void algorithmFailed(const QString& taskId, const QString& algorithmName,
                        const QString& errorMessage);

    /**
     * @brief 任务被取消
     *
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     */
    void algorithmCancelled(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 队列中的任务数量变化
     *
     * @param count 当前队列长度
     */
    void queuedTaskCountChanged(int count);

private:
    // 禁用拷贝
    AlgorithmManager(const AlgorithmManager&) = delete;
    AlgorithmManager& operator=(const AlgorithmManager&) = delete;

    // 根据结果类型处理算法结果（分发器）
    void handleAlgorithmResult(const AlgorithmResult& result);

    // 各类型结果处理方法（单一职责）
    void handleCurveResult(const AlgorithmResult& result);
    void handleMarkerResult(const AlgorithmResult& result);
    void handleRegionResult(const AlgorithmResult& result);
    void handleScalarResult(const AlgorithmResult& result);
    void handleCompositeResult(const AlgorithmResult& result);

    // 添加曲线（使用历史管理）
    void addCurveWithHistory(const ThermalCurve& curve);

    // ==================== 异步执行私有方法 ====================

    /**
     * @brief 验证异步执行的前置条件
     *
     * 验证算法、上下文、CurveManager、数据完整性。
     *
     * @param name 算法名称
     * @param context 算法上下文
     * @return 算法实例指针，验证失败返回 nullptr
     */
    IThermalAlgorithm* validateAsyncExecution(const QString& name, AlgorithmContext* context);

    /**
     * @brief 提交任务到工作线程执行
     *
     * 设置信号连接并调用 worker->executeTask()。
     *
     * @param task 任务对象
     * @param algorithm 算法实例
     * @param worker 工作线程
     */
    void submitTaskToWorker(AlgorithmTaskPtr task,
                           IThermalAlgorithm* algorithm,
                           AlgorithmWorker* worker);

    /**
     * @brief 处理队列中的下一个任务
     *
     * 当工作线程释放时调用，尝试从队列中取出任务执行。
     */
    void processQueue();

    /**
     * @brief 获取任务的算法名称
     *
     * @param taskId 任务ID
     * @return 算法名称，任务不存在返回空字符串
     */
    QString getTaskAlgorithmName(const QString& taskId);

    /**
     * @brief 清理任务（释放工作线程并移除任务记录）
     *
     * @param taskId 任务ID
     */
    void cleanupTask(const QString& taskId);

private slots:
    /**
     * @brief 工作线程任务开始槽函数
     */
    void onWorkerStarted(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 工作线程进度更新槽函数
     */
    void onWorkerProgress(const QString& taskId, int percentage, const QString& message);

    /**
     * @brief 工作线程任务完成槽函数
     */
    void onWorkerFinished(const QString& taskId, const QVariant& result, qint64 elapsedMs);

    /**
     * @brief 工作线程任务失败槽函数
     */
    void onWorkerFailed(const QString& taskId, const QString& errorMessage);

private:
    // ==================== 同步执行成员 ====================
    QMap<QString, IThermalAlgorithm*> m_algorithms;
    CurveManager* m_curveManager = nullptr;
    class HistoryManager* m_historyManager = nullptr;  // 历史管理器（可选）
    AlgorithmThreadManager* m_threadManager = nullptr; // 线程管理器（非拥有指针）

    // ==================== 异步执行成员 ====================

    /**
     * @brief 排队任务结构
     */
    struct QueuedTask {
        AlgorithmTaskPtr task;           ///< 任务对象
        IThermalAlgorithm* algorithm;    ///< 算法实例
        QString algorithmName;           ///< 算法名称（冗余，便于调试）
    };

    QQueue<QueuedTask> m_taskQueue;                    ///< 任务队列（FIFO）
    QMap<QString, AlgorithmTaskPtr> m_activeTasks;     ///< 活跃任务映射（taskId -> task）
    QMap<QString, AlgorithmWorker*> m_taskWorkers;     ///< 任务-工作线程映射（taskId -> worker）
    QSet<AlgorithmWorker*> m_connectedWorkers;         ///< 已连接信号的工作线程集合

public:
    void setHistoryManager(class HistoryManager* manager) { m_historyManager = manager; }
};

#endif // ALGORITHMANAGER_H

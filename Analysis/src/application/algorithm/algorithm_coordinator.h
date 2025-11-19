#ifndef APPLICATION_ALGORITHM_COORDINATOR_H
#define APPLICATION_ALGORITHM_COORDINATOR_H

#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/model/thermal_data_point.h"
#include <QObject>
#include "domain/algorithm/i_thermal_algorithm.h"
#include <QVector>
#include <QPointF>
#include <QVariantMap>
#include <optional>

class AlgorithmContext;
class AlgorithmManager;
class CurveManager;
class ThermalCurve;
class AlgorithmResult;

/**
 * @brief AlgorithmCoordinator
 *
 * 负责调度算法执行流程，根据算法描述触发参数收集、点选提示，
 * 并在上下文中记录输入输出。
 */
class AlgorithmCoordinator : public QObject {
    Q_OBJECT

public:
    explicit AlgorithmCoordinator(AlgorithmManager* manager, CurveManager* curveManager, AlgorithmContext* context, QObject* parent = nullptr);

    /**
     * @brief 处理点选结果（元数据驱动路径）
     * @param points 用户选择的点集合
     *
     * 验证点数量是否满足要求，然后执行算法。
     */
    void handlePointSelectionResult(const QVector<ThermalDataPoint>& points);

    /**
     * @brief 取消待处理请求
     *
     * 取消当前所有待处理的操作：
     * - 待处理的交互请求（参数收集、点选等）
     * - 正在执行的异步任务
     */
    void cancelPendingRequest();

    /**
     * @brief 算法执行的唯一入口
     * @param algorithmName 算法名称
     *
     * 根据算法的自描述信息（descriptor()）自动编排执行流程：
     * - 无需交互 → 直接执行
     * - 需要参数 → requestParameterDialog → submitParameters → ...
     * - 需要点选 → requestPointSelection → submitPoints → ...
     * - 复合交互 → 按 interactionOrder 顺序执行
     */
    void run(const QString& algorithmName);

    /**
     * @brief 用户提交参数后调用
     * @param parameters 用户输入的参数
     *
     * 保存参数到待处理请求，然后继续下一个交互步骤
     */
    void submitParameters(const QVariantMap& parameters);

    /**
     * @brief 用户完成点选后调用
     * @param points 用户选择的点
     *
     * 验证点数量，保存到待处理请求，然后继续下一个交互步骤
     */
    void submitPoints(const QVector<ThermalDataPoint>& points);

    /**
     * @brief 取消当前算法执行
     *
     * 清空待处理请求和正在执行的任务
     */
    void cancel();

    // ========== 工作流 API ==========

    /**
     * @brief 工作流状态枚举
     */
    enum class WorkflowStatus {
        Idle,       // 空闲
        Running,    // 运行中
        Completed,  // 已完成
        Failed,     // 失败
        Cancelled   // 已取消
    };

    /**
     * @brief 运行线性工作流（自动链式执行多个算法）
     * @param steps 算法步骤列表（按顺序执行）
     * @param curveIds 初始输入曲线ID列表
     * @return 工作流实例ID
     *
     * 使用示例:
     * @code
     * QString wfId = coordinator->runWorkflow(
     *     {"movingAverage", "differentiation", "peakDetection"},
     *     {"curve-001"}
     * );
     * @endcode
     */
    QString runWorkflow(const QStringList& steps, const QStringList& curveIds);

    /**
     * @brief 取消工作流
     * @param workflowId 工作流实例ID
     */
    void cancelWorkflow(const QString& workflowId);

signals:
    /**
     * @brief 请求弹出参数对话框
     * @param algorithmName 算法名称
     * @param descriptor 算法描述符（包含完整的参数定义）
     *
     * MainController 接收此信号后，根据 descriptor.parameters 动态创建
     * QDialog + QFormLayout 对话框，用户提交后调用 submitParameters()
     */
    void requestParameterDialog(const QString& algorithmName, const AlgorithmDescriptor& descriptor);

    /**
     * @brief 请求用户在图表上选点
     * @param algorithmName 算法名称
     * @param requiredPoints 所需点数
     * @param hint 提示信息（如"请选择基线起点和终点"）
     *
     * MainController 接收此信号后，通知 ChartView 进入点选模式，
     * 用户完成选点后调用 submitPoints()
     */
    void requestPointSelection(const QString& algorithmName, int requiredPoints, const QString& hint);

    /**
     * @brief 显示消息（用于状态提示）
     * @param text 消息文本
     */
    void showMessage(const QString& text);

    /**
     * @brief 算法执行失败
     * @param algorithmName 算法名称
     * @param reason 失败原因
     */
    void algorithmFailed(const QString& algorithmName, const QString& reason);

    /**
     * @brief 算法执行完成（携带完整结果信息）
     * @param taskId 任务ID（全局唯一）
     * @param algorithmName 算法名称
     * @param parentCurveId 来源曲线ID
     * @param result 算法执行结果
     *
     * 这是唯一的算法完成信号，替代旧的 algorithmSucceeded。
     * 携带完整的结果信息，避免下游必须访问 Context 内部实现。
     */
    void algorithmCompleted(const QString& taskId,
                           const QString& algorithmName,
                           const QString& parentCurveId,
                           const AlgorithmResult& result);

    /**
     * @brief 工作流执行完成
     * @param workflowId 工作流实例ID
     * @param outputCurveIds 最终输出曲线ID列表
     */
    void workflowCompleted(const QString& workflowId, const QStringList& outputCurveIds);

    /**
     * @brief 工作流执行失败
     * @param workflowId 工作流实例ID
     * @param errorMessage 错误信息
     */
    void workflowFailed(const QString& workflowId, const QString& errorMessage);

    /**
     * @brief 算法开始执行（异步模式）
     * @param taskId 任务ID
     * @param algorithmName 算法名称
     */
    void algorithmStarted(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 算法执行进度更新（异步模式）
     * @param taskId 任务ID
     * @param percentage 进度百分比 (0-100)
     * @param message 状态消息
     */
    void algorithmProgress(const QString& taskId, int percentage, const QString& message);

private slots:
    /**
     * @brief 同步算法执行完成
     * @param algorithmName 算法名称
     * @param result 算法执行结果
     *
     * 处理同步执行的算法结果：
     * 1. 生成 taskId
     * 2. 调用 context->saveResult() 保存结果
     * 3. 发射 algorithmCompleted 信号
     * 4. 调用 advanceWorkflow() 推进工作流（如有）
     */
    void onSyncAlgorithmResultReady(const QString& algorithmName, const AlgorithmResult& result);

    /**
     * @brief 异步任务开始执行
     */
    void onAsyncAlgorithmStarted(const QString& taskId, const QString& algorithmName);

    /**
     * @brief 异步任务进度更新
     */
    void onAsyncAlgorithmProgress(const QString& taskId, int percentage, const QString& message);

    /**
     * @brief 异步任务执行完成
     */
    void onAsyncAlgorithmFinished(const QString& taskId, const QString& algorithmName,
                                  const AlgorithmResult& result, qint64 elapsedMs);

    /**
     * @brief 异步任务执行失败
     */
    void onAsyncAlgorithmFailed(const QString& taskId, const QString& algorithmName,
                               const QString& errorMessage);

private:
    /**
     * @brief 待处理请求结构
     *
     * 保存算法执行过程中收集的数据：
     * - algorithmName: 算法名称
     * - descriptor: 算法完整自描述信息
     * - parameters: 已收集的参数
     * - points: 已收集的点
     * - currentStepIndex: 当前交互步骤索引
     */
    struct PendingRequest {
        QString algorithmName;
        AlgorithmDescriptor descriptor;
        QVariantMap parameters;
        QVector<ThermalDataPoint> points;
        int currentStepIndex = 0;
    };

    /**
     * @brief 处理下一个交互步骤
     *
     * 根据 descriptor.interactionOrder 或默认顺序执行交互流程：
     * 1. 检查当前步骤索引是否超出范围，如果是则调用 execute()
     * 2. 读取当前步骤类型（"parameters" 或 "points"）
     * 3. 发出对应的请求信号（requestParameterDialog 或 requestPointSelection）
     *
     * 无判断逻辑，完全依赖算法的自描述信息。
     */
    void processNextStep();

    /**
     * @brief 执行算法（所有交互完成后调用）
     *
     * 将收集的参数和点注入到 AlgorithmContext，然后提交到异步队列执行。
     */
    void execute();


    /**
     * @brief 重置所有状态（统一状态清理入口）
     *
     * 清理所有运行时状态，包括：
     * - 待处理请求 (m_pending)
     * - 当前任务ID (m_currentTaskId)
     *
     * 在以下场景调用：
     * - 取消操作
     * - 算法执行完成/失败
     * - 错误处理
     */
    void resetState();

    /**
     * @brief 获取算法描述符
     * @param algorithmName 算法名称
     * @return 如果算法存在返回描述符，否则返回 std::nullopt
     *
     * 从 AlgorithmManager 获取算法实例并调用 descriptor() 方法。
     * 自动填充缺失的元数据（交互类型、点选数量等）。
     */
    [[nodiscard]] std::optional<AlgorithmDescriptor> descriptorFor(const QString& algorithmName);

    /**
     * @brief 统一错误处理入口
     * @param algorithmName 算法名称
     * @param reason 错误原因描述
     *
     * 统一处理算法执行过程中的所有错误：
     * 1. 打印警告日志
     * 2. 清理所有状态（自动调用 resetState()）
     * 3. 发出失败信号通知 UI
     *
     * 使用场景：
     * - 前置条件检查失败
     * - 参数验证失败
     * - 算法执行失败
     */
    void handleError(const QString& algorithmName, const QString& reason);

    /**
     * @brief 推进工作流到下一步
     * @param taskId 当前完成的任务ID
     * @param algorithmName 当前完成的算法名称
     * @param result 当前算法的执行结果
     *
     * 工作流推进逻辑：
     * 1. 检查是否有活动工作流
     * 2. 验证算法名匹配当前步骤
     * 3. 记录当前步骤的输出曲线ID
     * 4. 推进到下一步或完成工作流
     */
    void advanceWorkflow(const QString& taskId,
                        const QString& algorithmName,
                        const AlgorithmResult& result);

    /**
     * @brief 检查算法依赖是否满足（工作流支持）
     * @param algorithmName 算法名称
     * @return 如果所有依赖满足返回 true，否则返回 false
     *
     * 校验逻辑：
     * 1. 获取算法描述符
     * 2. 检查 prerequisites 列表中的每个键
     * 3. 验证上下文中是否存在该键
     * 4. 缺失依赖时发出 showMessage 信号提示用户
     */
    [[nodiscard]] bool checkPrerequisites(const QString& algorithmName);

    // ========== 工作流结构 ==========

    /**
     * @brief 待处理工作流结构
     */
    struct PendingWorkflow {
        QString id;                         // 工作流实例ID
        QStringList steps;                  // 算法步骤列表
        int currentStepIndex = 0;           // 当前执行到第几步
        QStringList inputCurveIds;          // 初始输入曲线
        QHash<int, QStringList> stepOutputs; // 每步的输出曲线ID
        WorkflowStatus status = WorkflowStatus::Idle;
        QString errorMessage;
    };

    AlgorithmManager* m_algorithmManager = nullptr;
    CurveManager* m_curveManager = nullptr;
    AlgorithmContext* m_context = nullptr;

    std::optional<PendingRequest> m_pending;
    QString m_currentTaskId;
    std::optional<PendingWorkflow> m_currentWorkflow;  // 当前活动工作流
};

#endif // APPLICATION_ALGORITHM_COORDINATOR_H

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
     * @brief 算法执行的唯一入口（Phase 3 新架构）
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
     * @brief 用户提交参数后调用（Phase 3 新架构）
     * @param parameters 用户输入的参数
     *
     * 保存参数到待处理请求，然后继续下一个交互步骤
     */
    void submitParameters(const QVariantMap& parameters);

    /**
     * @brief 用户完成点选后调用（Phase 3 新架构）
     * @param points 用户选择的点
     *
     * 验证点数量，保存到待处理请求，然后继续下一个交互步骤
     */
    void submitPoints(const QVector<ThermalDataPoint>& points);

    /**
     * @brief 取消当前算法执行（Phase 3 新架构）
     *
     * 清空待处理请求和正在执行的任务
     */
    void cancel();

signals:
    /**
     * @brief 请求弹出参数对话框（Phase 3 新架构）
     * @param algorithmName 算法名称
     * @param descriptor 算法描述符（包含完整的参数定义）
     *
     * MainController 接收此信号后，根据 descriptor.parameters 动态创建
     * QDialog + QFormLayout 对话框，用户提交后调用 submitParameters()
     */
    void requestParameterDialog(const QString& algorithmName, const AlgorithmDescriptor& descriptor);

    /**
     * @brief 请求用户在图表上选点（Phase 3 新架构）
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
     * @brief 算法执行成功
     * @param algorithmName 算法名称
     */
    void algorithmSucceeded(const QString& algorithmName);

    // ==================== 异步执行信号（转发到 UI 层）====================

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
    void onAlgorithmResultReady(const QString& algorithmName, const AlgorithmResult& result);

    // ==================== 异步执行槽函数 ====================

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
     * @brief 简化的待处理请求结构（Phase 3 新架构）
     *
     * 相比旧的 MetadataPendingRequest，删除了复杂的状态机：
     * - 删除 PendingPhase 枚举
     * - 删除 curveId（从 CurveManager 获取活动曲线）
     * - 删除 needsPointSelection/requiredPointCount/pointSelectionHint（从 descriptor 获取）
     * - 添加 descriptor（算法完整自描述信息）
     * - 添加 currentStepIndex（当前交互步骤索引）
     */
    struct PendingRequest {
        QString algorithmName;                  // 算法名称
        AlgorithmDescriptor descriptor;         // 算法完整自描述信息
        QVariantMap parameters;                 // 已收集的参数
        QVector<ThermalDataPoint> points;       // 已收集的点
        int currentStepIndex = 0;               // 当前交互步骤索引
    };

    /**
     * @brief 处理下一个交互步骤（Phase 3 核心逻辑）
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
     * @brief 保存算法结果到上下文
     * @param algorithmName 算法名称
     * @param result 算法执行结果
     *
     * 将算法结果保存到上下文中，供后续查询和使用。
     * 保存两个键值对：
     * - latestResult(algorithmName): 完整的 AlgorithmResult 对象
     * - resultType(algorithmName): 结果类型（int）
     */
    void saveResultToContext(const QString& algorithmName, const AlgorithmResult& result);

    AlgorithmManager* m_algorithmManager = nullptr;
    CurveManager* m_curveManager = nullptr;
    AlgorithmContext* m_context = nullptr;

    // ==================== Phase 3 新架构状态 ====================
    std::optional<PendingRequest> m_pending;  ///< 当前待处理的算法请求（如果有）

    // ==================== 异步执行状态 ====================
    QString m_currentTaskId;  ///< 当前正在执行的异步任务ID（用于取消）
};

#endif // APPLICATION_ALGORITHM_COORDINATOR_H

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

    // ==================== 遗留路径（旧算法链路，仅供工具类使用）====================

    /**
     * @brief 处理算法触发请求（遗留路径）
     * @param algorithmName 算法名称
     * @param presetParameters 预设参数（可选）
     *
     * @deprecated 仅供峰面积等工具类使用，新算法应使用 runByName()
     *
     * 支持的交互类型（仅用于工具类）：
     * - None: 直接执行
     * - PointSelection: 请求用户选点（用于峰面积工具）
     *
     * 已废弃的交互类型（会报错）：
     * - ParameterDialog: 已被元数据驱动路径替代
     * - ParameterThenPoint: 已被元数据驱动路径替代
     */
    void handleAlgorithmTriggered(const QString& algorithmName, const QVariantMap& presetParameters = {});


    /**
     * @brief 处理点选结果
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
     * @brief 元数据驱动的算法执行入口（方案B）
     * @param algorithmName 算法名称
     *
     * 从 MetadataDescriptorRegistry 获取算法描述，
     * 根据描述驱动"参数窗→选点→执行"流程。
     *
     * 流程：
     * 1. 从注册表获取 App::AlgorithmDescriptor
     * 2. 如果需要参数，弹出 GenericAlgorithmDialog
     * 3. 如果需要选点，请求用户选点
     * 4. 执行算法
     */
    void runByName(const QString& algorithmName);

    /**
     * @brief 处理通用参数对话框提交结果（方案B）
     * @param algorithmName 算法名称
     * @param parameters 用户提交的参数
     *
     * 根据元数据描述决定下一步：
     * - 如果需要选点，进入选点阶段
     * - 否则直接执行算法
     */
    void handleGenericParameterSubmission(const QString& algorithmName, const QVariantMap& parameters);

signals:
    /**
     * @brief 请求用户在图表上选点
     * @param algorithmName 算法名称
     * @param curveId 目标曲线ID
     * @param requiredPoints 所需点数
     * @param hint 提示信息（如"请选择基线起点和终点"）
     */
    void requestPointSelection(
        const QString& algorithmName, const QString& curveId, int requiredPoints, const QString& hint);

    /**
     * @brief 请求弹出通用参数对话框（方案B）
     * @param algorithmName 算法名称
     * @param descriptor App::AlgorithmDescriptor 描述符
     *
     * 发出此信号后，UI 层应创建 GenericAlgorithmDialog 并显示。
     * 用户提交后调用 handleGenericParameterSubmission()。
     */
    void requestGenericParameterDialog(const QString& algorithmName, const QVariant& descriptor);

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
    enum class PendingPhase {
        None,
        AwaitParameters,
        AwaitPoints
    };

    struct PendingRequest {
        AlgorithmDescriptor descriptor;
        QString curveId;
        QVariantMap parameters;
        int pointsRequired = 0;
        QVector<ThermalDataPoint> collectedPoints;
        PendingPhase phase = PendingPhase::None;
    };

    // 元数据驱动流程的待处理请求（方案B）
    struct MetadataPendingRequest {
        QString algorithmName;
        QString curveId;
        QVariantMap parameters;
        bool needsPointSelection = false;
        int requiredPointCount = 0;
        QString pointSelectionHint;
        QVector<ThermalDataPoint> collectedPoints;
        PendingPhase phase = PendingPhase::None;
    };

    /**
     * @brief 检查算法前置条件是否满足
     * @param descriptor 算法描述符
     * @param curve 目标曲线（预留参数）
     * @return 如果所有前置条件都满足返回 true，否则返回 false
     *
     * 检查 descriptor.prerequisites 列出的所有上下文键是否存在。
     */
    bool ensurePrerequisites(const AlgorithmDescriptor& descriptor, ThermalCurve* curve);

    /**
     * @brief 填充默认参数
     * @param descriptor 算法描述符
     * @param parameters 参数映射表（输入/输出）
     * @return 如果所有必需参数都已就绪返回 true，否则返回 false
     *
     * 对于 parameters 中不存在的参数键：
     * - 如果 descriptor 提供了默认值，则填充
     * - 如果参数是必需的但无默认值，返回 false
     */
    bool populateDefaultParameters(const AlgorithmDescriptor& descriptor, QVariantMap& parameters) const;

    /**
     * @brief 执行算法（核心方法）
     * @param descriptor 算法描述符
     * @param curve 目标曲线
     * @param parameters 算法参数
     * @param points 用户选择的点（可选）
     *
     * 执行流程：
     * 1. 验证输入有效性
     * 2. 注入数据到上下文（曲线、参数、选点）
     * 3. 调用 AlgorithmManager::executeAsync() 提交任务
     * 4. 保存任务ID
     */
    void executeAlgorithm(const AlgorithmDescriptor& descriptor, ThermalCurve* curve, const QVariantMap& parameters, const QVector<ThermalDataPoint>& points);

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
    std::optional<PendingRequest> m_pending;

    // ==================== 元数据驱动流程状态（方案B）====================
    std::optional<MetadataPendingRequest> m_metadataPending;

    // ==================== 异步执行状态 ====================
    QString m_currentTaskId;  ///< 当前正在执行的异步任务ID（用于取消）
};

#endif // APPLICATION_ALGORITHM_COORDINATOR_H

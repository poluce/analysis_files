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

    void handleAlgorithmTriggered(const QString& algorithmName, const QVariantMap& presetParameters = {});
    void handleParameterSubmission(const QString& algorithmName, const QVariantMap& parameters);
    void handlePointSelectionResult(const QVector<ThermalDataPoint>& points);
    void cancelPendingRequest();

signals:
    void requestParameterDialog(
        const QString& algorithmName, const QList<AlgorithmParameterDefinition>& parameters, const QVariantMap& initialValues);
    void requestPointSelection(
        const QString& algorithmName, const QString& curveId, int requiredPoints, const QString& hint);
    void showMessage(const QString& text);
    void algorithmFailed(const QString& algorithmName, const QString& reason);
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

    bool ensurePrerequisites(const AlgorithmDescriptor& descriptor, ThermalCurve* curve);
    bool populateDefaultParameters(const AlgorithmDescriptor& descriptor, QVariantMap& parameters) const;
    void executeAlgorithm(const AlgorithmDescriptor& descriptor, ThermalCurve* curve, const QVariantMap& parameters, const QVector<ThermalDataPoint>& points);
    void resetPending();
    [[nodiscard]] std::optional<AlgorithmDescriptor> descriptorFor(const QString& algorithmName);

    AlgorithmManager* m_algorithmManager = nullptr;
    CurveManager* m_curveManager = nullptr;
    AlgorithmContext* m_context = nullptr;
    std::optional<PendingRequest> m_pending;

    // ==================== 异步执行状态 ====================
    QString m_currentTaskId;  ///< 当前正在执行的异步任务ID（用于取消）
};

#endif // APPLICATION_ALGORITHM_COORDINATOR_H

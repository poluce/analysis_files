#ifndef APPLICATION_ALGORITHM_COORDINATOR_H
#define APPLICATION_ALGORITHM_COORDINATOR_H

#include "domain/algorithm/algorithm_descriptor.h"
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
    void handlePointSelectionResult(const QVector<QPointF>& points);
    void cancelPendingRequest();

signals:
    void requestParameterDialog(
        const QString& algorithmName, const QList<AlgorithmParameterDefinition>& parameters, const QVariantMap& initialValues);
    void requestPointSelection(
        const QString& algorithmName, const QString& curveId, int requiredPoints, const QString& hint);
    void showMessage(const QString& text);
    void algorithmFailed(const QString& algorithmName, const QString& reason);
    void algorithmSucceeded(const QString& algorithmName);

private slots:
    void onAlgorithmResultReady(const QString& algorithmName, const AlgorithmResult& result);

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
        QVector<QPointF> collectedPoints;
        PendingPhase phase = PendingPhase::None;
    };

    bool ensurePrerequisites(const AlgorithmDescriptor& descriptor, ThermalCurve* curve);
    bool populateDefaultParameters(const AlgorithmDescriptor& descriptor, QVariantMap& parameters) const;
    void executeAlgorithm(const AlgorithmDescriptor& descriptor, ThermalCurve* curve, const QVariantMap& parameters, const QVector<QPointF>& points);
    void resetPending();
    [[nodiscard]] std::optional<AlgorithmDescriptor> descriptorFor(const QString& algorithmName);

    AlgorithmManager* m_algorithmManager = nullptr;
    CurveManager* m_curveManager = nullptr;
    AlgorithmContext* m_context = nullptr;
    std::optional<PendingRequest> m_pending;
};

#endif // APPLICATION_ALGORITHM_COORDINATOR_H

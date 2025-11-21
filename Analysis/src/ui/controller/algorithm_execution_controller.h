#ifndef UI_CONTROLLER_ALGORITHM_EXECUTION_CONTROLLER_H
#define UI_CONTROLLER_ALGORITHM_EXECUTION_CONTROLLER_H

#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/algorithm/algorithm_result.h"
#include "domain/model/thermal_data_point.h"

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QStringList>

class AlgorithmCoordinator;
class AlgorithmManager;
class CurveManager;
class ChartView;
class MessagePresenter;
class QProgressDialog;
class QWidget;
class IThermalAlgorithm;

class AlgorithmExecutionController : public QObject {
    Q_OBJECT

public:
    AlgorithmExecutionController(CurveManager* curveManager,
                                 AlgorithmManager* algorithmManager,
                                 AlgorithmCoordinator* algorithmCoordinator,
                                 MessagePresenter* messagePresenter,
                                 QObject* parent = nullptr);
    ~AlgorithmExecutionController() override;

    void setChartView(ChartView* chartView);
    void setDialogParent(QWidget* dialogParent);

    void requestAlgorithmRun(const QString& algorithmName, const QVariantMap& params = QVariantMap());

private slots:
    void onCoordinatorRequestPointSelection(const QString& algorithmName, int requiredPoints, const QString& hint);
    void onRequestParameterDialog(const QString& algorithmName, const AlgorithmDescriptor& descriptor);
    void onCoordinatorShowMessage(const QString& text);
    void onCoordinatorAlgorithmFailed(const QString& algorithmName, const QString& reason);
    void onAlgorithmCompleted(const QString& taskId,
                              const QString& algorithmName,
                              const QString& parentCurveId,
                              const AlgorithmResult& result);
    void onWorkflowCompleted(const QString& workflowId, const QStringList& outputCurveIds);
    void onWorkflowFailed(const QString& workflowId, const QString& errorMessage);
    void onAlgorithmStarted(const QString& taskId, const QString& algorithmName);
    void onAlgorithmProgress(const QString& taskId, int percentage, const QString& message);
    void handleProgressDialogCancelled();
    void onAlgorithmInteractionCompleted(const QString& algorithmName, const QVector<ThermalDataPoint>& points);

private:
    void connectCoordinatorSignals();
    void cleanupProgressDialog();
    QProgressDialog* ensureProgressDialog();
    QWidget* dialogParent() const;

    QWidget* createParameterWidget(const AlgorithmParameterDefinition& param, QWidget* parent);
    QVariantMap extractParameters(const QMap<QString, QWidget*>& widgets,
                                  const QList<AlgorithmParameterDefinition>& paramDefs);

    CurveManager* m_curveManager = nullptr;
    AlgorithmManager* m_algorithmManager = nullptr;
    AlgorithmCoordinator* m_algorithmCoordinator = nullptr;
    MessagePresenter* m_messagePresenter = nullptr;
    ChartView* m_chartView = nullptr;
    QWidget* m_dialogParent = nullptr;
    QProgressDialog* m_progressDialog = nullptr;
    QString m_currentTaskId;
    QString m_currentAlgorithmName;
};

#endif // UI_CONTROLLER_ALGORITHM_EXECUTION_CONTROLLER_H

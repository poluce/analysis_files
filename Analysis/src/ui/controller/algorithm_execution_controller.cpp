#include "ui/controller/algorithm_execution_controller.h"

#include "application/algorithm/algorithm_coordinator.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/curve/curve_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "domain/model/thermal_curve.h"
#include "ui/chart_view.h"
#include "ui/presenter/message_presenter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMap>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QtGlobal>

namespace {
QString stateNameFor(int newState)
{
    switch (newState) {
    case 0:
        return QStringLiteral("Idle");
    case 1:
        return QStringLiteral("WaitingForPoints");
    case 2:
        return QStringLiteral("PointsCompleted");
    case 3:
        return QStringLiteral("Executing");
    default:
        return QStringLiteral("Unknown");
    }
}
} // namespace

AlgorithmExecutionController::AlgorithmExecutionController(CurveManager* curveManager,
                                                           AlgorithmManager* algorithmManager,
                                                           AlgorithmCoordinator* algorithmCoordinator,
                                                           MessagePresenter* messagePresenter,
                                                           QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_algorithmManager(algorithmManager)
    , m_algorithmCoordinator(algorithmCoordinator)
    , m_messagePresenter(messagePresenter)
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_algorithmManager);
    Q_ASSERT(m_algorithmCoordinator);

    if (m_messagePresenter) {
        m_dialogParent = m_messagePresenter->parentWidget();
    }

    connectCoordinatorSignals();
}

AlgorithmExecutionController::~AlgorithmExecutionController() = default;

void AlgorithmExecutionController::setChartView(ChartView* chartView)
{
    m_chartView = chartView;
    if (!m_chartView) {
        return;
    }

    connect(m_chartView, &ChartView::algorithmInteractionCompleted,
            this, &AlgorithmExecutionController::onAlgorithmInteractionCompleted, Qt::UniqueConnection);

    connect(m_chartView, &ChartView::interactionStateChanged, this,
            [](int newState) { qDebug() << "ChartView 交互状态变化:" << stateNameFor(newState); }, Qt::UniqueConnection);
}

void AlgorithmExecutionController::setDialogParent(QWidget* dialogParent)
{
    m_dialogParent = dialogParent;
}

void AlgorithmExecutionController::requestAlgorithmRun(const QString& algorithmName, const QVariantMap& params)
{
    if (!m_algorithmCoordinator) {
        qWarning() << "AlgorithmExecutionController::requestAlgorithmRun - coordinator not set";
        return;
    }

    qDebug() << "AlgorithmExecutionController: 收到算法执行请求" << algorithmName
             << (params.isEmpty() ? "（无初始参数）" : "（包含初始参数）");

    Q_UNUSED(params);
    m_algorithmCoordinator->run(algorithmName);
}

void AlgorithmExecutionController::onCoordinatorRequestPointSelection(const QString& algorithmName,
                                                                      int requiredPoints,
                                                                      const QString& hint)
{
    if (!m_chartView) {
        qWarning() << "AlgorithmExecutionController::onCoordinatorRequestPointSelection - chart view missing";
        return;
    }

    ThermalCurve* activeCurve = m_curveManager ? m_curveManager->getActiveCurve() : nullptr;
    if (!activeCurve) {
        qWarning() << "AlgorithmExecutionController::onCoordinatorRequestPointSelection - no active curve";
        return;
    }

    QString displayName = algorithmName;
    if (m_algorithmManager) {
        if (IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName)) {
            displayName = algorithm->displayName();
        } else {
            qWarning() << "AlgorithmExecutionController::onCoordinatorRequestPointSelection - algorithm not found"
                       << algorithmName;
        }
    }

    m_chartView->startAlgorithmInteraction(algorithmName, displayName, qMax(1, requiredPoints), hint, activeCurve->id());

    if (!hint.isEmpty()) {
        onCoordinatorShowMessage(hint);
    }
}

void AlgorithmExecutionController::onRequestParameterDialog(const QString& algorithmName,
                                                            const AlgorithmDescriptor& descriptor)
{
    qDebug() << "[AlgorithmExecutionController] onRequestParameterDialog - 算法:" << algorithmName;
    qDebug() << "  参数数量:" << descriptor.parameters.size();

    QDialog* dlg = new QDialog(dialogParent());
    dlg->setWindowTitle(descriptor.displayName + QStringLiteral(" - 参数设置"));
    dlg->setMinimumWidth(400);

    QFormLayout* form = new QFormLayout(dlg);
    QMap<QString, QWidget*> widgets;
    for (const auto& param : descriptor.parameters) {
        QWidget* widget = createParameterWidget(param, dlg);
        if (widget) {
            QString label = param.label;
            if (!label.isEmpty() && !label.endsWith(':')) {
                label += ':';
            }
            form->addRow(label, widget);
            widgets[param.key] = widget;
        }
    }

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        QVariantMap parameters = extractParameters(widgets, descriptor.parameters);
        qDebug() << "[AlgorithmExecutionController] 用户提交参数:" << parameters;
        if (m_algorithmCoordinator) {
            m_algorithmCoordinator->submitParameters(parameters);
        }
    } else {
        qDebug() << "[AlgorithmExecutionController] 用户取消参数输入";
        if (m_algorithmCoordinator) {
            m_algorithmCoordinator->cancel();
        }
    }

    dlg->deleteLater();
}

void AlgorithmExecutionController::onCoordinatorShowMessage(const QString& text)
{
    if (text.isEmpty() || !m_messagePresenter) {
        return;
    }

    m_messagePresenter->showInformation(tr("提示"), text);
}

void AlgorithmExecutionController::onCoordinatorAlgorithmFailed(const QString& algorithmName, const QString& reason)
{
    qWarning() << "算法执行失败:" << algorithmName << reason;

    cleanupProgressDialog();
    m_currentTaskId.clear();
    m_currentAlgorithmName.clear();

    if (m_messagePresenter) {
        m_messagePresenter->showWarning(tr("算法失败"), tr("算法 %1 执行失败：%2").arg(algorithmName, reason));
    }
}

void AlgorithmExecutionController::onAlgorithmCompleted(const QString& taskId,
                                                        const QString& algorithmName,
                                                        const QString& parentCurveId,
                                                        const AlgorithmResult& result)
{
    Q_UNUSED(taskId);
    Q_UNUSED(parentCurveId);
    Q_UNUSED(result);

    cleanupProgressDialog();
    m_currentTaskId.clear();
    m_currentAlgorithmName.clear();

    qInfo() << "算法执行完成:" << algorithmName << "taskId:" << taskId;
}

void AlgorithmExecutionController::onWorkflowCompleted(const QString& workflowId, const QStringList& outputCurveIds)
{
    qInfo() << "工作流执行完成:" << workflowId << "输出曲线:" << outputCurveIds.size();

    cleanupProgressDialog();
    if (m_messagePresenter) {
        m_messagePresenter->showInformation(
            tr("工作流完成"),
            tr("工作流 %1 执行完成，生成 %2 条输出曲线。")
                .arg(workflowId)
                .arg(outputCurveIds.size()));
    }
}

void AlgorithmExecutionController::onWorkflowFailed(const QString& workflowId, const QString& errorMessage)
{
    qWarning() << "工作流执行失败:" << workflowId << errorMessage;

    cleanupProgressDialog();
    if (m_messagePresenter) {
        m_messagePresenter->showWarning(
            tr("工作流失败"),
            tr("工作流 %1 执行失败：%2").arg(workflowId, errorMessage));
    }
}

void AlgorithmExecutionController::onAlgorithmStarted(const QString& taskId, const QString& algorithmName)
{
    qDebug() << "[AlgorithmExecutionController] 算法开始执行:" << algorithmName << "taskId:" << taskId;

    m_currentTaskId = taskId;
    m_currentAlgorithmName = algorithmName;

    cleanupProgressDialog();

    QProgressDialog* dialog = ensureProgressDialog();
    {
        QSignalBlocker blocker(dialog);
        dialog->setWindowTitle(QStringLiteral("算法执行中"));
        dialog->setLabelText(QStringLiteral("正在执行算法: %1\n请稍候...").arg(algorithmName));
        dialog->setRange(0, 100);
        dialog->setValue(0);
        dialog->setMinimumDuration(0);
    }

    dialog->show();
}

void AlgorithmExecutionController::onAlgorithmProgress(const QString& taskId, int percentage, const QString& message)
{
    if (taskId != m_currentTaskId) {
        return;
    }

    if (!m_progressDialog) {
        qWarning() << "[AlgorithmExecutionController] 收到进度更新，但进度对话框不存在";
        return;
    }

    m_progressDialog->setValue(percentage);

    if (!message.isEmpty()) {
        QString text = m_progressDialog->labelText().split('\n').first();
        m_progressDialog->setLabelText(text + "\n" + message);
    }

    if (percentage % 20 == 0) {
        qDebug() << "[AlgorithmExecutionController] 进度更新:" << percentage << "%" << message;
    }
}

void AlgorithmExecutionController::handleProgressDialogCancelled()
{
    qDebug() << "[AlgorithmExecutionController] 用户取消算法:" << m_currentAlgorithmName
             << "taskId:" << m_currentTaskId;

    const QString taskId = m_currentTaskId;
    const QString algorithmName = m_currentAlgorithmName;

    bool cancelled = false;
    if (m_algorithmManager && !taskId.isEmpty()) {
        cancelled = m_algorithmManager->cancelTask(taskId);
    }

    if (cancelled) {
        qDebug() << "[AlgorithmExecutionController] 任务取消成功:" << taskId;
        cleanupProgressDialog();
        m_currentTaskId.clear();
        m_currentAlgorithmName.clear();

        if (m_messagePresenter && !algorithmName.isEmpty()) {
            m_messagePresenter->showInformation(QStringLiteral("任务已取消"),
                                                QStringLiteral("算法 %1 已取消。").arg(algorithmName));
        }
    } else {
        qWarning() << "[AlgorithmExecutionController] 任务取消失败（任务可能已完成）:" << taskId;
    }

    if (m_algorithmCoordinator) {
        m_algorithmCoordinator->cancelPendingRequest();
    }
}

void AlgorithmExecutionController::onAlgorithmInteractionCompleted(const QString& algorithmName,
                                                                    const QVector<ThermalDataPoint>& points)
{
    qDebug() << "AlgorithmExecutionController: 接收到算法交互完成信号 -" << algorithmName
             << ", 选点数量:" << points.size();

    if (!m_algorithmCoordinator) {
        qWarning() << "AlgorithmExecutionController::onAlgorithmInteractionCompleted - coordinator not set";
        return;
    }

    m_algorithmCoordinator->handlePointSelectionResult(points);
}

void AlgorithmExecutionController::connectCoordinatorSignals()
{
    if (!m_algorithmCoordinator) {
        return;
    }

    connect(m_algorithmCoordinator, &AlgorithmCoordinator::requestPointSelection,
            this, &AlgorithmExecutionController::onCoordinatorRequestPointSelection, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::requestParameterDialog,
            this, &AlgorithmExecutionController::onRequestParameterDialog, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::showMessage,
            this, &AlgorithmExecutionController::onCoordinatorShowMessage, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmFailed,
            this, &AlgorithmExecutionController::onCoordinatorAlgorithmFailed, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmCompleted,
            this, &AlgorithmExecutionController::onAlgorithmCompleted, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::workflowCompleted,
            this, &AlgorithmExecutionController::onWorkflowCompleted, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::workflowFailed,
            this, &AlgorithmExecutionController::onWorkflowFailed, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmStarted,
            this, &AlgorithmExecutionController::onAlgorithmStarted, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmProgress,
            this, &AlgorithmExecutionController::onAlgorithmProgress, Qt::UniqueConnection);
}

void AlgorithmExecutionController::cleanupProgressDialog()
{
    if (!m_progressDialog) {
        return;
    }

    QSignalBlocker blocker(m_progressDialog);
    m_progressDialog->hide();
    m_progressDialog->reset();
}

QProgressDialog* AlgorithmExecutionController::ensureProgressDialog()
{
    if (!m_progressDialog) {
        m_progressDialog = new QProgressDialog(dialogParent());
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setCancelButtonText(QStringLiteral("取消"));
        m_progressDialog->setAutoClose(false);
        m_progressDialog->setAutoReset(false);
        connect(m_progressDialog, &QProgressDialog::canceled,
                this, &AlgorithmExecutionController::handleProgressDialogCancelled, Qt::UniqueConnection);
    }
    return m_progressDialog;
}

QWidget* AlgorithmExecutionController::dialogParent() const
{
    if (m_dialogParent) {
        return m_dialogParent;
    }

    return m_messagePresenter ? m_messagePresenter->parentWidget() : nullptr;
}

QWidget* AlgorithmExecutionController::createParameterWidget(const AlgorithmParameterDefinition& param, QWidget* parent)
{
    switch (param.valueType) {
    case QVariant::Int: {
        auto* spinBox = new QSpinBox(parent);
        int minValue = param.constraints.value(QStringLiteral("min"), 0).toInt();
        int maxValue = param.constraints.value(QStringLiteral("max"), 100).toInt();
        spinBox->setMinimum(minValue);
        spinBox->setMaximum(maxValue);
        spinBox->setValue(param.defaultValue.toInt());
        return spinBox;
    }

    case QVariant::Double: {
        auto* spinBox = new QDoubleSpinBox(parent);
        double minValue = param.constraints.value(QStringLiteral("min"), 0.0).toDouble();
        double maxValue = param.constraints.value(QStringLiteral("max"), 100.0).toDouble();
        spinBox->setMinimum(minValue);
        spinBox->setMaximum(maxValue);
        int decimals = param.constraints.value(QStringLiteral("decimals"), 3).toInt();
        spinBox->setDecimals(decimals);
        double step = param.constraints.value(QStringLiteral("step"), 0.1).toDouble();
        spinBox->setSingleStep(step);
        spinBox->setValue(param.defaultValue.toDouble());
        return spinBox;
    }

    case QVariant::String: {
        auto* lineEdit = new QLineEdit(parent);
        lineEdit->setText(param.defaultValue.toString());
        return lineEdit;
    }

    case QVariant::Bool: {
        auto* checkBox = new QCheckBox(parent);
        checkBox->setChecked(param.defaultValue.toBool());
        return checkBox;
    }

    default:
        if (param.constraints.contains(QStringLiteral("options"))) {
            auto* comboBox = new QComboBox(parent);
            comboBox->addItems(param.constraints.value(QStringLiteral("options")).toStringList());
            comboBox->setCurrentIndex(param.defaultValue.toInt());
            return comboBox;
        }
        qWarning() << "[AlgorithmExecutionController] 不支持的参数类型:" << param.valueType << "参数:" << param.key;
        return nullptr;
    }
}

QVariantMap AlgorithmExecutionController::extractParameters(const QMap<QString, QWidget*>& widgets,
                                                            const QList<AlgorithmParameterDefinition>& paramDefs)
{
    QVariantMap parameters;

    for (const auto& param : paramDefs) {
        if (!widgets.contains(param.key)) {
            qWarning() << "[AlgorithmExecutionController] 参数控件不存在:" << param.key;
            continue;
        }

        QWidget* widget = widgets.value(param.key);
        QVariant value;

        switch (param.valueType) {
        case QVariant::Int:
            if (auto* spinBox = qobject_cast<QSpinBox*>(widget)) {
                value = spinBox->value();
            }
            break;

        case QVariant::Double:
            if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
                value = spinBox->value();
            }
            break;

        case QVariant::String:
            if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
                value = lineEdit->text();
            }
            break;

        case QVariant::Bool:
            if (auto* checkBox = qobject_cast<QCheckBox*>(widget)) {
                value = checkBox->isChecked();
            }
            break;

        default:
            if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
                value = comboBox->currentIndex();
            }
            break;
        }

        if (value.isValid()) {
            parameters[param.key] = value;
            qDebug() << "  提取参数:" << param.key << "=" << value;
        } else {
            qWarning() << "[AlgorithmExecutionController] 无法提取参数:" << param.key;
        }
    }

    return parameters;
}

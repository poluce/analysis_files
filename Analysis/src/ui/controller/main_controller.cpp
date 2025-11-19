#include "ui/controller/main_controller.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "ui/controller/curve_view_controller.h"
#include "ui/data_import_widget.h"
#include "ui/peak_area_dialog.h"
#include <QDebug>
#include <QtGlobal>

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_coordinator.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/history/algorithm_command.h"
#include "application/history/add_curve_command.h"
#include "application/history/clear_curves_command.h"
#include "application/history/remove_curve_command.h"
#include "application/history/remove_mass_loss_tool_command.h"
#include "application/history/remove_peak_area_tool_command.h"
#include "application/history/history_manager.h"
#include "domain/algorithm/algorithm_descriptor.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "ui/chart_view.h"
#include "ui/main_window.h"
#include "ui/thermal_chart.h"
#include "ui/trapezoid_measure_tool.h"
#include "ui/peak_area_tool.h"
#include <QMessageBox>
#include <QSignalBlocker>
#include <QProgressDialog>
#include <QInputDialog>
#include <memory>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>

MainController::MainController(CurveManager* curveManager,
                               AlgorithmManager* algorithmManager,
                               HistoryManager* historyManager,
                               QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_algorithmManager(algorithmManager)
    , m_historyManager(historyManager)
    , m_dataImportWidget(new DataImportWidget())
{
    Q_ASSERT(m_curveManager);
    Q_ASSERT(m_algorithmManager);
    Q_ASSERT(m_historyManager);
    qDebug() << "构造:  MainController";

    // 将 CurveManager 实例设置到算法服务中，以便能够创建新曲线
    m_algorithmManager->setCurveManager(m_curveManager);

    // ========== 信号连接设置 ==========
    // 命令路径：DataImportWidget → MainController
    connect(m_dataImportWidget, &DataImportWidget::previewRequested, this, &MainController::onPreviewRequested);
    connect(m_dataImportWidget, &DataImportWidget::importRequested, this, &MainController::onImportTriggered);
}

MainController::~MainController()
{
    delete m_dataImportWidget;
    delete m_progressDialog;
}

void MainController::setPlotWidget(ChartView* plotWidget)
{
    m_plotWidget = plotWidget;

    if (!m_plotWidget) {
        return;
    }

    // ==================== 连接活动算法状态机信号 ====================
    // 当用户完成算法交互（选点完成）时，自动触发算法执行
    connect(m_plotWidget, &ChartView::algorithmInteractionCompleted, this,
            [this](const QString& algorithmName, const QVector<ThermalDataPoint>& points) {
                qDebug() << "MainController: 接收到算法交互完成信号 -" << algorithmName
                         << ", 选点数量:" << points.size();

                if (!m_algorithmCoordinator) {
                    qWarning() << "MainController: AlgorithmCoordinator 未初始化，无法处理选点结果";
                    return;
                }

                // 将选点结果传递给协调器，继续算法执行流程
                m_algorithmCoordinator->handlePointSelectionResult(points);
            }, Qt::UniqueConnection);

    // 监听交互状态变化（用于调试和未来的状态栏更新）
    connect(m_plotWidget, &ChartView::interactionStateChanged, this,
            [](int newState) {
                QString stateName;
                switch (newState) {
                case 0: // Idle
                    stateName = "Idle";
                    break;
                case 1: // WaitingForPoints
                    stateName = "WaitingForPoints";
                    break;
                case 2: // PointsCompleted
                    stateName = "PointsCompleted";
                    break;
                case 3: // Executing
                    stateName = "Executing";
                    break;
                }
                qDebug() << "ChartView 交互状态变化:" << stateName;
            }, Qt::UniqueConnection);

    // 获取 ThermalChart 实例并连接工具删除请求信号
    ThermalChart* thermalChart = m_plotWidget->chart();
    if (thermalChart) {
        connect(thermalChart, &ThermalChart::massLossToolRemoveRequested,
                this, &MainController::onMassLossToolRemoveRequested, Qt::UniqueConnection);

        connect(thermalChart, &ThermalChart::peakAreaToolRemoveRequested,
                this, &MainController::onPeakAreaToolRemoveRequested, Qt::UniqueConnection);

        qDebug() << "MainController: 已连接视图工具删除请求信号";
    }

}

void MainController::attachMainWindow(MainWindow* mainWindow)
{
    if (!mainWindow) {
        return;
    }

    m_mainWindow = mainWindow;

    connect(mainWindow, &MainWindow::dataImportRequested, this, &MainController::onShowDataImport, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::curveDeleteRequested, this, &MainController::onCurveDeleteRequested, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::undoRequested, this, &MainController::onUndo, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::redoRequested, this, &MainController::onRedo, Qt::UniqueConnection);

    // 视图操作连接
    if (m_plotWidget) {
        connect(mainWindow, &MainWindow::fitViewRequested, m_plotWidget, &ChartView::rescaleAxes, Qt::UniqueConnection);
        connect(mainWindow, &MainWindow::massLossToolRequested, m_plotWidget, &ChartView::startMassLossTool, Qt::UniqueConnection);
        // 峰面积工具需要参数对话框，不直接连接到 ChartView
        connect(mainWindow, &MainWindow::peakAreaToolRequested, this, &MainController::onPeakAreaToolRequested, Qt::UniqueConnection);
    }

    // 算法接入（使用 lambda 适配不同参数的信号）
    connect(mainWindow, &MainWindow::algorithmRequested, this, [this](const QString& name) {
        onAlgorithmRequested(name, QVariantMap());
    }, Qt::UniqueConnection);

    connect(mainWindow, &MainWindow::algorithmRequestedWithParams, this, &MainController::onAlgorithmRequested, Qt::UniqueConnection);
}

void MainController::setCurveViewController(CurveViewController* ViewController)
{
    m_curveViewController = ViewController;
    // CurveViewController 不再负责选点功能，选点由 ChartView 状态机直接管理
}

void MainController::setAlgorithmCoordinator(AlgorithmCoordinator* coordinator, AlgorithmContext* context)
{
    m_algorithmCoordinator = coordinator;
    m_algorithmContext = context;

    if (!m_algorithmCoordinator) {
        return;
    }

    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::requestPointSelection, this,
        &MainController::onCoordinatorRequestPointSelection, Qt::UniqueConnection);

    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::requestParameterDialog, this,
        &MainController::onRequestParameterDialog, Qt::UniqueConnection);
    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::showMessage, this, &MainController::onCoordinatorShowMessage,
        Qt::UniqueConnection);
    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::algorithmFailed, this,
        &MainController::onCoordinatorAlgorithmFailed, Qt::UniqueConnection);
    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::algorithmSucceeded, this,
        &MainController::onCoordinatorAlgorithmSucceeded, Qt::UniqueConnection);

    // ==================== 连接异步执行进度信号 ====================
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmStarted,
            this, &MainController::onAlgorithmStarted, Qt::UniqueConnection);
    connect(m_algorithmCoordinator, &AlgorithmCoordinator::algorithmProgress,
            this, &MainController::onAlgorithmProgress, Qt::UniqueConnection);

    qDebug() << "[MainController] 已连接 AlgorithmCoordinator 的所有信号（包括参数对话框请求）";
}

// ==================== 完整性校验与状态标记 ====================

void MainController::initialize()
{
    // 断言所有必需依赖（构造函数注入 + setter 注入）
    Q_ASSERT(m_curveManager != nullptr);
    Q_ASSERT(m_algorithmManager != nullptr);
    Q_ASSERT(m_historyManager != nullptr);
    Q_ASSERT(m_plotWidget != nullptr);
    Q_ASSERT(m_mainWindow != nullptr);
    Q_ASSERT(m_curveViewController != nullptr);
    Q_ASSERT(m_algorithmCoordinator != nullptr);
    Q_ASSERT(m_algorithmContext != nullptr);

    // 标记为已初始化状态
    m_initialized = true;

    qDebug() << "[OK] MainController 初始化完成，所有依赖已就绪";
}

// ==================== 业务逻辑槽函数 ====================

void MainController::onShowDataImport() { m_dataImportWidget->show(); }

void MainController::onPreviewRequested(const QString& filePath)
{
    qDebug() << "控制器：收到预览文件请求：" << filePath;
    // 委托给 CurveManager，使用其注册的 Reader 体系
    FilePreviewData preview = m_curveManager->readFilePreview(filePath);
    m_dataImportWidget->setPreviewData(preview);
}

void MainController::onImportTriggered()
{
    qDebug() << "控制器：收到导入请求。";

    // 1. 从 m_dataImportWidget 获取所有用户配置
    QVariantMap config = m_dataImportWidget->getImportConfig();
    QString filePath = config.value("filePath").toString();

    if (filePath.isEmpty()) {
        qWarning() << "导入失败：文件路径为空。";
        return;
    }

    // 2. 使用命令模式清空已有曲线（支持撤销）
    auto clearCommand = std::make_unique<ClearCurvesCommand>(
        m_curveManager,
        QStringLiteral("导入前清空曲线")
    );
    if (!m_historyManager->executeCommand(std::move(clearCommand))) {
        qWarning() << "MainController::onImportTriggered - 清空曲线命令执行失败";
        return;
    }

    // 3. 委托给 CurveManager 读取文件（支持配置）
    QString curveId = m_curveManager->loadCurveFromFileWithConfig(filePath, config);

    if (curveId.isEmpty()) {
        qWarning() << "导入失败：读取文件失败或曲线无效。";
        return;
    }

    // 4. 设置为活动曲线
    m_curveManager->setActiveCurve(curveId);

    // 5. 发射信号，通知UI更新
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (curve) {
        emit curveAvailable(*curve);
    }

    // 6. 关闭导入窗口
    m_dataImportWidget->close();
}

// ========== 处理命令的槽函数（命令路径：UI → Controller → Service） ==========
void MainController::onAlgorithmRequested(const QString& algorithmName, const QVariantMap& params)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController: 接收到算法执行请求：" << algorithmName
             << (params.isEmpty() ? "（无参数）" : "（带参数）");

    // 参数说明：params 参数暂时未使用，因为参数收集现在由 AlgorithmCoordinator
    // 通过 requestParameterDialog 信号触发动态对话框完成
    Q_UNUSED(params);

    qDebug() << "MainController: 调用 run() 执行算法：" << algorithmName;
    m_algorithmCoordinator->run(algorithmName);
}


void MainController::onUndo()
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController: 执行撤销操作";

    if (!m_historyManager->canUndo()) {
        qDebug() << "MainController: 无可撤销的操作";
        return;
    }

    if (!m_historyManager->undo()) {
        qWarning() << "MainController: 撤销操作失败";
    }
    // 注意：曲线的删除由 CurveManager::curveRemoved 信号自动触发UI更新
}

void MainController::onRedo()
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController: 执行重做操作";

    if (!m_historyManager->canRedo()) {
        qDebug() << "MainController: 无可重做的操作";
        return;
    }

    if (!m_historyManager->redo()) {
        qWarning() << "MainController: 重做操作失败";
    }
    // 注意：曲线的添加由 CurveManager::curveAdded 信号自动触发UI更新
}

// ========== 响应UI的曲线删除请求 ==========
void MainController::onCurveDeleteRequested(const QString& curveId)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onCurveDeleteRequested - 曲线ID:" << curveId;

    // 1. 检查曲线是否存在
    ThermalCurve* curve = m_curveManager->getCurve(curveId);
    if (!curve) {
        qWarning() << "MainController::onCurveDeleteRequested - 曲线不存在:" << curveId;
        return;
    }

    // 2. 禁止删除主曲线（数据源）
    if (curve->isMainCurve()) {
        QMessageBox::warning(
            m_mainWindow,
            tr("无法删除主曲线"),
            tr("曲线 \"%1\" 是主曲线（数据源），不能被删除。\n\n"
               "主曲线是从文件导入的原始数据，是所有派生曲线的基础。\n"
               "如果需要移除，请使用 文件 → 清空项目 功能。")
                .arg(curve->name())
        );

        qWarning() << "MainController::onCurveDeleteRequested - 不允许删除主曲线:" << curveId;
        return;
    }

    // 3. 检查是否有子曲线（需要级联删除）
    bool cascadeDelete = false;
    if (m_curveManager->hasChildren(curveId)) {
        // 获取子曲线列表以显示详细信息
        QVector<ThermalCurve*> children = m_curveManager->getChildren(curveId);

        // 构建子曲线列表消息
        QString childrenList;
        for (int i = 0; i < children.size() && i < 5; ++i) {  // 最多显示5个
            childrenList += QString("  - %1\n").arg(children[i]->name());
        }
        if (children.size() > 5) {
            childrenList += QString("  - ... (还有 %1 个)\n").arg(children.size() - 5);
        }

        // 显示确认对话框
        QMessageBox::StandardButton reply = QMessageBox::question(
            m_mainWindow,
            tr("确认级联删除"),
            tr("曲线 \"%1\" 有 %2 个子曲线：\n\n%3\n"
               "删除此曲线将同时删除所有子曲线。\n\n"
               "是否继续删除？")
                .arg(curve->name())
                .arg(children.size())
                .arg(childrenList),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No  // 默认选择 No（更安全）
        );

        if (reply == QMessageBox::No) {
            qDebug() << "MainController::onCurveDeleteRequested - 用户取消删除:" << curveId;
            return;
        }

        cascadeDelete = true;  // 用户确认级联删除
    }

    // 4. 使用命令模式删除曲线（支持撤销）
    QString description = cascadeDelete
        ? QStringLiteral("删除曲线 \"%1\" 及其子曲线").arg(curve->name())
        : QStringLiteral("删除曲线 \"%1\"").arg(curve->name());

    auto removeCommand = std::make_unique<RemoveCurveCommand>(
        m_curveManager,
        curveId,
        cascadeDelete,
        description
    );

    if (!m_historyManager->executeCommand(std::move(removeCommand))) {
        qWarning() << "MainController::onCurveDeleteRequested - 删除曲线命令执行失败:" << curveId;
        return;
    }

    qDebug() << "MainController::onCurveDeleteRequested - 成功删除曲线:" << curveId
             << (cascadeDelete ? "（包括子曲线）" : "");
}

void MainController::onCoordinatorRequestPointSelection(
    const QString& algorithmName, int requiredPoints, const QString& hint)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onCoordinatorRequestPointSelection - 算法:" << algorithmName
             << ", 需要点数:" << requiredPoints;

    // ==================== 使用新的活动算法状态机 ====================
    // 获取算法的显示名称
    QString displayName = algorithmName;  // 默认使用算法名称
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (algorithm) {
        displayName = algorithm->displayName();
    } else {
        qWarning() << "MainController: 无法找到算法" << algorithmName << "，使用名称作为显示名";
    }

    // 启动算法交互状态机
    QString curveId;
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (activeCurve) {
        curveId = activeCurve->id();
    } else {
        qWarning() << "MainController: 没有活动曲线，无法启动点选";
        return;
    }

    m_plotWidget->startAlgorithmInteraction(algorithmName, displayName, qMax(1, requiredPoints), hint, curveId);

    // 显示提示信息（如果有）
    if (!hint.isEmpty()) {
        onCoordinatorShowMessage(hint);
    }
}

void MainController::onCoordinatorShowMessage(const QString& text)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    if (text.isEmpty()) {
        return;
    }

    QMessageBox::information(m_mainWindow, tr("提示"), text);
}

void MainController::onCoordinatorAlgorithmFailed(const QString& algorithmName, const QString& reason)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qWarning() << "算法执行失败:" << algorithmName << reason;

    cleanupProgressDialog();
    m_currentTaskId.clear();
    m_currentAlgorithmName.clear();

    // 显示错误提示
    QMessageBox::warning(
        m_mainWindow, tr("算法失败"), tr("算法 %1 执行失败：%2").arg(algorithmName, reason));
}

void MainController::onCoordinatorAlgorithmSucceeded(const QString& algorithmName)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qInfo() << "算法执行完成:" << algorithmName;

    cleanupProgressDialog();
    m_currentTaskId.clear();
    m_currentAlgorithmName.clear();
}

void MainController::onPeakAreaToolRequested()
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onPeakAreaToolRequested - 峰面积工具请求";

    // 检查是否有可用曲线
    if (m_curveManager->getAllCurves().isEmpty()) {
        QMessageBox::warning(
            m_mainWindow,
            tr("无可用曲线"),
            tr("请先导入数据文件，才能使用峰面积测量工具。")
        );
        return;
    }

    // 显示峰面积参数对话框，让用户选择：
    // 1. 计算曲线
    // 2. 基线类型（直线基线 或 参考曲线基线）
    // 3. 参考曲线（如果选择参考基线）
    PeakAreaDialog dialog(m_curveManager, m_mainWindow);
    if (dialog.exec() != QDialog::Accepted) {
        qDebug() << "MainController::onPeakAreaToolRequested - 用户取消";
        return;
    }

    // 获取用户选择
    QString curveId = dialog.selectedCurveId();
    bool useLinearBaseline = (dialog.baselineType() == PeakAreaDialog::BaselineType::Linear);
    QString referenceCurveId = dialog.referenceCurveId();

    qDebug() << "MainController::onPeakAreaToolRequested - 启动峰面积视图工具";
    qDebug() << "  计算曲线:" << curveId;
    qDebug() << "  基线类型:" << (useLinearBaseline ? "直线基线" : "参考曲线基线");
    if (!referenceCurveId.isEmpty()) {
        qDebug() << "  参考曲线:" << referenceCurveId;
    }

    // 启动峰面积工具
    m_plotWidget->startPeakAreaTool(curveId, useLinearBaseline, referenceCurveId);
}

// ==================== 异步执行进度反馈槽函数实现 ====================

void MainController::onAlgorithmStarted(const QString& taskId, const QString& algorithmName)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "[MainController] 算法开始执行:" << algorithmName << "taskId:" << taskId;

    // 保存任务ID
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
        dialog->setMinimumDuration(0);  // 立即显示
    }

    dialog->show();
}

void MainController::onAlgorithmProgress(const QString& taskId, int percentage, const QString& message)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    // 验证任务ID
    if (taskId != m_currentTaskId) {
        return;  // 不是当前任务的进度信号，忽略
    }

    if (!m_progressDialog) {
        qWarning() << "[MainController] 收到进度更新，但进度对话框不存在";
        return;
    }

    // 更新进度条
    m_progressDialog->setValue(percentage);

    // 更新状态文本
    if (!message.isEmpty()) {
        QString text = m_progressDialog->labelText().split('\n').first();  // 保留第一行（算法名称）
        m_progressDialog->setLabelText(text + "\n" + message);
    }

    // 调试日志（避免过度输出）
    if (percentage % 20 == 0) {
        qDebug() << "[MainController] 进度更新:" << percentage << "%" << message;
    }
}

void MainController::cleanupProgressDialog()
{
    if (!m_progressDialog) {
        return;
    }

    QSignalBlocker blocker(m_progressDialog);
    m_progressDialog->hide();
    m_progressDialog->reset();
}

QProgressDialog* MainController::ensureProgressDialog()
{
    if (!m_progressDialog) {
        m_progressDialog = new QProgressDialog(m_mainWindow);
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setCancelButtonText(QStringLiteral("取消"));
        m_progressDialog->setAutoClose(false);
        m_progressDialog->setAutoReset(false);
        connect(m_progressDialog, &QProgressDialog::canceled,
                this, &MainController::handleProgressDialogCancelled, Qt::UniqueConnection);
    }
    return m_progressDialog;
}

void MainController::handleProgressDialogCancelled()
{
    qDebug() << "[MainController] 用户点击取消按钮，尝试取消算法:" << m_currentAlgorithmName
             << "taskId:" << m_currentTaskId;

    const QString taskId = m_currentTaskId;
    const QString algorithmName = m_currentAlgorithmName;

    bool cancelled = false;
    if (m_algorithmManager && !taskId.isEmpty()) {
        cancelled = m_algorithmManager->cancelTask(taskId);
    }

    if (cancelled) {
        qDebug() << "[MainController] 任务取消成功:" << taskId;
        cleanupProgressDialog();
        m_currentTaskId.clear();
        m_currentAlgorithmName.clear();

        if (m_mainWindow && !algorithmName.isEmpty()) {
            QMessageBox::information(m_mainWindow, QStringLiteral("任务已取消"),
                                     QStringLiteral("算法 %1 已取消").arg(algorithmName));
        }
    } else {
        qWarning() << "[MainController] 任务取消失败（任务可能已完成）:" << taskId;
    }

    if (m_algorithmCoordinator) {
        m_algorithmCoordinator->cancelPendingRequest();
    }
}

void MainController::onRequestParameterDialog(const QString& algorithmName, const AlgorithmDescriptor& descriptor)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "[MainController] onRequestParameterDialog - 算法:" << algorithmName;
    qDebug() << "  参数数量:" << descriptor.parameters.size();

    // 创建对话框
    QDialog* dlg = new QDialog(m_mainWindow);
    dlg->setWindowTitle(descriptor.displayName + QStringLiteral(" - 参数设置"));
    dlg->setMinimumWidth(400);

    QFormLayout* form = new QFormLayout(dlg);

    // 存储控件引用（参数名 → 控件指针）
    QMap<QString, QWidget*> widgets;

    // 动态创建参数控件
    for (const auto& param : descriptor.parameters) {
        QWidget* widget = createParameterWidget(param, dlg);
        if (widget) {
            QString label = param.label;
            if (!label.isEmpty() && !label.endsWith(":")) {
                label += ":";
            }
            form->addRow(label, widget);
            widgets[param.key] = widget;
        }
    }

    // 添加按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    // 显示对话框并处理结果
    if (dlg->exec() == QDialog::Accepted) {
        QVariantMap parameters = extractParameters(widgets, descriptor.parameters);
        qDebug() << "[MainController] 用户提交参数:" << parameters;

        // 调用 AlgorithmCoordinator::submitParameters()
        if (m_algorithmCoordinator) {
            m_algorithmCoordinator->submitParameters(parameters);
        }
    } else {
        qDebug() << "[MainController] 用户取消参数输入";

        // 调用 AlgorithmCoordinator::cancel()
        if (m_algorithmCoordinator) {
            m_algorithmCoordinator->cancel();
        }
    }

    dlg->deleteLater();
}

QWidget* MainController::createParameterWidget(const AlgorithmParameterDefinition& param, QWidget* parent)
{
    switch (param.valueType) {

    // ==================== 整数类型 ====================
    case QVariant::Int:
        {
            QSpinBox* spinBox = new QSpinBox(parent);

            // 设置范围（从 constraints 中读取）
            int minValue = param.constraints.value("min", 0).toInt();
            int maxValue = param.constraints.value("max", 100).toInt();
            spinBox->setMinimum(minValue);
            spinBox->setMaximum(maxValue);

            // 设置默认值
            spinBox->setValue(param.defaultValue.toInt());

            return spinBox;
        }

    // ==================== 浮点类型 ====================
    case QVariant::Double:
        {
            QDoubleSpinBox* spinBox = new QDoubleSpinBox(parent);

            // 设置范围
            double minValue = param.constraints.value("min", 0.0).toDouble();
            double maxValue = param.constraints.value("max", 100.0).toDouble();
            spinBox->setMinimum(minValue);
            spinBox->setMaximum(maxValue);

            // 设置精度
            int decimals = param.constraints.value("decimals", 3).toInt();
            spinBox->setDecimals(decimals);

            // 设置步长
            double step = param.constraints.value("step", 0.1).toDouble();
            spinBox->setSingleStep(step);

            // 设置默认值
            spinBox->setValue(param.defaultValue.toDouble());

            return spinBox;
        }

    // ==================== 字符串类型 ====================
    case QVariant::String:
        {
            QLineEdit* lineEdit = new QLineEdit(parent);
            lineEdit->setText(param.defaultValue.toString());
            return lineEdit;
        }

    // ==================== 布尔类型 ====================
    case QVariant::Bool:
        {
            QCheckBox* checkBox = new QCheckBox(parent);
            checkBox->setChecked(param.defaultValue.toBool());
            return checkBox;
        }

    // NOTE: Enum 类型需要特殊处理（使用 constraints["options"]）
    default:
        // 尝试检查是否是枚举类型（通过 constraints["options"] 判断）
        if (param.constraints.contains("options")) {
            QComboBox* comboBox = new QComboBox(parent);
            QStringList options = param.constraints.value("options").toStringList();
            comboBox->addItems(options);
            comboBox->setCurrentIndex(param.defaultValue.toInt());
            return comboBox;
        }

        qWarning() << "[MainController] 不支持的参数类型:" << param.valueType
                   << "参数名:" << param.key;
        return nullptr;
    }
}

QVariantMap MainController::extractParameters(const QMap<QString, QWidget*>& widgets,
                                              const QList<AlgorithmParameterDefinition>& paramDefs)
{
    QVariantMap parameters;

    for (const auto& param : paramDefs) {
        if (!widgets.contains(param.key)) {
            qWarning() << "[MainController] 参数控件不存在:" << param.key;
            continue;
        }

        QWidget* widget = widgets[param.key];
        QVariant value;

        switch (param.valueType) {
        case QVariant::Int:
            if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget)) {
                value = spinBox->value();
            }
            break;

        case QVariant::Double:
            if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
                value = spinBox->value();
            }
            break;

        case QVariant::String:
            if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
                value = lineEdit->text();
            }
            break;

        case QVariant::Bool:
            if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
                value = checkBox->isChecked();
            }
            break;

        default:
            // 尝试枚举类型
            if (QComboBox* comboBox = qobject_cast<QComboBox*>(widget)) {
                value = comboBox->currentIndex();
            }
            break;
        }

        if (value.isValid()) {
            parameters[param.key] = value;
            qDebug() << "  提取参数:" << param.key << "=" << value;
        } else {
            qWarning() << "[MainController] 无法提取参数值:" << param.key;
        }
    }

    return parameters;
}

void MainController::onMassLossToolRemoveRequested(QGraphicsObject* tool)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onMassLossToolRemoveRequested - 收到删除请求";

    if (!tool) {
        qWarning() << "MainController::onMassLossToolRemoveRequested - 工具指针为空";
        return;
    }

    // 转换为 TrapezoidMeasureTool 以验证类型
    auto* measureTool = qobject_cast<TrapezoidMeasureTool*>(tool);
    if (!measureTool) {
        qWarning() << "MainController::onMassLossToolRemoveRequested - 工具类型转换失败";
        return;
    }

    // 获取 ThermalChart 实例
    ThermalChart* thermalChart = m_plotWidget->chart();
    if (!thermalChart) {
        qWarning() << "MainController::onMassLossToolRemoveRequested - ThermalChart 为空";
        return;
    }

    // 创建删除命令
    auto command = std::make_unique<RemoveMassLossToolCommand>(
        thermalChart,
        tool,
        "移除质量损失测量工具"
    );

    // 通过 HistoryManager 执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "MainController::onMassLossToolRemoveRequested - 命令执行失败";
        return;
    }

    qDebug() << "MainController::onMassLossToolRemoveRequested - 成功删除工具";
}

void MainController::onPeakAreaToolRemoveRequested(QGraphicsObject* tool)
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onPeakAreaToolRemoveRequested - 收到删除请求";

    if (!tool) {
        qWarning() << "MainController::onPeakAreaToolRemoveRequested - 工具指针为空";
        return;
    }

    // 转换为 PeakAreaTool 以验证类型
    auto* peakAreaTool = qobject_cast<PeakAreaTool*>(tool);
    if (!peakAreaTool) {
        qWarning() << "MainController::onPeakAreaToolRemoveRequested - 工具类型转换失败";
        return;
    }

    // 获取 ThermalChart 实例
    ThermalChart* thermalChart = m_plotWidget->chart();
    if (!thermalChart) {
        qWarning() << "MainController::onPeakAreaToolRemoveRequested - ThermalChart 为空";
        return;
    }

    // 创建删除命令
    auto command = std::make_unique<RemovePeakAreaToolCommand>(
        thermalChart,
        peakAreaTool,
        "移除峰面积工具"
    );

    // 通过 HistoryManager 执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "MainController::onPeakAreaToolRemoveRequested - 命令执行失败";
        return;
    }

    qDebug() << "MainController::onPeakAreaToolRemoveRequested - 成功删除工具";
}

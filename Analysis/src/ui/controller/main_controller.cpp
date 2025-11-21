#include "ui/controller/main_controller.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "ui/controller/curve_view_controller.h"
#include "ui/data_import_widget.h"
#include "ui/peak_area_dialog.h"
#include <QDebug>
#include <QtGlobal>

#include "application/algorithm/algorithm_manager.h"
#include "application/history/add_curve_command.h"
#include "application/history/clear_curves_command.h"
#include "application/history/remove_curve_command.h"
#include "application/history/add_mass_loss_tool_command.h"
#include "application/history/add_peak_area_tool_command.h"
#include "application/history/remove_mass_loss_tool_command.h"
#include "application/history/remove_peak_area_tool_command.h"
#include "application/history/history_manager.h"
#include "application/usecase/delete_curve_use_case.h"
#include "ui/chart_view.h"
#include "ui/controller/algorithm_execution_controller.h"
#include "ui/main_window.h"
#include "ui/presenter/message_presenter.h"
#include "ui/thermal_chart.h"
#include "ui/trapezoid_measure_tool.h"
#include "ui/peak_area_tool.h"
#include <memory>

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
}

void MainController::setPlotWidget(ChartView* plotWidget)
{
    m_plotWidget = plotWidget;

    if (!m_plotWidget) {
        return;
    }

    // 获取 ThermalChart 实例并连接工具删除请求信号
    ThermalChart* thermalChart = m_plotWidget->chart();
    if (thermalChart) {
        connect(thermalChart, &ThermalChart::massLossToolRemoveRequested,
                this, &MainController::onMassLossToolRemoveRequested, Qt::UniqueConnection);

        connect(thermalChart, &ThermalChart::peakAreaToolRemoveRequested,
                this, &MainController::onPeakAreaToolRemoveRequested, Qt::UniqueConnection);

        qDebug() << "MainController: 已连接视图工具删除请求信号";
    }

    // 连接工具创建请求信号（来自 ChartView，转发自 ThermalChartView）
    connect(m_plotWidget, &ChartView::massLossToolCreateRequested,
            this, &MainController::onMassLossToolCreateRequested, Qt::UniqueConnection);
    connect(m_plotWidget, &ChartView::peakAreaToolCreateRequested,
            this, &MainController::onPeakAreaToolCreateRequested, Qt::UniqueConnection);

    qDebug() << "MainController: 已连接视图工具创建请求信号";
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

void MainController::setMessagePresenter(MessagePresenter* presenter) { m_messagePresenter = presenter; }

void MainController::setAlgorithmExecutionController(AlgorithmExecutionController* controller)
{
    m_algorithmExecutionController = controller;
}

void MainController::setDeleteCurveUseCase(DeleteCurveUseCase* useCase)
{
    m_deleteCurveUseCase = useCase;
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
    Q_ASSERT(m_messagePresenter != nullptr);
    Q_ASSERT(m_algorithmExecutionController != nullptr);
    Q_ASSERT(m_deleteCurveUseCase != nullptr);

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

    if (!m_algorithmExecutionController) {
        qWarning() << "MainController: AlgorithmExecutionController 未配置，无法执行算法";
        return;
    }

    qDebug() << "MainController: 转发请求至 AlgorithmExecutionController:" << algorithmName;
    m_algorithmExecutionController->requestAlgorithmRun(algorithmName, params);
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

    if (!m_deleteCurveUseCase) {
        qWarning() << "MainController::onCurveDeleteRequested - DeleteCurveUseCase 未配置";
        return;
    }

    const DeleteCurveUseCase::Result result = m_deleteCurveUseCase->execute(curveId);

    switch (result.outcome) {
    case DeleteCurveUseCase::Outcome::Deleted:
        qDebug() << "MainController::onCurveDeleteRequested - 成功删除曲线:" << curveId
                 << (result.cascade ? "（包括子曲线）" : "");
        break;
    case DeleteCurveUseCase::Outcome::Cancelled:
        qDebug() << "MainController::onCurveDeleteRequested - 用户取消删除:" << curveId;
        break;
    case DeleteCurveUseCase::Outcome::NotFound:
        qWarning() << "MainController::onCurveDeleteRequested - 曲线不存在:" << curveId;
        break;
    case DeleteCurveUseCase::Outcome::Forbidden:
        qWarning() << "MainController::onCurveDeleteRequested - 不允许删除主曲线:" << curveId;
        break;
    case DeleteCurveUseCase::Outcome::Failed:
    default:
        qWarning() << "MainController::onCurveDeleteRequested - 删除曲线命令执行失败:" << curveId;
        break;
    }
}



void MainController::onPeakAreaToolRequested()
{
    Q_ASSERT(m_initialized);  // 确保依赖完整

    qDebug() << "MainController::onPeakAreaToolRequested - 峰面积工具请求";

    // 检查是否有可用曲线
    if (m_curveManager->getAllCurves().isEmpty()) {
        if (m_messagePresenter) {
            m_messagePresenter->showWarning(
                tr("无可用曲线"),
                tr("请先导入数据文件，才能使用峰面积测量工具。"));
        } else {
            qWarning() << "MainController::onPeakAreaToolRequested - MessagePresenter 未配置";
        }
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

    // 创建删除命令（使用 ChartView 而不是 ThermalChart）
    auto command = std::make_unique<RemoveMassLossToolCommand>(
        m_plotWidget,
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

    // 创建删除命令（使用 ChartView 而不是 ThermalChart）
    auto command = std::make_unique<RemovePeakAreaToolCommand>(
        m_plotWidget,
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

void MainController::onMassLossToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)
{
    Q_ASSERT(m_initialized);

    qDebug() << "MainController::onMassLossToolCreateRequested - 收到创建请求";

    // 创建添加命令
    auto command = std::make_unique<AddMassLossToolCommand>(
        m_plotWidget,
        point1,
        point2,
        curveId,
        "添加质量损失测量工具"
    );

    // 通过 HistoryManager 执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "MainController::onMassLossToolCreateRequested - 命令执行失败";
        return;
    }

    qDebug() << "MainController::onMassLossToolCreateRequested - 成功创建工具";
}

void MainController::onPeakAreaToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId,
                                                   bool useLinearBaseline, const QString& referenceCurveId)
{
    Q_ASSERT(m_initialized);

    qDebug() << "MainController::onPeakAreaToolCreateRequested - 收到创建请求";

    // 创建添加命令
    auto command = std::make_unique<AddPeakAreaToolCommand>(
        m_plotWidget,
        point1,
        point2,
        curveId,
        useLinearBaseline,
        referenceCurveId,
        "添加峰面积测量工具"
    );

    // 通过 HistoryManager 执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "MainController::onPeakAreaToolCreateRequested - 命令执行失败";
        return;
    }

    qDebug() << "MainController::onPeakAreaToolCreateRequested - 成功创建工具";
}

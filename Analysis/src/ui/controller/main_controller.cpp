#include "ui/controller/main_controller.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "infrastructure/io/text_file_reader.h"
#include "ui/controller/curve_view_controller.h"
#include "ui/data_import_widget.h"
#include <QDebug>
#include <QtGlobal>

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_coordinator.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/history/algorithm_command.h"
#include "application/history/history_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "ui/chart_view.h"
#include "ui/main_window.h"
#include <QMessageBox>
#include <memory>

MainController::MainController(CurveManager* curveManager, QObject* parent)
    : QObject(parent)
    , m_curveManager(curveManager)
    , m_dataImportWidget(new DataImportWidget())
    , m_textFileReader(new TextFileReader())
    , m_algorithmManager(AlgorithmManager::instance())
    , m_historyManager(&HistoryManager::instance())
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

    // 通知路径：AlgorithmManager → MainController（可选的转发）
    connect(m_algorithmManager, &AlgorithmManager::algorithmFinished, this, &MainController::onAlgorithmFinished);
}

MainController::~MainController()
{
    delete m_dataImportWidget;
    delete m_textFileReader;
}

void MainController::setPlotWidget(ChartView* plotWidget) { m_plotWidget = plotWidget; }

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

    // 算法接入（使用 lambda 适配不同参数的信号）
    connect(mainWindow, &MainWindow::algorithmRequested, this, [this](const QString& name) {
        onAlgorithmRequested(name, QVariantMap());
    }, Qt::UniqueConnection);

    connect(mainWindow, &MainWindow::newAlgorithmRequested, this, [this](const QString& name) {
        onAlgorithmRequested(name, QVariantMap());
    }, Qt::UniqueConnection);

    connect(mainWindow, &MainWindow::algorithmRequestedWithParams, this, &MainController::onAlgorithmRequested, Qt::UniqueConnection);
}

void MainController::setCurveViewController(CurveViewController* ViewController)
{
    m_curveViewController = ViewController;

    if (m_curveViewController) {
        connect(
            m_curveViewController, &CurveViewController::pointsPicked, this, &MainController::onPointsPickedFromView,
            Qt::UniqueConnection);
    }
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
        m_algorithmCoordinator, &AlgorithmCoordinator::showMessage, this, &MainController::onCoordinatorShowMessage,
        Qt::UniqueConnection);
    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::algorithmFailed, this,
        &MainController::onCoordinatorAlgorithmFailed, Qt::UniqueConnection);
    connect(
        m_algorithmCoordinator, &AlgorithmCoordinator::algorithmSucceeded, this,
        &MainController::onCoordinatorAlgorithmSucceeded, Qt::UniqueConnection);
}

void MainController::onShowDataImport() { m_dataImportWidget->show(); }

void MainController::onPreviewRequested(const QString& filePath)
{
    qDebug() << "控制器：收到预览文件请求：" << filePath;
    FilePreviewData preview = m_textFileReader->readPreview(filePath);
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

    // 2. 调用 m_textFileReader->read()，并传入配置
    ThermalCurve curve = m_textFileReader->read(filePath, config);

    if (curve.id().isEmpty()) {
        qWarning() << "导入失败：读取文件后曲线无效。";
        return;
    }

    // 3. 清空已有曲线，确保只保留最新导入
    m_curveManager->clearCurves();

    // 4. 调用 m_curveManager->addCurve() 添加新曲线
    m_curveManager->addCurve(curve);

    // 5. 设置新导入的曲线为活动曲线（默认选中）
    m_curveManager->setActiveCurve(curve.id());

    // 6. 发射信号，通知UI更新
    emit curveAvailable(curve);

    // 7. 关闭导入窗口
    m_dataImportWidget->close();
}

// ========== 处理命令的槽函数（命令路径：UI → Controller → Service） ==========
void MainController::onAlgorithmRequested(const QString& algorithmName, const QVariantMap& params)
{
    qDebug() << "MainController: 接收到算法执行请求：" << algorithmName
             << (params.isEmpty() ? "（无参数）" : "（带参数）");

    // 统一使用 AlgorithmCoordinator 架构
    if (!m_algorithmCoordinator) {
        qCritical() << "AlgorithmCoordinator 未初始化！无法执行算法：" << algorithmName;
        return;
    }

    m_algorithmCoordinator->handleAlgorithmTriggered(algorithmName, params);
}

void MainController::onAlgorithmFinished(const QString& curveId) { emit curveDataChanged(curveId); }

void MainController::onUndo()
{
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
    qDebug() << "MainController::onCurveDeleteRequested - 曲线ID:" << curveId;
    m_curveManager->removeCurve(curveId);
}

void MainController::onPointsPickedFromView(const QVector<QPointF>& points)
{
    if (!m_algorithmCoordinator) {
        qDebug() << "MainController::onPointsPickedFromView - 当前无算法协调器，忽略选点结果";
        return;
    }
    m_algorithmCoordinator->handlePointSelectionResult(points);
}

void MainController::onCoordinatorRequestPointSelection(
    const QString& algorithmName, const QString& curveId, int requiredPoints, const QString& hint)
{
    Q_UNUSED(algorithmName);
    Q_UNUSED(curveId);

    if (!m_curveViewController) {
        qWarning() << "MainController::onCoordinatorRequestPointSelection - 未绑定 CurveViewController";
        return;
    }

    m_curveViewController->setPickPointMode(qMax(1, requiredPoints));

    if (!hint.isEmpty()) {
        onCoordinatorShowMessage(hint);
    }
}

void MainController::onCoordinatorShowMessage(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    if (m_mainWindow) {
        QMessageBox::information(m_mainWindow, tr("提示"), text);
    } else {
        qInfo() << text;
    }
}

void MainController::onCoordinatorAlgorithmFailed(const QString& algorithmName, const QString& reason)
{
    qWarning() << "算法执行失败:" << algorithmName << reason;
    if (m_mainWindow) {
        QMessageBox::warning(
            m_mainWindow, tr("算法失败"), tr("算法 %1 执行失败：%2").arg(algorithmName, reason));
    }
}

void MainController::onCoordinatorAlgorithmSucceeded(const QString& algorithmName)
{
    qInfo() << "算法执行完成:" << algorithmName;
    // 预留：可在此刷新状态栏或提示用户
}

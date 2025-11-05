#include "ui/controller/main_controller.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "infrastructure/io/text_file_reader.h"
#include "ui/data_import_widget.h"
#include <QDebug>

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

void MainController::setPlotWidget(ChartView* plotWidget)
{
    m_plotWidget = plotWidget;
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
    connect(mainWindow, &MainWindow::peakAreaRequested, this, &MainController::onPeakAreaRequested, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::baselineRequested, this, &MainController::onBaselineRequested, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::algorithmRequested, this, &MainController::onAlgorithmRequested, Qt::UniqueConnection);
    connect(mainWindow, &MainWindow::algorithmRequestedWithParams, this, &MainController::onAlgorithmRequestedWithParams, Qt::UniqueConnection);
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
// TODO:目前是通过mainwindow 中直接调用，后续也改为信号槽链接
// 不带参数的算法调用
void MainController::onAlgorithmRequested(const QString& algorithmName)
{
    qDebug() << "MainController: 接收到算法执行请求：" << algorithmName;

    // 1. 业务规则检查
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法。";
        return;
    }

    // 2. 获取算法
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (!algorithm) {
        qWarning() << "找不到算法：" << algorithmName;
        return;
    }

    // 3. 创建新建曲线的算法命令
    auto command = std::make_unique<AlgorithmCommand>(algorithm, activeCurve, m_curveManager, algorithmName);

    // 4. 通过历史管理器执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "算法执行失败：" << algorithmName;
    }
    // 注意：新曲线的添加和UI更新由 CurveManager::curveAdded 信号自动触发
}
// TODO:目前是通过mainwindow 中直接调用，后续也改为信号槽链接
// 带参数的算法调用
void MainController::onAlgorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params)
{
    qDebug() << "带参数的算法调用";
    // 获取曲线
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        qWarning() << "没有可用的活动曲线来应用算法";
        return;
    }

    // 获取使用的算法
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (!algorithm) {
        qWarning() << "找不到算法" << algorithmName;
        return;
    }

    // 设置算法参数
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        algorithm->setParameter(it.key(), it.value());
    }

    // 创建算法命令
    auto command = std::make_unique<AlgorithmCommand>(algorithm, activeCurve, m_curveManager, algorithmName);

    // 通过历史管理器执行命令（支持撤销/重做）
    if (!m_historyManager->executeCommand(std::move(command))) {
        qWarning() << "算法执行失败：" << algorithmName;
    }

    // 注意：新曲线的添加和UI更新由 CurveManager::curveAdded 信号自动触发
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

// ========== 峰面积计算功能 ==========
void MainController::onPeakAreaRequested()
{
    qDebug() << "MainController::onPeakAreaRequested - 功能已停用";
    QMessageBox::information(m_mainWindow, tr("功能不可用"), tr("点拾取功能已移除，峰面积计算暂不可用。"));
}

// ========== 基线绘制功能 ==========
void MainController::onBaselineRequested()
{
    qDebug() << "MainController::onBaselineRequested - 功能已停用";
    QMessageBox::information(m_mainWindow, tr("功能不可用"), tr("点拾取功能已移除，基线绘制暂不可用。"));
}

// ========== 响应UI的曲线删除请求 ==========
void MainController::onCurveDeleteRequested(const QString& curveId)
{
    qDebug() << "MainController::onCurveDeleteRequested - 曲线ID:" << curveId;
    m_curveManager->removeCurve(curveId);
}


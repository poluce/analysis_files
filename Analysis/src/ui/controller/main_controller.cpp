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
    , m_historyManager(HistoryManager::instance())
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
    delete m_textFileReader;
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
            [this](const QString& algorithmName, const QVector<QPointF>& points) {
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
        // TODO: 放大和缩小功能需要在 ChartView 中实现
        // connect(mainWindow, &MainWindow::zoomInRequested, m_plotWidget, &ChartView::zoomIn, Qt::UniqueConnection);
        // connect(mainWindow, &MainWindow::zoomOutRequested, m_plotWidget, &ChartView::zoomOut, Qt::UniqueConnection);
    }

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

        // 用户确认，执行级联删除
        int totalDeleted = m_curveManager->removeCurveRecursively(curveId);
        qDebug() << "MainController::onCurveDeleteRequested - 级联删除完成，共删除" << totalDeleted
                 << "条曲线（包括子曲线）";
        return;
    }

    // 4. 没有子曲线，直接删除
    m_curveManager->removeCurve(curveId);
    qDebug() << "MainController::onCurveDeleteRequested - 成功删除曲线:" << curveId;
}


void MainController::onCoordinatorRequestPointSelection(
    const QString& algorithmName, const QString& curveId, int requiredPoints, const QString& hint)
{
    qDebug() << "MainController::onCoordinatorRequestPointSelection - 算法:" << algorithmName
             << ", 曲线ID:" << curveId
             << ", 需要点数:" << requiredPoints;

    if (!m_plotWidget) {
        qWarning() << "MainController::onCoordinatorRequestPointSelection - 未绑定 ChartView";
        return;
    }

    // ==================== 使用新的活动算法状态机 ====================
    // 获取算法的显示名称
    QString displayName = algorithmName;  // 默认使用算法名称
    IThermalAlgorithm* algorithm = m_algorithmManager->getAlgorithm(algorithmName);
    if (algorithm) {
        displayName = algorithm->displayName();
    } else {
        qWarning() << "MainController: 无法找到算法" << algorithmName << "，使用名称作为显示名";
    }

    // 启动算法交互状态机，传递曲线ID以确定选点标记应附着到哪个Y轴
    m_plotWidget->startAlgorithmInteraction(algorithmName, displayName, qMax(1, requiredPoints), hint, curveId);

    // 显示提示信息（如果有）
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

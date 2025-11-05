#include "main_window.h"
#include "application/history/history_manager.h"
#include "chart_view.h"
#include "project_explorer_view.h"

#include <QAction>
#include <QComboBox>
#include <QDebug>
#include <QDockWidget>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

MainWindow::MainWindow(ChartView* chartView, ProjectExplorerView* projectExplorer, QWidget* parent)
    : QMainWindow(parent)
    , m_projectExplorer(projectExplorer)
    , m_chartView(chartView)
{
    qDebug() << "构造:    MainWindow";

    resize(1600, 900);
    setWindowTitle(tr("热分析软件"));

    initRibbon();
    initCentral();
    initDockWidgets();
    initStatusBar();
    setupViewConnections();
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::on_toolButtonOpen_clicked()
{
    emit dataImportRequested();
}

void MainWindow::onProjectTreeContextMenuRequested(const QPoint& pos)
{
    QTreeView* tree = m_projectExplorer->treeView();

    const QModelIndex index = tree->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    const QString curveId = index.data(Qt::UserRole).toString();
    if (curveId.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction* deleteAction = menu.addAction(tr("删除"));
    if (menu.exec(tree->viewport()->mapToGlobal(pos)) == deleteAction) {
        emit curveDeleteRequested(curveId);
    }
}

// --- 主要初始化函数 ---

void MainWindow::initRibbon()
{
    qDebug() << "初始化功能区";
    QTabWidget* tabs = new QTabWidget();

    QWidget* fileTab = new QWidget();
    fileTab->setLayout(new QVBoxLayout());
    fileTab->layout()->setContentsMargins(0, 0, 0, 0);
    fileTab->layout()->addWidget(createFileToolBar());
    tabs->addTab(fileTab, tr("文件"));

    QWidget* viewTab = new QWidget();
    viewTab->setLayout(new QVBoxLayout());
    viewTab->layout()->setContentsMargins(0, 0, 0, 0);
    viewTab->layout()->addWidget(createViewToolBar());
    tabs->addTab(viewTab, tr("视图"));

    QWidget* mathTab = new QWidget();
    mathTab->setLayout(new QVBoxLayout());
    mathTab->layout()->setContentsMargins(0, 0, 0, 0);
    mathTab->layout()->addWidget(createMathToolBar());
    tabs->addTab(mathTab, tr("热力学"));

    setMenuWidget(tabs);
}

void MainWindow::initCentral()
{
    qDebug() << "初始化中央部件";
    m_chartView->setParent(this);
    setCentralWidget(m_chartView);
}

void MainWindow::initDockWidgets()
{
    qDebug() << "初始化停靠部件";
    setupLeftDock();
    setupRightDock();
}

void MainWindow::initStatusBar()
{
    statusBar()->showMessage(tr("准备就绪"), 3000);

    QLabel* versionLabel = new QLabel("v0.1.0-alpha");
    QLabel* zoomLabel = new QLabel(tr("缩放: 100%"));

    statusBar()->addPermanentWidget(versionLabel);
    statusBar()->addPermanentWidget(zoomLabel);
}

void MainWindow::setupViewConnections()
{
    QTreeView* tree = m_projectExplorer->treeView();

    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeView::customContextMenuRequested, this, &MainWindow::onProjectTreeContextMenuRequested);

    connect(m_projectExplorer, &ProjectExplorerView::deleteActionClicked, this, [this, tree]() {
        const QString curveId = tree->currentIndex().data(Qt::UserRole).toString();
        if (curveId.isEmpty()) {
            return;
        }
        emit curveDeleteRequested(curveId);
    });
}


void MainWindow::bindHistoryManager(HistoryManager& historyManager)
{
    m_historyManager = &historyManager;

    connect(&historyManager, &HistoryManager::historyChanged, this, &MainWindow::updateHistoryButtons);
    updateHistoryButtons();
}

// 创建文件工具栏
QToolBar* MainWindow::createFileToolBar()
{
    QToolBar* toolbar = new QToolBar();
    toolbar->addAction(style()->standardIcon(QStyle::SP_FileIcon), tr("新建项目"));
    toolbar->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("打开..."));
    toolbar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存"));
    toolbar->addSeparator();
    QAction* importDataAction = toolbar->addAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("导入数据..."));
    connect(importDataAction, &QAction::triggered, this, &MainWindow::on_toolButtonOpen_clicked);
    toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("导出图表..."));

    toolbar->addSeparator();

    // 添加撤销和重做按钮
    m_undoAction = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("撤销"));
    m_undoAction->setShortcut(QKeySequence::Undo);             // 绑定 Ctrl+z 快捷键
    m_undoAction->setShortcutContext(Qt::ApplicationShortcut); // 设置为应用程序级别快捷键
    m_undoAction->setEnabled(false);                           // 初始状态禁用
    this->addAction(m_undoAction);                             // 添加到 MainWindow 以确保快捷键全局可用
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undoRequested);

    m_redoAction = toolbar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("重做"));
    m_redoAction->setShortcut(QKeySequence::Redo);             // 绑定 Ctrl +y
    m_redoAction->setShortcutContext(Qt::ApplicationShortcut); // 设置为应用程序级别快捷键
    m_redoAction->setEnabled(false);                           // 初始状态禁用
    this->addAction(m_redoAction);                             // 添加到 MainWindow 以确保快捷键全局可用
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redoRequested);

    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    return toolbar;
}
// 创建视图
QToolBar* MainWindow::createViewToolBar()
{
    QToolBar* toolbar = new QToolBar(tr("视图"));
    toolbar->addAction(style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton), tr("放大"));
    toolbar->addAction(style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton), tr("缩小"));
    toolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("适应视图"));
    toolbar->addSeparator();
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    return toolbar;
}
// 创建数学页面
QToolBar* MainWindow::createMathToolBar()
{
    QToolBar* toolbar = new QToolBar();

    QAction* diffAction = toolbar->addAction(tr("微分算法"));
    diffAction->setData("differentiation");
    QAction* movAvgAction = toolbar->addAction(tr("移动平均滤波..."));

    QAction* integAction = toolbar->addAction(tr("积分"));
    integAction->setData("integration");

    toolbar->addSeparator();

    // 添加基线绘制按钮
    m_baselineAction = toolbar->addAction(tr("绘制基线"));
    m_baselineAction->setData("baseline_correction");
    // 添加峰面积计算按钮
    m_peakAreaAction = toolbar->addAction(tr("峰面积计算"));

//    connect(m_peakAreaAction, &QAction::triggered, this, [this]() {
//        qDebug() << "MainWindow: 峰面积按钮被点击";
//        emit peakAreaRequested();
//    });
//    connect(m_baselineAction, &QAction::triggered, this, [this]() {
//        qDebug() << "MainWindow: 基线按钮被点击";
//        emit baselineRequested();
//    });

    connect(m_baselineAction, &QAction::triggered, this, &MainWindow::onSimpleAlgorithmActionTriggered);
    connect(m_peakAreaAction, &QAction::triggered, this, &MainWindow::onSimpleAlgorithmActionTriggered);
    connect(diffAction, &QAction::triggered, this, &MainWindow::onSimpleAlgorithmActionTriggered);
    connect(movAvgAction, &QAction::triggered, this, &MainWindow::onMovingAverageAction);
    connect(integAction, &QAction::triggered, this, &MainWindow::onSimpleAlgorithmActionTriggered);

    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    return toolbar;
}

void MainWindow::setupLeftDock()
{
    m_projectExplorerDock = new QDockWidget(tr("项目浏览器"), this);
    m_projectExplorerDock->setWidget(m_projectExplorer);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectExplorerDock);
}

void MainWindow::setupRightDock()
{
    m_propertiesDock = new QDockWidget(tr("属性"), this);
    QWidget* propertiesWidget = new QWidget();
    QFormLayout* formLayout = new QFormLayout(propertiesWidget);

    formLayout->addRow(tr("名称:"), new QLineEdit("Sample 1"));
    QComboBox* modelCombo = new QComboBox();
    modelCombo->addItems({ "Model-Free", "Model-Based", "ASTM E698" });
    formLayout->addRow(tr("分析模型:"), modelCombo);
    formLayout->addRow(tr("升温速率:"), new QLineEdit("10 K/min"));

    m_propertiesDock->setWidget(propertiesWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
}
// 执行简单的通用算法
void MainWindow::onSimpleAlgorithmActionTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    QString algorithmName = action->data().toString();
    if (!algorithmName.isEmpty()) {
        emit algorithmRequested(algorithmName);
    }
}

void MainWindow::onMovingAverageAction()
{
    bool ok = false;
    int window = QInputDialog::getInt(this, tr("移动平均滤波"), tr("窗口大小 (点数):"), 5, 1, 999, 1, &ok);
    if (!ok) {
        return;
    }
    QVariantMap params;
    params.insert("window", window);
    emit algorithmRequestedWithParams("moving_average", params);
}


void MainWindow::updateHistoryButtons()
{
    if (!m_historyManager) {
        return;
    }

    const bool canUndo = m_historyManager->canUndo();
    const bool canRedo = m_historyManager->canRedo();

    m_undoAction->setEnabled(canUndo);
    m_redoAction->setEnabled(canRedo);

    qDebug() << "历史状态更新: 可撤销=" << canUndo << ", 可重做=" << canRedo;
}

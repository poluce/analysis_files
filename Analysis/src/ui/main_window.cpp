#include "main_window.h"
#include "application/history/history_manager.h"
#include "chart_view.h"
#include "project_explorer_view.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDockWidget>
#include <QFile>
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

    initCentral();
    initDockWidgets(); // 必须在 initRibbon() 之前，因为 Ribbon 中的视图工具栏需要引用停靠面板
    initRibbon();
    initStatusBar();
    setupViewConnections();
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::onOpenButtonClicked() { emit dataImportRequested(); }

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
    toolbar->addAction(QIcon("../../icons/new_file.svg"), tr("新建项目"));
    toolbar->addAction(QIcon("../../icons/open_folder.svg"), tr("打开..."));
    toolbar->addAction(QIcon("../../icons/save.svg"), tr("保存"));
    toolbar->addSeparator();
    QAction* importDataAction = toolbar->addAction(QIcon("../../icons/import.svg"), tr("导入数据..."));
    connect(importDataAction, &QAction::triggered, this, &MainWindow::onOpenButtonClicked);
    toolbar->addAction(QIcon("../../icons/export.svg"), tr("导出图表..."));

    toolbar->addSeparator();

    // 添加撤销和重做按钮
    m_undoAction = toolbar->addAction(QIcon("../../icons/undo.svg"), tr("撤销"));
    m_undoAction->setShortcut(QKeySequence::Undo);             // 绑定 Ctrl+z 快捷键
    m_undoAction->setShortcutContext(Qt::ApplicationShortcut); // 设置为应用程序级别快捷键
    m_undoAction->setEnabled(false);                           // 初始状态禁用
    this->addAction(m_undoAction);                             // 添加到 MainWindow 以确保快捷键全局可用
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undoRequested);

    m_redoAction = toolbar->addAction(QIcon("../../icons/redo.svg"), tr("重做"));
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

    // 放大按钮
    QAction* zoomInAction = toolbar->addAction(QIcon("../../icons/zoom_in.svg"), tr("放大"));
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomInRequested);

    // 缩小按钮
    QAction* zoomOutAction = toolbar->addAction(QIcon("../../icons/zoom_out.svg"), tr("缩小"));
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOutRequested);

    // 适应视图按钮
    QAction* fitViewAction = toolbar->addAction(QIcon("../../icons/fit_view.svg"), tr("适应视图"));
    connect(fitViewAction, &QAction::triggered, this, &MainWindow::fitViewRequested);

    toolbar->addSeparator();

    // 项目浏览器切换按钮
    m_toggleProjectExplorerAction = m_projectExplorerDock->toggleViewAction();
    m_toggleProjectExplorerAction->setText(tr("项目浏览器"));
    m_toggleProjectExplorerAction->setIcon(QIcon("../../icons/toggle_project.svg"));
    toolbar->addAction(m_toggleProjectExplorerAction);

    // 属性窗口切换按钮
    m_togglePropertiesAction = m_propertiesDock->toggleViewAction();
    m_togglePropertiesAction->setText(tr("属性窗口"));
    m_togglePropertiesAction->setIcon(QIcon("../../icons/toggle_properties.svg"));
    toolbar->addAction(m_togglePropertiesAction);

    toolbar->addSeparator();

    // 恢复默认布局按钮
    QAction* resetLayoutAction = toolbar->addAction(QIcon("../../icons/reset_layout.svg"), tr("恢复布局"));
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetLayoutRequested);

    toolbar->addSeparator();

    // 主题切换按钮
    QAction* themeAction = toolbar->addAction(style()->standardIcon(QStyle::SP_DesktopIcon), tr("切换主题"));
    connect(themeAction, &QAction::triggered, this, &MainWindow::onThemeToggleRequested);

    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    return toolbar;
}
// 创建数学页面
QToolBar* MainWindow::createMathToolBar()
{
    QToolBar* toolbar = new QToolBar();

    QAction* diffAction = toolbar->addAction(tr("微分算法"));
    diffAction->setData("differentiation");
    diffAction->setIcon(QIcon("../../icons/differentiation.svg"));

    QAction* movAvgAction = toolbar->addAction(tr("移动平均滤波..."));
    movAvgAction->setData("moving_average");  // 统一路径：设置 data
    movAvgAction->setIcon(QIcon("../../icons/moving_average.svg"));

    QAction* integAction = toolbar->addAction(tr("积分"));
    integAction->setData("integration");
    integAction->setIcon(QIcon("../../icons/integration.svg"));

    toolbar->addSeparator();

    // 添加基线校正按钮
    QAction* baselineAction = toolbar->addAction(tr("基线校正"));
    baselineAction->setData("baseline_correction");
    baselineAction->setIcon(QIcon("../../icons/baseline_correction.svg"));
    // 添加外推温度按钮
    QAction* tempExtrapolationAction = toolbar->addAction(tr("外推温度"));
    tempExtrapolationAction->setData("temperature_extrapolation");
    tempExtrapolationAction->setIcon(QIcon("../../icons/temperature_extrapolation.svg"));
    // 添加峰面积测量工具按钮（可交互式）
    QAction* peakAreaAction = toolbar->addAction(tr("峰面积"));
    peakAreaAction->setIcon(QIcon("../../icons/peak_area.svg"));
    connect(peakAreaAction, &QAction::triggered, this, &MainWindow::onPeakAreaToolRequested);
    // 添加质量损失测量工具按钮
    QAction* massLossAction = toolbar->addAction(tr("质量损失"));
    massLossAction->setIcon(QIcon("../../icons/mass_loss.svg"));
    connect(massLossAction, &QAction::triggered, this, &MainWindow::onMassLossToolRequested);

    // 统一路径：所有算法使用统一触发器
    connect(movAvgAction, &QAction::triggered, this, &MainWindow::onAlgorithmActionTriggered);
    connect(baselineAction, &QAction::triggered, this, &MainWindow::onAlgorithmActionTriggered);
    connect(tempExtrapolationAction, &QAction::triggered, this, &MainWindow::onAlgorithmActionTriggered);
    connect(diffAction, &QAction::triggered, this, &MainWindow::onAlgorithmActionTriggered);
    connect(integAction, &QAction::triggered, this, &MainWindow::onAlgorithmActionTriggered);

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
// 通用算法触发槽（统一处理，减少代码重复）
void MainWindow::onAlgorithmActionTriggered()
{
    auto* action = qobject_cast<QAction*>(sender());
    const QString algorithmName = action->data().toString();
    if (!algorithmName.isEmpty()) {
        emit algorithmRequested(algorithmName);
    }
}

void MainWindow::onMassLossToolRequested()
{
    qDebug() << "MainWindow::onMassLossToolRequested - 请求激活质量损失测量工具";
    emit massLossToolRequested();
}

void MainWindow::onPeakAreaToolRequested()
{
    qDebug() << "MainWindow::onPeakAreaToolRequested - 请求激活峰面积测量工具";
    emit peakAreaToolRequested();
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

void MainWindow::onResetLayoutRequested()
{
    qDebug() << "MainWindow::onResetLayoutRequested - 恢复默认布局";

    // 显示所有停靠面板
    m_projectExplorerDock->show();
    m_propertiesDock->show();

    // 恢复默认停靠位置
    addDockWidget(Qt::LeftDockWidgetArea, m_projectExplorerDock);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 取消浮动状态（如果面板被拖出主窗口）
    m_projectExplorerDock->setFloating(false);
    m_propertiesDock->setFloating(false);

    statusBar()->showMessage(tr("布局已恢复"), 2000);
}

void MainWindow::onThemeToggleRequested()
{
    // 切换主题状态
    m_isDarkTheme = !m_isDarkTheme;

    // 选择对应的样式表文件
    QString stylePath = m_isDarkTheme ? "../../styles/fluent_dark.qss" : "../../styles/fluent_light.qss";

    QFile styleFile(stylePath);
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = styleFile.readAll();
        qApp->setStyleSheet(styleSheet);
        styleFile.close();

        // 更新图表主题（Qt Charts 需要单独设置）
        if (m_chartView) {
            m_chartView->setDarkTheme(m_isDarkTheme);
        }

        QString themeName = m_isDarkTheme ? tr("暗色主题") : tr("亮色主题");
        statusBar()->showMessage(tr("已切换到 %1").arg(themeName), 2000);
        qDebug() << "MainWindow::onThemeToggleRequested - 切换到" << themeName;
    } else {
        qWarning() << "无法加载样式表文件:" << stylePath;
        statusBar()->showMessage(tr("无法加载主题文件"), 2000);
    }
}

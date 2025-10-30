#include "mainwindow.h"
#include "PlotWidget.h"
#include "application/curve/CurveManager.h"
#include "ui/controller/MainController.h"
#include "domain/model/ThermalCurve.h"

#include <QDockWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QIcon>
#include <QDebug>
#include <QStandardItemModel>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QStyle>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
  qDebug() << "MainWindow constructor called.";
  resize(1600, 900);
  setWindowTitle(tr("热分析软件"));

  initRibbon();
  initCentral();
  initDockWidgets();
  initStatusBar();
  initComponents(); // 初始化组件
  
  setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
    // 析构函数体。子QObject会被自动删除。
}

void MainWindow::initComponents()
{
    m_curveManager = new CurveManager(this);
    m_mainController = new MainController(m_curveManager, this);

    // 连接控制器信号到主窗口的槽，用于更新项目浏览器
    connect(m_mainController, &MainController::curveAvailable, this, &MainWindow::onCurveAvailable);

    // 连接控制器信号到图表视图，用于绘制曲线
    connect(m_mainController, &MainController::curveAvailable, m_chartView, &PlotWidget::addCurve);
}

void MainWindow::on_toolButtonOpen_clicked()
{
    // 调用控制器显示导入窗口
    m_mainController->onShowDataImport();
}

void MainWindow::onCurveAvailable(const ThermalCurve& curve)
{
    qDebug() << "MainWindow: Received curve" << curve.name();
    // 在树模型中查找“导入文件”项
    QStandardItem* rootItem = m_projectTreeModel->invisibleRootItem();
    QStandardItem* importationsItem = nullptr;
    for (int i = 0; i < rootItem->rowCount(); ++i) {
        if (rootItem->child(i)->text() == tr("导入文件")) {
            importationsItem = rootItem->child(i);
            break;
        }
    }

    if (!importationsItem) {
        // 如果该项不存在，则创建它
        importationsItem = new QStandardItem(tr("导入文件"));
        rootItem->appendRow(importationsItem);
    }

    // 将新曲线名称添加到树中
    QStandardItem* curveItem = new QStandardItem(curve.name());
    curveItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    curveItem->setCheckable(true);
    curveItem->setCheckState(Qt::Checked);
    importationsItem->appendRow(curveItem);
}


// --- 主要初始化函数 ---

void MainWindow::initRibbon() {
  qDebug() << "initRibbon called.";
  
  QTabWidget* tabs = new QTabWidget();
  
  // 创建带有独立工具栏的标签页
  QWidget* fileTab = new QWidget();
  fileTab->setLayout(new QVBoxLayout());
  fileTab->layout()->setContentsMargins(0,0,0,0);
  fileTab->layout()->addWidget(createFileToolBar());
  tabs->addTab(fileTab, tr("文件"));

  QWidget* viewTab = new QWidget();
  viewTab->setLayout(new QVBoxLayout());
  viewTab->layout()->setContentsMargins(0,0,0,0);
  viewTab->layout()->addWidget(createViewToolBar());
  tabs->addTab(viewTab, tr("视图"));

  QWidget* mathTab = new QWidget();
  mathTab->setLayout(new QVBoxLayout());
  mathTab->layout()->setContentsMargins(0,0,0,0);
  mathTab->layout()->addWidget(createMathToolBar());
  tabs->addTab(mathTab, tr("数学工具"));

  setMenuWidget(tabs);
}

void MainWindow::initCentral() {
  qDebug() << "initCentral called.";
  m_chartView = new PlotWidget;
  setCentralWidget(m_chartView);
}

void MainWindow::initDockWidgets() {
  qDebug() << "initDockWidgets called.";
  setupLeftDock();
  setupRightDock();
}

void MainWindow::initStatusBar() {
    statusBar()->showMessage(tr("准备就绪"), 3000);

    QLabel *versionLabel = new QLabel("v0.1.0-alpha");
    QLabel *zoomLabel = new QLabel(tr("缩放: 100%"));
    
    statusBar()->addPermanentWidget(versionLabel);
    statusBar()->addPermanentWidget(zoomLabel);
}

// --- UI 设置助手函数 ---

QToolBar* MainWindow::createFileToolBar() {
    QToolBar* toolbar = new QToolBar(tr("文件"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_FileIcon), tr("新建项目"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogOpenButton), tr("打开..."));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存"));
    toolbar->addSeparator();
    QAction* importDataAction = toolbar->addAction(this->style()->standardIcon(QStyle::SP_DirOpenIcon), tr("导入数据..."));
    connect(importDataAction, &QAction::triggered, this, &MainWindow::on_toolButtonOpen_clicked);
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowUp), tr("导出图表..."));
    return toolbar;
}

QToolBar* MainWindow::createViewToolBar() {
    QToolBar* toolbar = new QToolBar(tr("视图"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton), tr("放大"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton), tr("缩小"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_BrowserReload), tr("适应视图"));
    toolbar->addSeparator();
    toolbar->addAction(tr("平移工具"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowRight), tr("选择工具"));
    return toolbar;
}

QToolBar* MainWindow::createMathToolBar() {
    QToolBar* toolbar = new QToolBar(tr("数学"));
    toolbar->addAction(tr("基线校正"));
    toolbar->addAction(tr("归一化"));
    QAction* diffAction = toolbar->addAction(tr("微分算法"));
    toolbar->addAction(tr("动力学计算..."));
    connect(diffAction, &QAction::triggered, this, &MainWindow::onDifferentialAlgorithmAction);
    return toolbar;
}

void MainWindow::setupLeftDock() {
  m_projectExplorerDock = new QDockWidget(tr("项目浏览器"), this);
  m_projectExplorer = new ProjectExplorer(this);

  m_projectTreeModel = new QStandardItemModel(this);
  QStandardItem *parentItem = m_projectTreeModel->invisibleRootItem();

  QStandardItem *importItem = new QStandardItem(tr("导入文件"));
  parentItem->appendRow(importItem);

  m_projectExplorer->setModel(m_projectTreeModel);

  m_projectExplorerDock->setWidget(m_projectExplorer);
  addDockWidget(Qt::LeftDockWidgetArea, m_projectExplorerDock);
}

void MainWindow::setupRightDock() {
  m_propertiesDock = new QDockWidget(tr("属性"), this);
  QWidget* propertiesWidget = new QWidget();
  QFormLayout* formLayout = new QFormLayout(propertiesWidget);

  formLayout->addRow(tr("名称:"), new QLineEdit("Sample 1"));
  QComboBox* modelCombo = new QComboBox();
  modelCombo->addItems({"Model-Free", "Model-Based", "ASTM E698"});
  formLayout->addRow(tr("分析模型:"), modelCombo);
  formLayout->addRow(tr("升温速率:"), new QLineEdit("10 K/min"));

  m_propertiesDock->setWidget(propertiesWidget);
  addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
}

void MainWindow::onDifferentialAlgorithmAction()
{
    // TODO: 检查是否有激活的曲线
    m_mainController->onAlgorithmRequested("differentiation");
}


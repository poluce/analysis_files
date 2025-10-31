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
#include <QInputDialog>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    qDebug() << "主窗口构造函数被调用";
    resize(1600, 900);
    setWindowTitle(tr("热分析软件"));

    // 先创建 CurveManager（需要传递给 PlotWidget）
    m_curveManager = new CurveManager(this);

    initRibbon();
    initCentral();
    initDockWidgets();
    initStatusBar();
    initComponents();
    setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::initComponents()
{
    // m_curveManager 已在构造函数中创建
    m_mainController = new MainController(m_curveManager, this);

    // ========== 信号连接（命令路径：UI → Controller） ==========
    // 更新项目浏览器
    connect(m_mainController, &MainController::curveAvailable, this, &MainWindow::onCurveAvailable);

    // ========== 信号连接（通知路径：Service → UI） ==========
    // PlotWidget 已自动监听 CurveManager::curveAdded，无需重复连接
    // 算法生成新曲线 → 更新项目浏览器
    connect(m_curveManager, &CurveManager::curveAdded, this, &MainWindow::onCurveAdded);

    // ========== UI 内部信号 ==========
    // 选中曲线 → 设置活动曲线
    connect(m_chartView, &PlotWidget::curveSelected, m_curveManager, &CurveManager::setActiveCurve);
}

void MainWindow::on_toolButtonOpen_clicked()
{
    m_mainController->onShowDataImport();
}

void MainWindow::onCurveAvailable(const ThermalCurve &curve)
{
    qDebug() << "主窗口：接收到曲线" << curve.name();

    // 在树模型中查找“导入文件”项
    QStandardItem *rootItem = m_projectTreeModel->invisibleRootItem();
    QStandardItem *importationsItem = nullptr;
    for (int i = 0; i < rootItem->rowCount(); ++i) {
        if (rootItem->child(i)->text() == tr("导入文件")) {
            importationsItem = rootItem->child(i);
            break;
        }
    }

    if (!importationsItem) {
        importationsItem = new QStandardItem(tr("导入文件"));
        rootItem->appendRow(importationsItem);
    }

    QString displayName = curve.name();
    const QString sourceFile = curve.getMetadata().additional.value("source_file").toString();
    if (!sourceFile.isEmpty()) {
        QFileInfo fileInfo(sourceFile);
        if (!fileInfo.fileName().isEmpty()) {
            displayName = fileInfo.fileName();
        }
    }

    QStandardItem *curveItem = new QStandardItem(displayName);
    curveItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    curveItem->setCheckable(true);
    curveItem->setCheckState(Qt::Checked);
    if (!sourceFile.isEmpty()) {
        curveItem->setToolTip(tr("信号: %1\n文件: %2").arg(curve.name(), sourceFile));
    }
    importationsItem->appendRow(curveItem);
}

void MainWindow::onCurveAdded(const QString &curveId)
{
    ThermalCurve *curve = m_curveManager->getCurve(curveId);
    if (curve) {
        // 更新项目浏览器
        onCurveAvailable(*curve);
        // PlotWidget 已通过监听 CurveManager::curveAdded 自动绘制曲线
        // 无需手动调用 m_chartView->addCurve(*curve);
    }
}

// --- 主要初始化函数 ---

void MainWindow::initRibbon()
{
    qDebug() << "初始化功能区";

    QTabWidget *tabs = new QTabWidget();

    QWidget *fileTab = new QWidget();
    fileTab->setLayout(new QVBoxLayout());
    fileTab->layout()->setContentsMargins(0, 0, 0, 0);
    fileTab->layout()->addWidget(createFileToolBar());
    tabs->addTab(fileTab, tr("文件"));

    QWidget *viewTab = new QWidget();
    viewTab->setLayout(new QVBoxLayout());
    viewTab->layout()->setContentsMargins(0, 0, 0, 0);
    viewTab->layout()->addWidget(createViewToolBar());
    tabs->addTab(viewTab, tr("视图"));

    QWidget *mathTab = new QWidget();
    mathTab->setLayout(new QVBoxLayout());
    mathTab->layout()->setContentsMargins(0, 0, 0, 0);
    mathTab->layout()->addWidget(createMathToolBar());
    tabs->addTab(mathTab, tr("数学工具"));

    setMenuWidget(tabs);
}

void MainWindow::initCentral()
{
    qDebug() << "初始化中央部件";
    m_chartView = new PlotWidget(m_curveManager, this);
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

    QLabel *versionLabel = new QLabel("v0.1.0-alpha");
    QLabel *zoomLabel = new QLabel(tr("缩放: 100%"));

    statusBar()->addPermanentWidget(versionLabel);
    statusBar()->addPermanentWidget(zoomLabel);
}

QToolBar *MainWindow::createFileToolBar()
{
    QToolBar *toolbar = new QToolBar(tr("文件"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_FileIcon), tr("新建项目"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogOpenButton), tr("打开..."));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存"));
    toolbar->addSeparator();
    QAction *importDataAction = toolbar->addAction(this->style()->standardIcon(QStyle::SP_DirOpenIcon), tr("导入数据..."));
    connect(importDataAction, &QAction::triggered, this, &MainWindow::on_toolButtonOpen_clicked);
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowUp), tr("导出图表..."));
    return toolbar;
}

QToolBar *MainWindow::createViewToolBar()
{
    QToolBar *toolbar = new QToolBar(tr("视图"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton), tr("放大"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton), tr("缩小"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_BrowserReload), tr("适应视图"));
    toolbar->addSeparator();
    toolbar->addAction(tr("平移工具"));
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowRight), tr("选择工具"));
    return toolbar;
}

QToolBar *MainWindow::createMathToolBar()
{
    QToolBar *toolbar = new QToolBar(tr("数学"));
    toolbar->addAction(tr("基线校正"));
    toolbar->addAction(tr("归一化"));
    QAction *diffAction = toolbar->addAction(tr("微分算法"));
    QAction *movAvgAction = toolbar->addAction(tr("移动平均滤波..."));
    QAction *integAction = toolbar->addAction(tr("积分"));
    QAction *kinAction = toolbar->addAction(tr("动力学计算..."));
    connect(diffAction, &QAction::triggered, this, &MainWindow::onDifferentialAlgorithmAction);
    connect(movAvgAction, &QAction::triggered, this, &MainWindow::onMovingAverageAction);
    connect(integAction, &QAction::triggered, this, &MainWindow::onIntegrationAction);
    Q_UNUSED(kinAction);
    return toolbar;
}

void MainWindow::setupLeftDock()
{
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

void MainWindow::setupRightDock()
{
    m_propertiesDock = new QDockWidget(tr("属性"), this);
    QWidget *propertiesWidget = new QWidget();
    QFormLayout *formLayout = new QFormLayout(propertiesWidget);

    formLayout->addRow(tr("名称:"), new QLineEdit("Sample 1"));
    QComboBox *modelCombo = new QComboBox();
    modelCombo->addItems({"Model-Free", "Model-Based", "ASTM E698"});
    formLayout->addRow(tr("分析模型:"), modelCombo);
    formLayout->addRow(tr("升温速率:"), new QLineEdit("10 K/min"));

    m_propertiesDock->setWidget(propertiesWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
}

void MainWindow::onDifferentialAlgorithmAction()
{
    m_mainController->onAlgorithmRequested("differentiation");
}

void MainWindow::onMovingAverageAction()
{
    bool ok = false;
    int window = QInputDialog::getInt(this,
                                      tr("移动平均滤波"),
                                      tr("窗口大小 (点数):"),
                                      5,
                                      1,
                                      999,
                                      1,
                                      &ok);
    if (!ok) {
        return;
    }
    QVariantMap params;
    params.insert("window", window);
    m_mainController->onAlgorithmRequestedWithParams("moving_average", params);
}

void MainWindow::onIntegrationAction()
{
    m_mainController->onAlgorithmRequested("integration");
}


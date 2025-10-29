#include "mainwindow.h"
#include "PlotWidget.h"
#include "form.h" // Include the Form header
#include <QTreeView>
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
#include <QTreeWidgetItem> // Added for QTreeWidgetItem

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
    , form(nullptr) // Initialize form pointer
{
  qDebug() << "MainWindow constructor called.";
  resize(1600, 900);
  setWindowTitle("Thermal Analysis Software");

  initRibbon();
  initCentral();
  initDockWidgets();
  initStatusBar();
  
  setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
    // Destructor body
}

void MainWindow::on_toolButtonOpen_clicked()
{
    if (!form) {
        form = new Form;
        connect(form, &Form::dataReady, this, &MainWindow::onDataReady);
    }
    form->show();
}

void MainWindow::onDataReady(const QVariantMap& data)
{
    int index = dataList.size() + 1;

    QVariantMap newData;
    temperatureKey = QString("温度%1").arg(index);
    timeKey = QString("时间%1").arg(index);
    customColumnKey = QString("%1%2").arg(form->lineEdit_5()).arg(index);
    velocityKey = QString("速率%1").arg(index);

    // 优化: 使用 const 引用避免复制,并预分配内存
    const QVariantList& tempList = data.value("温度").toList();
    const QVariantList& timeList = data.value("时间").toList();
    const QVariantList& customList = data.value(form->lineEdit_5()).toList();

    QList<double> temperatureData;
    QList<double> timeData;
    QList<double> customData;
    QList<double> velocityData;

    temperatureData.reserve(tempList.size());
    timeData.reserve(timeList.size());
    customData.reserve(customList.size());

    for (const QVariant& v : tempList) {
        temperatureData << v.toDouble();
    }
    for (const QVariant& v : timeList) {
        timeData << v.toDouble();
    }
    for (const QVariant& v : customList) {
        customData << v.toDouble();
    }

    if (data.contains("速率")) {
        const QVariantList& velocityList = data.value("速率").toList();
        velocityData.reserve(velocityList.size());
        for (const QVariant& v : velocityList) {
            velocityData << v.toDouble();
        }
    }

    newData[temperatureKey] = QVariant::fromValue(temperatureData);
    newData[timeKey] = QVariant::fromValue(timeData);
    newData[customColumnKey] = QVariant::fromValue(customData);
    if (!velocityData.isEmpty()) {
        newData[velocityKey] = QVariant::fromValue(velocityData);
    }

    this->dataList.append(newData);

    QString textFromLineEdit = form->textEdit();

    QStandardItem* textEditItem = new QStandardItem(textFromLineEdit);
    textEditItem->setToolTip(textFromLineEdit);
    textEditItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    treeModel_->invisibleRootItem()->appendRow(textEditItem);

    QStandardItem* temperatureItem = new QStandardItem(temperatureKey);
    temperatureItem->setFlags(temperatureItem->flags() | Qt::ItemIsUserCheckable);
    temperatureItem->setCheckState(Qt::Unchecked);
    textEditItem->appendRow(temperatureItem);

    QStandardItem* timeItem = new QStandardItem(timeKey);
    timeItem->setFlags(timeItem->flags() | Qt::ItemIsUserCheckable);
    timeItem->setCheckState(Qt::Unchecked);
    textEditItem->appendRow(timeItem);

    QStandardItem* customColumnItem = new QStandardItem(customColumnKey);
    customColumnItem->setFlags(customColumnItem->flags() | Qt::ItemIsUserCheckable);
    customColumnItem->setCheckState(Qt::Unchecked);
    textEditItem->appendRow(customColumnItem);
}

// --- Main Initializers ---

void MainWindow::initRibbon() {
  qDebug() << "initRibbon called.";
  
  QTabWidget* tabs = new QTabWidget();
  
  // Create tabs with their own toolbars
  QWidget* fileTab = new QWidget();
  fileTab->setLayout(new QVBoxLayout());
  fileTab->layout()->setContentsMargins(0,0,0,0);
  fileTab->layout()->addWidget(createFileToolBar());
  tabs->addTab(fileTab, "File");

  QWidget* viewTab = new QWidget();
  viewTab->setLayout(new QVBoxLayout());
  viewTab->layout()->setContentsMargins(0,0,0,0);
  viewTab->layout()->addWidget(createViewToolBar());
  tabs->addTab(viewTab, "View");

  QWidget* mathTab = new QWidget();
  mathTab->setLayout(new QVBoxLayout());
  mathTab->layout()->setContentsMargins(0,0,0,0);
  mathTab->layout()->addWidget(createMathToolBar());
  tabs->addTab(mathTab, "Math Functions");

  setMenuWidget(tabs);
}

void MainWindow::initCentral() {
  qDebug() << "initCentral called.";
  plotWidget_ = new PlotWidget;
  setCentralWidget(plotWidget_);
}

void MainWindow::initDockWidgets() {
  qDebug() << "initDockWidgets called.";
  setupLeftDock();
  setupRightDock();
}

void MainWindow::initStatusBar() {
    statusBar()->showMessage("Ready", 3000);

    QLabel *versionLabel = new QLabel("v0.1.0-alpha");
    QLabel *zoomLabel = new QLabel("Zoom: 100%");
    
    statusBar()->addPermanentWidget(versionLabel);
    statusBar()->addPermanentWidget(zoomLabel);
}

// --- UI Setup Helpers ---

QToolBar* MainWindow::createFileToolBar() {
    QToolBar* toolbar = new QToolBar("File");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_FileIcon), "New Project");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogOpenButton), "Open...");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_DialogSaveButton), "Save");
    toolbar->addSeparator();
    QAction* importDataAction = toolbar->addAction(this->style()->standardIcon(QStyle::SP_DirOpenIcon), "Import Data...");
    connect(importDataAction, &QAction::triggered, this, &MainWindow::on_toolButtonOpen_clicked);
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowUp), "Export Chart...");
    return toolbar;
}

QToolBar* MainWindow::createViewToolBar() {
    QToolBar* toolbar = new QToolBar("View");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarHorizontalExtensionButton), "Zoom In");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ToolBarVerticalExtensionButton), "Zoom Out");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_BrowserReload), "Fit View");
    toolbar->addSeparator();
    toolbar->addAction("Pan Tool");
    toolbar->addAction(this->style()->standardIcon(QStyle::SP_ArrowRight), "Select Tool");
    return toolbar;
}

QToolBar* MainWindow::createMathToolBar() {
    QToolBar* toolbar = new QToolBar("Math");
    toolbar->addAction("Baseline Correction");
    toolbar->addAction("Normalize");
    toolbar->addAction("Calculate Kinetics...");
    return toolbar;
}

void MainWindow::setupLeftDock() {
  leftDock_ = new QDockWidget("Project Explorer", this);
  leftTree_ = new QTreeView;
  leftTree_->setHeaderHidden(true);
  
  treeModel_ = new QStandardItemModel(this);
  QStandardItem *parentItem = treeModel_->invisibleRootItem();

  QStandardItem *importItem = new QStandardItem("Importations");
  parentItem->appendRow(importItem);

  QStandardItem *sample1 = new QStandardItem("Sample 1 (DSC)");
  sample1->setCheckable(true);
  sample1->setCheckState(Qt::Checked);
  importItem->appendRow(sample1);

  QStandardItem *heatFlow = new QStandardItem("Heat Flow");
  heatFlow->setCheckable(true);
  heatFlow->setCheckState(Qt::Checked);
  sample1->appendRow(heatFlow);

  QStandardItem *temp = new QStandardItem("Temperature");
  temp->setCheckable(true);
  sample1->appendRow(temp);

  QStandardItem *kineticsItem = new QStandardItem("Kinetic Results");
  parentItem->appendRow(kineticsItem);
  
  leftTree_->setModel(treeModel_);
  leftTree_->expandAll();

  leftDock_->setWidget(leftTree_);
  addDockWidget(Qt::LeftDockWidgetArea, leftDock_);
}

void MainWindow::setupRightDock() {
  rightDock_ = new QDockWidget("Properties", this);
  QWidget* propertiesWidget = new QWidget();
  QFormLayout* formLayout = new QFormLayout(propertiesWidget);

  formLayout->addRow("Name:", new QLineEdit("Sample 1"));
  QComboBox* modelCombo = new QComboBox();
  modelCombo->addItems({"Model-Free", "Model-Based", "ASTM E698"});
  formLayout->addRow("Analysis Model:", modelCombo);
  formLayout->addRow("Heating Rate:", new QLineEdit("10 K/min"));

  rightDock_->setWidget(propertiesWidget);
  addDockWidget(Qt::RightDockWidgetArea, rightDock_);
}

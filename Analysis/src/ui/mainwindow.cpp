#include "mainwindow.h"
#include "PlotWidget.h"
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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
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

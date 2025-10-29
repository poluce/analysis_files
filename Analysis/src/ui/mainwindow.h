#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QTreeView;
class PlotWidget;
class QDockWidget;
class QStandardItemModel;
class QToolBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // Initializers
    void initRibbon();
    void initDockWidgets();
    void initCentral();
    void initStatusBar();

    // UI Setup Helpers
    void setupLeftDock();
    void setupRightDock();
    QToolBar* createFileToolBar();
    QToolBar* createViewToolBar();
    QToolBar* createMathToolBar();

    // UI Members
    QTreeView* leftTree_;
    QStandardItemModel* treeModel_;
    PlotWidget* plotWidget_;
    QDockWidget* leftDock_;
    QDockWidget* rightDock_;
};
#endif // MAINWINDOW_H

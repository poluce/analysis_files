#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVariantMap>

class QTreeView;
class PlotWidget;
class QDockWidget;
class QStandardItemModel;
class QToolBar;
class Form; // Forward declaration

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onDataReady(const QVariantMap& data);
    void on_toolButtonOpen_clicked();

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

    // Data Import Members
    Form* form;
    QList<QVariantMap> dataList;
    QString temperatureKey;
    QString timeKey;
    QString customColumnKey;
    QString velocityKey;
};
#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "ProjectExplorer.h"

// 前置声明
class PlotWidget;
class QDockWidget;
class QStandardItemModel;
class QToolBar;
class CurveManager;
class MainController;
class ThermalCurve;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_toolButtonOpen_clicked();
    void onCurveAvailable(const ThermalCurve& curve);
    void onCurveAdded(const QString& curveId);

    // 算法按钮
    void onDifferentialAlgorithmAction();
    void onMovingAverageAction();
    void onIntegrationAction();

private:
    // 初始化函数
    void initRibbon();
    void initDockWidgets();
    void initCentral();
    void initStatusBar();
    void initComponents();

    // UI设置助手函数
    void setupLeftDock();
    void setupRightDock();
    QToolBar* createFileToolBar();
    QToolBar* createViewToolBar();
    QToolBar* createMathToolBar();

    // --- UI 成员 ---
    ProjectExplorer* m_projectExplorer;
    QStandardItemModel* m_projectTreeModel;
    PlotWidget* m_chartView;
    QDockWidget* m_projectExplorerDock;
    QDockWidget* m_propertiesDock;

    // --- 服务与控制器 ---
    CurveManager* m_curveManager;
    MainController* m_mainController;
};
#endif // MAINWINDOW_H

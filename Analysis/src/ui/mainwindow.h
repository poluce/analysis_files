#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "ProjectExplorerView.h"

// 前置声明
class ChartView;
class QDockWidget;
class QStandardItemModel;
class QStandardItem;
class QToolBar;
class CurveManager;
class MainController;
class CurveViewController;
class ThermalCurve;
class CurveTreeModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void curveDeleteRequested(const QString& curveId);

private slots:
    void on_toolButtonOpen_clicked();
    void onProjectTreeContextMenuRequested(const QPoint& pos);
    void onCurveAdded(const QString& curveId);

    // 算法按钮
    void onSimpleAlgorithmActionTriggered();
    void onMovingAverageAction();

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
    ProjectExplorerView* m_projectExplorer;
    CurveTreeModel* m_curveTreeModel;
    ChartView* m_chartView;
    QDockWidget* m_projectExplorerDock;
    QDockWidget* m_propertiesDock;

    // --- 操作 ---
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_peakAreaAction;
    QAction* m_baselineAction;

    // --- 服务与控制器 ---
    CurveManager* m_curveManager;
    MainController* m_mainController;
    CurveViewController* m_curveViewController;  // 新增：曲线视图控制器
};
#endif // MAINWINDOW_H

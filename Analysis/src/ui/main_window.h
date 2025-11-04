#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

// 前置声明
class ChartView;
class ProjectExplorerView;
class QDockWidget;
class QAction;
class QToolBar;
class MainController;
class CurveViewController;
class QPoint;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ChartView* chartView, ProjectExplorerView* projectExplorer, QWidget* parent = nullptr);
    ~MainWindow();

    void attachControllers(MainController* mainController, CurveViewController* curveViewController);

    ChartView* chartView() const { return m_chartView; }

signals:
    void curveDeleteRequested(const QString& curveId);

private slots:
    void on_toolButtonOpen_clicked();
    void onProjectTreeContextMenuRequested(const QPoint& pos);

    // 算法按钮
    void onSimpleAlgorithmActionTriggered();
    void onMovingAverageAction();

private:
    // 初始化函数
    void initRibbon();
    void initDockWidgets();
    void initCentral();
    void initStatusBar();

    // UI设置助手函数
    void setupLeftDock();
    void setupRightDock();
    QToolBar* createFileToolBar();
    QToolBar* createViewToolBar();
    QToolBar* createMathToolBar();

    void setupViewConnections();

    // --- UI 成员 ---
    ProjectExplorerView* m_projectExplorer { nullptr };
    ChartView* m_chartView { nullptr };
    QDockWidget* m_projectExplorerDock { nullptr };
    QDockWidget* m_propertiesDock { nullptr };

    // --- 操作 ---
    QAction* m_undoAction { nullptr };
    QAction* m_redoAction { nullptr };
    QAction* m_peakAreaAction { nullptr };
    QAction* m_baselineAction { nullptr };

    // --- 服务与控制器 ---
    MainController* m_mainController { nullptr };
    CurveViewController* m_curveViewController { nullptr }; // 新增：曲线视图控制器
};
#endif // MAINWINDOW_H

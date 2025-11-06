#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVariantMap>

// 前置声明
class ChartView;
class ProjectExplorerView;
class QDockWidget;
class QAction;
class QToolBar;
class QPoint;
class HistoryManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ChartView* chartView, ProjectExplorerView* projectExplorer, QWidget* parent = nullptr);
    ~MainWindow();

    void bindHistoryManager(HistoryManager& historyManager);

    ChartView* chartView() const { return m_chartView; }

signals:
    void curveDeleteRequested(const QString& curveId);
    void dataImportRequested();
    void undoRequested();
    void redoRequested();
    void newAlgorithmRequested(const QString& algorithmName);
    void algorithmRequested(const QString& algorithmName);
    void algorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params);

private slots:
    void on_toolButtonOpen_clicked();
    void onProjectTreeContextMenuRequested(const QPoint& pos);

    // 算法按钮
    void onAlgorithmActionTriggered();  // 通用算法触发（简化版）
    void onMovingAverageAction();

private:
    // 辅助方法：从 QAction 提取算法名称并发射信号
    void triggerAlgorithmFromAction(void (MainWindow::*signal)(const QString&));
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
    void updateHistoryButtons();

    // --- UI 成员 ---
    ProjectExplorerView* m_projectExplorer { nullptr };
    ChartView* m_chartView { nullptr };
    QDockWidget* m_projectExplorerDock { nullptr };
    QDockWidget* m_propertiesDock { nullptr };

    // --- 操作 ---
    QAction* m_undoAction { nullptr };
    QAction* m_redoAction { nullptr };

    // --- 服务与控制器 ---
    HistoryManager* m_historyManager { nullptr };
};
#endif // MAINWINDOW_H

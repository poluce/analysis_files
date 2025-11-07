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

/**
 * @brief MainWindow 是应用程序的主窗口
 *
 * 职责：
 * - 管理整体布局（菜单、工具栏、停靠面板）
 * - 接收预构造的 ChartView 与 ProjectExplorerView
 * - 仅负责布局与信号转发，不包含业务逻辑
 *
 * UI 组件：
 * - 中央区域：ChartView（图表显示）
 * - 左侧停靠面板：ProjectExplorerView（项目浏览器）
 * - 右侧停靠面板：属性面板（预留）
 * - 工具栏：文件操作、视图操作、数学运算
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param chartView 图表视图组件（由外部创建）
     * @param projectExplorer 项目浏览器组件（由外部创建）
     * @param parent 父窗口
     */
    MainWindow(ChartView* chartView, ProjectExplorerView* projectExplorer, QWidget* parent = nullptr);
    ~MainWindow();

    /**
     * @brief 绑定历史管理器，用于更新撤销/重做按钮状态
     * @param historyManager 历史管理器引用
     */
    void bindHistoryManager(HistoryManager& historyManager);

    /**
     * @brief 获取图表视图组件
     * @return ChartView 指针
     */
    ChartView* chartView() const { return m_chartView; }

signals:
    /**
     * @brief 请求删除曲线
     * @param curveId 要删除的曲线ID
     */
    void curveDeleteRequested(const QString& curveId);

    /**
     * @brief 请求打开数据导入对话框
     */
    void dataImportRequested();

    /**
     * @brief 请求撤销操作
     */
    void undoRequested();

    /**
     * @brief 请求重做操作
     */
    void redoRequested();

    /**
     * @brief 请求执行新算法（旧版信号，保留兼容）
     * @param algorithmName 算法名称
     */
    void newAlgorithmRequested(const QString& algorithmName);

    /**
     * @brief 请求执行算法
     * @param algorithmName 算法名称
     */
    void algorithmRequested(const QString& algorithmName);

    /**
     * @brief 请求执行带参数的算法
     * @param algorithmName 算法名称
     * @param params 算法参数
     */
    void algorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params);

    // 视图操作信号

    /**
     * @brief 请求放大视图
     */
    void zoomInRequested();

    /**
     * @brief 请求缩小视图
     */
    void zoomOutRequested();

    /**
     * @brief 请求自适应视图（显示所有数据）
     */
    void fitViewRequested();

private slots:
    /**
     * @brief 处理"打开文件"按钮点击
     */
    void on_toolButtonOpen_clicked();

    /**
     * @brief 处理项目树右键菜单请求
     * @param pos 鼠标位置
     */
    void onProjectTreeContextMenuRequested(const QPoint& pos);

    /**
     * @brief 通用算法触发槽（从QAction中提取算法名称）
     */
    void onAlgorithmActionTriggered();

    /**
     * @brief 移动平均滤波算法触发槽
     */
    void onMovingAverageAction();

private:
    /**
     * @brief 辅助方法：从 QAction 提取算法名称并发射信号
     * @param signal 要发射的信号的成员函数指针
     */
    void triggerAlgorithmFromAction(void (MainWindow::*signal)(const QString&));

    // 初始化函数

    /**
     * @brief 初始化工具栏（Ribbon风格）
     */
    void initRibbon();

    /**
     * @brief 初始化停靠面板
     */
    void initDockWidgets();

    /**
     * @brief 初始化中央区域
     */
    void initCentral();

    /**
     * @brief 初始化状态栏
     */
    void initStatusBar();

    // UI设置助手函数

    /**
     * @brief 设置左侧停靠面板（项目浏览器）
     */
    void setupLeftDock();

    /**
     * @brief 设置右侧停靠面板（属性面板）
     */
    void setupRightDock();

    /**
     * @brief 创建文件操作工具栏
     * @return 工具栏指针
     */
    QToolBar* createFileToolBar();

    /**
     * @brief 创建视图操作工具栏
     * @return 工具栏指针
     */
    QToolBar* createViewToolBar();

    /**
     * @brief 创建数学运算工具栏
     * @return 工具栏指针
     */
    QToolBar* createMathToolBar();

    /**
     * @brief 设置视图相关的信号连接
     */
    void setupViewConnections();

    /**
     * @brief 更新撤销/重做按钮的可用状态
     */
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

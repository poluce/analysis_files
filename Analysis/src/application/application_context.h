#ifndef APPLICATION_CONTEXT_H
#define APPLICATION_CONTEXT_H

#include <QObject>

class CurveManager;
class ProjectTreeManager;
class MainWindow;
class MainController;
class CurveViewController;
class ChartView;
class ProjectExplorerView;
class AlgorithmContext;
class AlgorithmCoordinator;
class AlgorithmThreadManager;
class AlgorithmManager;
class HistoryManager;

/**
 * @brief ApplicationContext 统一管理应用启动时的 MVC 各实例创建顺序。
 *
 * 初始化顺序：
 * 1. Model（CurveManager、ProjectTreeManager）
 * 2. View（MainWindow 及其内部组件）
 * 3. Controller（MainController、CurveViewController）
 */
class ApplicationContext : public QObject {
    Q_OBJECT

public:
    explicit ApplicationContext(QObject* parent = nullptr);
    ~ApplicationContext() override;

    /**
     * @brief 启动应用程序，初始化所有组件并显示主窗口
     *
     * 按照正确的依赖顺序创建所有 MVC 实例：
     * 1. 数据层（CurveManager、ProjectTreeManager）
     * 2. 视图层（MainWindow、ChartView、ProjectExplorerView）
     * 3. 控制层（MainController、CurveViewController、AlgorithmCoordinator）
     */
    void start();

private:
    /**
     * @brief 注册所有内置算法到 AlgorithmManager
     *
     * 包括：微分、积分、移动平均滤波、基线校正等
     */
    void registerAlgorithms();

    /**
     * @brief 注册算法的元数据描述器到注册表（方案B）
     *
     * 包括：移动平均滤波、基线校正等的参数和交互描述
     */
    void registerMetadataDescriptors();

    // Infrastructure（基础设施层）
    AlgorithmThreadManager* m_threadManager { nullptr };
    HistoryManager* m_historyManager { nullptr };

    // Application Layer（应用层）
    AlgorithmManager* m_algorithmManager { nullptr };
    CurveManager* m_curveManager { nullptr };
    ProjectTreeManager* m_projectTreeManager { nullptr };
    AlgorithmContext* m_algorithmContext { nullptr };
    AlgorithmCoordinator* m_algorithmCoordinator { nullptr };

    // Presentation Layer（表示层）
    ChartView* m_chartView { nullptr };
    ProjectExplorerView* m_projectExplorerView { nullptr };
    MainWindow* m_mainWindow { nullptr };

    // Controller Layer（控制层）
    MainController* m_mainController { nullptr };
    CurveViewController* m_curveViewController { nullptr };
};

#endif // APPLICATION_CONTEXT_H

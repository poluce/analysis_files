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

/**
 * @brief ApplicationContext 统一管理应用启动时的 MVC 实例创建顺序。
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

    void start();

private:
    void registerAlgorithms();

    // Model
    CurveManager* m_curveManager { nullptr };
    ProjectTreeManager* m_projectTreeManager { nullptr };

    // View
    ChartView* m_chartView { nullptr };
    ProjectExplorerView* m_projectExplorerView { nullptr };
    MainWindow* m_mainWindow { nullptr };

    // Controller
    MainController* m_mainController { nullptr };
    CurveViewController* m_curveViewController { nullptr };
};

#endif // APPLICATION_CONTEXT_H

#include "application_context.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_coordinator.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/algorithm/algorithm_thread_manager.h"
#include "application/curve/curve_manager.h"
#include "application/history/history_manager.h"
#include "application/project/project_tree_manager.h"
#include "infrastructure/algorithm/baseline_correction_algorithm.h"
#include "infrastructure/algorithm/differentiation_algorithm.h"
#include "infrastructure/algorithm/integration_algorithm.h"
#include "infrastructure/algorithm/moving_average_filter_algorithm.h"
#include "infrastructure/algorithm/temperature_extrapolation_algorithm.h"
// 注：峰面积已改为视图层工具，不再作为算法注册
#include "ui/chart_view.h"
#include "ui/controller/curve_view_controller.h"
#include "ui/controller/algorithm_execution_controller.h"
#include "ui/controller/main_controller.h"
#include "ui/presenter/message_presenter.h"
#include "ui/main_window.h"
#include "ui/project_explorer_view.h"
#include "application/usecase/delete_curve_use_case.h"

ApplicationContext::ApplicationContext(QObject* parent)
    : QObject(parent)
{
    // ==================== 正确的依赖注入初始化顺序 ====================

    // 1. 基础设施层（无依赖）
    m_threadManager = new AlgorithmThreadManager(this);
    m_threadManager->setMaxThreads(1);  // 配置：默认单线程模式

    m_historyManager = new HistoryManager(this);

    // 2. 领域模型层
    m_curveManager = new CurveManager(this);

    // 3. 应用服务层（依赖注入）
    m_algorithmManager = new AlgorithmManager(
        m_threadManager,  // 显式注入 ThreadManager
        this
    );
    m_algorithmManager->setCurveManager(m_curveManager);
    m_algorithmManager->setHistoryManager(m_historyManager);

    m_algorithmContext = new AlgorithmContext(this);

    m_algorithmCoordinator = new AlgorithmCoordinator(
        m_algorithmManager,
        m_curveManager,
        m_algorithmContext,
        this
    );

    m_projectTreeManager = new ProjectTreeManager(m_curveManager, this);

    // 4. 表示层（UI）
    m_chartView = new ChartView();
    m_chartView->setCurveManager(m_curveManager);  // 设置曲线管理器，用于获取曲线数据

    m_projectExplorerView = new ProjectExplorerView();

    m_mainWindow = new MainWindow(m_chartView, m_projectExplorerView);
    m_mainWindow->bindHistoryManager(*m_historyManager);  // 传递实例而非单例
    m_messagePresenter = new MessagePresenter(m_mainWindow, this);

    // 5. 控制器层
    m_mainController = new MainController(
        m_curveManager,
        m_algorithmManager,  // 直接传递实例
        m_historyManager,    // 直接传递实例
        this
    );
    m_mainController->setPlotWidget(m_chartView);
    m_mainController->attachMainWindow(m_mainWindow);
    m_algorithmExecutionController = new AlgorithmExecutionController(
        m_curveManager,
        m_algorithmManager,
        m_algorithmCoordinator,
        m_messagePresenter,
        this);
    m_algorithmExecutionController->setChartView(m_chartView);
    m_algorithmExecutionController->setDialogParent(m_mainWindow);

    m_deleteCurveUseCase = new DeleteCurveUseCase(
        m_curveManager,
        m_historyManager,
        m_messagePresenter,
        this);

    m_mainController->setMessagePresenter(m_messagePresenter);
    m_mainController->setAlgorithmExecutionController(m_algorithmExecutionController);
    m_mainController->setDeleteCurveUseCase(m_deleteCurveUseCase);

    m_curveViewController = new CurveViewController(
        m_curveManager,
        m_chartView,
        m_projectTreeManager,
        m_projectExplorerView,
        this
    );
    m_mainController->setCurveViewController(m_curveViewController);

    // ==================== 完整性校验与状态标记 ====================
    // 所有依赖注入完成后，调用 initialize() 进行完整性校验
    m_mainController->initialize();

    // 6. 注册算法（最后）
    registerAlgorithms();
}

ApplicationContext::~ApplicationContext()
{
    delete m_mainWindow;
    m_mainWindow = nullptr;
}

void ApplicationContext::start() { m_mainWindow->show(); }

void ApplicationContext::registerAlgorithms()
{
    // 使用成员变量，不再调用单例
    m_algorithmManager->registerAlgorithm(new DifferentiationAlgorithm());
    m_algorithmManager->registerAlgorithm(new MovingAverageFilterAlgorithm());
    m_algorithmManager->registerAlgorithm(new IntegrationAlgorithm());
    m_algorithmManager->registerAlgorithm(new BaselineCorrectionAlgorithm());
    m_algorithmManager->registerAlgorithm(new TemperatureExtrapolationAlgorithm());
    // 注：峰面积已改为视图层工具，不再作为算法注册
}

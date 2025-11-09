#include "application_context.h"

#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_coordinator.h"
#include "application/algorithm/algorithm_manager.h"
#include "application/curve/curve_manager.h"
#include "application/history/history_manager.h"
#include "application/project/project_tree_manager.h"
#include "infrastructure/algorithm/baseline_correction_algorithm.h"
#include "infrastructure/algorithm/differentiation_algorithm.h"
#include "infrastructure/algorithm/integration_algorithm.h"
#include "infrastructure/algorithm/moving_average_filter_algorithm.h"
#include "infrastructure/algorithm/peak_area_algorithm.h"
#include "ui/chart_view.h"
#include "ui/controller/curve_view_controller.h"
#include "ui/controller/main_controller.h"
#include "ui/main_window.h"
#include "ui/project_explorer_view.h"

ApplicationContext::ApplicationContext(QObject* parent)
    : QObject(parent)
{
    registerAlgorithms();

    // 1. Model
    m_curveManager = new CurveManager(this);
    m_projectTreeManager = new ProjectTreeManager(m_curveManager, this);

    // Algorithm coordination
    m_algorithmContext = new AlgorithmContext(this);
    m_algorithmCoordinator
        = new AlgorithmCoordinator(AlgorithmManager::instance(), m_curveManager, m_algorithmContext, this);

    // 设置 AlgorithmManager 的历史管理器（用于撤销/重做）
    AlgorithmManager::instance()->setHistoryManager(HistoryManager::instance());

    // 2. View
    m_chartView = new ChartView();
    m_chartView->setCurveManager(m_curveManager);  // 设置曲线管理器，用于获取曲线数据
    m_projectExplorerView = new ProjectExplorerView();
    m_mainWindow = new MainWindow(m_chartView, m_projectExplorerView);
    m_mainWindow->bindHistoryManager(*HistoryManager::instance());

    // 连接 AlgorithmManager 的标注点信号到 ChartView（使用 lambda 处理默认参数）
    connect(AlgorithmManager::instance(), &AlgorithmManager::markersGenerated,
            m_chartView, [this](const QString& curveId, const QList<QPointF>& markers, const QColor& color) {
                m_chartView->addCurveMarkers(curveId, markers, color, 12.0);  // size 使用默认值 12.0
            });

    // 3. Controller
    m_mainController = new MainController(m_curveManager, this);
    m_mainController->setPlotWidget(m_chartView);
    m_mainController->setAlgorithmCoordinator(m_algorithmCoordinator, m_algorithmContext);
    m_mainController->attachMainWindow(m_mainWindow);

    m_curveViewController
        = new CurveViewController(m_curveManager, m_chartView, m_projectTreeManager, m_projectExplorerView, this);
    m_mainController->setCurveViewController(m_curveViewController);

}

ApplicationContext::~ApplicationContext()
{
    delete m_mainWindow;
    m_mainWindow = nullptr;
}

void ApplicationContext::start() { m_mainWindow->show(); }

void ApplicationContext::registerAlgorithms()
{
    auto* manager = AlgorithmManager::instance();
    manager->registerAlgorithm(new DifferentiationAlgorithm());
    manager->registerAlgorithm(new MovingAverageFilterAlgorithm());
    manager->registerAlgorithm(new IntegrationAlgorithm());

    manager->registerAlgorithm(new BaselineCorrectionAlgorithm());
    manager->registerAlgorithm(new PeakAreaAlgorithm());
}

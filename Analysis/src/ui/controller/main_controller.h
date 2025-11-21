#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QPointF>
#include "domain/model/thermal_data_point.h"

// 前置声明
class CurveManager;
class DataImportWidget;
class ThermalCurve;
class AlgorithmManager;
class HistoryManager;
class ChartView;
class MainWindow;
class CurveViewController;
class QGraphicsObject;
class MessagePresenter;
class AlgorithmExecutionController;
class DeleteCurveUseCase;

/**
 * @brief MainController 协调UI和应用服务。
 */
class MainController : public QObject {
    Q_OBJECT

public:
    explicit MainController(CurveManager* curveManager,
                            AlgorithmManager* algorithmManager,
                            HistoryManager* historyManager,
                            QObject* parent = nullptr);
    ~MainController();

    // 设置 ChartView（用于基线绘制和交互控制）
    void setPlotWidget(ChartView* plotWidget);
    void attachMainWindow(MainWindow* mainWindow);
    void setCurveViewController(CurveViewController* ViewController);
    void setMessagePresenter(MessagePresenter* presenter);
    void setAlgorithmExecutionController(AlgorithmExecutionController* controller);
    void setDeleteCurveUseCase(DeleteCurveUseCase* useCase);

    /**
     * @brief 完整性校验与状态标记
     *
     * 在所有依赖注入完成后调用，集中断言所有必需依赖非空，
     * 并将控制器标记为"已初始化状态"。
     *
     * 调用顺序：构造函数 → setter 注入 → initialize()
     */
    void initialize();

signals:
    /**
     * @brief 当一个曲线被加载并可用于在UI的其他部分显示时发出此信号。
     * @param curve 加载的曲线对象。
     */
    void curveAvailable(const ThermalCurve& curve);

public slots:
    /**
     * @brief 显示数据导入窗口。
     */
    void onShowDataImport();

    /**
     * @brief 执行算法（支持可选参数）
     * @param algorithmName 算法名称
     * @param params 算法参数（可选，默认为空）
     */
    void onAlgorithmRequested(const QString& algorithmName, const QVariantMap& params = QVariantMap());

    /**
     * @brief 撤销最近的操作。
     */
    void onUndo();

    /**
     * @brief 重做最近被撤销的操作。
     */
    void onRedo();

    // 响应UI的曲线删除请求
    void onCurveDeleteRequested(const QString& curveId);

    /**
     * @brief 处理峰面积工具请求
     */
    void onPeakAreaToolRequested();

private slots:
    /**
     * @brief 处理来自数据导入窗口的文件预览请求。
     * @param filePath 要预览的文件的路径。
     */
    void onPreviewRequested(const QString& filePath);

    /**
     * @brief 处理最终的数据导入请求。
     */
    void onImportTriggered();
    void onMassLossToolRemoveRequested(QGraphicsObject* tool);

    /**
     * @brief 处理峰面积工具删除请求
     * @param tool 待删除的工具对象
     *
     * 创建 RemovePeakAreaToolCommand 并通过 HistoryManager 执行
     */
    void onPeakAreaToolRemoveRequested(QGraphicsObject* tool);

    /**
     * @brief 处理质量损失工具创建请求
     *
     * 创建 AddMassLossToolCommand 并通过 HistoryManager 执行
     */
    void onMassLossToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId);

    /**
     * @brief 处理峰面积工具创建请求
     *
     * 创建 AddPeakAreaToolCommand 并通过 HistoryManager 执行
     */
    void onPeakAreaToolCreateRequested(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId,
                                       bool useLinearBaseline, const QString& referenceCurveId);

private:
    // ==================== 初始化状态标记 ====================
    bool m_initialized = false;  // 防止"半初始化对象"的运行时错误

    // ==================== 依赖注入 ====================
    // 构造函数注入（核心依赖）
    CurveManager* m_curveManager;         // 非拥有指针
    AlgorithmManager* m_algorithmManager; // 非拥有指针
    HistoryManager* m_historyManager;     // 非拥有指针

    // Setter 注入（延迟依赖，但也是必需的）
    ChartView* m_plotWidget = nullptr;    // 非拥有指针
    MainWindow* m_mainWindow = nullptr;   // 非拥有指针
    CurveViewController* m_curveViewController = nullptr;
    MessagePresenter* m_messagePresenter = nullptr;
    AlgorithmExecutionController* m_algorithmExecutionController = nullptr;
    DeleteCurveUseCase* m_deleteCurveUseCase = nullptr;

    // 拥有的对象
    DataImportWidget* m_dataImportWidget; // 拥有指针

};

#endif // MAINCONTROLLER_H

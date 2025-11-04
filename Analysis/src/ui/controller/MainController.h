#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QPointF>

// 前置声明
class CurveManager;
class DataImportWidget;
class TextFileReader;
class ThermalCurve;
class AlgorithmManager; // 添加
class HistoryManager; // 添加
class PeakAreaDialog; // 添加
class ChartView; // 添加
class InteractionController; // 添加

/**
 * @brief MainController 协调UI和应用服务。
 */
class MainController : public QObject
{
    Q_OBJECT

public:
    explicit MainController(CurveManager* curveManager, QObject *parent = nullptr);
    ~MainController();

    // 设置 ChartView（用于基线绘制和交互控制）
    void setPlotWidget(ChartView* plotWidget);

    // 获取 InteractionController（用于外部访问，如果需要）
    InteractionController* interactionController() const { return m_interactionController; }

signals:
    /**
     * @brief 当一个曲线被加载并可用于在UI的其他部分显示时发出此信号。
     * @param curve 加载的曲线对象。
     */
    void curveAvailable(const ThermalCurve& curve);
    void curveDataChanged(const QString& curveId); // 添加

public slots:
    /**
     * @brief 显示数据导入窗口。
     */
    void onShowDataImport();
    void onAlgorithmRequested(const QString& algorithmName); // 简单执行，无参数
    // 带参数执行算法（例如移动平均的窗口大小等）
    void onAlgorithmRequestedWithParams(const QString& algorithmName, const QVariantMap& params);

    /**
     * @brief 撤销最近的操作。
     */
    void onUndo();

    /**
     * @brief 重做最近被撤销的操作。
     */
    void onRedo();

    // 峰面积和基线功能
    void onPeakAreaRequested();
    void onBaselineRequested();

    // 响应 InteractionController 的点拾取完成信号（统一处理）
    void onPointsSelected(const QVector<QPointF>& points);

    // 响应UI的曲线删除请求
    void onCurveDeleteRequested(const QString& curveId);

    // 响应UI的进度更新请求
    void onPointPickingProgress(int picked, int total);

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
    void onAlgorithmFinished(const QString& curveId); // 添加

private:
    // 辅助方法
    void handlePeakAreaCalculation(const QVector<QPointF>& points);
    void handleBaselineDrawing(const QVector<QPointF>& points);

    CurveManager* m_curveManager;         // 非拥有指针
    DataImportWidget* m_dataImportWidget; // 拥有指针
    TextFileReader* m_textFileReader;     // 拥有指针
    AlgorithmManager* m_algorithmService; // 添加
    HistoryManager* m_historyManager;     // 添加，非拥有指针（单例）
    PeakAreaDialog* m_peakAreaDialog = nullptr; // 添加，拥有指针
    ChartView* m_plotWidget = nullptr;   // 添加，非拥有指针
    InteractionController* m_interactionController = nullptr; // 添加，拥有指针

    // 标记当前的点拾取目的
    enum class PickingPurpose { None, PeakArea, Baseline };
    PickingPurpose m_currentPickingPurpose = PickingPurpose::None;
};

#endif // MAINCONTROLLER_H

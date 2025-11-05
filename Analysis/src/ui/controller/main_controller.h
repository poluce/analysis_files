#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

// 前置声明
class CurveManager;
class DataImportWidget;
class TextFileReader;
class ThermalCurve;
class AlgorithmManager; // 添加
class HistoryManager; // 添加
class ChartView; // 添加
class MainWindow;

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
    void attachMainWindow(MainWindow* mainWindow);


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

    // 响应UI的曲线删除请求
    void onCurveDeleteRequested(const QString& curveId);

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
    CurveManager* m_curveManager;         // 非拥有指针
    DataImportWidget* m_dataImportWidget; // 拥有指针
    TextFileReader* m_textFileReader;     // 拥有指针
    AlgorithmManager* m_algorithmManager; // 添加
    HistoryManager* m_historyManager;     // 添加，非拥有指针（单例）
    ChartView* m_plotWidget = nullptr;   // 非拥有指针
    MainWindow* m_mainWindow = nullptr;   // 非拥有指针
};

#endif // MAINCONTROLLER_H

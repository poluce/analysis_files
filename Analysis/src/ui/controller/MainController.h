#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>

// 前置声明
class CurveManager;
class DataImportWidget;
class TextFileReader;
class ThermalCurve;
class AlgorithmService; // 添加

/**
 * @brief MainController 协调UI和应用服务。
 */
class MainController : public QObject
{
    Q_OBJECT

public:
    explicit MainController(CurveManager* curveManager, QObject *parent = nullptr);
    ~MainController();

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
    void onAlgorithmRequested(const QString& algorithmName); // 添加

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
    AlgorithmService* m_algorithmService; // 添加
};

#endif // MAINCONTROLLER_H
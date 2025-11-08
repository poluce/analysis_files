#ifndef ALGORITHMANAGER_H
#define ALGORITHMANAGER_H

#include "domain/algorithm/i_thermal_algorithm.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

// 前置声明
class ThermalCurve;
class CurveManager;
class AlgorithmContext;

/**
 * @brief 算法服务类 - 管理算法注册和执行
 *
 * 纯上下文驱动设计：所有算法通过 executeWithContext() 执行。
 */
class AlgorithmManager : public QObject {
    Q_OBJECT
public:
    static AlgorithmManager* instance();

    void setCurveManager(CurveManager* manager);
    void registerAlgorithm(IThermalAlgorithm* algorithm);
    IThermalAlgorithm* getAlgorithm(const QString& name);

    /**
     * @brief 上下文驱动的算法执行（唯一执行接口）
     *
     * 算法从上下文中拉取所需数据，执行后返回结果。
     *
     * @param name 算法名称
     * @param context 算法上下文（包含曲线、参数、选择的点等）
     */
    void executeWithContext(const QString& name, AlgorithmContext* context);

signals:
    /**
     * @brief 算法执行完成信号
     *
     * @param algorithmName 算法名称
     * @param result 结构化的执行结果（AlgorithmResult）
     */
    void algorithmResultReady(const QString& algorithmName, const AlgorithmResult& result);

    /**
     * @brief 算法执行失败信号
     *
     * @param algorithmName 算法名称
     * @param errorMessage 错误信息
     */
    void algorithmExecutionFailed(const QString& algorithmName, const QString& errorMessage);

    /**
     * @brief 算法生成标注点信号
     *
     * @param curveId 关联的曲线ID
     * @param markers 标注点列表（数据坐标）
     * @param color 标注点颜色
     */
    void markersGenerated(const QString& curveId, const QList<QPointF>& markers, const QColor& color);

    /**
     * @brief 算法请求添加浮动标签信号
     *
     * @param text 标签文本
     * @param dataPos 数据坐标位置
     * @param curveId 关联的曲线ID
     */
    void floatingLabelRequested(const QString& text, const QPointF& dataPos, const QString& curveId);

private:
    explicit AlgorithmManager(QObject* parent = nullptr);
    ~AlgorithmManager();
    AlgorithmManager(const AlgorithmManager&) = delete;
    AlgorithmManager& operator=(const AlgorithmManager&) = delete;

    // 根据结果类型处理算法结果
    void handleAlgorithmResult(const AlgorithmResult& result);

    // 添加曲线（使用历史管理）
    void addCurveWithHistory(const ThermalCurve& curve);

    // 创建输出曲线的通用方法（向后兼容，已废弃）
    void createAndAddOutputCurve(
        IThermalAlgorithm* algorithm,
        ThermalCurve* parentCurve,
        const QVector<ThermalDataPoint>& outputData,
        bool useHistoryManager = false);

    QMap<QString, IThermalAlgorithm*> m_algorithms;
    CurveManager* m_curveManager = nullptr;
    class HistoryManager* m_historyManager = nullptr;  // 历史管理器（可选）

public:
    void setHistoryManager(class HistoryManager* manager) { m_historyManager = manager; }
};

#endif // ALGORITHMANAGER_H

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
     * @param outputType 输出类型（Curve, Area, Annotation等）
     * @param result 执行结果（类型根据outputType决定）
     */
    void algorithmResultReady(
        const QString& algorithmName, IThermalAlgorithm::OutputType outputType,
        const QVariant& result);

private:
    explicit AlgorithmManager(QObject* parent = nullptr);
    ~AlgorithmManager();
    AlgorithmManager(const AlgorithmManager&) = delete;
    AlgorithmManager& operator=(const AlgorithmManager&) = delete;

    // 根据输出类型处理算法结果
    void handleAlgorithmResult(
        IThermalAlgorithm* algorithm, ThermalCurve* parentCurve, const QVariant& result);

    // 创建输出曲线的通用方法（封装重复逻辑）
    void createAndAddOutputCurve(
        IThermalAlgorithm* algorithm,
        ThermalCurve* parentCurve,
        const QVector<ThermalDataPoint>& outputData,
        bool useHistoryManager = false);

    QMap<QString, IThermalAlgorithm*> m_algorithms;
    CurveManager* m_curveManager = nullptr;
};

#endif // ALGORITHMANAGER_H

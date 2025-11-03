#ifndef ALGORITHMSERVICE_H
#define ALGORITHMSERVICE_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVariant>
#include "domain/algorithm/IThermalAlgorithm.h"

// 前置声明
class ThermalCurve;
class CurveManager;

/**
 * @brief 算法服务类 - 管理算法注册和执行
 *
 * 支持两种执行模式：
 * 1. execute() - 旧接口，用于简单的单曲线算法（A类）
 * 2. executeWithInputs() - 新接口，支持复杂输入/输出（B-E类）
 */
class AlgorithmService : public QObject
{
    Q_OBJECT
public:
    static AlgorithmService* instance();

    void setCurveManager(CurveManager* manager);
    void registerAlgorithm(IThermalAlgorithm* algorithm);
    IThermalAlgorithm* getAlgorithm(const QString& name);

    // 旧接口：简单的单曲线算法执行（保持向后兼容）
    void execute(const QString& name, ThermalCurve* curve);

    // 新接口：支持灵活输入/输出的算法执行
    void executeWithInputs(const QString& name, const QVariantMap& inputs);

signals:
    void algorithmFinished(const QString& curveId);

    // 新信号：算法执行完成（包含输出类型和结果）
    void algorithmResultReady(const QString& algorithmName,
                             IThermalAlgorithm::OutputType outputType,
                             const QVariant& result);

private:
    explicit AlgorithmService(QObject *parent = nullptr);
    ~AlgorithmService();
    AlgorithmService(const AlgorithmService&) = delete;
    AlgorithmService& operator=(const AlgorithmService&) = delete;

    // 根据输出类型处理算法结果
    void handleAlgorithmResult(IThermalAlgorithm* algorithm,
                              ThermalCurve* parentCurve,
                              const QVariant& result);

    QMap<QString, IThermalAlgorithm*> m_algorithms;
    CurveManager* m_curveManager = nullptr;
};

#endif // ALGORITHMSERVICE_H

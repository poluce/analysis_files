#include "AlgorithmService.h"
#include "domain/algorithm/IThermalAlgorithm.h"
#include "domain/model/ThermalCurve.h"
#include <QDebug>

AlgorithmService* AlgorithmService::instance()
{
    static AlgorithmService service;
    return &service;
}

AlgorithmService::AlgorithmService(QObject *parent) : QObject(parent)
{
}

AlgorithmService::~AlgorithmService()
{
    qDeleteAll(m_algorithms);
}

void AlgorithmService::registerAlgorithm(IThermalAlgorithm* algorithm)
{
    if (algorithm) {
        qDebug() << "Registering algorithm:" << algorithm->name();
        m_algorithms.insert(algorithm->name(), algorithm);
    }
}

IThermalAlgorithm* AlgorithmService::getAlgorithm(const QString& name)
{
    return m_algorithms.value(name, nullptr);
}

void AlgorithmService::execute(const QString& name, ThermalCurve* curve)
{
    if (!curve) {
        qWarning() << "Algorithm execution failed: curve is null.";
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "Algorithm execution failed: could not find algorithm" << name;
        return;
    }

    qDebug() << "Executing algorithm" << name << "on curve" << curve->name();

    // 获取数据，处理，然后设置回曲线
    const auto inputData = curve->getProcessedData();
    const auto outputData = algorithm->process(inputData);
    curve->setProcessedData(outputData);

    emit algorithmFinished(curve->id());
}

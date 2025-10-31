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
        qDebug() << "注册算法:" << algorithm->name();
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
        qWarning() << "算法执行失败：曲线为空。";
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "算法执行失败：找不到算法" << name;
        return;
    }

    qDebug() << "正在执行算法" << name << "于曲线" << curve->name();

    // 获取数据，处理，然后设置回曲线
    const auto inputData = curve->getProcessedData();
    const auto outputData = algorithm->process(inputData);
    curve->setProcessedData(outputData);

    emit algorithmFinished(curve->id());
}

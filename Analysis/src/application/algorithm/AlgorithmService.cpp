#include "AlgorithmService.h"
#include "domain/algorithm/IThermalAlgorithm.h"
#include "application/curve/CurveManager.h"
#include "domain/model/ThermalCurve.h"
#include <QDebug>
#include <QUuid>

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

void AlgorithmService::setCurveManager(CurveManager* manager)
{
    m_curveManager = manager;
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

    if (!m_curveManager) {
        qWarning() << "算法执行失败：CurveManager 未设置。";
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "算法执行失败：找不到算法" << name;
        return;
    }

    qDebug() << "正在执行算法" << name << "于曲线" << curve->name();

    // 1. 执行算法处理
    const auto inputData = curve->getProcessedData();
    const auto outputData = algorithm->process(inputData);

    if (outputData.isEmpty()) {
        qWarning() << "算法" << name << "未生成任何数据。";
        return;
    }

    // 2. 创建新曲线
    QString newId = QUuid::createUuid().toString();
    QString newName = QString("%1 (%2)").arg(curve->name()).arg(algorithm->name());
    ThermalCurve newCurve(newId, newName);

    // 3. 填充新曲线数据和元数据
    newCurve.setProcessedData(outputData);
    newCurve.setMetadata(curve->getMetadata()); // 复制元数据

    // 根据算法和原曲线类型，设置新曲线的类型
    if (algorithm->name() == "differentiation" && curve->type() == CurveType::TGA) {
        newCurve.setType(CurveType::DTG);
    } else {
        newCurve.setType(curve->type()); // 默认复制原类型
    }

    // 4. 通过 CurveManager 添加新曲线
    m_curveManager->addCurve(newCurve);

    // curveAdded 信号将由 CurveManager 发出，UI应响应那个信号
    // emit algorithmFinished(curve->id()); // 旧信号不再适用
}

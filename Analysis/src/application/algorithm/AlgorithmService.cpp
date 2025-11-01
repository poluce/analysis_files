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
    QString newName = algorithm->displayName(); // 使用中文显示名称
    ThermalCurve newCurve(newId, newName);

    // 3. 填充新曲线数据和元数据
    newCurve.setProcessedData(outputData);
    newCurve.setMetadata(curve->getMetadata()); // 复制元数据
    newCurve.setParentId(curve->id()); // 设置父曲线ID
    newCurve.setProjectName(curve->projectName()); // 继承项目名称

    // 4. 设置新曲线的类型
    // 仪器类型继承自父曲线（算法不改变仪器类型）
    newCurve.setInstrumentType(curve->instrumentType());
    // 信号类型由算法决定（Raw -> Derivative 或 Derivative -> Raw）
    newCurve.setSignalType(algorithm->getOutputSignalType(curve->signalType()));

    // 5. 通过 CurveManager 添加新曲线
    m_curveManager->addCurve(newCurve);

    // 6. 设置新生成的曲线为活动曲线（默认选中）
    m_curveManager->setActiveCurve(newId);

    // curveAdded 信号将由 CurveManager 发出，UI应响应那个信号
    // emit algorithmFinished(curve->id()); // 旧信号不再适用
}

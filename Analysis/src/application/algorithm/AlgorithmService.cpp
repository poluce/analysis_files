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

// ==================== 新接口方法实现 ====================

void AlgorithmService::executeWithInputs(const QString& name, const QVariantMap& inputs)
{
    if (!m_curveManager) {
        qWarning() << "算法执行失败：CurveManager 未设置。";
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "算法执行失败：找不到算法" << name;
        return;
    }

    // 验证输入
    if (!inputs.contains("mainCurve")) {
        qWarning() << "算法执行失败：输入缺少主曲线（mainCurve）。";
        return;
    }

    auto mainCurve = inputs["mainCurve"].value<ThermalCurve*>();
    if (!mainCurve) {
        qWarning() << "算法执行失败：主曲线为空。";
        return;
    }

    qDebug() << "正在执行算法" << name << "（新接口）于曲线" << mainCurve->name();
    qDebug() << "输入类型:" << static_cast<int>(algorithm->inputType());
    qDebug() << "输出类型:" << static_cast<int>(algorithm->outputType());

    // 执行算法
    QVariant result = algorithm->execute(inputs);

    if (result.isNull() || !result.isValid()) {
        qWarning() << "算法" << name << "未生成有效结果。";
        return;
    }

    // 根据输出类型处理结果
    handleAlgorithmResult(algorithm, mainCurve, result);

    // 发出新信号
    emit algorithmResultReady(name, algorithm->outputType(), result);
}

void AlgorithmService::handleAlgorithmResult(IThermalAlgorithm* algorithm,
                                            ThermalCurve* parentCurve,
                                            const QVariant& result)
{
    if (!algorithm || !parentCurve || !m_curveManager) {
        return;
    }

    auto outputType = algorithm->outputType();

    switch (outputType) {
    case IThermalAlgorithm::OutputType::Curve: {
        // 输出为新曲线（最常见情况）
        auto outputData = result.value<QVector<ThermalDataPoint>>();
        if (outputData.isEmpty()) {
            qWarning() << "算法输出曲线数据为空。";
            return;
        }

        // 创建新曲线
        QString newId = QUuid::createUuid().toString();
        QString newName = algorithm->displayName();
        ThermalCurve newCurve(newId, newName);

        // 填充数据和元数据
        newCurve.setProcessedData(outputData);
        newCurve.setMetadata(parentCurve->getMetadata());
        newCurve.setParentId(parentCurve->id());
        newCurve.setProjectName(parentCurve->projectName());

        // 设置类型
        newCurve.setInstrumentType(parentCurve->instrumentType());
        newCurve.setSignalType(algorithm->getOutputSignalType(parentCurve->signalType()));

        // 添加并激活新曲线
        m_curveManager->addCurve(newCurve);
        m_curveManager->setActiveCurve(newId);

        qDebug() << "新曲线" << newName << "已创建（ID:" << newId << "）";
        break;
    }

    case IThermalAlgorithm::OutputType::Area: {
        // 输出为面积值（如峰面积）
        // 结果应该是 QVariantMap {"area": double, "unit": QString, ...}
        qDebug() << "面积计算结果:" << result;
        // TODO: 在后续实现中，这里可以创建一个标注对象或面积结果对象
        // 目前仅输出日志
        break;
    }

    case IThermalAlgorithm::OutputType::Intersection: {
        // 输出为交点集合
        qDebug() << "交点计算结果:" << result;
        // TODO: 处理交点结果
        break;
    }

    case IThermalAlgorithm::OutputType::Annotation: {
        // 输出为标注（如峰位置、切线等）
        qDebug() << "标注结果:" << result;
        // TODO: 处理标注结果
        break;
    }

    case IThermalAlgorithm::OutputType::MultipleCurves: {
        // 输出为多条曲线
        qDebug() << "多曲线输出结果:" << result;
        // TODO: 处理多曲线输出
        break;
    }

    default:
        qWarning() << "未知的输出类型:" << static_cast<int>(outputType);
        break;
    }
}

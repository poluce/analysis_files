#include "algorithm_manager.h"
#include "application/curve/curve_manager.h"
#include "application/history/add_curve_command.h"
#include "application/history/history_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>
#include <QUuid>
#include <memory>

AlgorithmManager* AlgorithmManager::instance()
{
    static AlgorithmManager service;
    return &service;
}

AlgorithmManager::AlgorithmManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "构造:    AlgorithmManager";
}

AlgorithmManager::~AlgorithmManager() { qDeleteAll(m_algorithms); }

void AlgorithmManager::setCurveManager(CurveManager* manager) { m_curveManager = manager; }

void AlgorithmManager::registerAlgorithm(IThermalAlgorithm* algorithm)
{
    if (algorithm) {
        qDebug() << "注册算法:" << algorithm->name();
        m_algorithms.insert(algorithm->name(), algorithm);
    }
}

IThermalAlgorithm* AlgorithmManager::getAlgorithm(const QString& name) { return m_algorithms.value(name, nullptr); }

void AlgorithmManager::execute(const QString& name, ThermalCurve* curve)
{
    if (!curve || !m_curveManager) {
        qWarning() << "算法执行失败：" << (!curve ? "曲线为空" : "CurveManager 未设置");
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "算法执行失败：找不到算法" << name;
        return;
    }

    qDebug() << "正在执行算法" << name << "于曲线" << curve->name();

    // 执行算法处理
    const auto outputData = algorithm->process(curve->getProcessedData());

    if (outputData.isEmpty()) {
        qWarning() << "算法" << name << "未生成任何数据。";
        return;
    }

    // 使用统一方法创建并添加输出曲线
    createAndAddOutputCurve(algorithm, curve, outputData, false);
}

// ==================== 新接口方法实现 ====================

void AlgorithmManager::executeWithInputs(const QString& name, const QVariantMap& inputs)
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

void AlgorithmManager::handleAlgorithmResult(IThermalAlgorithm* algorithm, ThermalCurve* parentCurve, const QVariant& result)
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

        // 使用统一方法创建并添加输出曲线（使用历史管理）
        createAndAddOutputCurve(algorithm, parentCurve, outputData, true);
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

void AlgorithmManager::createAndAddOutputCurve(
    IThermalAlgorithm* algorithm,
    ThermalCurve* parentCurve,
    const QVector<ThermalDataPoint>& outputData,
    bool useHistoryManager)
{
    if (!algorithm || !parentCurve || !m_curveManager) {
        qWarning() << "创建输出曲线失败：参数无效";
        return;
    }

    // 创建新曲线
    const QString newId = QUuid::createUuid().toString();
    const QString newName = algorithm->displayName();
    ThermalCurve newCurve(newId, newName);

    // 填充数据和元数据
    newCurve.setProcessedData(outputData);
    newCurve.setMetadata(parentCurve->getMetadata());
    newCurve.setParentId(parentCurve->id());
    newCurve.setProjectName(parentCurve->projectName());

    // 设置类型
    newCurve.setInstrumentType(parentCurve->instrumentType());
    newCurve.setSignalType(algorithm->getOutputSignalType(parentCurve->signalType()));

    // 添加到管理器（根据是否使用历史管理）
    if (useHistoryManager) {
        const QString description = QStringLiteral("执行 %1 算法").arg(algorithm->displayName());
        auto command = std::make_unique<AddCurveCommand>(m_curveManager, newCurve, description);
        if (!HistoryManager::instance().executeCommand(std::move(command))) {
            qWarning() << "算法结果入栈失败，放弃添加新曲线";
        }
    } else {
        m_curveManager->addCurve(newCurve);
        m_curveManager->setActiveCurve(newId);
    }
}

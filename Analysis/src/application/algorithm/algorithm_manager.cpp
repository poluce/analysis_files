#include "algorithm_manager.h"
#include "application/algorithm/algorithm_context.h"
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

IThermalAlgorithm* AlgorithmManager::getAlgorithm(const QString& name)
{
    return m_algorithms.value(name, nullptr);
}

// ==================== 上下文驱动执行实现 ====================

void AlgorithmManager::executeWithContext(const QString& name, AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "算法执行失败：上下文为空。";
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

    // 从上下文获取主曲线
    auto activeCurve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
    if (!activeCurve.has_value()) {
        auto mainCurve = context->get<ThermalCurve*>(ContextKeys::MainCurve);
        if (!mainCurve.has_value()) {
            qWarning() << "算法执行失败：上下文中缺少主曲线（activeCurve 或 mainCurve）。";
            return;
        }
        activeCurve = mainCurve;
    }

    ThermalCurve* curve = activeCurve.value();
    if (!curve) {
        qWarning() << "算法执行失败：主曲线为空指针。";
        return;
    }

    qDebug() << "正在执行算法" << name << "（上下文驱动）于曲线" << curve->name();
    qDebug() << "输入类型:" << static_cast<int>(algorithm->inputType());
    qDebug() << "输出类型:" << static_cast<int>(algorithm->outputType());

    // ==================== 两阶段执行机制 ====================
    // 阶段1：准备上下文并验证数据完整性
    bool isReady = algorithm->prepareContext(context);

    if (!isReady) {
        qWarning() << "算法" << name << "数据不完整，无法执行";
        qWarning() << "  可能原因：缺少必需的用户交互数据（如选点、参数）";
        return;
    }

    qDebug() << "算法" << name << "数据就绪，开始执行";

    // 阶段2：执行算法（算法从上下文拉取完整数据）
    AlgorithmResult result = algorithm->executeWithContext(context);

    // 检查执行状态
    if (result.hasError()) {
        qWarning() << "算法" << name << "执行失败:" << result.errorMessage();
        emit algorithmExecutionFailed(name, result.errorMessage());
        return;
    }

    qDebug() << result.toString();

    // 根据输出类型处理结果
    handleAlgorithmResult(result);

    // 发出成功信号
    emit algorithmResultReady(name, result);
}

void AlgorithmManager::handleAlgorithmResult(const AlgorithmResult& result)
{
    if (!result.isSuccess() || !m_curveManager) {
        return;
    }

    auto resultType = result.type();

    switch (resultType) {
    case ResultType::Curve: {
        // 输出为新曲线（最常见情况）
        if (!result.hasCurves()) {
            qWarning() << "算法结果中没有曲线数据";
            return;
        }

        // 添加所有输出曲线
        for (const ThermalCurve& curve : result.curves()) {
            addCurveWithHistory(curve);
        }
        break;
    }

    case ResultType::Marker: {
        // 输出为标注点
        qDebug() << "标注点数量:" << result.markerCount();
        for (int i = 0; i < result.markerCount(); ++i) {
            qDebug() << "  标注点" << i << ":" << result.markers()[i];
        }
        // TODO: 发送标注点到 ChartView
        break;
    }

    case ResultType::Region: {
        // 输出为区域
        qDebug() << "区域数量:" << result.regionCount();
        // TODO: 发送区域到 ChartView（用于阴影填充）
        break;
    }

    case ResultType::ScalarValue: {
        // 输出为标量值
        qDebug() << "标量结果:";
        for (auto it = result.allMeta().constBegin(); it != result.allMeta().constEnd(); ++it) {
            qDebug() << "  " << it.key() << ":" << it.value();
        }
        // TODO: 显示结果对话框或状态栏
        break;
    }

    case ResultType::Composite: {
        // 混合输出：依次处理所有输出
        qDebug() << "混合结果:";

        if (result.hasCurves()) {
            qDebug() << "  包含" << result.curveCount() << "条曲线";
            for (const ThermalCurve& curve : result.curves()) {
                addCurveWithHistory(curve);
            }
        }

        if (result.hasMarkers()) {
            qDebug() << "  包含" << result.markerCount() << "个标注点";
            // TODO: 发送标注点到 ChartView
        }

        if (result.hasRegions()) {
            qDebug() << "  包含" << result.regionCount() << "个区域";
            // TODO: 发送区域到 ChartView
        }

        if (result.hasMeta("area")) {
            qDebug() << "  面积:" << result.area() << result.meta("unit").toString();
        }
        break;
    }

    default:
        qWarning() << "未知的结果类型:" << static_cast<int>(resultType);
        break;
    }
}

void AlgorithmManager::addCurveWithHistory(const ThermalCurve& curve)
{
    if (!m_curveManager) {
        qWarning() << "CurveManager 为空，无法添加曲线";
        return;
    }

    // 使用历史管理添加曲线
    if (m_historyManager) {
        auto command = std::make_unique<AddCurveCommand>(m_curveManager, curve);
        m_historyManager->executeCommand(std::move(command));
        qDebug() << "通过历史管理添加曲线:" << curve.name() << "ID:" << curve.id();
    } else {
        m_curveManager->addCurve(curve);
        m_curveManager->setActiveCurve(curve.id());
        qDebug() << "直接添加曲线:" << curve.name() << "ID:" << curve.id();
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
        if (!HistoryManager::instance()->executeCommand(std::move(command))) {
            qWarning() << "算法结果入栈失败，放弃添加新曲线";
        }
    } else {
        m_curveManager->addCurve(newCurve);
        m_curveManager->setActiveCurve(newId);
    }
}

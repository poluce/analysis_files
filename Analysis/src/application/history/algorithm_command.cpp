#include "algorithm_command.h"
#include "application/curve/curve_manager.h"
#include <QDebug>
#include <QUuid>

AlgorithmCommand::AlgorithmCommand(
    IThermalAlgorithm* algorithm, ThermalCurve* inputCurve, CurveManager* curveManager, const QString& algorithmName)
    : m_algorithm(algorithm)
    , m_inputCurve(inputCurve)
    , m_curveManager(curveManager)
    , m_algorithmName(algorithmName)
    , m_inputCurveName(inputCurve ? inputCurve->name() : "Unknown")
    , m_newCurveData("", "") // 空构造，稍后填充
    , m_executed(false)
{
    qDebug() << "构造:  AlgorithmCommand";
}

bool AlgorithmCommand::execute()
{
    qDebug() << "AlgorithmCommand::execute ：name->" << m_algorithmName;

    // 指针有效性检查
    if (!m_algorithm || !m_inputCurve || !m_curveManager) {
        qWarning() << "[错误] 指针检查失败:";
        return false;
    }

    try {
        // 1. 执行算法处理
        const auto inputData = m_inputCurve->getProcessedData();
        const auto outputData = m_algorithm->process(inputData);
        // 检查输出数据是否有效
        if (outputData.isEmpty()) {
            qWarning() << "[错误] 算法返回空数据";
            return false;
        }

        // 2. 创建新曲线
        m_newCurveId = QUuid::createUuid().toString();
        QString newName = m_algorithm->displayName(); // 使用中文显示名称
        m_newCurveData = ThermalCurve(m_newCurveId, newName);
        // 3. 填充新曲线数据和元数据
        m_newCurveData.setProcessedData(outputData);
        m_newCurveData.setMetadata(m_inputCurve->getMetadata());    // 复制元数据
        m_newCurveData.setParentId(m_inputCurve->id());             // 设置父曲线ID
        m_newCurveData.setProjectName(m_inputCurve->projectName()); // 继承项目名称

        // 仪器类型继承自父曲线（算法不改变仪器类型）
        m_newCurveData.setInstrumentType(m_inputCurve->instrumentType());
        SignalType inputSignalType = m_inputCurve->signalType();
        SignalType outputSignalType = m_algorithm->getOutputSignalType(inputSignalType);

        m_newCurveData.setSignalType(outputSignalType);

        // 5. 通过 CurveManager 添加新曲线
        m_curveManager->addCurve(m_newCurveData);

        // 6. 设置新生成的曲线为活动曲线（默认选中）
        m_curveManager->setActiveCurve(m_newCurveId);

        m_executed = true;
        qDebug() << "执行算法" << m_algorithmName << "于曲线" << m_inputCurveName << "，创建新曲线" << newName
                 << "ID:" << m_newCurveId;

        return true;
    } catch (const std::exception& e) {
        qCritical() << "========== 捕获 std::exception 异常 ==========";
        qCritical() << "异常信息:" << e.what();
        qCritical() << "异常类型:" << typeid(e).name();
        return false;
    } catch (...) {
        qCritical() << "========== 捕获未知类型异常 ==========";
        qCritical() << "无法获取异常详细信息";
        return false;
    }
}

bool AlgorithmCommand::undo()
{
    if (!m_curveManager) {
        qWarning() << "AlgorithmCommand::undo: CurveManager指针为空";
        return false;
    }

    if (!m_executed) {
        qWarning() << "AlgorithmCommand::undo: 命令尚未执行，无法撤销";
        return false;
    }

    // 删除新创建的曲线
    if (m_curveManager->removeCurve(m_newCurveId)) {
        qDebug() << "AlgorithmCommand: 撤销算法" << m_algorithmName << "，删除曲线" << m_newCurveData.name()
                 << "ID:" << m_newCurveId;
        return true;
    } else {
        qWarning() << "AlgorithmCommand::undo: 删除曲线失败，ID:" << m_newCurveId;
        return false;
    }
}

bool AlgorithmCommand::redo()
{
    if (!m_curveManager) {
        qWarning() << "AlgorithmCommand::redo: CurveManager指针为空";
        return false;
    }

    if (!m_executed) {
        qWarning() << "AlgorithmCommand::redo: 命令尚未执行，无法重做";
        return false;
    }

    // 重新添加曲线（使用缓存的数据，避免重新计算）
    m_curveManager->addCurve(m_newCurveData);
    m_curveManager->setActiveCurve(m_newCurveId);

    qDebug() << "AlgorithmCommand: 重做算法" << m_algorithmName << "，重新添加曲线" << m_newCurveData.name()
             << "ID:" << m_newCurveId;

    return true;
}

QString AlgorithmCommand::description() const { return QString("对曲线'%1'执行%2").arg(m_inputCurveName, m_algorithmName); }

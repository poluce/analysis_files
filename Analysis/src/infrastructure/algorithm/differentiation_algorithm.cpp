#include "differentiation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <cmath>

DifferentiationAlgorithm::DifferentiationAlgorithm()
{
    qDebug() << "构造: DifferentiationAlgorithm";
}

QString DifferentiationAlgorithm::name() const
{
    return "differentiation";
}

QString DifferentiationAlgorithm::displayName() const
{
    return "微分";
}

QString DifferentiationAlgorithm::category() const
{
    return "Analysis";
}

SignalType DifferentiationAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 微分算法将原始信号转换为微分信号（适用所有仪器类型）
    return (inputType == SignalType::Raw) ? SignalType::Derivative : inputType;
}

IThermalAlgorithm::InputType DifferentiationAlgorithm::inputType() const
{
    // A类算法：单曲线，无需用户交互
    return InputType::None;
}

IThermalAlgorithm::OutputType DifferentiationAlgorithm::outputType() const
{
    // 输出新曲线（微分曲线）
    return OutputType::Curve;
}

AlgorithmDescriptor DifferentiationAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::None;  // 简单算法，无需交互
    desc.parameters = {
        { "halfWin", "半窗口", QVariant::Int, m_halfWin, false, {{ "min", 1 }} },
        { "dt", "时间步长", QVariant::Double, m_dt, false, {{ "min", 1e-6 }} },
        { "enableDebug", "启用调试", QVariant::Bool, m_enableDebug, false, {} },
    };
    return desc;
}

// ==================== 上下文驱动执行接口实现 ====================

void DifferentiationAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        return;
    }

    // 如果上下文中没有参数，注入默认值
    if (!context->contains("param.halfWin")) {
        context->setValue("param.halfWin", m_halfWin, "DifferentiationAlgorithm::prepareContext");
    }
    if (!context->contains("param.dt")) {
        context->setValue("param.dt", m_dt, "DifferentiationAlgorithm::prepareContext");
    }
    if (!context->contains("param.enableDebug")) {
        context->setValue("param.enableDebug", m_enableDebug, "DifferentiationAlgorithm::prepareContext");
    }

    qDebug() << "DifferentiationAlgorithm::prepareContext - 参数已准备";
}

QVariant DifferentiationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "DifferentiationAlgorithm::executeWithContext - 上下文为空！";
        return QVariant();
    }

    // 从上下文拉取活动曲线
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "DifferentiationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return QVariant();
    }

    // 从上下文拉取参数（使用默认值作为fallback）
    int halfWin = context->get<int>("param.halfWin").value_or(m_halfWin);
    double dt = context->get<double>("param.dt").value_or(m_dt);
    bool enableDebug = context->get<bool>("param.enableDebug").value_or(m_enableDebug);

    // 获取输入数据
    const QVector<ThermalDataPoint>& inputData = curve.value()->getProcessedData();

    // 执行微分算法（核心逻辑）
    QVector<ThermalDataPoint> outputData;

    const int minPoints = 2 * halfWin + 1;
    if (inputData.size() < minPoints) {
        qWarning() << "微分算法: 数据点不足! 需要至少" << minPoints << "个点，实际只有" << inputData.size() << "个点";
        return QVariant::fromValue(outputData);
    }

    if (enableDebug) {
        qDebug() << "========== DTG微分算法开始（上下文驱动）==========";
        qDebug() << "输入数据点数:" << inputData.size();
        qDebug() << "半窗口大小:" << halfWin << "(从上下文获取)";
        qDebug() << "时间步长:" << dt << "(从上下文获取)";
    }

    const double windowTime = halfWin * dt;
    outputData.reserve(inputData.size() - 2 * halfWin);

    int positiveCount = 0;
    int negativeCount = 0;
    int zeroCount = 0;

    for (int i = halfWin; i < inputData.size() - halfWin; ++i) {
        double sum_before = 0.0;
        double sum_after = 0.0;

        for (int j = 1; j <= halfWin; ++j) {
            sum_before += inputData[i - j].value;
            sum_after += inputData[i + j].value;
        }

        const double dy = sum_after - sum_before;
        const double derivative = dy / windowTime / halfWin;

        if (derivative > 0.0001) {
            positiveCount++;
        } else if (derivative < -0.0001) {
            negativeCount++;
        } else {
            zeroCount++;
        }

        ThermalDataPoint point;
        point.temperature = inputData[i].temperature;
        point.value = derivative;
        point.time = inputData[i].time;

        outputData.append(point);
    }

    if (enableDebug) {
        qDebug() << "\n========== 微分统计 ==========";
        qDebug() << "输出数据点数:" << outputData.size();
        qDebug() << "正值点数:" << positiveCount << "(" << (100.0 * positiveCount / outputData.size()) << "%)";
        qDebug() << "负值点数:" << negativeCount << "(" << (100.0 * negativeCount / outputData.size()) << "%)";
        qDebug() << "接近零点数:" << zeroCount << "(" << (100.0 * zeroCount / outputData.size()) << "%)";
        qDebug() << "========== 微分算法结束 ==========\n";
    }

    return QVariant::fromValue(outputData);
}

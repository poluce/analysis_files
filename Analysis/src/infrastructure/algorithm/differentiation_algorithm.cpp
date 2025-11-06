#include "differentiation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <cmath>

/**
 * @brief DTG微分算法 - 大窗口平滑中心差分法
 * @details 使用前后各halfWin个点的和之差计算导数
 *          derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 */
DifferentiationAlgorithm::DifferentiationAlgorithm() { qDebug() << "构造:  DifferentiationAlgorithm"; }

QVector<ThermalDataPoint> DifferentiationAlgorithm::process(const QVector<ThermalDataPoint>& inputData)
{
    QVector<ThermalDataPoint> outputData;

    // 参数验证
    const int minPoints = 2 * m_halfWin + 1;
    if (inputData.size() < minPoints) {
        qWarning() << "微分算法: 数据点不足! 需要至少" << minPoints << "个点，实际只有" << inputData.size() << "个点";
        return outputData;
    }

    if (m_enableDebug) {
        qDebug() << "========== DTG微分算法开始 ==========";
        qDebug() << "输入数据点数:" << inputData.size();
        qDebug() << "半窗口大小:" << m_halfWin;
        qDebug() << "时间步长:" << m_dt;
    }

    // 计算窗口时间
    const double windowTime = m_halfWin * m_dt;

    // 预留空间
    outputData.reserve(inputData.size() - 2 * m_halfWin);

    int positiveCount = 0;
    int negativeCount = 0;
    int zeroCount = 0;

    // 遍历可计算微分的点
    for (int i = m_halfWin; i < inputData.size() - m_halfWin; ++i) {
        double sum_before = 0.0; // 前窗口和
        double sum_after = 0.0;  // 后窗口和

        // 计算前后窗口的和
        for (int j = 1; j <= m_halfWin; ++j) {
            sum_before += inputData[i - j].value;
            sum_after += inputData[i + j].value;
        }

        // 计算差值
        const double dy = sum_after - sum_before;

        // 归一化得到微分值
        const double derivative = dy / windowTime / m_halfWin;

        // 统计正负值
        if (derivative > 0.0001) {
            positiveCount++;
        } else if (derivative < -0.0001) {
            negativeCount++;
        } else {
            zeroCount++;
        }

        // 保存结果
        ThermalDataPoint point;
        point.temperature = inputData[i].temperature;
        point.value = derivative;
        point.time = inputData[i].time;

        outputData.append(point);
    }

    if (m_enableDebug) {
        qDebug() << "\n========== 微分统计 ==========";
        qDebug() << "输出数据点数:" << outputData.size();
        qDebug() << "正值点数:" << positiveCount << "(" << (100.0 * positiveCount / outputData.size()) << "%)";
        qDebug() << "负值点数:" << negativeCount << "(" << (100.0 * negativeCount / outputData.size()) << "%)";
        qDebug() << "接近零点数:" << zeroCount << "(" << (100.0 * zeroCount / outputData.size()) << "%)";
        qDebug() << "========== 微分算法结束 ==========\n";
    }

    return outputData;
}

QString DifferentiationAlgorithm::name() const { return "differentiation"; }

QString DifferentiationAlgorithm::displayName() const { return "微分"; }

QString DifferentiationAlgorithm::category() const { return "Analysis"; }

QVariantMap DifferentiationAlgorithm::parameters() const
{
    QVariantMap params;
    params.insert("halfWin", m_halfWin);
    params.insert("dt", m_dt);
    params.insert("enableDebug", m_enableDebug);
    return params;
}

void DifferentiationAlgorithm::setParameter(const QString& key, const QVariant& value)
{
    if (key == "halfWin") {
        bool ok = false;
        int v = value.toInt(&ok);
        if (ok && v >= 1) {
            m_halfWin = v;
            qDebug() << "DTG微分: 半窗口大小已设置为" << m_halfWin;
        }
    } else if (key == "dt") {
        bool ok = false;
        double v = value.toDouble(&ok);
        if (ok && v > 0.0) {
            m_dt = v;
            qDebug() << "DTG微分: 时间步长已设置为" << m_dt;
        }
    } else if (key == "enableDebug") {
        m_enableDebug = value.toBool();
        qDebug() << "DTG微分: 调试输出已" << (m_enableDebug ? "启用" : "禁用");
    }
    // 向后兼容旧参数名
    else if (key == "smoothWindow") {
        bool ok = false;
        int v = value.toInt(&ok);
        if (ok && v >= 1) {
            m_halfWin = v / 2; // 将旧的smoothWindow转换为halfWin
            qDebug() << "DTG微分: 使用旧参数smoothWindow=" << v << "，已转换为halfWin=" << m_halfWin;
        }
    }
}

void DifferentiationAlgorithm::setParameter(const QVariantMap& params)
{
    for (auto it = params.begin(); it != params.end(); ++it) {
        setParameter(it.key(), it.value());
    }
}

SignalType DifferentiationAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 微分算法将原始信号转换为微分信号（适用所有仪器类型）
    if (inputType == SignalType::Raw) {
        return SignalType::Derivative;
    }
    // 如果输入已经是微分信号，则保持不变
    return inputType;
}

// ==================== 新接口方法实现 ====================

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
    desc.interaction = AlgorithmInteraction::ParameterDialog;
    desc.parameters = {
        { QStringLiteral("halfWin"), QStringLiteral("半窗口"), QVariant::Int, m_halfWin, true, { { QStringLiteral("min"), 1 } } },
        { QStringLiteral("dt"), QStringLiteral("时间步长"), QVariant::Double, m_dt, true, { { QStringLiteral("min"), 1e-6 } } },
        { QStringLiteral("enableDebug"), QStringLiteral("启用调试输出"), QVariant::Bool, m_enableDebug, false, {} },
    };
    return desc;
}

// ==================== 上下文驱动接口实现 ====================

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

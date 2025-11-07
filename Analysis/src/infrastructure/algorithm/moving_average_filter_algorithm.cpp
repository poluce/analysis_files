#include "moving_average_filter_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <QtGlobal>

MovingAverageFilterAlgorithm::MovingAverageFilterAlgorithm()
{
    qDebug() << "构造: MovingAverageFilterAlgorithm";
}

QString MovingAverageFilterAlgorithm::name() const
{
    return "moving_average";
}

QString MovingAverageFilterAlgorithm::displayName() const
{
    return "滤波";
}

QString MovingAverageFilterAlgorithm::category() const
{
    return "Preprocess";
}

SignalType MovingAverageFilterAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 移动平均滤波是预处理算法，不改变信号类型
    // 输出信号类型与输入信号类型相同
    return inputType;
}

IThermalAlgorithm::InputType MovingAverageFilterAlgorithm::inputType() const
{
    // A类算法：单曲线，无需用户交互
    return InputType::None;
}

IThermalAlgorithm::OutputType MovingAverageFilterAlgorithm::outputType() const
{
    // 输出新曲线（滤波后的曲线）
    return OutputType::Curve;
}

AlgorithmDescriptor MovingAverageFilterAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::ParameterDialog;
    desc.parameters = {
        { QStringLiteral("window"), QStringLiteral("窗口大小"), QVariant::Int, m_window, true, { { QStringLiteral("min"), 1 } } },
    };
    return desc;
}

// ==================== 上下文驱动执行接口实现 ====================

bool MovingAverageFilterAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "MovingAverageFilterAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "MovingAverageFilterAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：注入默认参数（如果需要）
    if (!context->contains("param.window")) {
        context->setValue("param.window", m_window, "MovingAverageFilterAlgorithm::prepareContext");
    }

    qDebug() << "MovingAverageFilterAlgorithm::prepareContext - 数据就绪";
    return true;
}

AlgorithmResult MovingAverageFilterAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "MovingAverageFilterAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("moving_average", "上下文为空");
    }

    // 2. 拉取曲线
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "MovingAverageFilterAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("moving_average", "无法获取活动曲线");
    }

    ThermalCurve* inputCurve = curve.value();

    // 3. 拉取参数（使用 value_or() 提供默认值）
    int window = context->get<int>("param.window").value_or(m_window);

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& inputData = inputCurve->getProcessedData();

    // 5. 执行核心算法逻辑（移动平均滤波）
    QVector<ThermalDataPoint> outputData;
    const int n = inputData.size();
    if (n == 0) {
        qWarning() << "MovingAverageFilterAlgorithm::executeWithContext - 输入数据为空！";
        return AlgorithmResult::failure("moving_average", "输入数据为空");
    }

    // 安全窗口（最少为1）
    const int w = qMax(1, window);
    const int half = w / 2; // 对称窗口

    outputData.resize(n);
    for (int i = 0; i < n; ++i) {
        int left = qMax(0, i - half);
        int right = qMin(n - 1, i + half);
        // 当窗口为偶数时，右侧自然比左侧多1个元素，这里不强行平衡

        double sum = 0.0;
        int count = 0;
        for (int j = left; j <= right; ++j) {
            sum += inputData[j].value;
            ++count;
        }

        ThermalDataPoint p;
        p.temperature = inputData[i].temperature;
        p.time = inputData[i].time;
        p.value = (count > 0) ? (sum / count) : inputData[i].value;
        outputData[i] = p;
    }

    qDebug() << "MovingAverageFilterAlgorithm::executeWithContext - 完成，窗口大小:" << w << "，输出数据点数:" << outputData.size();

    // 6. 创建结果对象
    AlgorithmResult result = AlgorithmResult::success(
        "moving_average",
        inputCurve->id(),
        ResultType::Curve
    );

    // 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), displayName());
    outputCurve.setProcessedData(outputData);
    outputCurve.setInstrumentType(inputCurve->instrumentType());
    outputCurve.setSignalType(getOutputSignalType(inputCurve->signalType()));
    outputCurve.setParentId(inputCurve->id());
    outputCurve.setProjectName(inputCurve->projectName());
    outputCurve.setMetadata(inputCurve->getMetadata());

    // 填充结果
    result.setCurve(outputCurve);
    result.setMeta("method", "Moving Average");
    result.setMeta("windowSize", w);
    result.setMeta("label", "滤波曲线");

    return result;
}

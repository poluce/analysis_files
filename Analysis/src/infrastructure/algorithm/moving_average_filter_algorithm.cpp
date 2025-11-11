#include "moving_average_filter_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QVariant>
#include <QUuid>
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

// ==================== 曲线属性声明接口实现 ====================

bool MovingAverageFilterAlgorithm::isAuxiliaryCurve() const
{
    // 移动平均滤波曲线是辅助曲线
    // 原因：平滑处理不改变数据类型，应该继承父曲线的Y轴以保持视觉一致性
    return true;
}

bool MovingAverageFilterAlgorithm::isStronglyBound() const
{
    // 滤波曲线是非强绑定曲线
    // 原因：滤波后的曲线可以作为独立的分析对象，应该在树中独立显示
    return false;
}

// ==================== 上下文驱动执行接口实现 ====================

bool MovingAverageFilterAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "MovingAverageFilterAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "MovingAverageFilterAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：注入默认参数（如果需要）
    if (!context->contains(ContextKeys::ParamWindow)) {
        context->setValue(ContextKeys::ParamWindow, m_window, "MovingAverageFilterAlgorithm::prepareContext");
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

    // 2. 拉取曲线（上下文存储的是副本，线程安全）
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "MovingAverageFilterAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("moving_average", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 3. 拉取参数（使用 value_or() 提供默认值）
    int window = context->get<int>(ContextKeys::ParamWindow).value_or(m_window);

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& inputData = inputCurve.getProcessedData();

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

    // 进度报告：计算总迭代次数
    int lastReportedProgress = 0;

    for (int i = 0; i < n; ++i) {
        // 检查取消标志（每100次迭代）
        if (i % 100 == 0 && shouldCancel()) {
            qWarning() << "MovingAverageFilterAlgorithm: 用户取消执行";
            return AlgorithmResult::failure("moving_average", "用户取消执行");
        }

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

        // 进度报告（每10%）
        int currentProgress = (i * 100) / n;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("已处理 %1/%2 点").arg(i + 1).arg(n));
        }
    }

    // 最终进度报告
    reportProgress(100, "滤波完成");

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
    outputCurve.setIsAuxiliaryCurve(this->isAuxiliaryCurve());  // 设置辅助曲线标志
    outputCurve.setIsStronglyBound(this->isStronglyBound());    // 设置强绑定标志

    // 填充结果
    result.setCurve(outputCurve);
    result.setMeta("method", "Moving Average");
    result.setMeta("windowSize", w);
    result.setMeta("label", "滤波曲线");

    return result;
}

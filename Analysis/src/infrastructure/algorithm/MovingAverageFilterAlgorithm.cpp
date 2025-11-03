#include "MovingAverageFilterAlgorithm.h"
#include "domain/model/ThermalDataPoint.h"
#include <QtGlobal>

QVector<ThermalDataPoint> MovingAverageFilterAlgorithm::process(const QVector<ThermalDataPoint>& inputData)
{
    QVector<ThermalDataPoint> output;
    const int n = inputData.size();
    if (n == 0) return output;

    // 安全窗口（最少为1）
    const int w = qMax(1, m_window);
    const int half = w / 2; // 对称窗口

    output.resize(n);
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
        output[i] = p;
    }

    return output;
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

QVariantMap MovingAverageFilterAlgorithm::parameters() const
{
    QVariantMap params;
    params.insert("window", m_window);
    return params;
}

void MovingAverageFilterAlgorithm::setParameter(const QString& key, const QVariant& value)
{
    if (key == "window") {
        bool ok = false;
        int v = value.toInt(&ok);
        if (ok && v >= 1) {
            m_window = v;
        }
    }
}

SignalType MovingAverageFilterAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 移动平均滤波是预处理算法，不改变信号类型
    // 输出信号类型与输入信号类型相同
    return inputType;
}

// ==================== 新接口方法实现 ====================

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


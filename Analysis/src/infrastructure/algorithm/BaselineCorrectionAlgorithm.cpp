#include "BaselineCorrectionAlgorithm.h"
#include "domain/model/ThermalDataPoint.h"
#include <QDebug>
#include <QtMath>
#include <QPointF>

BaselineCorrectionAlgorithm::BaselineCorrectionAlgorithm()
    : m_subtractBaseline(false)
{
}

QVector<ThermalDataPoint> BaselineCorrectionAlgorithm::process(const QVector<ThermalDataPoint>& inputData)
{
    // 旧接口不支持选点，返回空
    Q_UNUSED(inputData);
    qWarning() << "基线校正算法需要使用新接口 execute()，旧接口不支持";
    return QVector<ThermalDataPoint>();
}

QString BaselineCorrectionAlgorithm::name() const
{
    return "baseline_correction";
}

QString BaselineCorrectionAlgorithm::displayName() const
{
    return m_subtractBaseline ? "基线校正" : "基线";
}

QString BaselineCorrectionAlgorithm::category() const
{
    return "Preprocess";
}

QVariantMap BaselineCorrectionAlgorithm::parameters() const
{
    QVariantMap params;
    params.insert("subtractBaseline", m_subtractBaseline);
    return params;
}

void BaselineCorrectionAlgorithm::setParameter(const QString& key, const QVariant& value)
{
    if (key == "subtractBaseline") {
        m_subtractBaseline = value.toBool();
        qDebug() << "基线校正：" << (m_subtractBaseline ? "扣除基线" : "仅显示基线");
    }
}

SignalType BaselineCorrectionAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 基线校正不改变信号类型
    return inputType;
}

// ==================== 新接口方法实现 ====================

IThermalAlgorithm::InputType BaselineCorrectionAlgorithm::inputType() const
{
    // B类算法：需要用户选择2个点
    return InputType::PointSelection;
}

IThermalAlgorithm::OutputType BaselineCorrectionAlgorithm::outputType() const
{
    // 输出新曲线（基线曲线或扣除基线后的曲线）
    return OutputType::Curve;
}

QString BaselineCorrectionAlgorithm::userPrompt() const
{
    return "请在曲线上选择两个点定义基线范围（起点和终点）";
}

QVariant BaselineCorrectionAlgorithm::execute(const QVariantMap& inputs)
{
    // 1. 验证输入
    if (!inputs.contains("mainCurve")) {
        qWarning() << "基线校正：缺少主曲线";
        return QVariant();
    }

    if (!inputs.contains("points")) {
        qWarning() << "基线校正：缺少选择的点";
        return QVariant();
    }

    // 2. 提取数据
    auto mainCurve = inputs["mainCurve"].value<ThermalCurve*>();
    if (!mainCurve) {
        qWarning() << "基线校正：主曲线为空";
        return QVariant();
    }

    auto points = inputs["points"].value<QVector<QPointF>>();
    if (points.size() < 2) {
        qWarning() << "基线校正：需要至少2个点，实际只有" << points.size() << "个点";
        return QVariant();
    }

    const QVector<ThermalDataPoint>& curveData = mainCurve->getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "基线校正：曲线数据为空";
        return QVariant();
    }

    // 3. 使用前两个点
    QPointF point1 = points[0];  // (temperature, value)
    QPointF point2 = points[1];

    qDebug() << "基线校正：点1 =" << point1 << ", 点2 =" << point2;

    // 4. 生成基线
    QVector<ThermalDataPoint> baseline = generateBaseline(curveData, point1, point2);

    if (baseline.isEmpty()) {
        qWarning() << "基线校正：生成基线失败";
        return QVariant();
    }

    // 5. 根据参数决定输出类型
    QVector<ThermalDataPoint> result;

    if (m_subtractBaseline) {
        // 输出扣除基线后的曲线
        qDebug() << "基线校正：输出扣除基线后的曲线";
        result.reserve(curveData.size());

        for (int i = 0; i < curveData.size() && i < baseline.size(); ++i) {
            ThermalDataPoint p = curveData[i];
            p.value = curveData[i].value - baseline[i].value;
            result.append(p);
        }
    } else {
        // 输出基线曲线
        qDebug() << "基线校正：输出基线曲线";
        result = baseline;
    }

    qDebug() << "基线校正完成：生成" << result.size() << "个数据点";
    return QVariant::fromValue(result);
}

QVector<ThermalDataPoint> BaselineCorrectionAlgorithm::generateBaseline(
    const QVector<ThermalDataPoint>& curveData,
    const QPointF& point1,
    const QPointF& point2) const
{
    QVector<ThermalDataPoint> baseline;

    if (curveData.isEmpty()) {
        return baseline;
    }

    // 确保 point1 的温度小于 point2
    double temp1 = qMin(point1.x(), point2.x());
    double temp2 = qMax(point1.x(), point2.x());
    double value1 = (point1.x() < point2.x()) ? point1.y() : point2.y();
    double value2 = (point1.x() < point2.x()) ? point2.y() : point1.y();

    qDebug() << "生成基线：温度范围 [" << temp1 << "," << temp2 << "]";
    qDebug() << "生成基线：值范围 [" << value1 << "," << value2 << "]";

    // 线性插值斜率
    double slope = 0.0;
    if (!qFuzzyCompare(temp2, temp1)) {
        slope = (value2 - value1) / (temp2 - temp1);
    }

    // 为所有数据点生成基线值
    baseline.reserve(curveData.size());

    for (const auto& point : curveData) {
        ThermalDataPoint baselinePoint;
        baselinePoint.temperature = point.temperature;
        baselinePoint.time = point.time;

        // 线性插值计算基线值
        if (point.temperature < temp1) {
            // 温度低于起点，使用起点值
            baselinePoint.value = value1;
        } else if (point.temperature > temp2) {
            // 温度高于终点，使用终点值
            baselinePoint.value = value2;
        } else {
            // 温度在范围内，线性插值
            baselinePoint.value = value1 + slope * (point.temperature - temp1);
        }

        baseline.append(baselinePoint);
    }

    return baseline;
}

ThermalDataPoint BaselineCorrectionAlgorithm::findNearestPoint(
    const QVector<ThermalDataPoint>& curveData,
    double temperature) const
{
    if (curveData.isEmpty()) {
        return ThermalDataPoint();
    }

    // 找到最接近指定温度的点
    int nearestIdx = 0;
    double minDist = qAbs(curveData[0].temperature - temperature);

    for (int i = 1; i < curveData.size(); ++i) {
        double dist = qAbs(curveData[i].temperature - temperature);
        if (dist < minDist) {
            minDist = dist;
            nearestIdx = i;
        }
    }

    return curveData[nearestIdx];
}

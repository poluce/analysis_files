#include "baseline_correction_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QDebug>
#include <QPointF>
#include <QVariant>
#include <QtMath>

BaselineCorrectionAlgorithm::BaselineCorrectionAlgorithm()
{
    qDebug() << "构造: BaselineCorrectionAlgorithm";
}

QString BaselineCorrectionAlgorithm::name() const
{
    return "baseline_correction";
}

QString BaselineCorrectionAlgorithm::displayName() const
{
    return "基线";
}

QString BaselineCorrectionAlgorithm::category() const
{
    return "Preprocess";
}

SignalType BaselineCorrectionAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 基线校正不改变信号类型
    return inputType;
}

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

AlgorithmDescriptor BaselineCorrectionAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::PointSelection;
    desc.requiredPointCount = 2;
    desc.pointSelectionHint = "请在曲线上选择两个点定义基线范围（起点和终点）";
    return desc;
}

// ==================== 上下文驱动执行接口实现 ====================

bool BaselineCorrectionAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：验证交互式算法的选点数据
    // 基线校正需要用户选择至少2个点
    auto points = context->get<QVector<QPointF>>("selectedPoints");
    if (!points.has_value() || points.value().size() < 2) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 需要至少2个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

    qDebug() << "BaselineCorrectionAlgorithm::prepareContext - 数据就绪，选点数:" << points.value().size();
    return true;
}

QVariant BaselineCorrectionAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 上下文为空！";
        return QVariant();
    }

    // 2. 拉取曲线
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 无法获取活动曲线！";
        return QVariant();
    }

    // 3. 拉取选择的点
    auto pointsOpt = context->get<QVector<QPointF>>("selectedPoints");
    if (!pointsOpt.has_value()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 无法获取选择的点！";
        return QVariant();
    }

    QVector<QPointF> points = pointsOpt.value();
    if (points.size() < 2) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 需要至少2个点，实际只有" << points.size() << "个点";
        return QVariant();
    }

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& curveData = curve.value()->getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 曲线数据为空！";
        return QVariant();
    }

    // 5. 使用前两个点生成基线
    QPointF point1 = points[0]; // (temperature, value)
    QPointF point2 = points[1];

    qDebug() << "BaselineCorrectionAlgorithm::executeWithContext - 点1 =" << point1 << ", 点2 =" << point2;

    // 6. 执行核心算法逻辑（生成基线）
    QVector<ThermalDataPoint> baseline = generateBaseline(curveData, point1, point2);

    if (baseline.isEmpty()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 生成基线失败！";
        return QVariant();
    }

    qDebug() << "BaselineCorrectionAlgorithm::executeWithContext - 完成，生成基线数据点数:" << baseline.size();

    // 7. 返回结果
    return QVariant::fromValue(baseline);
}

QVector<ThermalDataPoint> BaselineCorrectionAlgorithm::generateBaseline(
    const QVector<ThermalDataPoint>& curveData, const QPointF& point1, const QPointF& point2) const
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

ThermalDataPoint
BaselineCorrectionAlgorithm::findNearestPoint(const QVector<ThermalDataPoint>& curveData, double temperature) const
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

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

// ==================== 曲线属性声明接口实现 ====================

bool BaselineCorrectionAlgorithm::isAuxiliaryCurve() const
{
    // 基线曲线是辅助曲线
    // 原因：基线用于参考和校正，应该继承父曲线的Y轴以便直观对比
    return true;
}

bool BaselineCorrectionAlgorithm::isStronglyBound() const
{
    // 基线曲线是强绑定曲线
    // 原因：基线仅作为父曲线的辅助参考，不应独立显示，随父曲线隐藏
    return true;
}

// ==================== 上下文驱动执行接口实现 ====================

bool BaselineCorrectionAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：验证交互式算法的选点数据
    // 基线校正需要用户选择至少2个点
    auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!points.has_value() || points.value().size() < 2) {
        qWarning() << "BaselineCorrectionAlgorithm::prepareContext - 需要至少2个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

    qDebug() << "BaselineCorrectionAlgorithm::prepareContext - 数据就绪，选点数:" << points.value().size();
    return true;
}

AlgorithmResult BaselineCorrectionAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("baseline_correction", "上下文为空");
    }

    // 2. 拉取曲线
    auto curve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
    if (!curve.has_value() || !curve.value()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("baseline_correction", "无法获取活动曲线");
    }

    ThermalCurve* inputCurve = curve.value();

    // 3. 拉取选择的点（ThermalDataPoint 类型）
    auto pointsOpt = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!pointsOpt.has_value()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 无法获取选择的点！";
        return AlgorithmResult::failure("baseline_correction", "无法获取选择的点");
    }

    QVector<ThermalDataPoint> selectedPoints = pointsOpt.value();
    if (selectedPoints.size() < 2) {
        QString error = QString("需要至少2个点，实际只有 %1 个点").arg(selectedPoints.size());
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("baseline_correction", error);
    }

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& curveData = inputCurve->getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 曲线数据为空！";
        return AlgorithmResult::failure("baseline_correction", "曲线数据为空");
    }

    // 5. 将 ThermalDataPoint 转换为 QPointF（使用温度作为X轴，值作为Y轴）
    // 基线始终基于温度，与显示的横轴模式无关
    QPointF point1(selectedPoints[0].temperature, selectedPoints[0].value);
    QPointF point2(selectedPoints[1].temperature, selectedPoints[1].value);

    qDebug() << "BaselineCorrectionAlgorithm::executeWithContext - 点1 =" << point1 << ", 点2 =" << point2;

    // 6. 执行核心算法逻辑（生成基线）
    QVector<ThermalDataPoint> baseline = generateBaseline(curveData, point1, point2);

    if (baseline.isEmpty()) {
        qWarning() << "BaselineCorrectionAlgorithm::executeWithContext - 生成基线失败！";
        return AlgorithmResult::failure("baseline_correction", "生成基线失败");
    }

    qDebug() << "BaselineCorrectionAlgorithm::executeWithContext - 完成，生成基线数据点数:" << baseline.size();

    // 7. 创建混合结果对象（曲线 + 标注点）
    AlgorithmResult result = AlgorithmResult::success(
        "baseline_correction",
        inputCurve->id(),
        ResultType::Composite  // 混合输出：曲线 + 标注点
    );

    // 创建输出曲线
    ThermalCurve outputCurve(QUuid::createUuid().toString(), displayName());
    outputCurve.setProcessedData(baseline);
    outputCurve.setInstrumentType(inputCurve->instrumentType());
    outputCurve.setSignalType(SignalType::Baseline);
    outputCurve.setParentId(inputCurve->id());
    outputCurve.setProjectName(inputCurve->projectName());
    outputCurve.setMetadata(inputCurve->getMetadata());
    outputCurve.setIsAuxiliaryCurve(this->isAuxiliaryCurve());  // 设置辅助曲线标志
    outputCurve.setIsStronglyBound(this->isStronglyBound());    // 设置强绑定标志

    // 填充结果
    result.setCurve(outputCurve);

    // 添加标注点（用户选择的基线定义点）
    result.addMarker(point1, "基线起点");
    result.addMarker(point2, "基线终点");

    // 添加元数据
    result.setMeta("correctionType", "Linear");
    result.setMeta("baselinePointCount", selectedPoints.size());
    result.setMeta("temperatureRange", QString("%1 - %2").arg(point1.x()).arg(point2.x()));
    result.setMeta("label", "基线曲线");
    result.setMeta("markerColor", QColor(Qt::red));  // 使用红色，与用户选点时的颜色一致

    return result;
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

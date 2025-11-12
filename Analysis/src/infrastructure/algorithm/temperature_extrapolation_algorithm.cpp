#include "temperature_extrapolation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QColor>
#include <QDebug>
#include <QPointF>
#include <QVariant>
#include <QtMath>

// ==================== 调试开关 ====================
// 设置为 1 启用调试日志，设置为 0 禁用（生产环境）
#define DEBUG_TEMPERATURE_EXTRAPOLATION 1

TemperatureExtrapolationAlgorithm::TemperatureExtrapolationAlgorithm()
{
#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "构造: TemperatureExtrapolationAlgorithm";
#endif
}

QString TemperatureExtrapolationAlgorithm::name() const
{
    return "temperature_extrapolation";
}

QString TemperatureExtrapolationAlgorithm::displayName() const
{
    return "外推温度";
}

QString TemperatureExtrapolationAlgorithm::category() const
{
    return "Analysis";
}

SignalType TemperatureExtrapolationAlgorithm::getOutputSignalType(SignalType inputType) const
{
    // 外推温度不改变信号类型，保持原样
    return inputType;
}

IThermalAlgorithm::InputType TemperatureExtrapolationAlgorithm::inputType() const
{
    // B类算法：需要用户选择至少3个点
    return InputType::PointSelection;
}

IThermalAlgorithm::OutputType TemperatureExtrapolationAlgorithm::outputType() const
{
    // 输出标注（外推点、切线）
    return OutputType::Annotation;
}

AlgorithmDescriptor TemperatureExtrapolationAlgorithm::descriptor() const
{
    AlgorithmDescriptor desc;
    desc.name = name();
    desc.interaction = AlgorithmInteraction::PointSelection;
    desc.requiredPointCount = 3;
    desc.pointSelectionHint = "请选择3个点：前2个点定义切线区域，第3个点定义基线水平";
    return desc;
}

// ==================== 曲线属性声明接口实现 ====================

bool TemperatureExtrapolationAlgorithm::isAuxiliaryCurve() const
{
    // 外推温度不生成曲线，返回 false
    return false;
}

bool TemperatureExtrapolationAlgorithm::isStronglyBound() const
{
    // 外推温度不生成曲线，返回 false
    return false;
}

// ==================== 上下文驱动执行接口实现 ====================

bool TemperatureExtrapolationAlgorithm::prepareContext(AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 上下文为空";
        return false;
    }

    // 阶段1：验证必需数据是否存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：验证交互式算法的选点数据
    // 外推温度需要用户选择至少3个点
    auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!points.has_value() || points.value().size() < 3) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 需要至少3个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::prepareContext - 数据就绪，选点数:" << points.value().size();
#endif
    return true;
}

AlgorithmResult TemperatureExtrapolationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // 1. 验证上下文
    if (!context) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 上下文为空！";
        return AlgorithmResult::failure("temperature_extrapolation", "上下文为空");
    }

    // 2. 拉取曲线（上下文存储的是副本，线程安全）
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("temperature_extrapolation", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 3. 拉取选择的点（ThermalDataPoint 类型）
    auto pointsOpt = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!pointsOpt.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 无法获取选择的点！";
        return AlgorithmResult::failure("temperature_extrapolation", "无法获取选择的点");
    }

    QVector<ThermalDataPoint> selectedPoints = pointsOpt.value();
    if (selectedPoints.size() < 3) {
        QString error = QString("需要至少3个点，实际只有 %1 个点").arg(selectedPoints.size());
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    // 4. 获取输入数据
    const QVector<ThermalDataPoint>& curveData = inputCurve.getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 曲线数据为空！";
        return AlgorithmResult::failure("temperature_extrapolation", "曲线数据为空");
    }

    // 5. 提取关键点
    ThermalDataPoint tangentPoint1 = selectedPoints[0];  // 切线区域起点
    ThermalDataPoint tangentPoint2 = selectedPoints[1];  // 切线区域终点
    ThermalDataPoint baselinePoint = selectedPoints[2];  // 基线点

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 切线点1:"
             << tangentPoint1.temperature << "," << tangentPoint1.value;
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 切线点2:"
             << tangentPoint2.temperature << "," << tangentPoint2.value;
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 基线点:"
             << baselinePoint.temperature << "," << baselinePoint.value;
#endif

    // 6. 提取拟合区域的数据点
    QVector<ThermalDataPoint> fittingRegion = extractFittingRegion(
        curveData, tangentPoint1, tangentPoint2
    );

    if (fittingRegion.size() < 2) {
        QString error = "拟合区域数据点不足（至少需要2个点）";
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 拟合区域点数:" << fittingRegion.size();
#endif

    // 7. 拟合切线（最小二乘法）
    double slope = 0.0;
    double intercept = 0.0;
    if (!fitTangentLine(fittingRegion, slope, intercept)) {
        QString error = "切线拟合失败";
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 切线参数: y =" << slope << "* x +" << intercept;
#endif

    // 检查是否被用户取消
    if (shouldCancel()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 用户取消执行！";
        return AlgorithmResult::failure("temperature_extrapolation", "用户取消执行");
    }

    // 8. 计算外推温度（切线与基线的交点）
    double baselineY = baselinePoint.value;
    double extrapolatedTemp = calculateIntersection(slope, intercept, baselineY);

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 外推温度:" << extrapolatedTemp;
#endif

    // 9. 创建结果对象（混合输出：标注点 + 切线 + 数值）
    AlgorithmResult result = AlgorithmResult::success(
        "temperature_extrapolation",
        inputCurve.id(),
        ResultType::Composite  // 混合输出
    );

    // 添加外推点标注
    QPointF extrapolatedPoint(extrapolatedTemp, baselineY);
    result.addMarker(extrapolatedPoint, "外推点");

    // 添加切线端点标注
    result.addMarker(QPointF(tangentPoint1.temperature, tangentPoint1.value), "切线起点");
    result.addMarker(QPointF(tangentPoint2.temperature, tangentPoint2.value), "切线终点");

    // 添加基线点标注
    result.addMarker(QPointF(baselinePoint.temperature, baselinePoint.value), "基线点");

    // 添加切线的可视化（作为区域）
    QPolygonF tangentLine;
    double temp1 = qMin(tangentPoint1.temperature, tangentPoint2.temperature);
    double temp2 = qMax(tangentPoint1.temperature, tangentPoint2.temperature);

    // 延长切线到外推点
    double extendedTemp1 = qMin(temp1, extrapolatedTemp) - 10;  // 延长10°C
    double extendedTemp2 = qMax(temp2, extrapolatedTemp) + 10;

    tangentLine << QPointF(extendedTemp1, slope * extendedTemp1 + intercept);
    tangentLine << QPointF(extendedTemp2, slope * extendedTemp2 + intercept);
    result.addRegion(tangentLine, "切线");

    // 格式化显示文本
    QString tempText = formatTemperatureText(extrapolatedTemp, inputCurve.instrumentType());

    // 添加元数据
    result.setMeta("extrapolatedTemperature", extrapolatedTemp);
    result.setMeta("slope", slope);
    result.setMeta("intercept", intercept);
    result.setMeta("baselineY", baselineY);
    result.setMeta("instrumentType", static_cast<int>(inputCurve.instrumentType()));
    result.setMeta("label", tempText);  // 用于 UI 显示的文本
    result.setMeta("markerColor", QColor(Qt::red));

    // 标签显示位置（外推点旁边）
    QPointF labelPos(extrapolatedTemp + 5, baselineY + 0.5);
    result.setMeta("labelPosition", labelPos);

    return result;
}

// ==================== 私有辅助函数实现 ====================

bool TemperatureExtrapolationAlgorithm::fitTangentLine(
    const QVector<ThermalDataPoint>& points,
    double& slope,
    double& intercept) const
{
    int n = points.size();
    if (n < 2) {
        return false;
    }

    // 最小二乘法拟合直线 y = k*x + b
    // k = (n*Σxy - Σx*Σy) / (n*Σx² - (Σx)²)
    // b = (Σy - k*Σx) / n

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXY = 0.0;
    double sumX2 = 0.0;

    // 进度报告：计算总迭代次数
    const int totalIterations = n;
    int lastReportedProgress = 0;

    for (int i = 0; i < n; ++i) {
        // 检查取消标志（每100次迭代）
        if (i % 100 == 0 && shouldCancel()) {
            qWarning() << "TemperatureExtrapolationAlgorithm: 用户取消执行";
            return false;
        }

        double x = points[i].temperature;
        double y = points[i].value;

        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;

        // 进度报告（每10%）
        int currentProgress = (i * 100) / totalIterations;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("拟合切线 %1/%2").arg(i + 1).arg(totalIterations));
        }
    }

    // 计算斜率和截距
    double denominator = n * sumX2 - sumX * sumX;
    if (qAbs(denominator) < 1e-10) {
        // 避免除以零（所有点的X坐标相同）
        return false;
    }

    slope = (n * sumXY - sumX * sumY) / denominator;
    intercept = (sumY - slope * sumX) / n;

    // 最终进度报告
    reportProgress(100, "切线拟合完成");

    return true;
}

double TemperatureExtrapolationAlgorithm::calculateIntersection(
    double slope,
    double intercept,
    double baselineY) const
{
    // 切线方程: y = slope * x + intercept
    // 基线方程: y = baselineY
    // 交点: slope * x + intercept = baselineY
    // 解得: x = (baselineY - intercept) / slope

    if (qAbs(slope) < 1e-10) {
        // 斜率接近于零，切线与基线平行，无交点
        qWarning() << "TemperatureExtrapolationAlgorithm::calculateIntersection - 切线与基线平行，无交点！";
        return 0.0;
    }

    double x = (baselineY - intercept) / slope;
    return x;
}

QVector<ThermalDataPoint> TemperatureExtrapolationAlgorithm::extractFittingRegion(
    const QVector<ThermalDataPoint>& curveData,
    const ThermalDataPoint& point1,
    const ThermalDataPoint& point2) const
{
    QVector<ThermalDataPoint> region;

    // 确保 temp1 < temp2
    double temp1 = qMin(point1.temperature, point2.temperature);
    double temp2 = qMax(point1.temperature, point2.temperature);

    // 提取温度范围内的所有数据点
    for (const ThermalDataPoint& point : curveData) {
        if (point.temperature >= temp1 && point.temperature <= temp2) {
            region.append(point);
        }
    }

    return region;
}

QString TemperatureExtrapolationAlgorithm::formatTemperatureText(
    double temperature,
    InstrumentType instrumentType) const
{
    Q_UNUSED(instrumentType);

    // 格式化数值，保留2位小数
    QString valueStr = QString::number(temperature, 'f', 2);

    return QString("外推温度 = %1 °C").arg(valueStr);
}

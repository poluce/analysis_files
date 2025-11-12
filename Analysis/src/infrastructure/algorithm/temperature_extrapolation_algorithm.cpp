#include "temperature_extrapolation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "application/curve/curve_manager.h"
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
    // B类算法：需要用户选择2个点（切线区域）
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
    desc.requiredPointCount = 2;  // 只需要2个点定义切线区域
    desc.pointSelectionHint = "请选择2个点定义切线区域（峰的前沿或后沿）";
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

    // 阶段1：验证活动曲线存在
    auto curve = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curve.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 缺少活动曲线";
        return false;
    }

    // 阶段2：验证基线曲线存在
    ThermalCurve baselineCurve;
    if (!findBaselineCurve(context, baselineCurve)) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 未找到基线曲线，请先使用'基线校正'功能绘制基线";
        return false;
    }

    // 阶段3：验证选点数据（2个点定义切线区域）
    auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!points.has_value() || points.value().size() < 2) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 需要2个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::prepareContext - 数据就绪";
    qDebug() << "  - 选点数:" << points.value().size();
    qDebug() << "  - 基线曲线:" << baselineCurve.name();
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

    // 2. 拉取活动曲线
    auto curveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!curveOpt.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 无法获取活动曲线！";
        return AlgorithmResult::failure("temperature_extrapolation", "无法获取活动曲线");
    }

    const ThermalCurve& inputCurve = curveOpt.value();

    // 3. 查找基线曲线
    ThermalCurve baselineCurve;
    if (!findBaselineCurve(context, baselineCurve)) {
        QString error = "未找到基线曲线，请先使用'基线校正'功能绘制基线";
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    const QVector<ThermalDataPoint>& baselineData = baselineCurve.getProcessedData();
    if (baselineData.isEmpty()) {
        QString error = "基线曲线数据为空";
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    // 4. 拉取选择的点（2个点定义切线区域）
    auto pointsOpt = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!pointsOpt.has_value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 无法获取选择的点！";
        return AlgorithmResult::failure("temperature_extrapolation", "无法获取选择的点");
    }

    QVector<ThermalDataPoint> selectedPoints = pointsOpt.value();
    if (selectedPoints.size() < 2) {
        QString error = QString("需要2个点，实际只有 %1 个点").arg(selectedPoints.size());
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    // 5. 获取输入数据
    const QVector<ThermalDataPoint>& curveData = inputCurve.getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 曲线数据为空！";
        return AlgorithmResult::failure("temperature_extrapolation", "曲线数据为空");
    }

    // 6. 提取切线区域的两个点
    ThermalDataPoint tangentPoint1 = selectedPoints[0];
    ThermalDataPoint tangentPoint2 = selectedPoints[1];

    double temp1 = qMin(tangentPoint1.temperature, tangentPoint2.temperature);
    double temp2 = qMax(tangentPoint1.temperature, tangentPoint2.temperature);

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 切线区域:"
             << "[" << temp1 << "," << temp2 << "]";
#endif

    // 7. 验证峰范围是否在基线范围内
    if (!validatePeakRange(baselineData, temp1, temp2)) {
        QString error = QString("峰范围 [%1, %2] 超出基线范围，请在基线覆盖的温度区域内选择")
                            .arg(temp1).arg(temp2);
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    // 8. 提取拟合区域的数据点
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

    // 9. 拟合切线（最小二乘法）
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

    // 10. 计算切线与基线曲线的交点
    double extrapolatedTemp = 0.0;
    double baselineMinTemp = baselineData.first().temperature;
    double baselineMaxTemp = baselineData.last().temperature;

    if (!calculateIntersectionWithBaseline(slope, intercept, baselineData,
                                            baselineMinTemp, baselineMaxTemp,
                                            extrapolatedTemp)) {
        QString error = "未找到切线与基线的交点";
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::executeWithContext - 外推温度:" << extrapolatedTemp;
#endif

    // 11. 获取外推点的 Y 值（基线上的值）
    double extrapolatedY = getBaselineYAtTemperature(baselineData, extrapolatedTemp);

    // 12. 创建结果对象（混合输出：标注点 + 切线 + 数值）
    AlgorithmResult result = AlgorithmResult::success(
        "temperature_extrapolation",
        inputCurve.id(),
        ResultType::Composite
    );

    // 添加外推点标注
    QPointF extrapolatedPoint(extrapolatedTemp, extrapolatedY);
    result.addMarker(extrapolatedPoint, "外推点");

    // 添加切线端点标注
    result.addMarker(QPointF(tangentPoint1.temperature, tangentPoint1.value), "切线起点");
    result.addMarker(QPointF(tangentPoint2.temperature, tangentPoint2.value), "切线终点");

    // 添加切线的可视化（作为区域）
    // 延长切线穿过外推点
    double extendedTemp1 = qMin(temp1, extrapolatedTemp) - 10;
    double extendedTemp2 = qMax(temp2, extrapolatedTemp) + 10;

    QPolygonF tangentLine;
    tangentLine << QPointF(extendedTemp1, slope * extendedTemp1 + intercept);
    tangentLine << QPointF(extendedTemp2, slope * extendedTemp2 + intercept);
    result.addRegion(tangentLine, "切线");

    // 格式化显示文本
    QString tempText = formatTemperatureText(extrapolatedTemp, inputCurve.instrumentType());

    // 添加元数据
    result.setMeta("extrapolatedTemperature", extrapolatedTemp);
    result.setMeta("slope", slope);
    result.setMeta("intercept", intercept);
    result.setMeta("baselineCurveId", baselineCurve.id());
    result.setMeta("baselineCurveName", baselineCurve.name());
    result.setMeta("instrumentType", static_cast<int>(inputCurve.instrumentType()));
    result.setMeta("label", tempText);
    result.setMeta("markerColor", QColor(Qt::red));

    // 标签显示位置（外推点旁边）
    QPointF labelPos(extrapolatedTemp + 5, extrapolatedY + 0.5);
    result.setMeta("labelPosition", labelPos);

    return result;
}

// ==================== 私有辅助函数实现 ====================

bool TemperatureExtrapolationAlgorithm::findBaselineCurve(AlgorithmContext* context, ThermalCurve& baselineCurve) const
{
    if (!context) {
        return false;
    }

    // 从上下文获取 CurveManager
    auto curveManagerOpt = context->get<CurveManager*>("curveManager");
    if (!curveManagerOpt.has_value() || !curveManagerOpt.value()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::findBaselineCurve - CurveManager 未设置";
        return false;
    }

    CurveManager* curveManager = curveManagerOpt.value();

    // 获取活动曲线
    auto activeCurveOpt = context->get<ThermalCurve>(ContextKeys::ActiveCurve);
    if (!activeCurveOpt.has_value()) {
        return false;
    }

    const ThermalCurve& activeCurve = activeCurveOpt.value();
    QString parentId = activeCurve.id();

    // 使用 CurveManager 的 getBaselines() 方法获取所有基线曲线
    QVector<ThermalCurve*> baselines = curveManager->getBaselines(parentId);

    if (baselines.isEmpty()) {
        return false;
    }

    // 使用第一条基线曲线（如果有多条，可以让用户选择或使用最新的）
    baselineCurve = *baselines.first();  // 复制曲线数据

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::findBaselineCurve - 找到基线曲线:" << baselineCurve.name();
    if (baselines.size() > 1) {
        qDebug() << "  提示：找到" << baselines.size() << "条基线曲线，使用第一条";
    }
#endif

    return true;
}

double TemperatureExtrapolationAlgorithm::getBaselineYAtTemperature(
    const QVector<ThermalDataPoint>& baselineData,
    double temperature) const
{
    if (baselineData.isEmpty()) {
        return 0.0;
    }

    // 如果温度在范围外，返回边界值
    if (temperature <= baselineData.first().temperature) {
        return baselineData.first().value;
    }
    if (temperature >= baselineData.last().temperature) {
        return baselineData.last().value;
    }

    // 线性插值
    for (int i = 0; i < baselineData.size() - 1; ++i) {
        double t1 = baselineData[i].temperature;
        double t2 = baselineData[i + 1].temperature;

        if (temperature >= t1 && temperature <= t2) {
            double y1 = baselineData[i].value;
            double y2 = baselineData[i + 1].value;

            // 线性插值公式
            double ratio = (temperature - t1) / (t2 - t1);
            return y1 + ratio * (y2 - y1);
        }
    }

    return 0.0;
}

bool TemperatureExtrapolationAlgorithm::validatePeakRange(
    const QVector<ThermalDataPoint>& baselineData,
    double peakTemp1,
    double peakTemp2) const
{
    if (baselineData.isEmpty()) {
        return false;
    }

    double baselineMinTemp = baselineData.first().temperature;
    double baselineMaxTemp = baselineData.last().temperature;

    // 检查峰范围是否完全在基线范围内
    bool inRange = (peakTemp1 >= baselineMinTemp && peakTemp1 <= baselineMaxTemp) &&
                   (peakTemp2 >= baselineMinTemp && peakTemp2 <= baselineMaxTemp);

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::validatePeakRange:";
    qDebug() << "  - 基线范围: [" << baselineMinTemp << "," << baselineMaxTemp << "]";
    qDebug() << "  - 峰范围: [" << peakTemp1 << "," << peakTemp2 << "]";
    qDebug() << "  - 验证结果:" << (inRange ? "通过" : "失败");
#endif

    return inRange;
}

bool TemperatureExtrapolationAlgorithm::calculateIntersectionWithBaseline(
    double slope,
    double intercept,
    const QVector<ThermalDataPoint>& baselineData,
    double searchRangeMin,
    double searchRangeMax,
    double& intersectionTemp) const
{
    if (baselineData.isEmpty()) {
        return false;
    }

    // 使用迭代法查找交点
    // 切线方程: y_tangent = slope * x + intercept
    // 基线方程: y_baseline = getBaselineYAtTemperature(x)
    // 交点条件: |y_tangent - y_baseline| < epsilon

    const double epsilon = 0.001;  // 精度阈值
    const double step = 0.1;       // 搜索步长（°C）

    double minDistance = std::numeric_limits<double>::max();
    double bestTemp = searchRangeMin;

    for (double temp = searchRangeMin; temp <= searchRangeMax; temp += step) {
        double yTangent = slope * temp + intercept;
        double yBaseline = getBaselineYAtTemperature(baselineData, temp);

        double distance = qAbs(yTangent - yBaseline);

        if (distance < minDistance) {
            minDistance = distance;
            bestTemp = temp;
        }

        if (distance < epsilon) {
            // 找到交点
            intersectionTemp = temp;
#if DEBUG_TEMPERATURE_EXTRAPOLATION
            qDebug() << "TemperatureExtrapolationAlgorithm::calculateIntersectionWithBaseline - 找到交点:"
                     << intersectionTemp << ", 误差:" << distance;
#endif
            return true;
        }
    }

    // 如果没有找到精确交点，返回最接近的点
    if (minDistance < 0.1) {  // 允许稍大的误差
        intersectionTemp = bestTemp;
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "TemperatureExtrapolationAlgorithm::calculateIntersectionWithBaseline - 找到近似交点:"
                 << intersectionTemp << ", 误差:" << minDistance;
#endif
        return true;
    }

    return false;
}

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
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXY = 0.0;
    double sumX2 = 0.0;

    const int totalIterations = n;
    int lastReportedProgress = 0;

    for (int i = 0; i < n; ++i) {
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

        int currentProgress = (i * 100) / totalIterations;
        if (currentProgress >= lastReportedProgress + 10) {
            lastReportedProgress = currentProgress;
            reportProgress(currentProgress, QString("拟合切线 %1/%2").arg(i + 1).arg(totalIterations));
        }
    }

    double denominator = n * sumX2 - sumX * sumX;
    if (qAbs(denominator) < 1e-10) {
        return false;
    }

    slope = (n * sumXY - sumX * sumY) / denominator;
    intercept = (sumY - slope * sumX) / n;

    reportProgress(100, "切线拟合完成");

    return true;
}

QVector<ThermalDataPoint> TemperatureExtrapolationAlgorithm::extractFittingRegion(
    const QVector<ThermalDataPoint>& curveData,
    const ThermalDataPoint& point1,
    const ThermalDataPoint& point2) const
{
    QVector<ThermalDataPoint> region;

    double temp1 = qMin(point1.temperature, point2.temperature);
    double temp2 = qMax(point1.temperature, point2.temperature);

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

    QString valueStr = QString::number(temperature, 'f', 2);

    return QString("外推温度 = %1 °C").arg(valueStr);
}

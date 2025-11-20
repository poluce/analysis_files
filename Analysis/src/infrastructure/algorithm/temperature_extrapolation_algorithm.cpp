#include "temperature_extrapolation_algorithm.h"
#include "application/algorithm/algorithm_context.h"
#include "application/curve/curve_manager.h"
#include "domain/model/thermal_curve.h"
#include "domain/model/thermal_data_point.h"
#include <QColor>
#include <QDebug>
#include <QPointF>
#include <QUuid>
#include <QVariant>
#include <QtMath>
#include <algorithm>
#include <limits>

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
    desc.displayName = displayName();
    desc.category = category();
    desc.needsParameters = false;
    desc.needsPointSelection = true;
    desc.requiredPointCount = 2;
    desc.pointSelectionHint = "请选择2个点定义反应特征区域：\n"
                               "  点1: 反应前的平坦基线处（左侧）\n"
                               "  点2: 反应后的平坦处（右侧）";

    // 依赖声明（工作流支持）
    desc.prerequisites.append(ContextKeys::ActiveCurve);
    desc.prerequisites.append(ContextKeys::SelectedPoints);
    // 输出声明
    desc.produces.append("curves");   // 切线、基线延长线
    desc.produces.append("markers");  // 外推点、拐点
    desc.produces.append("scalar");   // 外推温度数值

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

    // 阶段2：验证曲线数据非空
    const auto& curveData = curve.value().getProcessedData();
    if (curveData.isEmpty()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 曲线数据为空";
        return false;
    }

    // 阶段3：验证选点数据（2个点定义反应特征区域）
    auto points = context->get<QVector<ThermalDataPoint>>(ContextKeys::SelectedPoints);
    if (!points.has_value() || points.value().size() < 2) {
        qWarning() << "TemperatureExtrapolationAlgorithm::prepareContext - 需要2个选点，当前"
                   << (points.has_value() ? points.value().size() : 0) << "个";
        return false;  // 数据不完整，等待用户选点
    }

    // 注意：不再强制要求基线曲线存在
    // 新算法使用混合策略：优先使用现有基线，降级为局部拟合

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "TemperatureExtrapolationAlgorithm::prepareContext - 数据就绪";
    qDebug() << "  - 曲线:" << curve.value().name();
    qDebug() << "  - 数据点数:" << curveData.size();
    qDebug() << "  - 选点数:" << points.value().size();

    // 检查是否有现有基线
    ThermalCurve baselineCurve;
    if (findBaselineCurve(context, baselineCurve)) {
        qDebug() << "  - 现有基线:" << baselineCurve.name();
    } else {
        qDebug() << "  - 无现有基线，将使用局部拟合";
    }
#endif
    return true;
}

AlgorithmResult TemperatureExtrapolationAlgorithm::executeWithContext(AlgorithmContext* context)
{
    // ==================== 标准外推法 (ISO 11358-1 / ASTM E2550) ====================

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
    const QVector<ThermalDataPoint>& curveData = inputCurve.getProcessedData();

    if (curveData.isEmpty()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 曲线数据为空！";
        return AlgorithmResult::failure("temperature_extrapolation", "曲线数据为空");
    }

    // 3. 拉取选择的点（2个点定义反应特征区域）
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

    // 4. 确定搜索范围 [T1, T2]
    double T1 = qMin(selectedPoints[0].temperature, selectedPoints[1].temperature);
    double T2 = qMax(selectedPoints[0].temperature, selectedPoints[1].temperature);

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "========== 标准外推法执行开始 ==========";
    qDebug() << "曲线:" << inputCurve.name();
    qDebug() << "选点范围: [" << T1 << "," << T2 << "]";
#endif

    // 检查是否被用户取消
    if (shouldCancel()) {
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext - 用户取消执行！";
        return AlgorithmResult::failure("temperature_extrapolation", "用户取消执行");
    }

    // 5. 计算 Y 轴范围（用于归一化斜率）
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();
    for (const auto& pt : curveData) {
        yMin = qMin(yMin, pt.value);
        yMax = qMax(yMax, pt.value);
    }
    double yRange = yMax - yMin;

    // 6. 基线拟合（自适应稳健策略 + 两点兜底）
    LinearFit baseline = fitBaselineAdaptive(curveData, T1, T2, yRange);
    if (!baseline.valid) {
        QString error = QString("基线拟合失败: %1").arg(baseline.quality.rejectReason);
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    reportProgress(30, "基线拟合完成");

    // 7. 拐点检测（稳健版：滑动窗口 OLS + 前沿约束 + 百分位阈值）
    InflectionPoint inflection = detectInflectionPointRobust(curveData, T1, T2);
    if (!inflection.valid) {
        QString error = QString("在范围 [%1, %2] 内未找到拐点").arg(T1).arg(T2);
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    reportProgress(50, "拐点检测完成");

    // 8. 切线计算（在拐点处，y = k*x + b）
    LinearFit tangent(inflection.slope, inflection.value - inflection.slope * inflection.temperature);

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "切线参数: y =" << tangent.slope << "* x +" << tangent.intercept;
#endif

    reportProgress(70, "切线计算完成");

    // 9. 计算交点（带合理性约束）
    double confidence = 0;
    QString warning;
    QPointF onsetPoint = calculateLineIntersectionConstrained(baseline, tangent, T1, T2, confidence, warning);

    if (confidence == 0 || onsetPoint.isNull()) {
        QString error = QString("交点计算失败: %1").arg(warning.isEmpty() ? "基线与切线平行" : warning);
        qWarning() << "TemperatureExtrapolationAlgorithm::executeWithContext -" << error;
        return AlgorithmResult::failure("temperature_extrapolation", error);
    }

    double T_onset = onsetPoint.x();
    double Y_onset = onsetPoint.y();

    // 记录警告
    if (!warning.isEmpty()) {
        qWarning() << "外推温度计算警告:" << warning;
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "外推温度:" << T_onset << "°C";
    qDebug() << "外推点 Y 值:" << Y_onset;
#endif

    reportProgress(80, "交点计算完成");

    // 9. 创建结果对象（混合输出）
    AlgorithmResult result = AlgorithmResult::success(
        "temperature_extrapolation",
        inputCurve.id(),
        ResultType::Composite
    );

    // 10. 创建辅助曲线

    // 10.1 切线曲线（从拐点向两侧延伸）
    double tangentStart = qMin(inflection.temperature - 30, T_onset - 10);
    double tangentEnd = qMax(inflection.temperature + 30, T2 + 10);
    ThermalCurve tangentCurve = createTangentCurve(inputCurve, tangent, tangentStart, tangentEnd);
    result.addCurve(tangentCurve);

    // 10.2 基线延长线（从 T1 左侧延伸到交点）
    double baselineStart = T1 - 30;
    double baselineEnd = qMax(T_onset + 10, T1 + 30);
    ThermalCurve baselineExtCurve = createBaselineExtensionCurve(inputCurve, baseline, baselineStart, baselineEnd);
    result.addCurve(baselineExtCurve);

    // 11. 添加标注点

    // 11.1 外推点（主要结果）
    result.addMarker(onsetPoint, QString("外推: %1°C").arg(T_onset, 0, 'f', 1));

    // 11.2 拐点
    result.addMarker(QPointF(inflection.temperature, inflection.value), "拐点");

    // 12. 添加元数据
    result.setMeta(MetaKeys::ExtrapolatedTemperature, T_onset);
    result.setMeta(MetaKeys::Slope, tangent.slope);
    result.setMeta(MetaKeys::Intercept, tangent.intercept);
    result.setMeta("baseline.slope", baseline.slope);
    result.setMeta("baseline.intercept", baseline.intercept);
    result.setMeta("baseline.r2", baseline.r2);
    result.setMeta("baseline.slopeNormalized", baseline.quality.slopeNormalized);
    result.setMeta("baseline.method", baseline.quality.rejectReason.isEmpty() ? "自适应拟合" : baseline.quality.rejectReason);
    result.setMeta("inflection.temperature", inflection.temperature);
    result.setMeta("inflection.value", inflection.value);
    result.setMeta("inflection.slope", inflection.slope);
    result.setMeta("confidence", confidence);
    result.setMeta("warning", warning);
    result.setMeta(MetaKeys::InstrumentType, static_cast<int>(inputCurve.instrumentType()));
    result.setMeta(MetaKeys::MarkerColor, QColor(Qt::red));

    reportProgress(100, "外推温度计算完成");

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "========== 标准外推法执行完成 ==========";
    qDebug() << "结果:";
    qDebug() << "  - 外推温度:" << T_onset << "°C";
    qDebug() << "  - 拐点温度:" << inflection.temperature << "°C";
    qDebug() << "  - 基线 R²:" << baseline.r2;
    qDebug() << "  - 辅助曲线:" << result.curveCount() << "条";
    qDebug() << "  - 标注点:" << result.markerCount() << "个";
#endif

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

// ==================== 新增：标准外推法核心函数实现 ====================

LinearFit TemperatureExtrapolationAlgorithm::fitInitialBaseline(
    const QVector<ThermalDataPoint>& data,
    double T_end,
    int pointCount) const
{
    if (data.isEmpty() || pointCount < 2) {
        return LinearFit();
    }

    // 找到 T_end 对应的索引
    int endIdx = -1;
    for (int i = 0; i < data.size(); ++i) {
        if (data[i].temperature >= T_end) {
            endIdx = i;
            break;
        }
    }

    if (endIdx < 0) {
        endIdx = data.size() - 1;
    }

    // 从 endIdx 向左取 pointCount 个点
    int startIdx = qMax(0, endIdx - pointCount);
    int actualCount = endIdx - startIdx;

    if (actualCount < 2) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qWarning() << "fitInitialBaseline: 基线拟合点数不足，实际" << actualCount << "点";
#endif
        return LinearFit();
    }

    // 最小二乘法拟合
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (int i = startIdx; i < endIdx; ++i) {
        double x = data[i].temperature;
        double y = data[i].value;
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double n = actualCount;
    double denominator = n * sumX2 - sumX * sumX;

    if (qAbs(denominator) < 1e-10) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qWarning() << "fitInitialBaseline: 拟合失败，分母为零";
#endif
        return LinearFit();
    }

    double slope = (n * sumXY - sumX * sumY) / denominator;
    double intercept = (sumY - slope * sumX) / n;

    // 计算 R²
    double meanY = sumY / n;
    double ssTotal = 0.0, ssResidual = 0.0;

    for (int i = startIdx; i < endIdx; ++i) {
        double y = data[i].value;
        double yPred = slope * data[i].temperature + intercept;
        ssTotal += (y - meanY) * (y - meanY);
        ssResidual += (y - yPred) * (y - yPred);
    }

    double r2 = (ssTotal > 0) ? (1.0 - ssResidual / ssTotal) : 1.0;

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "fitInitialBaseline: 基线拟合完成";
    qDebug() << "  - 拟合点数:" << actualCount;
    qDebug() << "  - 温度范围: [" << data[startIdx].temperature << "," << data[endIdx - 1].temperature << "]";
    qDebug() << "  - 斜率:" << slope << ", 截距:" << intercept << ", R²:" << r2;
#endif

    return LinearFit(slope, intercept, r2);
}

InflectionPoint TemperatureExtrapolationAlgorithm::detectInflectionPoint(
    const QVector<ThermalDataPoint>& data,
    double T_start,
    double T_end) const
{
    if (data.size() < 3) {
        return InflectionPoint();
    }

    // 找到搜索范围的索引
    int startIdx = -1, endIdx = -1;

    for (int i = 0; i < data.size(); ++i) {
        if (startIdx < 0 && data[i].temperature >= T_start) {
            startIdx = i;
        }
        if (data[i].temperature <= T_end) {
            endIdx = i;
        }
    }

    if (startIdx < 0 || endIdx < 0 || startIdx >= endIdx - 1) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qWarning() << "detectInflectionPoint: 搜索范围无效";
#endif
        return InflectionPoint();
    }

    // 确保有足够的点进行中心差分
    startIdx = qMax(startIdx, 1);
    endIdx = qMin(endIdx, data.size() - 2);

    InflectionPoint result;
    double maxAbsSlope = 0.0;

    for (int i = startIdx; i <= endIdx; ++i) {
        // 中心差分计算斜率
        double dx = data[i + 1].temperature - data[i - 1].temperature;
        double dy = data[i + 1].value - data[i - 1].value;

        if (qAbs(dx) < 1e-10) {
            continue;
        }

        double slope = dy / dx;
        double absSlope = qAbs(slope);

        if (absSlope > maxAbsSlope) {
            maxAbsSlope = absSlope;
            result.index = i;
            result.temperature = data[i].temperature;
            result.value = data[i].value;
            result.slope = slope;
            result.valid = true;
        }
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    if (result.valid) {
        qDebug() << "detectInflectionPoint: 找到拐点";
        qDebug() << "  - 温度:" << result.temperature << "°C";
        qDebug() << "  - 值:" << result.value;
        qDebug() << "  - 斜率:" << result.slope;
    } else {
        qWarning() << "detectInflectionPoint: 未找到拐点";
    }
#endif

    return result;
}

QPointF TemperatureExtrapolationAlgorithm::calculateLineIntersection(
    const LinearFit& line1,
    const LinearFit& line2) const
{
    if (!line1.valid || !line2.valid) {
        return QPointF();
    }

    // 解方程: a1*x + b1 = a2*x + b2
    // x = (b2 - b1) / (a1 - a2)
    double a1 = line1.slope;
    double b1 = line1.intercept;
    double a2 = line2.slope;
    double b2 = line2.intercept;

    double denominator = a1 - a2;

    if (qAbs(denominator) < 1e-10) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qWarning() << "calculateLineIntersection: 两条直线平行，无交点";
#endif
        return QPointF();
    }

    double x = (b2 - b1) / denominator;
    double y = a1 * x + b1;

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "calculateLineIntersection: 交点 (" << x << "," << y << ")";
#endif

    return QPointF(x, y);
}

LinearFit TemperatureExtrapolationAlgorithm::getBaselineWithHybridStrategy(
    AlgorithmContext* context,
    const QVector<ThermalDataPoint>& curveData,
    double T1,
    int fallbackPointCount) const
{
    // 策略1: 优先搜索现有基线曲线
    ThermalCurve baselineCurve;
    if (findBaselineCurve(context, baselineCurve)) {
        const auto& baselineData = baselineCurve.getProcessedData();
        if (!baselineData.isEmpty()) {
            // 检查基线是否覆盖 T1
            double minTemp = baselineData.first().temperature;
            double maxTemp = baselineData.last().temperature;

            if (T1 >= minTemp && T1 <= maxTemp) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
                qDebug() << "getBaselineWithHybridStrategy: 使用现有基线曲线:" << baselineCurve.name();
#endif
                // 从基线曲线数据拟合直线
                return fitInitialBaseline(baselineData, T1, fallbackPointCount);
            }
        }
    }

    // 策略2: 降级为局部拟合
#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "getBaselineWithHybridStrategy: 降级为局部拟合";
#endif
    return fitInitialBaseline(curveData, T1, fallbackPointCount);
}

ThermalCurve TemperatureExtrapolationAlgorithm::createTangentCurve(
    const ThermalCurve& parent,
    const LinearFit& tangent,
    double T_start,
    double T_end) const
{
    // 生成切线数据点
    QVector<ThermalDataPoint> tangentData;
    const int numPoints = 100;
    double step = (T_end - T_start) / (numPoints - 1);

    for (int i = 0; i < numPoints; ++i) {
        double T = T_start + i * step;
        double y = tangent.valueAt(T);

        ThermalDataPoint pt;
        pt.temperature = T;
        pt.time = 0;
        pt.value = y;
        tangentData.append(pt);
    }

    // 创建曲线
    ThermalCurve curve;
    curve = ThermalCurve(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        parent.name() + " - 切线"
    );
    curve.setInstrumentType(parent.instrumentType());
    curve.setSignalType(SignalType::Raw);
    curve.setRawData(tangentData);
    curve.setProcessedData(tangentData);

    // 设置辅助曲线属性
    curve.setIsAuxiliaryCurve(true);
    curve.setIsStronglyBound(true);
    curve.setParentId(parent.id());

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "createTangentCurve: 创建切线曲线";
    qDebug() << "  - ID:" << curve.id();
    qDebug() << "  - 父曲线:" << parent.id();
    qDebug() << "  - 温度范围: [" << T_start << "," << T_end << "]";
#endif

    return curve;
}

ThermalCurve TemperatureExtrapolationAlgorithm::createBaselineExtensionCurve(
    const ThermalCurve& parent,
    const LinearFit& baseline,
    double T_start,
    double T_end) const
{
    // 生成基线延长线数据点
    QVector<ThermalDataPoint> baselineData;
    const int numPoints = 100;
    double step = (T_end - T_start) / (numPoints - 1);

    for (int i = 0; i < numPoints; ++i) {
        double T = T_start + i * step;
        double y = baseline.valueAt(T);

        ThermalDataPoint pt;
        pt.temperature = T;
        pt.time = 0;
        pt.value = y;
        baselineData.append(pt);
    }

    // 创建曲线
    ThermalCurve curve;
    curve = ThermalCurve(
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        parent.name() + " - 基线"
    );
    curve.setInstrumentType(parent.instrumentType());
    curve.setSignalType(SignalType::Baseline);
    curve.setRawData(baselineData);
    curve.setProcessedData(baselineData);

    // 设置辅助曲线属性
    curve.setIsAuxiliaryCurve(true);
    curve.setIsStronglyBound(true);
    curve.setParentId(parent.id());

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "createBaselineExtensionCurve: 创建基线延长线曲线";
    qDebug() << "  - ID:" << curve.id();
    qDebug() << "  - 父曲线:" << parent.id();
    qDebug() << "  - 温度范围: [" << T_start << "," << T_end << "]";
#endif

    return curve;
}

// ==================== Phase 2: 稳健算法函数实现 ====================

LinearFit TemperatureExtrapolationAlgorithm::fitBaselineAdaptive(
    const QVector<ThermalDataPoint>& data,
    double T1,
    double T2,
    double yRange) const
{
    LinearFit result;

    // 参数配置
    const double deltaT_min = 5.0;   // 最小温度窗 (K)
    const double deltaT_max = 30.0;  // 最大温度窗 (K)
    const int minPoints = 20;        // 最少拟合点数
    const double r2Threshold = 0.95; // R² 门槛
    const double slopeThreshold = 0.005; // 归一化斜率门槛

    // 1. 确定搜索范围 [T1 - deltaT_max, T1 - deltaT_min]
    double searchStart = T1 - deltaT_max;
    double searchEnd = T1 - deltaT_min;

    // 2. 找到搜索范围内的数据点索引
    int searchStartIdx = -1, searchEndIdx = -1;
    for (int i = 0; i < data.size(); ++i) {
        if (searchStartIdx < 0 && data[i].temperature >= searchStart) {
            searchStartIdx = i;
        }
        if (data[i].temperature <= searchEnd) {
            searchEndIdx = i;
        }
    }

    // 检查是否有足够的数据点
    if (searchStartIdx < 0 || searchEndIdx < 0 || searchEndIdx - searchStartIdx < minPoints) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "fitBaselineAdaptive: 搜索范围数据点不足，尝试两点基线兜底";
#endif
        return fitBaselineTwoPoint(data, T1, T2);
    }

    // 3. 滑动窗口搜索最佳基线段
    double bestVariance = std::numeric_limits<double>::max();
    int bestWindowStart = -1;
    int bestWindowEnd = -1;

    const int windowSize = qMin(minPoints, searchEndIdx - searchStartIdx);

    for (int start = searchStartIdx; start + windowSize <= searchEndIdx; ++start) {
        double variance = calculateDerivativeVariance(data, start, start + windowSize);

        if (variance < bestVariance) {
            bestVariance = variance;
            bestWindowStart = start;
            bestWindowEnd = start + windowSize;
        }
    }

    // 4. 对最佳窗口做 OLS 拟合
    if (bestWindowStart >= 0) {
        result = fitLinear(data, bestWindowStart, bestWindowEnd);

        // 5. 质量评估
        result.quality.r2 = result.r2;
        result.quality.slopeNormalized = qAbs(result.slope) / (yRange > 0 ? yRange : 1.0);
        result.quality.varianceRatio = bestVariance;

        // 6. 质量门槛检查
        if (result.r2 < r2Threshold) {
            result.quality.isAcceptable = false;
            result.quality.rejectReason = QString("R² = %1 < %2").arg(result.r2, 0, 'f', 3).arg(r2Threshold);
        } else if (result.quality.slopeNormalized > slopeThreshold) {
            result.quality.isAcceptable = false;
            result.quality.rejectReason = QString("归一化斜率 = %1 > %2")
                .arg(result.quality.slopeNormalized, 0, 'f', 4).arg(slopeThreshold);
        } else {
            result.quality.isAcceptable = true;
            result.quality.rejectReason.clear();
        }

        result.valid = result.quality.isAcceptable;

#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "fitBaselineAdaptive: 自适应拟合完成";
        qDebug() << "  - R²:" << result.r2;
        qDebug() << "  - 归一化斜率:" << result.quality.slopeNormalized;
        qDebug() << "  - 导数方差:" << bestVariance;
        qDebug() << "  - 质量达标:" << result.quality.isAcceptable;
        if (!result.quality.rejectReason.isEmpty()) {
            qDebug() << "  - 拒绝原因:" << result.quality.rejectReason;
        }
#endif
    }

    // 7. 如果质量不达标，尝试兜底方案
    if (!result.valid || !result.quality.isAcceptable) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "fitBaselineAdaptive: 自适应拟合失败，尝试两点基线兜底";
#endif
        LinearFit fallback = fitBaselineTwoPoint(data, T1, T2);
        if (fallback.valid) {
            return fallback;
        }
    }

    return result;
}

LinearFit TemperatureExtrapolationAlgorithm::fitBaselineTwoPoint(
    const QVector<ThermalDataPoint>& data,
    double T1,
    double T2) const
{
    LinearFit result;

    // 找到最接近 T1 和 T2 的数据点
    int idx1 = -1, idx2 = -1;
    double minDist1 = std::numeric_limits<double>::max();
    double minDist2 = std::numeric_limits<double>::max();

    for (int i = 0; i < data.size(); ++i) {
        double dist1 = qAbs(data[i].temperature - T1);
        double dist2 = qAbs(data[i].temperature - T2);

        if (dist1 < minDist1) {
            minDist1 = dist1;
            idx1 = i;
        }
        if (dist2 < minDist2) {
            minDist2 = dist2;
            idx2 = i;
        }
    }

    if (idx1 < 0 || idx2 < 0 || idx1 == idx2) {
        result.valid = false;
        result.quality.rejectReason = "无法找到两点基线的数据点";
        return result;
    }

    // 计算两点连线
    double x1 = data[idx1].temperature;
    double y1 = data[idx1].value;
    double x2 = data[idx2].temperature;
    double y2 = data[idx2].value;

    double dx = x2 - x1;
    if (qAbs(dx) < 1e-9) {
        result.valid = false;
        result.quality.rejectReason = "两点温度相同";
        return result;
    }

    result.slope = (y2 - y1) / dx;
    result.intercept = y1 - result.slope * x1;
    result.r2 = 1.0;  // 两点拟合 R² 恒为 1
    result.valid = true;

    // 质量评估：两点基线标记为兜底方案
    result.quality.r2 = 1.0;
    result.quality.isAcceptable = true;
    result.quality.rejectReason = "两点基线（兜底方案）";

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "fitBaselineTwoPoint: 使用两点基线兜底";
    qDebug() << "  - T1=" << T1 << ", T2=" << T2;
    qDebug() << "  - 斜率:" << result.slope << ", 截距:" << result.intercept;
#endif

    return result;
}

double TemperatureExtrapolationAlgorithm::calculateDerivativeVariance(
    const QVector<ThermalDataPoint>& data,
    int startIdx,
    int endIdx) const
{
    if (endIdx - startIdx < 3) {
        return std::numeric_limits<double>::max();
    }

    QVector<double> derivatives;
    for (int i = startIdx + 1; i < endIdx; ++i) {
        double dx = data[i].temperature - data[i-1].temperature;
        if (qAbs(dx) > 1e-9) {
            double dy = data[i].value - data[i-1].value;
            derivatives.append(dy / dx);
        }
    }

    if (derivatives.isEmpty()) {
        return std::numeric_limits<double>::max();
    }

    // 计算方差
    double mean = 0;
    for (double d : derivatives) {
        mean += d;
    }
    mean /= derivatives.size();

    double variance = 0;
    for (double d : derivatives) {
        variance += (d - mean) * (d - mean);
    }
    variance /= derivatives.size();

    return variance;
}

LinearFit TemperatureExtrapolationAlgorithm::fitLinear(
    const QVector<ThermalDataPoint>& data,
    int startIdx,
    int endIdx) const
{
    LinearFit result;

    int n = endIdx - startIdx;
    if (n < 2) {
        return result;
    }

    // 最小二乘法拟合
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;

    for (int i = startIdx; i < endIdx; ++i) {
        double x = data[i].temperature;
        double y = data[i].value;
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double denominator = n * sumX2 - sumX * sumX;
    if (qAbs(denominator) < 1e-10) {
        return result;
    }

    result.slope = (n * sumXY - sumX * sumY) / denominator;
    result.intercept = (sumY - result.slope * sumX) / n;

    // 计算 R²
    double meanY = sumY / n;
    double ssTotal = 0.0, ssResidual = 0.0;

    for (int i = startIdx; i < endIdx; ++i) {
        double y = data[i].value;
        double yPred = result.slope * data[i].temperature + result.intercept;
        ssTotal += (y - meanY) * (y - meanY);
        ssResidual += (y - yPred) * (y - yPred);
    }

    result.r2 = (ssTotal > 0) ? (1.0 - ssResidual / ssTotal) : 1.0;
    result.valid = true;

    return result;
}

InflectionPoint TemperatureExtrapolationAlgorithm::detectInflectionPointRobust(
    const QVector<ThermalDataPoint>& data,
    double T1,
    double T2) const
{
    InflectionPoint result;

    // 参数配置
    const int slopeWindowSize = 11;  // 斜率估计窗口（奇数）
    const double percentileThreshold = 0.80;  // 斜率需超过 80 百分位
    const bool searchLeadingHalfOnly = true;  // 只搜索前沿

    // 1. 确定搜索范围
    double searchEnd = searchLeadingHalfOnly ? (T1 + T2) / 2.0 : T2;

    // 找到范围内的数据点
    int startIdx = -1, endIdx = -1;
    for (int i = 0; i < data.size(); ++i) {
        if (startIdx < 0 && data[i].temperature >= T1) {
            startIdx = i;
        }
        if (data[i].temperature <= searchEnd) {
            endIdx = i;
        }
    }

    if (startIdx < 0 || endIdx < 0 || endIdx - startIdx < slopeWindowSize) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "detectInflectionPointRobust: 搜索范围数据点不足，降级到简单检测";
#endif
        return detectInflectionPoint(data, T1, T2);
    }

    // 2. 计算平滑斜率（滑动窗口 OLS）
    QVector<double> smoothSlopes;
    QVector<int> slopeIndices;

    int halfWin = slopeWindowSize / 2;
    for (int i = startIdx + halfWin; i <= endIdx - halfWin; ++i) {
        double slope = calculateLocalSlope(data, i - halfWin, i + halfWin);
        smoothSlopes.append(qAbs(slope));
        slopeIndices.append(i);
    }

    if (smoothSlopes.isEmpty()) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "detectInflectionPointRobust: 无法计算平滑斜率，降级到简单检测";
#endif
        return detectInflectionPoint(data, T1, T2);
    }

    // 3. 计算斜率阈值（百分位）
    QVector<double> sortedSlopes = smoothSlopes;
    std::sort(sortedSlopes.begin(), sortedSlopes.end());
    int thresholdIdx = static_cast<int>(sortedSlopes.size() * percentileThreshold);
    thresholdIdx = qMin(thresholdIdx, sortedSlopes.size() - 1);
    double slopeThreshold = sortedSlopes[thresholdIdx];

    // 4. 找到超过阈值的最大斜率点
    double maxSlope = 0;
    int maxSlopeIdx = -1;

    for (int i = 0; i < smoothSlopes.size(); ++i) {
        if (smoothSlopes[i] >= slopeThreshold && smoothSlopes[i] > maxSlope) {
            maxSlope = smoothSlopes[i];
            maxSlopeIdx = slopeIndices[i];
        }
    }

    if (maxSlopeIdx < 0) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "detectInflectionPointRobust: 未找到满足阈值的拐点，降级到简单检测";
#endif
        return detectInflectionPoint(data, T1, T2);
    }

    // 5. 二阶导过零确认（可选）
    bool hasZeroCrossing = checkSecondDerivativeZeroCrossing(data, maxSlopeIdx, halfWin);
    if (!hasZeroCrossing) {
#if DEBUG_TEMPERATURE_EXTRAPOLATION
        qDebug() << "detectInflectionPointRobust: 警告 - 拐点处二阶导未明确过零";
#endif
    }

    // 6. 填充结果
    result.index = maxSlopeIdx;
    result.temperature = data[maxSlopeIdx].temperature;
    result.value = data[maxSlopeIdx].value;
    result.slope = calculateLocalSlope(data, maxSlopeIdx - halfWin, maxSlopeIdx + halfWin);
    result.valid = true;

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "detectInflectionPointRobust: 找到稳健拐点";
    qDebug() << "  - 温度:" << result.temperature << "°C";
    qDebug() << "  - 值:" << result.value;
    qDebug() << "  - 平滑斜率:" << result.slope;
    qDebug() << "  - 二阶导过零:" << hasZeroCrossing;
#endif

    return result;
}

double TemperatureExtrapolationAlgorithm::calculateLocalSlope(
    const QVector<ThermalDataPoint>& data,
    int startIdx,
    int endIdx) const
{
    // 简单线性回归
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = endIdx - startIdx + 1;

    if (n < 2) return 0;

    for (int i = startIdx; i <= endIdx; ++i) {
        double x = data[i].temperature;
        double y = data[i].value;
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double denom = n * sumX2 - sumX * sumX;
    if (qAbs(denom) < 1e-12) return 0;

    return (n * sumXY - sumX * sumY) / denom;
}

bool TemperatureExtrapolationAlgorithm::checkSecondDerivativeZeroCrossing(
    const QVector<ThermalDataPoint>& data,
    int centerIdx,
    int halfWindow) const
{
    if (centerIdx - halfWindow < 1 || centerIdx + halfWindow >= data.size() - 1) {
        return true;  // 边界情况，假设通过
    }

    // 计算二阶导（用有限差分近似）
    auto secondDeriv = [&](int i) {
        double h1 = data[i].temperature - data[i-1].temperature;
        double h2 = data[i+1].temperature - data[i].temperature;
        if (qAbs(h1) < 1e-9 || qAbs(h2) < 1e-9) return 0.0;

        double d1 = (data[i].value - data[i-1].value) / h1;
        double d2 = (data[i+1].value - data[i].value) / h2;
        return (d2 - d1) / ((h1 + h2) / 2);
    };

    int beforeIdx = centerIdx - halfWindow / 2;
    int afterIdx = centerIdx + halfWindow / 2;

    beforeIdx = qMax(1, beforeIdx);
    afterIdx = qMin(data.size() - 2, afterIdx);

    double d2_before = secondDeriv(beforeIdx);
    double d2_after = secondDeriv(afterIdx);

    // 检查符号变化
    return (d2_before * d2_after <= 0);
}

QPointF TemperatureExtrapolationAlgorithm::calculateLineIntersectionConstrained(
    const LinearFit& baseline,
    const LinearFit& tangent,
    double T1,
    double T2,
    double& outConfidence,
    QString& outWarning) const
{
    outConfidence = 1.0;
    outWarning.clear();

    // 1. 基本有效性检查
    if (!baseline.valid || !tangent.valid) {
        outConfidence = 0;
        outWarning = "基线或切线无效";
        return QPointF();
    }

    // 2. 平行检查
    double slopeDiff = qAbs(tangent.slope - baseline.slope);
    if (slopeDiff < 1e-9) {
        outConfidence = 0;
        outWarning = "切线与基线平行，无法计算交点";
        return QPointF();
    }

    // 3. 夹角约束
    double angle1 = qAtan(baseline.slope);
    double angle2 = qAtan(tangent.slope);
    double angleDiff = qAbs(angle1 - angle2) * 180.0 / M_PI;  // 转为度

    const double minAngle = 5.0;  // 最小夹角（度）
    if (angleDiff < minAngle) {
        outConfidence *= 0.5;
        outWarning = QString("切线与基线夹角过小 (%1°)，结果可能不准确")
            .arg(angleDiff, 0, 'f', 1);
    }

    // 4. 计算交点
    double x = (baseline.intercept - tangent.intercept) / (tangent.slope - baseline.slope);
    double y = tangent.slope * x + tangent.intercept;

    // 5. 位置约束
    const double tolerance = 20.0;  // 允许的温度偏差 (K)
    if (x < T1 - tolerance || x > T2 + tolerance) {
        outConfidence *= 0.3;
        QString posWarning = QString("交点温度 (%1) 远离选择范围 [%2, %3]")
            .arg(x, 0, 'f', 1).arg(T1, 0, 'f', 1).arg(T2, 0, 'f', 1);
        if (outWarning.isEmpty()) {
            outWarning = posWarning;
        } else {
            outWarning += "；" + posWarning;
        }
    }

    // 6. 极端值检查
    if (x < T1 - 100 || x > T2 + 100) {
        outConfidence = 0;
        outWarning = QString("交点温度异常 (%1 K)，基线或切线可能存在严重偏差")
            .arg(x, 0, 'f', 1);
    }

#if DEBUG_TEMPERATURE_EXTRAPOLATION
    qDebug() << "calculateLineIntersectionConstrained:";
    qDebug() << "  - 交点: (" << x << "," << y << ")";
    qDebug() << "  - 夹角:" << angleDiff << "°";
    qDebug() << "  - 可信度:" << outConfidence;
    if (!outWarning.isEmpty()) {
        qDebug() << "  - 警告:" << outWarning;
    }
#endif

    return QPointF(x, y);
}

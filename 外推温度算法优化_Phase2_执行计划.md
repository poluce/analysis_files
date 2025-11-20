# 外推温度算法优化 Phase 2 - 执行计划

## 问题诊断

### 问题 1：基线拟合窗口选择不当（固定 N 点）
- **位置**: `temperature_extrapolation_algorithm.cpp:548`
- **现状**: `fitInitialBaseline()` 用 T1 左侧"固定 20 个点"做最小二乘直线拟合，未判断是否是"平稳段"
- **影响**: 样本点密度变化、T1 贴近过渡段、或左侧含轻微弯曲/噪声时，拟合出的基线会明显偏斜
- **结果**: 基线斜率偏大或方向错误，和切线交点被拉走

### 问题 2：基线质量没有门槛校验
- **位置**: `temperature_extrapolation_algorithm.cpp:196`
- **现状**: 只判断 `baseline.valid`，而 `fitInitialBaseline()` 只要算出系数就置为 valid，即使 R² 很差也会被接受
- **结果**: 劣质基线也被用于交点计算，导致外推温度失真

### 问题 3：混合基线策略继承同样弱点
- **位置**: `temperature_extrapolation_algorithm.cpp:734`
- **现状**: 即使找到了已有基线，仍在其数据上以"固定 20 点 + 无质量门槛"重拟合
- **结果**: 若现有基线在 T1 附近并非严格平稳，也会导致同样问题

### 问题 4：拐点/切线选取对噪声和边缘选择敏感
- **位置**: `temperature_extrapolation_algorithm.cpp:629`
- **现状**: `detectInflectionPoint()` 直接对原始数据做三点中心差分，选择区间内全局 |dy/dx| 最大点
- **影响**: 未平滑、未限制在 leading edge（前沿），也未做 SNR/阈值过滤
- **结果**: 易被噪声尖峰、肩峰或 trailing edge（后沿）误导，切线方向错误

### 问题 5：交点计算缺少合理性约束
- **位置**: `temperature_extrapolation_algorithm.cpp:700`
- **现状**: `calculateLineIntersection()` 只判平行，不判夹角过小、交点是否远离 [T1, T2] 合理范围
- **结果**: 当切线或基线有偏差时，交点会落到明显不合理的位置

---

## 执行计划

### Phase 2.1：基线拟合稳健化

#### 任务 2.1.1：添加基线质量评估结构体
**文件**: `temperature_extrapolation_algorithm.h`

```cpp
// 基线质量评估结果
struct BaselineQuality {
    double r2;              // 决定系数
    double slopeNormalized; // 归一化斜率 (slope / Y_range)
    double varianceRatio;   // 导数方差比
    bool isAcceptable;      // 是否达标
    QString rejectReason;   // 拒绝原因（如有）

    BaselineQuality()
        : r2(0), slopeNormalized(0), varianceRatio(0)
        , isAcceptable(false), rejectReason("未评估") {}
};

// 扩展 LinearFit 结构体
struct LinearFit {
    double slope;
    double intercept;
    double r2;
    bool valid;
    BaselineQuality quality;  // 新增：质量评估

    // ... 其他成员
};
```

#### 任务 2.1.2：实现自适应稳健基线拟合
**文件**: `temperature_extrapolation_algorithm.cpp`

**新函数**: `fitBaselineAdaptive()`

```cpp
/**
 * @brief 自适应稳健基线拟合
 *
 * 算法流程：
 * 1. 在 [T1 - deltaT_max, T1 - deltaT_min] 范围内滑动窗口搜索
 * 2. 对每个窗口计算一阶导数方差和 R²
 * 3. 选择方差最小（或 R² 最大）的子窗做 OLS
 * 4. 应用质量门槛验证
 *
 * @param data 曲线数据
 * @param T1 用户选择的起始温度
 * @param yRange 数据 Y 轴范围（用于归一化斜率）
 * @return LinearFit 拟合结果（含质量评估）
 */
LinearFit TemperatureExtrapolationAlgorithm::fitBaselineAdaptive(
    const QVector<ThermalDataPoint>& data,
    double T1,
    double yRange) const
{
    LinearFit result;

    // 参数配置
    const double deltaT_min = 5.0;   // 最小温度窗 (K)
    const double deltaT_max = 30.0;  // 最大温度窗 (K)
    const int minPoints = 40;        // 最少拟合点数
    const double r2Threshold = 0.98; // R² 门槛
    const double slopeThreshold = 0.001; // 归一化斜率门槛

    // 1. 确定搜索范围 [T1 - deltaT_max, T1 - deltaT_min]
    double searchStart = T1 - deltaT_max;
    double searchEnd = T1 - deltaT_min;

    // 2. 滑动窗口搜索最佳基线段
    double bestVariance = std::numeric_limits<double>::max();
    int bestWindowStart = -1;
    int bestWindowEnd = -1;

    // 找到搜索范围内的数据点索引
    int searchStartIdx = -1, searchEndIdx = -1;
    for (int i = 0; i < data.size(); ++i) {
        if (searchStartIdx < 0 && data[i].temperature >= searchStart) {
            searchStartIdx = i;
        }
        if (data[i].temperature <= searchEnd) {
            searchEndIdx = i;
        }
    }

    if (searchStartIdx < 0 || searchEndIdx < 0 || searchEndIdx - searchStartIdx < minPoints) {
        result.valid = false;
        result.quality.rejectReason = "基线搜索范围内数据点不足";
        return result;
    }

    // 3. 滑动窗口，计算每个窗口的导数方差
    const int windowSize = qMin(minPoints, searchEndIdx - searchStartIdx);

    for (int start = searchStartIdx; start + windowSize <= searchEndIdx; ++start) {
        // 计算该窗口内的一阶导数方差
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
            result.quality.rejectReason = QString("R² = %1 < %2").arg(result.r2).arg(r2Threshold);
        } else if (result.quality.slopeNormalized > slopeThreshold) {
            result.quality.isAcceptable = false;
            result.quality.rejectReason = QString("归一化斜率 = %1 > %2")
                .arg(result.quality.slopeNormalized).arg(slopeThreshold);
        } else {
            result.quality.isAcceptable = true;
            result.quality.rejectReason.clear();
        }

        result.valid = result.quality.isAcceptable;
    }

    return result;
}

/**
 * @brief 计算数据段的一阶导数方差
 */
double TemperatureExtrapolationAlgorithm::calculateDerivativeVariance(
    const QVector<ThermalDataPoint>& data,
    int startIdx,
    int endIdx) const
{
    if (endIdx - startIdx < 3) return std::numeric_limits<double>::max();

    QVector<double> derivatives;
    for (int i = startIdx + 1; i < endIdx; ++i) {
        double dx = data[i].temperature - data[i-1].temperature;
        if (qAbs(dx) > 1e-9) {
            double dy = data[i].value - data[i-1].value;
            derivatives.append(dy / dx);
        }
    }

    if (derivatives.isEmpty()) return std::numeric_limits<double>::max();

    // 计算方差
    double mean = 0;
    for (double d : derivatives) mean += d;
    mean /= derivatives.size();

    double variance = 0;
    for (double d : derivatives) variance += (d - mean) * (d - mean);
    variance /= derivatives.size();

    return variance;
}

/**
 * @brief 兜底基线：使用用户选择的两点连线
 *
 * 当自适应搜索无法找到合适的平稳基线段时，
 * 使用用户选择的 T1 和 T2 两点直接连线作为基线。
 * 这在 DSC/TGA 峰两端都接近基线的情况下是合理的。
 *
 * @param data 曲线数据
 * @param T1 用户选择的起始温度
 * @param T2 用户选择的结束温度
 * @return LinearFit 两点连线拟合结果
 */
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

    qDebug() << "使用两点基线兜底: T1=" << T1 << "T2=" << T2
             << "slope=" << result.slope;

    return result;
}
```

#### 任务 2.1.4：修改 fitBaselineAdaptive 添加兜底逻辑

在 `fitBaselineAdaptive()` 函数末尾添加兜底逻辑：

```cpp
    // 如果质量不达标，尝试兜底方案
    if (!result.valid || !result.quality.isAcceptable) {
        qDebug() << "自适应基线拟合失败，尝试两点基线兜底";

        // 需要传入 T2，修改函数签名或从上下文获取
        // 这里假设通过额外参数传入
        LinearFit fallback = fitBaselineTwoPoint(data, T1, T2_fallback);

        if (fallback.valid) {
            qDebug() << "使用两点基线作为兜底方案";
            return fallback;
        }
    }

    return result;
}
```

#### 任务 2.1.3：修改 executeWithContext 使用新拟合函数
**文件**: `temperature_extrapolation_algorithm.cpp`

```cpp
// 在 executeWithContext 中替换基线拟合调用
// 旧代码：
// LinearFit baseline = fitInitialBaseline(curveData, T1);

// 新代码：
double yMin = std::numeric_limits<double>::max();
double yMax = std::numeric_limits<double>::lowest();
for (const auto& pt : curveData) {
    yMin = qMin(yMin, pt.value);
    yMax = qMax(yMax, pt.value);
}
double yRange = yMax - yMin;

LinearFit baseline = fitBaselineAdaptive(curveData, T1, yRange);

if (!baseline.valid) {
    qWarning() << "基线拟合失败:" << baseline.quality.rejectReason;
    return AlgorithmResult::failure("temperatureExtrapolation", parentCurveId,
        QString("基线拟合质量不达标: %1\n建议：扩大左边界或先进行基线校正")
            .arg(baseline.quality.rejectReason));
}
```

---

### Phase 2.2：拐点检测稳健化

#### 任务 2.2.1：实现平滑斜率估计
**文件**: `temperature_extrapolation_algorithm.cpp`

**新函数**: `detectInflectionPointRobust()`

```cpp
/**
 * @brief 稳健拐点检测
 *
 * 算法改进：
 * 1. 使用滑动窗口 OLS 估计局部斜率（代替三点差分）
 * 2. 只在 leading half 搜索
 * 3. SNR/阈值过滤
 * 4. 二阶导过零确认
 */
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
        result.valid = false;
        return result;
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
        result.valid = false;
        return result;
    }

    // 3. 计算斜率阈值（百分位）
    QVector<double> sortedSlopes = smoothSlopes;
    std::sort(sortedSlopes.begin(), sortedSlopes.end());
    int thresholdIdx = static_cast<int>(sortedSlopes.size() * percentileThreshold);
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
        result.valid = false;
        return result;
    }

    // 5. 二阶导过零确认（可选）
    // 检查该点附近二阶导是否有符号变化
    bool hasSecondDerivZeroCrossing = checkSecondDerivativeZeroCrossing(
        data, maxSlopeIdx, halfWin);

    if (!hasSecondDerivZeroCrossing) {
        qDebug() << "警告：拐点处二阶导未明确过零，结果可能不稳定";
    }

    // 6. 填充结果
    result.index = maxSlopeIdx;
    result.temperature = data[maxSlopeIdx].temperature;
    result.value = data[maxSlopeIdx].value;
    result.slope = calculateLocalSlope(data, maxSlopeIdx - halfWin, maxSlopeIdx + halfWin);
    result.valid = true;

    return result;
}

/**
 * @brief 计算局部斜率（滑动窗口 OLS）
 */
double TemperatureExtrapolationAlgorithm::calculateLocalSlope(
    const QVector<ThermalDataPoint>& data,
    int startIdx,
    int endIdx) const
{
    // 简单线性回归
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = endIdx - startIdx + 1;

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

/**
 * @brief 检查二阶导数过零
 */
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

    double d2_before = secondDeriv(centerIdx - halfWindow / 2);
    double d2_after = secondDeriv(centerIdx + halfWindow / 2);

    // 检查符号变化
    return (d2_before * d2_after <= 0);
}
```

---

### Phase 2.3：交点合理性约束

#### 任务 2.3.1：增强交点计算函数
**文件**: `temperature_extrapolation_algorithm.cpp`

**修改函数**: `calculateLineIntersection()`

```cpp
/**
 * @brief 计算两条直线的交点（带合理性约束）
 *
 * @param baseline 基线拟合
 * @param tangent 切线拟合
 * @param T1 用户选择的起始温度
 * @param T2 用户选择的结束温度
 * @param outConfidence 输出：结果可信度 (0-1)
 * @param outWarning 输出：警告信息
 * @return QPointF 交点坐标
 */
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

    // 1. 基本平行检查
    double slopeDiff = qAbs(tangent.slope - baseline.slope);
    if (slopeDiff < 1e-9) {
        outConfidence = 0;
        outWarning = "切线与基线平行，无法计算交点";
        return QPointF();
    }

    // 2. 夹角约束
    double angle1 = qAtan(baseline.slope);
    double angle2 = qAtan(tangent.slope);
    double angleDiff = qAbs(angle1 - angle2) * 180.0 / M_PI;  // 转为度

    const double minAngle = 5.0;  // 最小夹角（度）
    if (angleDiff < minAngle) {
        outConfidence *= 0.5;
        outWarning = QString("切线与基线夹角过小 (%1°)，结果可能不准确")
            .arg(angleDiff, 0, 'f', 1);
    }

    // 3. 计算交点
    double x = (baseline.intercept - tangent.intercept) / (tangent.slope - baseline.slope);
    double y = tangent.slope * x + tangent.intercept;

    // 4. 位置约束
    const double tolerance = 20.0;  // 允许的温度偏差 (K)
    if (x < T1 - tolerance || x > T2 + tolerance) {
        outConfidence *= 0.3;
        if (outWarning.isEmpty()) {
            outWarning = QString("交点温度 (%1) 远离选择范围 [%2, %3]")
                .arg(x, 0, 'f', 1).arg(T1, 0, 'f', 1).arg(T2, 0, 'f', 1);
        } else {
            outWarning += QString("；交点温度远离选择范围");
        }
    }

    // 5. 极端值检查
    if (x < T1 - 100 || x > T2 + 100) {
        outConfidence = 0;
        outWarning = QString("交点温度异常 (%1 K)，基线或切线可能存在严重偏差")
            .arg(x, 0, 'f', 1);
    }

    return QPointF(x, y);
}
```

#### 任务 2.3.2：在 executeWithContext 中使用带约束的交点计算

```cpp
// 替换原有交点计算
// 旧代码：
// QPointF onset = calculateLineIntersection(baseline, tangentFit);

// 新代码：
double confidence = 0;
QString warning;
QPointF onset = calculateLineIntersectionConstrained(
    baseline, tangentFit, T1, T2, confidence, warning);

if (confidence == 0) {
    return AlgorithmResult::failure("temperatureExtrapolation", parentCurveId,
        QString("交点计算失败: %1").arg(warning));
}

// 将可信度和警告写入结果 meta
if (confidence < 1.0) {
    qWarning() << "外推温度计算警告:" << warning;
}
```

---

### Phase 2.4：混合基线策略优化

#### 任务 2.4.1：重写 getBaselineWithHybridStrategy
**文件**: `temperature_extrapolation_algorithm.cpp`

```cpp
LinearFit TemperatureExtrapolationAlgorithm::getBaselineWithHybridStrategy(
    const QVector<ThermalDataPoint>& curveData,
    double T1,
    double yRange,
    AlgorithmContext* context) const
{
    // 1. 优先查找已有基线
    auto curveManager = context->get<CurveManager*>("curveManager");
    auto activeCurve = context->get<ThermalCurve*>("activeCurve");

    if (curveManager.has_value() && activeCurve.has_value()) {
        QVector<ThermalCurve*> baselines = curveManager.value()->getBaselines(
            activeCurve.value()->id());

        for (ThermalCurve* baseline : baselines) {
            // 检查基线是否覆盖 T1 附近
            const auto& baselineData = baseline->getProcessedData();
            if (baselineData.isEmpty()) continue;

            double baselineMinT = baselineData.first().temperature;
            double baselineMaxT = baselineData.last().temperature;

            if (baselineMinT <= T1 && baselineMaxT >= T1) {
                qDebug() << "使用已有基线:" << baseline->name();

                // 对已有基线数据做自适应拟合（同样应用质量门槛）
                LinearFit result = fitBaselineAdaptive(baselineData, T1, yRange);

                if (result.quality.isAcceptable) {
                    qDebug() << "已有基线质量达标, R²=" << result.r2;
                    return result;
                } else {
                    qDebug() << "已有基线质量不达标:" << result.quality.rejectReason;
                    // 继续尝试原始曲线
                }
            }
        }
    }

    // 2. 降级到原始曲线的局部拟合
    qDebug() << "使用原始曲线进行基线拟合";
    return fitBaselineAdaptive(curveData, T1, yRange);
}
```

---

### Phase 2.5：AlgorithmResult 增强

#### 任务 2.5.1：添加可信度和警告到结果
**文件**: `temperature_extrapolation_algorithm.cpp`

在 `executeWithContext` 返回结果时：

```cpp
// 设置结果 meta 信息
result.setMeta("confidence", confidence);
result.setMeta("warning", warning);
result.setMeta("baselineR2", baseline.r2);
result.setMeta("baselineSlopeNormalized", baseline.quality.slopeNormalized);

// 如果可信度低，添加到显示标签
QString labelText = QString("外推温度: %1 °C").arg(onset.x(), 0, 'f', 1);
if (confidence < 0.8) {
    labelText += QString("\n(可信度: %1%)").arg(confidence * 100, 0, 'f', 0);
}
if (!warning.isEmpty()) {
    labelText += QString("\n警告: %1").arg(warning);
}
```

---

## 函数声明汇总

需要在 `temperature_extrapolation_algorithm.h` 中添加以下声明：

```cpp
private:
    // Phase 2.1: 基线拟合稳健化
    LinearFit fitBaselineAdaptive(
        const QVector<ThermalDataPoint>& data,
        double T1,
        double T2,  // 新增：用于兜底
        double yRange) const;

    LinearFit fitBaselineTwoPoint(
        const QVector<ThermalDataPoint>& data,
        double T1,
        double T2) const;

    double calculateDerivativeVariance(
        const QVector<ThermalDataPoint>& data,
        int startIdx,
        int endIdx) const;

    LinearFit fitLinear(
        const QVector<ThermalDataPoint>& data,
        int startIdx,
        int endIdx) const;

    // Phase 2.2: 拐点检测稳健化
    InflectionPoint detectInflectionPointRobust(
        const QVector<ThermalDataPoint>& data,
        double T1,
        double T2) const;

    double calculateLocalSlope(
        const QVector<ThermalDataPoint>& data,
        int startIdx,
        int endIdx) const;

    bool checkSecondDerivativeZeroCrossing(
        const QVector<ThermalDataPoint>& data,
        int centerIdx,
        int halfWindow) const;

    // Phase 2.3: 交点合理性约束
    QPointF calculateLineIntersectionConstrained(
        const LinearFit& baseline,
        const LinearFit& tangent,
        double T1,
        double T2,
        double& outConfidence,
        QString& outWarning) const;

    // Phase 2.4: 混合基线策略
    LinearFit getBaselineWithHybridStrategy(
        const QVector<ThermalDataPoint>& curveData,
        double T1,
        double yRange,
        AlgorithmContext* context) const;
```

---

## 执行顺序

| 阶段 | 任务 | 优先级 | 预计工作量 |
|------|------|--------|-----------|
| 2.1 | 基线拟合稳健化 | 高 | 2-3 小时 |
| 2.2 | 拐点检测稳健化 | 高 | 2-3 小时 |
| 2.3 | 交点合理性约束 | 中 | 1 小时 |
| 2.4 | 混合基线策略优化 | 中 | 1 小时 |
| 2.5 | AlgorithmResult 增强 | 低 | 0.5 小时 |

**建议执行顺序**: 2.1 → 2.2 → 2.3 → 2.4 → 2.5

---

## 测试要点

### 基线拟合测试
- [ ] 正常平稳段：R² > 0.98，斜率接近 0
- [ ] T1 贴近过渡段：应搜索到更左侧的平稳段
- [ ] 噪声数据：应选择方差最小的窗口
- [ ] 数据点稀疏：应正确判断点数不足

### 拐点检测测试
- [ ] 清晰单峰：应准确定位最大斜率点
- [ ] 噪声数据：平滑后应稳定
- [ ] 肩峰干扰：应选择 leading edge
- [ ] 边界情况：T1/T2 接近数据边缘

### 交点约束测试
- [ ] 正常情况：可信度 = 1.0
- [ ] 小夹角：可信度降低，有警告
- [ ] 交点远离范围：可信度降低，有警告
- [ ] 平行线：返回失败

### 混合策略测试
- [ ] 有已有基线且质量好：使用已有基线
- [ ] 有已有基线但质量差：降级到原始曲线
- [ ] 无已有基线：直接使用原始曲线

---

## 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 自适应搜索耗时增加 | 性能 | 限制最大搜索范围，优化循环 |
| 质量门槛过严 | 用户体验 | 提供参数调整选项 |
| 百分位阈值不适用所有数据 | 准确性 | 根据数据类型调整 |
| 二阶导检查假阳性 | 准确性 | 作为警告而非硬性要求 |

---

## 配置参数汇总

建议通过 `AlgorithmContext` 暴露以下可配置参数：

```cpp
// 基线拟合
"param.baseline.deltaTmin"      // 默认 5.0 K
"param.baseline.deltaTmax"      // 默认 30.0 K
"param.baseline.minPoints"      // 默认 40
"param.baseline.r2Threshold"    // 默认 0.98
"param.baseline.slopeThreshold" // 默认 0.001

// 拐点检测
"param.inflection.windowSize"   // 默认 11
"param.inflection.percentile"   // 默认 0.80
"param.inflection.leadingOnly"  // 默认 true

// 交点约束
"param.intersection.minAngle"   // 默认 5.0°
"param.intersection.tolerance"  // 默认 20.0 K
```

---

## 附录：相关文件

- **算法实现**: `Analysis/src/infrastructure/algorithm/temperature_extrapolation_algorithm.cpp`
- **算法头文件**: `Analysis/src/infrastructure/algorithm/temperature_extrapolation_algorithm.h`
- **Phase 1 文档**: `外推温度算法优化_执行文档.md`

/**
 * ============================================================================
 * 微分算法函数库
 * ============================================================================
 * 文件名: 微分算法函数封装.cpp
 * 描述: 封装TG分析软件中使用的各类微分算法
 * 日期: 2025-10-31
 * ============================================================================
 */

#include <QList>
#include <QVector>
#include <QPointF>
#include <cmath>
#include <algorithm>

/**
 * ============================================================================
 * 1. DTG微分算法 - 大窗口平滑中心差分法
 * ============================================================================
 *
 * 算法原理:
 *   对于位置i的点，计算前后各halfWin个点的和之差，然后归一化
 *   derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 *   其中 j = 1 到 halfWin
 *
 * 参数说明:
 *   @param xData - 输入X轴数据（如温度、时间）
 *   @param yData - 输入Y轴数据（如质量、热流）
 *   @param halfWin - 半窗口大小，默认50点
 *   @param dt - 虚拟时间步长，默认0.1
 *   @param outX - 输出微分曲线的X坐标
 *   @param outY - 输出微分曲线的Y坐标（导数值）
 *
 * 返回值:
 *   @return true-成功, false-失败（数据点不足）
 *
 * 注意事项:
 *   - 需要至少 (2×halfWin + 1) 个数据点
 *   - 前后各halfWin个点无法计算微分
 *   - 窗口越大，曲线越平滑，但分辨率越低
 *
 * 使用示例:
 *   QList<double> tempX, massY, dtgX, dtgY;
 *   // ... 填充tempX和massY数据 ...
 *   if (calculateDTGDerivative(tempX, massY, 50, 0.1, dtgX, dtgY)) {
 *       // 成功计算微分
 *   }
 */
bool calculateDTGDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    int halfWin,
    double dt,
    QList<double>& outX,
    QList<double>& outY)
{
    // 参数验证
    if (xData.size() != yData.size()) {
        return false; // X和Y数据长度不匹配
    }

    const int minPoints = 2 * halfWin + 1;
    if (yData.size() < minPoints) {
        return false; // 数据点不足
    }

    // 清空输出
    outX.clear();
    outY.clear();

    // 计算窗口时间
    const double windowTime = halfWin * dt;

    // 遍历可计算微分的点
    for (int i = halfWin; i < yData.size() - halfWin; ++i) {
        double sum_before = 0.0;  // 前窗口和
        double sum_after = 0.0;   // 后窗口和

        // 计算前后窗口的和
        for (int j = 1; j <= halfWin; ++j) {
            sum_before += yData[i - j];
            sum_after += yData[i + j];
        }

        // 计算差值
        const double dy = sum_after - sum_before;

        // 归一化得到微分值
        const double derivative = dy / windowTime / halfWin;

        // 保存结果
        outX.append(xData[i]);
        outY.append(derivative);
    }

    return true;
}


/**
 * ============================================================================
 * 2. 电化学微分算法 - 不对称窗口差分法
 * ============================================================================
 *
 * 算法原理:
 *   前窗口包含中心点(i-24到i，共25点)
 *   后窗口不含中心点(i+1到i+25，共25点)
 *   derivative = (sum_after - sum_before) × 归一化系数
 *
 * 参数说明:
 *   @param xData - 输入X轴数据
 *   @param yData - 输入Y轴数据
 *   @param windowSize - 窗口大小，默认25
 *   @param normFactor - 归一化系数，默认24/25=0.96
 *   @param outX - 输出微分曲线的X坐标
 *   @param outY - 输出微分曲线的Y坐标
 *
 * 返回值:
 *   @return true-成功, false-失败
 *
 * 使用示例:
 *   calculateElectrochemicalDerivative(voltageX, currentY, 25, 0.96, derivX, derivY);
 */
bool calculateElectrochemicalDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    int windowSize,
    double normFactor,
    QList<double>& outX,
    QList<double>& outY)
{
    // 参数验证
    if (xData.size() != yData.size()) {
        return false;
    }

    const int minPoints = 2 * windowSize;
    if (yData.size() < minPoints) {
        return false;
    }

    // 清空输出
    outX.clear();
    outY.clear();

    // 遍历可计算的点
    for (int i = windowSize - 1; i < yData.size() - windowSize; ++i) {
        double sum_before = 0.0;  // 前窗口和（包含中心点）
        double sum_after = 0.0;   // 后窗口和（不含中心点）

        // 前窗口: i-(windowSize-1) 到 i
        for (int j = i - (windowSize - 1); j <= i; ++j) {
            sum_before += yData[j];
        }

        // 后窗口: i+1 到 i+windowSize
        for (int j = i + 1; j <= i + windowSize; ++j) {
            sum_after += yData[j];
        }

        // 计算微分值
        double derivative = (sum_after - sum_before) * normFactor;

        // 保存结果
        outX.append(xData[i]);
        outY.append(derivative);
    }

    return true;
}


/**
 * ============================================================================
 * 3. 移动平均平滑算法
 * ============================================================================
 *
 * 算法原理:
 *   对每个点，计算其前后windowSize/2个点的平均值
 *   边界处自动缩小窗口
 *
 * 参数说明:
 *   @param data - 输入数据
 *   @param windowSize - 窗口大小（总点数）
 *
 * 返回值:
 *   @return 平滑后的数据
 *
 * 使用示例:
 *   QList<double> smoothed = smoothWithMovingAverage(rawData, 10);
 */
QList<double> smoothWithMovingAverage(const QList<double>& data, int windowSize)
{
    QList<double> smoothData;

    if (data.isEmpty() || windowSize <= 0) {
        return smoothData;
    }

    int halfWindowSize = windowSize / 2;

    for (int i = 0; i < data.size(); ++i) {
        // 确定窗口范围（自动处理边界）
        int start = std::max(0, i - halfWindowSize);
        int end = std::min(data.size(), i + halfWindowSize + 1);

        // 计算窗口内的平均值
        double sum = 0.0;
        for (int j = start; j < end; ++j) {
            sum += data[j];
        }

        smoothData.append(sum / (end - start));
    }

    return smoothData;
}


/**
 * ============================================================================
 * 4. 对称中心差分 - 标准二阶精度算法
 * ============================================================================
 *
 * 算法原理:
 *   derivative[i] = (y[i+1] - y[i-1]) / (x[i+1] - x[i-1])
 *   适合高信噪比数据
 *
 * 参数说明:
 *   @param xData - X轴数据（必须单调）
 *   @param yData - Y轴数据
 *   @param outX - 输出X坐标
 *   @param outY - 输出导数值
 *
 * 返回值:
 *   @return true-成功, false-失败
 */
bool calculateCentralDifference(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY)
{
    if (xData.size() != yData.size() || yData.size() < 3) {
        return false;
    }

    outX.clear();
    outY.clear();

    // 计算中间点的导数
    for (int i = 1; i < yData.size() - 1; ++i) {
        double dx = xData[i + 1] - xData[i - 1];
        double dy = yData[i + 1] - yData[i - 1];

        if (std::abs(dx) < 1e-10) {
            continue; // 跳过X值相同的点
        }

        outX.append(xData[i]);
        outY.append(dy / dx);
    }

    return true;
}


/**
 * ============================================================================
 * 5. 五点模板差分 - 四阶精度算法
 * ============================================================================
 *
 * 算法原理:
 *   derivative[i] = (-y[i+2] + 8×y[i+1] - 8×y[i-1] + y[i-2]) / (12×dx)
 *   精度高于中心差分，适合中等噪声数据
 *
 * 参数说明:
 *   @param xData - X轴数据（假设等间距）
 *   @param yData - Y轴数据
 *   @param outX - 输出X坐标
 *   @param outY - 输出导数值
 *
 * 返回值:
 *   @return true-成功, false-失败
 */
bool calculateFivePointDifference(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY)
{
    if (xData.size() != yData.size() || yData.size() < 5) {
        return false;
    }

    outX.clear();
    outY.clear();

    // 计算平均步长
    double dx = 0.0;
    for (int i = 1; i < xData.size(); ++i) {
        dx += (xData[i] - xData[i - 1]);
    }
    dx /= (xData.size() - 1);

    // 计算导数
    for (int i = 2; i < yData.size() - 2; ++i) {
        double derivative = (-yData[i + 2] + 8.0 * yData[i + 1]
                            - 8.0 * yData[i - 1] + yData[i - 2]) / (12.0 * dx);

        outX.append(xData[i]);
        outY.append(derivative);
    }

    return true;
}


/**
 * ============================================================================
 * 6. 自适应窗口微分 - 根据噪声水平自动选择窗口大小
 * ============================================================================
 *
 * 算法原理:
 *   1. 估算数据噪声水平
 *   2. 根据噪声选择合适的窗口大小
 *   3. 调用DTG微分算法
 *
 * 参数说明:
 *   @param xData - X轴数据
 *   @param yData - Y轴数据
 *   @param outX - 输出X坐标
 *   @param outY - 输出导数值
 *   @param outWindowSize - 实际使用的窗口大小（输出参数）
 *
 * 返回值:
 *   @return true-成功, false-失败
 */
bool calculateAdaptiveDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY,
    int* outWindowSize = nullptr)
{
    if (yData.size() < 10) {
        return false;
    }

    // 估算噪声水平（使用相邻点差分的标准差）
    QList<double> differences;
    for (int i = 1; i < std::min(100, yData.size()); ++i) {
        differences.append(std::abs(yData[i] - yData[i - 1]));
    }

    // 计算中位数（更鲁棒的噪声估计）
    std::sort(differences.begin(), differences.end());
    double noiseLevel = differences[differences.size() / 2];

    // 根据噪声水平选择窗口大小
    int halfWin;
    if (noiseLevel < 0.01) {
        halfWin = 5;   // 低噪声：小窗口
    } else if (noiseLevel < 0.1) {
        halfWin = 25;  // 中噪声：中等窗口
    } else {
        halfWin = 50;  // 高噪声：大窗口
    }

    // 确保数据点足够
    while (yData.size() < 2 * halfWin + 1 && halfWin > 2) {
        halfWin /= 2;
    }

    if (outWindowSize) {
        *outWindowSize = halfWin;
    }

    // 调用DTG微分算法
    return calculateDTGDerivative(xData, yData, halfWin, 0.1, outX, outY);
}


/**
 * ============================================================================
 * 7. 先平滑再微分 - 两步法
 * ============================================================================
 *
 * 算法原理:
 *   1. 先用移动平均平滑数据
 *   2. 再对平滑后的数据计算中心差分
 *
 * 参数说明:
 *   @param xData - X轴数据
 *   @param yData - Y轴数据
 *   @param smoothWindow - 平滑窗口大小
 *   @param outX - 输出X坐标
 *   @param outY - 输出导数值
 *
 * 返回值:
 *   @return true-成功, false-失败
 */
bool calculateSmoothThenDifferentiate(
    const QList<double>& xData,
    const QList<double>& yData,
    int smoothWindow,
    QList<double>& outX,
    QList<double>& outY)
{
    if (yData.size() < smoothWindow + 3) {
        return false;
    }

    // 步骤1: 平滑
    QList<double> smoothedY = smoothWithMovingAverage(yData, smoothWindow);

    // 步骤2: 中心差分
    return calculateCentralDifference(xData, smoothedY, outX, outY);
}


/**
 * ============================================================================
 * 8. QPointF向量版本的DTG微分（适配Qt图表）
 * ============================================================================
 *
 * 直接处理QVector<QPointF>格式的数据，方便与Qt Charts集成
 */
QVector<QPointF> calculateDTGDerivativeFromPoints(
    const QVector<QPointF>& points,
    int halfWin = 50,
    double dt = 0.1)
{
    QVector<QPointF> result;

    if (points.size() < 2 * halfWin + 1) {
        return result; // 数据点不足
    }

    const double windowTime = halfWin * dt;

    for (int i = halfWin; i < points.size() - halfWin; ++i) {
        double sum_before = 0.0;
        double sum_after = 0.0;

        for (int j = 1; j <= halfWin; ++j) {
            sum_before += points[i - j].y();
            sum_after += points[i + j].y();
        }

        const double dy = sum_after - sum_before;
        const double derivative = dy / windowTime / halfWin;

        result.append(QPointF(points[i].x(), derivative));
    }

    return result;
}


/**
 * ============================================================================
 * 9. 查找最大导数点
 * ============================================================================
 *
 * 在微分曲线中找到绝对值最大的导数点
 *
 * 参数说明:
 *   @param derivativeX - 微分曲线的X坐标
 *   @param derivativeY - 微分曲线的Y坐标
 *   @param maxX - 输出最大导数点的X坐标
 *   @param maxY - 输出最大导数值
 *
 * 返回值:
 *   @return true-成功找到, false-数据为空
 */
bool findMaxDerivativePoint(
    const QList<double>& derivativeX,
    const QList<double>& derivativeY,
    double& maxX,
    double& maxY)
{
    if (derivativeY.isEmpty()) {
        return false;
    }

    int maxIndex = 0;
    double maxAbsValue = std::abs(derivativeY[0]);

    for (int i = 1; i < derivativeY.size(); ++i) {
        double absValue = std::abs(derivativeY[i]);
        if (absValue > maxAbsValue) {
            maxAbsValue = absValue;
            maxIndex = i;
        }
    }

    maxX = derivativeX[maxIndex];
    maxY = derivativeY[maxIndex];

    return true;
}


/**
 * ============================================================================
 * 10. 查找微分曲线的极值点（峰和谷）
 * ============================================================================
 *
 * 找出导数曲线的所有局部极大值和极小值
 *
 * 参数说明:
 *   @param derivativeX - 微分曲线X坐标
 *   @param derivativeY - 微分曲线Y坐标
 *   @param threshold - 极值判定阈值（相对于最大值的比例）
 *   @param peakX - 输出所有峰的X坐标
 *   @param peakY - 输出所有峰的Y值
 *   @param valleyX - 输出所有谷的X坐标
 *   @param valleyY - 输出所有谷的Y值
 */
void findDerivativeExtrema(
    const QList<double>& derivativeX,
    const QList<double>& derivativeY,
    double threshold,
    QList<double>& peakX,
    QList<double>& peakY,
    QList<double>& valleyX,
    QList<double>& valleyY)
{
    peakX.clear();
    peakY.clear();
    valleyX.clear();
    valleyY.clear();

    if (derivativeY.size() < 3) {
        return;
    }

    // 找出最大绝对值
    double maxAbs = 0.0;
    for (double y : derivativeY) {
        maxAbs = std::max(maxAbs, std::abs(y));
    }

    double minThreshold = maxAbs * threshold;

    // 遍历寻找极值点
    for (int i = 1; i < derivativeY.size() - 1; ++i) {
        double prev = derivativeY[i - 1];
        double curr = derivativeY[i];
        double next = derivativeY[i + 1];

        // 检查是否为峰（局部极大值）
        if (curr > prev && curr > next && curr > minThreshold) {
            peakX.append(derivativeX[i]);
            peakY.append(curr);
        }
        // 检查是否为谷（局部极小值）
        else if (curr < prev && curr < next && curr < -minThreshold) {
            valleyX.append(derivativeX[i]);
            valleyY.append(curr);
        }
    }
}


/**
 * ============================================================================
 * 使用示例代码
 * ============================================================================
 */

/*
// 示例1: 计算DTG微分曲线
void exampleDTGCalculation()
{
    // 准备数据
    QList<double> temperature;  // 温度数据
    QList<double> mass;         // 质量数据

    // 假设已经从文件或传感器读取了数据
    // temperature << 25.0 << 26.0 << 27.0 << ...;
    // mass << 100.0 << 99.8 << 99.5 << ...;

    // 计算DTG微分
    QList<double> dtgX, dtgY;
    if (calculateDTGDerivative(temperature, mass, 50, 0.1, dtgX, dtgY)) {
        // 成功，dtgX和dtgY包含微分曲线数据
        qDebug() << "DTG计算成功，共" << dtgY.size() << "个点";

        // 查找最大导数点
        double maxTempAtMaxRate, maxRate;
        if (findMaxDerivativePoint(dtgX, dtgY, maxTempAtMaxRate, maxRate)) {
            qDebug() << "最大分解速率在温度" << maxTempAtMaxRate
                     << "°C，速率为" << maxRate << "%/min";
        }
    } else {
        qDebug() << "数据点不足，无法计算DTG";
    }
}

// 示例2: 自适应窗口微分
void exampleAdaptiveDerivative()
{
    QList<double> xData, yData;
    // ... 加载数据 ...

    QList<double> derivX, derivY;
    int usedWindow;

    if (calculateAdaptiveDerivative(xData, yData, derivX, derivY, &usedWindow)) {
        qDebug() << "自动选择窗口大小:" << usedWindow;
    }
}

// 示例3: 先平滑再微分
void exampleSmoothDifferentiate()
{
    QList<double> xData, yData;
    // ... 加载含噪数据 ...

    QList<double> derivX, derivY;

    // 使用15点平滑窗口
    if (calculateSmoothThenDifferentiate(xData, yData, 15, derivX, derivY)) {
        qDebug() << "平滑后微分计算完成";
    }
}

// 示例4: Qt图表集成
void exampleQtChartsIntegration()
{
    QLineSeries* originalSeries = ...; // 原始曲线
    const QVector<QPointF>& points = originalSeries->pointsVector();

    // 计算微分
    QVector<QPointF> dtgPoints = calculateDTGDerivativeFromPoints(points, 50, 0.1);

    // 创建微分曲线
    QLineSeries* dtgSeries = new QLineSeries();
    dtgSeries->replace(dtgPoints);
    dtgSeries->setName("DTG微分");

    // 添加到图表...
}

// 示例5: 查找所有峰
void exampleFindPeaks()
{
    QList<double> derivX, derivY;
    // ... 计算微分 ...

    QList<double> peakX, peakY, valleyX, valleyY;

    // 查找所有超过最大值10%的峰和谷
    findDerivativeExtrema(derivX, derivY, 0.1, peakX, peakY, valleyX, valleyY);

    qDebug() << "找到" << peakX.size() << "个峰";
    qDebug() << "找到" << valleyX.size() << "个谷";
}
*/


/**
 * ============================================================================
 * 算法选择指南
 * ============================================================================
 *
 * 1. 高噪声TG/DSC数据 → calculateDTGDerivative (halfWin=50)
 * 2. 中等噪声数据 → calculateDTGDerivative (halfWin=25) 或 calculateElectrochemicalDerivative
 * 3. 低噪声数据 → calculateCentralDifference 或 calculateFivePointDifference
 * 4. 不确定噪声水平 → calculateAdaptiveDerivative (自动选择)
 * 5. 极高噪声数据 → calculateSmoothThenDifferentiate (先平滑)
 * 6. Qt图表应用 → calculateDTGDerivativeFromPoints
 *
 * ============================================================================
 * 参数调优建议
 * ============================================================================
 *
 * halfWin (半窗口大小):
 *   - 5-10: 快速响应，保留细节，噪声抑制弱
 *   - 25: 平衡模式，适合大多数情况
 *   - 50: 强平滑，适合高噪声数据
 *   - 100+: 极强平滑，会丢失很多细节
 *
 * smoothWindow (平滑窗口):
 *   - 建议为采样频率的1-2秒跨度
 *   - 太小：平滑效果不明显
 *   - 太大：会产生延迟和失真
 *
 * ============================================================================
 */

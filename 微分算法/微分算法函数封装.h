/**
 * ============================================================================
 * 微分算法函数库 - 头文件
 * ============================================================================
 * 文件名: 微分算法函数封装.h
 * 描述: TG分析软件微分算法的函数声明
 * 日期: 2025-10-31
 * ============================================================================
 */

#ifndef DERIVATIVE_ALGORITHMS_H
#define DERIVATIVE_ALGORITHMS_H

#include <QList>
#include <QVector>
#include <QPointF>

/**
 * ============================================================================
 * 函数声明
 * ============================================================================
 */

/**
 * @brief DTG微分算法 - 大窗口平滑中心差分法
 * @details 使用前后各halfWin个点的和之差计算导数
 *          derivative[i] = (Σy[i+j] - Σy[i-j]) / (windowTime × halfWin)
 *
 * @param xData 输入X轴数据（温度/时间）
 * @param yData 输入Y轴数据（质量/热流）
 * @param halfWin 半窗口大小，默认50点
 * @param dt 虚拟时间步长，默认0.1
 * @param outX 输出微分曲线的X坐标
 * @param outY 输出微分曲线的Y坐标（导数值）
 * @return true-成功, false-失败（数据点不足）
 *
 * @note 需要至少 (2×halfWin + 1) 个数据点
 * @note 前后各halfWin个点无法计算微分
 */
bool calculateDTGDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    int halfWin,
    double dt,
    QList<double>& outX,
    QList<double>& outY);


/**
 * @brief 电化学微分算法 - 不对称窗口差分法
 * @details 前窗口包含中心点，后窗口不含中心点
 *          derivative = (sum_after - sum_before) × normFactor
 *
 * @param xData 输入X轴数据
 * @param yData 输入Y轴数据
 * @param windowSize 窗口大小，默认25
 * @param normFactor 归一化系数，默认24/25=0.96
 * @param outX 输出微分曲线的X坐标
 * @param outY 输出微分曲线的Y坐标
 * @return true-成功, false-失败
 */
bool calculateElectrochemicalDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    int windowSize,
    double normFactor,
    QList<double>& outX,
    QList<double>& outY);


/**
 * @brief 移动平均平滑算法
 * @details 对每个点计算其前后windowSize/2个点的平均值
 *
 * @param data 输入数据
 * @param windowSize 窗口大小（总点数）
 * @return 平滑后的数据
 */
QList<double> smoothWithMovingAverage(
    const QList<double>& data,
    int windowSize);


/**
 * @brief 对称中心差分 - 标准二阶精度算法
 * @details derivative[i] = (y[i+1] - y[i-1]) / (x[i+1] - x[i-1])
 *          适合高信噪比数据
 *
 * @param xData X轴数据（必须单调）
 * @param yData Y轴数据
 * @param outX 输出X坐标
 * @param outY 输出导数值
 * @return true-成功, false-失败
 */
bool calculateCentralDifference(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY);


/**
 * @brief 五点模板差分 - 四阶精度算法
 * @details derivative[i] = (-y[i+2] + 8×y[i+1] - 8×y[i-1] + y[i-2]) / (12×dx)
 *          精度高于中心差分，适合中等噪声数据
 *
 * @param xData X轴数据（假设等间距）
 * @param yData Y轴数据
 * @param outX 输出X坐标
 * @param outY 输出导数值
 * @return true-成功, false-失败
 */
bool calculateFivePointDifference(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY);


/**
 * @brief 自适应窗口微分 - 根据噪声水平自动选择窗口大小
 * @details 自动估算数据噪声，选择合适的窗口大小
 *
 * @param xData X轴数据
 * @param yData Y轴数据
 * @param outX 输出X坐标
 * @param outY 输出导数值
 * @param outWindowSize 实际使用的窗口大小（可选输出参数）
 * @return true-成功, false-失败
 */
bool calculateAdaptiveDerivative(
    const QList<double>& xData,
    const QList<double>& yData,
    QList<double>& outX,
    QList<double>& outY,
    int* outWindowSize = nullptr);


/**
 * @brief 先平滑再微分 - 两步法
 * @details 先用移动平均平滑数据，再计算中心差分
 *
 * @param xData X轴数据
 * @param yData Y轴数据
 * @param smoothWindow 平滑窗口大小
 * @param outX 输出X坐标
 * @param outY 输出导数值
 * @return true-成功, false-失败
 */
bool calculateSmoothThenDifferentiate(
    const QList<double>& xData,
    const QList<double>& yData,
    int smoothWindow,
    QList<double>& outX,
    QList<double>& outY);


/**
 * @brief QPointF向量版本的DTG微分（适配Qt图表）
 * @details 直接处理QVector<QPointF>格式的数据
 *
 * @param points 输入点集
 * @param halfWin 半窗口大小，默认50
 * @param dt 时间步长，默认0.1
 * @return 微分后的点集
 */
QVector<QPointF> calculateDTGDerivativeFromPoints(
    const QVector<QPointF>& points,
    int halfWin = 50,
    double dt = 0.1);


/**
 * @brief 查找最大导数点
 * @details 在微分曲线中找到绝对值最大的导数点
 *
 * @param derivativeX 微分曲线的X坐标
 * @param derivativeY 微分曲线的Y坐标
 * @param maxX 输出最大导数点的X坐标
 * @param maxY 输出最大导数值
 * @return true-成功找到, false-数据为空
 */
bool findMaxDerivativePoint(
    const QList<double>& derivativeX,
    const QList<double>& derivativeY,
    double& maxX,
    double& maxY);


/**
 * @brief 查找微分曲线的极值点（峰和谷）
 * @details 找出导数曲线的所有局部极大值和极小值
 *
 * @param derivativeX 微分曲线X坐标
 * @param derivativeY 微分曲线Y坐标
 * @param threshold 极值判定阈值（相对于最大值的比例，0.0-1.0）
 * @param peakX 输出所有峰的X坐标
 * @param peakY 输出所有峰的Y值
 * @param valleyX 输出所有谷的X坐标
 * @param valleyY 输出所有谷的Y值
 */
void findDerivativeExtrema(
    const QList<double>& derivativeX,
    const QList<double>& derivativeY,
    double threshold,
    QList<double>& peakX,
    QList<double>& peakY,
    QList<double>& valleyX,
    QList<double>& valleyY);


/**
 * ============================================================================
 * 快速使用指南
 * ============================================================================
 *
 * 场景1: 标准TG分析
 * ------------------
 * QList<double> temp, mass, dtgX, dtgY;
 * calculateDTGDerivative(temp, mass, 50, 0.1, dtgX, dtgY);
 *
 * 场景2: 噪声自适应
 * ------------------
 * int usedWindow;
 * calculateAdaptiveDerivative(xData, yData, outX, outY, &usedWindow);
 *
 * 场景3: Qt图表
 * ------------------
 * QVector<QPointF> dtgPoints = calculateDTGDerivativeFromPoints(
 *     originalSeries->pointsVector(), 50, 0.1);
 *
 * 场景4: 查找特征点
 * ------------------
 * double maxTempAtMaxRate, maxRate;
 * findMaxDerivativePoint(dtgX, dtgY, maxTempAtMaxRate, maxRate);
 *
 * ============================================================================
 * 参数推荐值
 * ============================================================================
 *
 * DTG微分 halfWin:
 *   - 低噪声: 10-25
 *   - 中噪声: 25-50 (推荐)
 *   - 高噪声: 50-100
 *
 * 平滑窗口 smoothWindow:
 *   - 快速采样(10Hz+): 10-30
 *   - 中速采样(1-10Hz): 5-15
 *   - 慢速采样(<1Hz): 3-10
 *
 * 极值检测 threshold:
 *   - 严格检测: 0.3-0.5
 *   - 标准检测: 0.1-0.3 (推荐)
 *   - 宽松检测: 0.05-0.1
 *
 * ============================================================================
 */

#endif // DERIVATIVE_ALGORITHMS_H

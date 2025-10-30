#ifndef THERMALDATAPOINT_H
#define THERMALDATAPOINT_H

#include <QVariantMap>

/**
 * @brief ThermalDataPoint 结构体代表热分析曲线中的单个数据点。
 *
 * 它存储温度、时间以及测量值（例如热流或质量）的核心值，
 * 以及任何附加的元数据。
 */
struct ThermalDataPoint
{
    double temperature = 0.0; // 温度值
    double value = 0.0;       // 热流/质量值
    double time = 0.0;        // 时间戳

    QVariantMap metadata;     // 扩展元数据
};

#endif // THERMALDATAPOINT_H
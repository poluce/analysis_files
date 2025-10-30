#ifndef THERMALCURVE_H
#define THERMALCURVE_H

#include "ThermalDataPoint.h"
#include <QVector>
#include <QString>
#include <QVariantMap>

/**
 * @brief 定义热分析曲线的类型 (例如, DSC, TGA)。
 */
enum class CurveType {
    DSC,
    TGA,
    DTG
};

/**
 * @brief 存储与实验或曲线相关的元数据。
 */
struct CurveMetadata {
    QString device;         // 实验设备
    QString sampleName;     // 样品名称
    double sampleMass = 0.0;  // 样品质量
    QVariantMap additional; // 其他自定义参数
};

/**
 * @brief ThermalCurve 类代表一个完整的热分析数据曲线。
 *
 * 它持有从文件加载的原始、不可变的数据，以及一份可由算法处理的数据副本。
 * 这样可以方便地实现撤销/重做和重置功能。
 */
class ThermalCurve
{
public:
    explicit ThermalCurve(QString id, QString name);

    // --- 获取器 ---
    QString id() const;
    QString name() const;
    CurveType type() const;
    const QVector<ThermalDataPoint>& getRawData() const;
    const QVector<ThermalDataPoint>& getProcessedData() const;
    const CurveMetadata& getMetadata() const;

    // --- 设置器 ---
    void setName(const QString& name);
    void setType(CurveType type);
    void setRawData(const QVector<ThermalDataPoint>& data);
    void setProcessedData(const QVector<ThermalDataPoint>& data);
    void setMetadata(const CurveMetadata& metadata);

    /**
     * @brief 将处理后的数据重置为原始数据。
     */
    void resetToRaw();

private:
    QString m_id;     // 唯一标识
    QString m_name;   // 曲线名称
    CurveType m_type; // 曲线类型

    QVector<ThermalDataPoint> m_rawData;      // 原始数据 (只读)
    QVector<ThermalDataPoint> m_processedData; // 处理后数据

    CurveMetadata m_metadata; // 实验参数
};

#endif // THERMALCURVE_H
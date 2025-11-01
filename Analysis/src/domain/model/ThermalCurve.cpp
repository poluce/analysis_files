#include "ThermalCurve.h"

ThermalCurve::ThermalCurve(QString id, QString name)
    : m_id(std::move(id)),
      m_name(std::move(name)),
      m_instrumentType(InstrumentType::TGA),  // 默认仪器类型
      m_signalType(SignalType::Raw)           // 默认为原始信号
{
}

QString ThermalCurve::id() const
{
    return m_id;
}

QString ThermalCurve::name() const
{
    return m_name;
}

QString ThermalCurve::projectName() const
{
    return m_projectName;
}

InstrumentType ThermalCurve::instrumentType() const
{
    return m_instrumentType;
}

SignalType ThermalCurve::signalType() const
{
    return m_signalType;
}

const QVector<ThermalDataPoint>& ThermalCurve::getRawData() const
{
    return m_rawData;
}

const QVector<ThermalDataPoint>& ThermalCurve::getProcessedData() const
{
    return m_processedData;
}

const CurveMetadata& ThermalCurve::getMetadata() const
{
    return m_metadata;
}

QString ThermalCurve::parentId() const
{
    return m_parentId;
}

void ThermalCurve::setName(const QString& name)
{
    m_name = name;
}

void ThermalCurve::setProjectName(const QString& projectName)
{
    m_projectName = projectName;
}

void ThermalCurve::setInstrumentType(InstrumentType type)
{
    m_instrumentType = type;
}

void ThermalCurve::setSignalType(SignalType type)
{
    m_signalType = type;
}

void ThermalCurve::setRawData(const QVector<ThermalDataPoint>& data)
{
    m_rawData = data;
    m_processedData = data; // 初始状态下，处理后的数据是原始数据的副本
}

void ThermalCurve::setProcessedData(const QVector<ThermalDataPoint>& data)
{
    m_processedData = data;
}

void ThermalCurve::setMetadata(const CurveMetadata& metadata)
{
    m_metadata = metadata;
}

void ThermalCurve::setParentId(const QString& parentId)
{
    m_parentId = parentId;
}

void ThermalCurve::resetToRaw()
{
    m_processedData = m_rawData;
}

QString ThermalCurve::getYAxisLabel() const
{
    // 根据仪器类型和信号类型动态生成 Y 轴标签
    switch (m_instrumentType) {
        case InstrumentType::TGA:
            if (m_signalType == SignalType::Raw) {
                // TGA 原始数据：根据是否有初始质量决定单位
                if (m_metadata.sampleMass > 0.0) {
                    return QStringLiteral("质量 (%)");
                } else {
                    return QStringLiteral("质量 (mg)");
                }
            } else {
                // TGA 微分数据
                return QStringLiteral("质量变化率 (%/°C)");
            }

        case InstrumentType::DSC:
            if (m_signalType == SignalType::Raw) {
                return QStringLiteral("热流 (W/g)");
            } else {
                return QStringLiteral("热流变化率 (W/g/°C)");
            }

        case InstrumentType::ARC:
            if (m_signalType == SignalType::Raw) {
                return QStringLiteral("压力 (Pa)");
            } else {
                return QStringLiteral("压力变化率 (Pa/°C)");
            }
    }

    // 默认返回（理论上不应该到达这里）
    return QStringLiteral("值");
}

QString ThermalCurve::getPhysicalQuantityName() const
{
    // 根据仪器类型返回物理量名称（不含单位）
    switch (m_instrumentType) {
        case InstrumentType::TGA:
            if (m_signalType == SignalType::Raw) {
                return QStringLiteral("质量");
            } else {
                return QStringLiteral("质量变化率");
            }

        case InstrumentType::DSC:
            if (m_signalType == SignalType::Raw) {
                return QStringLiteral("热流");
            } else {
                return QStringLiteral("热流变化率");
            }

        case InstrumentType::ARC:
            if (m_signalType == SignalType::Raw) {
                return QStringLiteral("压力");
            } else {
                return QStringLiteral("压力变化率");
            }
    }

    // 默认返回
    return QStringLiteral("值");
}
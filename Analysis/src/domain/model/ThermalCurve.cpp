#include "ThermalCurve.h"

ThermalCurve::ThermalCurve(QString id, QString name)
    : m_id(std::move(id)), m_name(std::move(name)), m_type(CurveType::DSC) // 默认类型
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

CurveType ThermalCurve::type() const
{
    return m_type;
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

void ThermalCurve::setName(const QString& name)
{
    m_name = name;
}

void ThermalCurve::setType(CurveType type)
{
    m_type = type;
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

void ThermalCurve::resetToRaw()
{
    m_processedData = m_rawData;
}
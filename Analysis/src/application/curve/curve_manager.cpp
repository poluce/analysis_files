#include "curve_manager.h"
#include "infrastructure/io/text_file_reader.h"
#include <QDebug>

CurveManager::CurveManager(QObject* parent)
    : QObject(parent)
    , m_activeCurveId("")
{
    qDebug() << "构造:    CurveManager";
    registerDefaultReaders();
}

CurveManager::~CurveManager() = default;

void CurveManager::addCurve(const ThermalCurve& curve)
{
    if (curve.id().isEmpty() || m_curves.contains(curve.id())) {
        qWarning() << "尝试添加一个ID为空或重复的曲线:" << curve.id();
        return;
    }
    m_curves.insert(curve.id(), curve);
    emit curveAdded(curve.id());
    qDebug() << "曲线已添加到管理器。ID:" << curve.id();
}

void CurveManager::clearCurves()
{
    if (m_curves.isEmpty()) {
        return;
    }

    m_curves.clear();
    m_activeCurveId.clear();

    emit curvesCleared();
    emit activeCurveChanged(m_activeCurveId);

    qDebug() << "CurveManager: 已清空现有曲线";
}

bool CurveManager::removeCurve(const QString& curveId)
{
    if (!m_curves.contains(curveId)) {
        return false;
    }

    m_curves.remove(curveId);

    if (m_activeCurveId == curveId) {
        m_activeCurveId.clear();
        emit activeCurveChanged(m_activeCurveId);
    }

    emit curveRemoved(curveId);
    qDebug() << "CurveManager: 已删除曲线" << curveId;
    return true;
}

void CurveManager::registerDefaultReaders()
{
    m_readers.push_back(std::make_unique<TextFileReader>());
    // 未来可以在此处添加更多的读取器
}

bool CurveManager::loadCurveFromFile(const QString& filePath)
{
    const IFileReader* reader = nullptr;
    for (const auto& r : m_readers) {
        if (r->canRead(filePath)) {
            reader = r.get();
            break;
        }
    }

    if (!reader) {
        qWarning() << "未找到适用于文件的读取器:" << filePath;
        return false;
    }

    try {
        // 传递一个空的config以兼容新的read接口
        ThermalCurve newCurve = reader->read(filePath, QVariantMap());
        const QString curveId = newCurve.id();

        if (m_curves.contains(curveId)) {
            qWarning() << "ID为" << curveId << "的曲线已存在，将被覆盖。";
        }

        m_curves.insert(curveId, std::move(newCurve));
        emit curveAdded(curveId);
        return true;

    } catch (const std::exception& e) {
        qCritical() << "读取文件失败:" << filePath << "错误:" << e.what();
        return false;
    }
}

// This function appears to be legacy code and is unused in the new workflow.
// The parameters are commented out to suppress unused parameter warnings.
void CurveManager::processRawData(const QString& /*curveId*/, const QMap<QString, int>& /*columnMapping*/)
{
    // Legacy code body commented out to prevent potential issues.
    /*
    ThermalCurve* curve = getCurve(curveId);
    if (!curve) {
        qWarning() << "processRawData: 未找到ID为" << curveId << "的曲线。";
        return;
    }

    QVariant rawDataTableVar = curve->getMetadata().additional.value("raw_data_table");
    if (!rawDataTableVar.canConvert<QVector<QVector<double>>>()) {
        qWarning() << "processRawData: 在曲线" << curveId << "的元数据中未找到 raw_data_table。";
        return;
    }
    const auto rawDataTable = rawDataTableVar.value<QVector<QVector<double>>>();

    const int timeCol = columnMapping.value("time", -1);
    const int tempCol = columnMapping.value("temperature", -1);
    const int valueCol = columnMapping.value("value", -1);

    QVector<ThermalDataPoint> processedData;
    processedData.reserve(rawDataTable.size());

    for (const auto& row : rawDataTable) {
        ThermalDataPoint point;
        if (timeCol != -1 && timeCol < row.size()) {
            point.time = row[timeCol];
        }
        if (tempCol != -1 && tempCol < row.size()) {
            point.temperature = row[tempCol];
        }
        if (valueCol != -1 && valueCol < row.size()) {
            point.value = row[valueCol];
        }
        processedData.append(point);
    }

    curve->setRawData(processedData);

    emit curveDataChanged(curveId);
    */
}

ThermalCurve* CurveManager::getCurve(const QString& curveId)
{
    auto it = m_curves.find(curveId);
    if (it != m_curves.end()) {
        return &it.value();
    }
    return nullptr; // Fix: Added missing return statement
}

const QMap<QString, ThermalCurve>& CurveManager::getAllCurves() const { return m_curves; }

void CurveManager::setActiveCurve(const QString& curveId)
{
    if (m_activeCurveId != curveId) {
        if (m_curves.contains(curveId) || curveId.isEmpty()) {
            m_activeCurveId = curveId;
            emit activeCurveChanged(m_activeCurveId);
        }
    }
}

ThermalCurve* CurveManager::getActiveCurve()
{
    if (m_activeCurveId.isEmpty()) {
        return nullptr;
    }
    return getCurve(m_activeCurveId);
}

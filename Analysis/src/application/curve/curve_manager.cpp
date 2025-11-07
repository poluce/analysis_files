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

QVector<ThermalCurve*> CurveManager::getBaselines(const QString& curveId)
{
    QVector<ThermalCurve*> baselines;

    // 查找所有符合条件的基线
    for (auto it = m_curves.begin(); it != m_curves.end(); ++it) {
        ThermalCurve& curve = it.value();
        if (curve.parentId() == curveId && curve.signalType() == SignalType::Baseline) {
            baselines.append(&curve);
        }
    }

    return baselines;
}

bool CurveManager::hasChildren(const QString& curveId) const
{
    // 遍历所有曲线，查找是否有任何曲线的 parentId 等于 curveId
    for (auto it = m_curves.constBegin(); it != m_curves.constEnd(); ++it) {
        const ThermalCurve& curve = it.value();
        if (curve.parentId() == curveId) {
            return true; // 找到至少一个子曲线
        }
    }
    return false; // 没有找到子曲线
}

QVector<ThermalCurve*> CurveManager::getChildren(const QString& curveId)
{
    QVector<ThermalCurve*> children;

    // 查找所有 parentId 等于 curveId 的曲线
    for (auto it = m_curves.begin(); it != m_curves.end(); ++it) {
        ThermalCurve& curve = it.value();
        if (curve.parentId() == curveId) {
            children.append(&curve);
        }
    }

    return children;
}

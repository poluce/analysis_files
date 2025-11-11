#include "application/algorithm/algorithm_context.h"

#include <QDebug>

AlgorithmContext::AlgorithmContext(QObject* parent)
    : QObject(parent)
{
}

bool AlgorithmContext::contains(const QString& key) const { return m_entries.contains(key); }

QVariant AlgorithmContext::value(const QString& key, const QVariant& defaultValue) const
{
    return m_entries.contains(key) ? m_entries.value(key).storedValue : defaultValue;
}

void AlgorithmContext::setValue(const QString& key, const QVariant& value, const QString& source)
{
    if (!value.isValid()) {
        qWarning() << "AlgorithmContext::setValue 无效值被忽略，key =" << key;
        return;
    }

    const bool exists = m_entries.contains(key);
    Entry entry{ value, source };
    m_entries.insert(key, entry);

    if (!exists || m_entries.value(key).storedValue != value) {
        emit valueChanged(key, value);
    }
}

void AlgorithmContext::remove(const QString& key)
{
    if (!m_entries.contains(key)) {
        return;
    }
    m_entries.remove(key);
    emit valueRemoved(key);
}

void AlgorithmContext::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    const auto keys = m_entries.keys();
    m_entries.clear();
    for (const QString& key : keys) {
        emit valueRemoved(key);
    }
}

QStringList AlgorithmContext::keys(const QString& prefix) const
{
    if (prefix.isEmpty()) {
        return m_entries.keys();
    }

    QStringList filtered;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            filtered.append(it.key());
        }
    }
    return filtered;
}

QVariantMap AlgorithmContext::values(const QString& prefix) const
{
    QVariantMap map;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (prefix.isEmpty() || it.key().startsWith(prefix)) {
            map.insert(it.key(), it.value().storedValue);
        }
    }
    return map;
}

AlgorithmContext* AlgorithmContext::clone() const
{
    // 创建新的上下文对象（无父对象，由调用者管理生命周期）
    AlgorithmContext* copy = new AlgorithmContext(nullptr);

    // 深拷贝所有键值对
    // QHash 的拷贝是深拷贝（结构）
    // Entry 结构中的 QVariant 和 QString 也会深拷贝
    // 对于指针类型（如 ThermalCurve*），只拷贝指针值（合法，用于只读访问）
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString& key = it.key();
        const Entry& entry = it.value();

        // 直接拷贝整个 Entry（包括 storedValue 和 source）
        copy->m_entries.insert(key, entry);
    }

    qDebug() << "[AlgorithmContext] Created clone with" << copy->m_entries.size() << "entries";

    return copy;
}

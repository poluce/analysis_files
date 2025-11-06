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

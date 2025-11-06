#ifndef APPLICATION_ALGORITHM_CONTEXT_H
#define APPLICATION_ALGORITHM_CONTEXT_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <functional>
#include <optional>

/**
 * @brief AlgorithmContext
 *
 * 一个轻量级的键值存储，用于在算法执行链路之间传递数据。
 * 主要场景：
 * - 缓存用户输入（参数、选点、选择的曲线等）
 * - 存放算法输出供后续算法引用
 * - 记录待处理的算法请求状态
 */
class AlgorithmContext : public QObject {
    Q_OBJECT

public:
    explicit AlgorithmContext(QObject* parent = nullptr);

    bool contains(const QString& key) const;
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    template <typename T>
    std::optional<T> get(const QString& key) const
    {
        if (!m_entries.contains(key)) {
            return std::nullopt;
        }
        return m_entries.value(key).storedValue.template value<T>();
    }

    void setValue(const QString& key, const QVariant& value, const QString& source = QString());
    void remove(const QString& key);
    void clear();

    QStringList keys(const QString& prefix = QString()) const;
    QVariantMap values(const QString& prefix = QString()) const;

signals:
    void valueChanged(const QString& key, const QVariant& value);
    void valueRemoved(const QString& key);

private:
    struct Entry {
        QVariant storedValue;
        QString source;
    };

    QHash<QString, Entry> m_entries;
};

#endif // APPLICATION_ALGORITHM_CONTEXT_H

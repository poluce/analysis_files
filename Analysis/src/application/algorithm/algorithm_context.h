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

    /**
     * @brief 检查上下文中是否包含指定键
     * @param key 键名
     * @return 如果存在返回 true，否则返回 false
     */
    bool contains(const QString& key) const;

    /**
     * @brief 获取指定键的值（QVariant 类型）
     * @param key 键名
     * @param defaultValue 默认值（如果键不存在）
     * @return 键对应的值，如果不存在则返回默认值
     */
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 获取指定键的值（类型安全的模板方法）
     *
     * 使用 std::optional 包装结果，避免在键不存在时返回默认构造的对象。
     *
     * @tparam T 目标类型
     * @param key 键名
     * @return std::optional<T> - 如果键存在且类型匹配返回值，否则返回 std::nullopt
     *
     * 示例：
     * @code
     * auto curve = context->get<ThermalCurve*>("activeCurve");
     * if (curve.has_value()) {
     *     // 使用 curve.value()
     * }
     * @endcode
     */
    template <typename T>
    std::optional<T> get(const QString& key) const
    {
        if (!m_entries.contains(key)) {
            return std::nullopt;
        }
        return m_entries.value(key).storedValue.template value<T>();
    }

    /**
     * @brief 设置键值对
     * @param key 键名
     * @param value 值
     * @param source 数据来源（可选，用于调试和追踪）
     */
    void setValue(const QString& key, const QVariant& value, const QString& source = QString());

    /**
     * @brief 移除指定键
     * @param key 键名
     */
    void remove(const QString& key);

    /**
     * @brief 清空所有键值对
     */
    void clear();

    /**
     * @brief 获取所有键的列表（可选按前缀过滤）
     * @param prefix 键名前缀（可选，如 "param." 获取所有参数键）
     * @return 键名列表
     */
    QStringList keys(const QString& prefix = QString()) const;

    /**
     * @brief 获取所有键值对（可选按前缀过滤）
     * @param prefix 键名前缀（可选）
     * @return 键值对映射表
     */
    QVariantMap values(const QString& prefix = QString()) const;

signals:
    /**
     * @brief 当键值发生改变时发射此信号
     * @param key 改变的键名
     * @param value 新值
     */
    void valueChanged(const QString& key, const QVariant& value);

    /**
     * @brief 当键被移除时发射此信号
     * @param key 被移除的键名
     */
    void valueRemoved(const QString& key);

private:
    struct Entry {
        QVariant storedValue;
        QString source;
    };

    QHash<QString, Entry> m_entries;
};

#endif // APPLICATION_ALGORITHM_CONTEXT_H

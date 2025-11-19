#include "application/algorithm/algorithm_context.h"

#include <QDebug>
#include <QDateTime>
#include <QUuid>

AlgorithmContext::AlgorithmContext(QObject* parent)
    : QObject(parent)
{
}

bool AlgorithmContext::contains(const QString& key) const { return m_entries.contains(key); }

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

    // 拷贝历史深度设置
    copy->m_historyDepth = m_historyDepth;

    qDebug() << "[AlgorithmContext] Created clone with" << copy->m_entries.size() << "entries";

    return copy;
}

// ========== 算法结果专用 API 实现 ==========

void AlgorithmContext::saveResult(
    const QString& taskId,
    const QString& algorithm,
    const QString& parentCurveId,
    const AlgorithmResult& result)
{
    if (taskId.isEmpty() || algorithm.isEmpty() || parentCurveId.isEmpty()) {
        qWarning() << "[AlgorithmContext] saveResult 参数不完整，已忽略";
        return;
    }

    // 1. 主存储（byTask）- 不可覆盖
    QString taskKey = OutputKeys::byTask(algorithm, taskId);
    setValue(taskKey, QVariant::fromValue(result), "AlgorithmCoordinator");

    // 2. 更新最新任务ID指针
    QString latestKey = OutputKeys::latestTaskId(algorithm, parentCurveId);
    setValue(latestKey, taskId, "AlgorithmCoordinator");

    // 3. 更新历史任务ID队列（LRU）
    QString historyKey = OutputKeys::historyTaskIds(algorithm, parentCurveId);
    QStringList history = get<QStringList>(historyKey).value_or(QStringList());

    // 追加到队列头部
    history.prepend(taskId);

    // LRU 裁剪：超出深度时删除旧任务
    if (history.size() > m_historyDepth) {
        QStringList toRemove = history.mid(m_historyDepth);
        history = history.mid(0, m_historyDepth);

        // 删除超出深度的主存储
        for (const QString& oldTaskId : toRemove) {
            QString oldKey = OutputKeys::byTask(algorithm, oldTaskId);
            remove(oldKey);
        }

        qDebug() << "[AlgorithmContext] LRU裁剪:" << algorithm << parentCurveId
                 << "删除" << toRemove.size() << "个旧任务";
    }

    setValue(historyKey, history, "AlgorithmCoordinator");

    qInfo() << "[AlgorithmContext] 保存结果:"
            << "taskId=" << taskId
            << "algorithm=" << algorithm
            << "curveId=" << parentCurveId
            << "历史深度=" << history.size();
}

std::optional<AlgorithmResult> AlgorithmContext::latestResult(
    const QString& algorithm,
    const QString& curveId) const
{
    // 1. 读取最新任务ID指针
    QString latestKey = OutputKeys::latestTaskId(algorithm, curveId);
    auto taskIdOpt = get<QString>(latestKey);

    if (!taskIdOpt.has_value() || taskIdOpt.value().isEmpty()) {
        return std::nullopt;
    }

    // 2. 展开 taskId → 读取主存储
    QString taskId = taskIdOpt.value();
    QString taskKey = OutputKeys::byTask(algorithm, taskId);

    return get<AlgorithmResult>(taskKey);
}

QVector<AlgorithmResult> AlgorithmContext::historyResults(
    const QString& algorithm,
    const QString& curveId,
    int limit) const
{
    // 1. 读取历史任务ID队列
    QString historyKey = OutputKeys::historyTaskIds(algorithm, curveId);
    auto historyOpt = get<QStringList>(historyKey);

    if (!historyOpt.has_value()) {
        return QVector<AlgorithmResult>();
    }

    QStringList taskIds = historyOpt.value();

    // 2. 限制返回数量
    if (limit > 0 && taskIds.size() > limit) {
        taskIds = taskIds.mid(0, limit);
    }

    // 3. 展开 taskId 列表 → 读取主存储
    QVector<AlgorithmResult> results;
    for (const QString& taskId : taskIds) {
        QString taskKey = OutputKeys::byTask(algorithm, taskId);
        auto resultOpt = get<AlgorithmResult>(taskKey);

        if (resultOpt.has_value()) {
            results.append(resultOpt.value());
        }
    }

    return results;
}

void AlgorithmContext::setHistoryDepth(int depth)
{
    if (depth <= 0) {
        qWarning() << "[AlgorithmContext] 无效的历史深度:" << depth << "（必须 > 0）";
        return;
    }

    m_historyDepth = depth;
    qInfo() << "[AlgorithmContext] 设置历史深度为:" << depth;
}

int AlgorithmContext::historyDepth() const
{
    return m_historyDepth;
}

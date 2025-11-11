#include "algorithm_task.h"
#include "algorithm_context.h"
#include <QDebug>

AlgorithmTask::AlgorithmTask(const QString& algorithmName, AlgorithmContext* contextSnapshot)
    : m_taskId(QUuid::createUuid().toString())
    , m_algorithmName(algorithmName)
    , m_contextSnapshot(contextSnapshot)
    , m_createdAt(QDateTime::currentDateTime())
    , m_isCancelled(false)
{
    qDebug() << "[AlgorithmTask] Created task" << m_taskId
             << "for algorithm" << m_algorithmName
             << "at" << m_createdAt.toString("hh:mm:ss.zzz");
}

AlgorithmTask::~AlgorithmTask()
{
    qDebug() << "[AlgorithmTask] Destroying task" << m_taskId
             << "for algorithm" << m_algorithmName;

    // 清理上下文快照（任务独占所有权）
    delete m_contextSnapshot;
    m_contextSnapshot = nullptr;
}

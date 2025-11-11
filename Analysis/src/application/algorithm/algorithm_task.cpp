#include "algorithm_task.h"
#include "algorithm_context.h"
#include "domain/model/thermal_curve.h"
#include <QDebug>

AlgorithmTask::AlgorithmTask(const QString& algorithmName, AlgorithmContext* contextSnapshot)
    : m_taskId(QUuid::createUuid().toString())
    , m_algorithmName(algorithmName)
    , m_contextSnapshot(contextSnapshot)
    , m_createdAt(QDateTime::currentDateTime())
    , m_isCancelled(false)
{
    // 线程安全处理：从上下文中提取原始曲线指针，创建深拷贝
    // 这样工作线程访问的是独立的副本，不受主线程修改影响
    if (m_contextSnapshot) {
        auto curvePtr = m_contextSnapshot->get<ThermalCurve*>(ContextKeys::ActiveCurve);
        if (curvePtr.has_value() && curvePtr.value()) {
            ThermalCurve* originalCurve = curvePtr.value();

            // 创建深拷贝（使用拷贝构造函数）
            m_curveCopy.reset(new ThermalCurve(*originalCurve));

            // 将深拷贝的指针替换到上下文中
            m_contextSnapshot->setValue(
                ContextKeys::ActiveCurve,
                QVariant::fromValue(m_curveCopy.data()),
                "AlgorithmTask (deep copy)"
            );

            qDebug() << "[AlgorithmTask] Created deep copy of curve" << originalCurve->id()
                     << "for thread-safe execution";
        }
    }

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

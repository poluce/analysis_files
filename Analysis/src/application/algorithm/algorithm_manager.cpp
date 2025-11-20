#include "algorithm_manager.h"
#include "application/algorithm/algorithm_context.h"
#include "application/algorithm/algorithm_worker.h"
#include "application/algorithm/algorithm_thread_manager.h"
#include "application/curve/curve_manager.h"
#include "application/history/add_curve_command.h"
#include "application/history/history_manager.h"
#include "domain/algorithm/i_thermal_algorithm.h"
#include "domain/model/thermal_curve.h"
#include <QColor>
#include <QDebug>
#include <QUuid>
#include <QMetaObject>
#include <memory>

AlgorithmManager::AlgorithmManager(AlgorithmThreadManager* threadManager,
                                   QObject* parent)
    : QObject(parent)
    , m_threadManager(threadManager)
{
    Q_ASSERT(m_threadManager != nullptr);  // 依赖注入保证非空
    qDebug() << "构造:    AlgorithmManager";

    // 注册元类型，用于跨线程信号传递
    qRegisterMetaType<AlgorithmTaskPtr>("AlgorithmTaskPtr");
    qRegisterMetaType<AlgorithmResult>("AlgorithmResult");
    qRegisterMetaType<IThermalAlgorithm*>("IThermalAlgorithm*");  // 注册算法指针类型（用于 QMetaObject::invokeMethod）

    // 连接线程管理器的 workerReleased 信号，用于处理队列
    connect(m_threadManager, &AlgorithmThreadManager::workerReleased,
            this, &AlgorithmManager::processQueue);
}

AlgorithmManager::~AlgorithmManager() { qDeleteAll(m_algorithms); }

void AlgorithmManager::setCurveManager(CurveManager* manager) { m_curveManager = manager; }

void AlgorithmManager::registerAlgorithm(IThermalAlgorithm* algorithm)
{
    if (algorithm) {
        qDebug() << "注册算法:" << algorithm->name();
        m_algorithms.insert(algorithm->name(), algorithm);
    }
}

IThermalAlgorithm* AlgorithmManager::getAlgorithm(const QString& name)
{
    return m_algorithms.value(name, nullptr);
}

// ==================== 算法结果处理 ====================

void AlgorithmManager::handleAlgorithmResult(const AlgorithmResult& result)
{
    if (!result.isSuccess() || !m_curveManager) {
        return;
    }

    switch (result.type()) {
    case ResultType::Curve:       handleCurveResult(result); break;
    case ResultType::Marker:      handleMarkerResult(result); break;
    case ResultType::Region:      handleRegionResult(result); break;
    case ResultType::ScalarValue: handleScalarResult(result); break;
    case ResultType::Composite:   handleCompositeResult(result); break;
    default:
        qWarning() << "未知的结果类型:" << static_cast<int>(result.type());
        break;
    }
}

void AlgorithmManager::handleCurveResult(const AlgorithmResult& result)
{
    if (!result.hasCurves()) {
        qWarning() << "算法结果中没有曲线数据";
        return;
    }

    for (const ThermalCurve& curve : result.curves()) {
        addCurveWithHistory(curve);
    }
}

void AlgorithmManager::handleMarkerResult(const AlgorithmResult& result)
{
    qDebug() << "标注点数量:" << result.markerCount();
    for (int i = 0; i < result.markerCount(); ++i) {
        qDebug() << "  标注点" << i << ":" << result.markers()[i];
    }

    if (result.hasMarkers()) {
        createMarkerCurve(result.parentCurveId(), result.markers(), result);
    }
}

void AlgorithmManager::handleRegionResult(const AlgorithmResult& result)
{
    qDebug() << "区域数量:" << result.regionCount();
    // TODO: 发送区域到 ChartView（用于阴影填充）
}

void AlgorithmManager::handleScalarResult(const AlgorithmResult& result)
{
    qDebug() << "标量结果:";
    for (auto it = result.allMeta().constBegin(); it != result.allMeta().constEnd(); ++it) {
        qDebug() << "  " << it.key() << ":" << it.value();
    }
    // TODO: 显示结果对话框或状态栏
}

void AlgorithmManager::handleCompositeResult(const AlgorithmResult& result)
{
    qDebug() << "混合结果:";

    if (result.hasCurves()) {
        qDebug() << "  包含" << result.curveCount() << "条曲线";
        for (const ThermalCurve& curve : result.curves()) {
            addCurveWithHistory(curve);
        }
    }

    if (result.hasMarkers()) {
        qDebug() << "  包含" << result.markerCount() << "个标注点";

        // 标注点关联到父曲线
        QString targetCurveId = result.parentCurveId();
        createMarkerCurve(targetCurveId, result.markers(), result);
    }

    if (result.hasRegions()) {
        qDebug() << "  包含" << result.regionCount() << "个区域";
        // TODO: 发送区域到 ChartView
    }

    if (result.hasMeta(MetaKeys::Area)) {
        qDebug() << "  面积:" << result.area() << result.meta(MetaKeys::Unit).toString();
    }
}

void AlgorithmManager::addCurveWithHistory(const ThermalCurve& curve)
{
    if (!m_curveManager) {
        qWarning() << "CurveManager 为空，无法添加曲线";
        return;
    }

    // 使用历史管理添加曲线
    if (m_historyManager) {
        auto command = std::make_unique<AddCurveCommand>(m_curveManager, curve);
        m_historyManager->executeCommand(std::move(command));
        qDebug() << "通过历史管理添加曲线:" << curve.name() << "ID:" << curve.id();
    } else {
        m_curveManager->addCurve(curve);
        m_curveManager->setActiveCurve(curve.id());
        qDebug() << "直接添加曲线:" << curve.name() << "ID:" << curve.id();
    }
}

void AlgorithmManager::createMarkerCurve(const QString& parentCurveId,
                                          const QList<QPointF>& markers,
                                          const AlgorithmResult& result)
{
    if (!m_curveManager || markers.isEmpty()) {
        return;
    }

    // 获取父曲线信息
    ThermalCurve* parentCurve = m_curveManager->getCurve(parentCurveId);
    if (!parentCurve) {
        qWarning() << "父曲线不存在:" << parentCurveId;
        return;
    }

    // 将 QPointF 转换为 ThermalDataPoint
    QVector<ThermalDataPoint> dataPoints;
    for (const QPointF& point : markers) {
        ThermalDataPoint dp;
        dp.temperature = point.x();
        dp.time = 0.0;  // 标记点没有时间信息
        dp.value = point.y();
        dataPoints.append(dp);
    }

    // 创建标记点曲线
    ThermalCurve markerCurve;
    markerCurve.setId(QUuid::createUuid().toString(QUuid::WithoutBraces));
    markerCurve.setName(result.algorithmKey() + "-标记点");
    markerCurve.setParentId(parentCurveId);
    markerCurve.setInstrumentType(parentCurve->instrumentType());
    markerCurve.setSignalType(SignalType::Marker);
    markerCurve.setPlotStyle(PlotStyle::Scatter);
    markerCurve.setRawData(dataPoints);
    markerCurve.setProcessedData(dataPoints);

    // 设置显示颜色
    QColor markerColor = result.metaValue<QColor>(MetaKeys::MarkerColor, QColor(Qt::red));
    markerCurve.setColor(markerColor);

    // 标记为辅助曲线和强绑定
    markerCurve.setAuxiliaryCurve(true);
    markerCurve.setStronglyBound(true);

    addCurveWithHistory(markerCurve);

    qDebug() << "创建标记点曲线:" << markerCurve.name() << "ID:" << markerCurve.id()
             << "父曲线:" << parentCurveId << "点数:" << markers.size();
}

// ==================== 异步执行实现 ====================

IThermalAlgorithm* AlgorithmManager::validateAsyncExecution(const QString& name, AlgorithmContext* context)
{
    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "[AlgorithmManager] 算法不存在:" << name;
        return nullptr;
    }

    if (!context) {
        qWarning() << "[AlgorithmManager] 上下文为空";
        return nullptr;
    }

    if (!m_curveManager) {
        qWarning() << "[AlgorithmManager] CurveManager 未设置";
        return nullptr;
    }

    context->setValue(ContextKeys::CurveManager, QVariant::fromValue(m_curveManager));

    if (!algorithm->prepareContext(context)) {
        qWarning() << "[AlgorithmManager] prepareContext 失败，数据不完整";
        return nullptr;
    }

    return algorithm;
}

QString AlgorithmManager::executeAsync(const QString& name, AlgorithmContext* context)
{
    IThermalAlgorithm* algorithm = validateAsyncExecution(name, context);
    if (!algorithm) {
        return QString();
    }

    AlgorithmContext* contextSnapshot = context->clone();
    if (!contextSnapshot) {
        qWarning() << "[AlgorithmManager] 上下文克隆失败";
        return QString();
    }

    AlgorithmTaskPtr task = QSharedPointer<AlgorithmTask>::create(name, contextSnapshot);
    QString taskId = task->taskId();

    qDebug() << "[AlgorithmManager] 创建任务" << taskId << "算法:" << name;

    m_activeTasks[taskId] = task;

    auto [worker, thread] = m_threadManager->acquireWorker();
    Q_UNUSED(thread);

    if (!worker) {
        QueuedTask queuedTask{task, algorithm, name};
        m_taskQueue.enqueue(queuedTask);

        qDebug() << "[AlgorithmManager] 所有线程忙，任务" << taskId << "加入队列"
                 << "队列长度:" << m_taskQueue.size();

        emit algorithmQueued(taskId, name);
        emit queuedTaskCountChanged(m_taskQueue.size());

        return taskId;
    }

    submitTaskToWorker(task, algorithm, worker);

    return taskId;
}

void AlgorithmManager::submitTaskToWorker(AlgorithmTaskPtr task,
                                         IThermalAlgorithm* algorithm,
                                         AlgorithmWorker* worker)
{
    QString taskId = task->taskId();

    qDebug() << "[AlgorithmManager] 提交任务" << taskId << "到 worker" << worker;

    if (!m_connectedWorkers.contains(worker)) {
        connect(worker, &AlgorithmWorker::taskStarted,
                this, &AlgorithmManager::onWorkerStarted);
        connect(worker, &AlgorithmWorker::taskProgress,
                this, &AlgorithmManager::onWorkerProgress);
        connect(worker, &AlgorithmWorker::taskFinished,
                this, &AlgorithmManager::onWorkerFinished);
        connect(worker, &AlgorithmWorker::taskFailed,
                this, &AlgorithmManager::onWorkerFailed);

        m_connectedWorkers.insert(worker);

        qDebug() << "[AlgorithmManager] 已连接 worker 信号" << worker;
    }

    m_taskWorkers[taskId] = worker;

    QMetaObject::invokeMethod(worker, "executeTask", Qt::QueuedConnection,
                             Q_ARG(AlgorithmTaskPtr, task),
                             Q_ARG(IThermalAlgorithm*, algorithm));
}

void AlgorithmManager::processQueue()
{
    if (m_taskQueue.isEmpty()) {
        return;
    }

    qDebug() << "[AlgorithmManager] 处理队列，长度" << m_taskQueue.size();

    auto [worker, thread] = m_threadManager->acquireWorker();
    Q_UNUSED(thread);

    if (!worker) {
        qDebug() << "[AlgorithmManager] 没有空闲线程，等待下次";
        return;
    }

    QueuedTask queuedTask = m_taskQueue.dequeue();

    qDebug() << "[AlgorithmManager] 取出任务" << queuedTask.task->taskId()
             << "剩余队列:" << m_taskQueue.size();

    emit queuedTaskCountChanged(m_taskQueue.size());

    submitTaskToWorker(queuedTask.task, queuedTask.algorithm, worker);
}

bool AlgorithmManager::cancelTask(const QString& taskId)
{
    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[AlgorithmManager] 任务不存在" << taskId;
        return false;
    }

    AlgorithmTaskPtr task = m_activeTasks[taskId];
    QString algorithmName = task->algorithmName();

    qDebug() << "[AlgorithmManager] 取消任务" << taskId << "算法:" << algorithmName;

    if (m_taskWorkers.contains(taskId)) {
        AlgorithmWorker* worker = m_taskWorkers[taskId];
        QMetaObject::invokeMethod(worker, "requestCancellation", Qt::QueuedConnection);

        qDebug() << "[AlgorithmManager] 请求 worker 取消" << worker << taskId;

        emit algorithmCancelled(taskId, algorithmName);
        return true;
    }

    for (int i = 0; i < m_taskQueue.size(); ++i) {
        if (m_taskQueue[i].task->taskId() == taskId) {
            m_taskQueue.removeAt(i);
            m_activeTasks.remove(taskId);

            qDebug() << "[AlgorithmManager] 从队列移除" << taskId
                     << "剩余:" << m_taskQueue.size();

            emit queuedTaskCountChanged(m_taskQueue.size());
            emit algorithmCancelled(taskId, algorithmName);
            return true;
        }
    }

    qWarning() << "[AlgorithmManager] 任务" << taskId << "既不在执行也不在队列中";
    return false;
}

QString AlgorithmManager::getTaskAlgorithmName(const QString& taskId)
{
    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[AlgorithmManager] 任务不存在" << taskId;
        return QString();
    }

    return m_activeTasks[taskId]->algorithmName();
}

void AlgorithmManager::cleanupTask(const QString& taskId)
{
    if (m_taskWorkers.contains(taskId)) {
        AlgorithmWorker* worker = m_taskWorkers[taskId];
        m_threadManager->releaseWorker(worker);
        m_taskWorkers.remove(taskId);
    }

    m_activeTasks.remove(taskId);

    qDebug() << "[AlgorithmManager] 任务" << taskId << "已清理，剩余活跃任务:" << m_activeTasks.size();
}

// ==================== 工作线程信号处理槽函数 ====================

void AlgorithmManager::onWorkerStarted(const QString& taskId, const QString& algorithmName)
{
    qDebug() << "[AlgorithmManager] 任务开始" << taskId << "算法:" << algorithmName;

    emit algorithmStarted(taskId, algorithmName);
}

void AlgorithmManager::onWorkerProgress(const QString& taskId, int percentage, const QString& message)
{
    emit algorithmProgress(taskId, percentage, message);
}

void AlgorithmManager::onWorkerFinished(const QString& taskId, const QVariant& result, qint64 elapsedMs)
{
    qDebug() << "[AlgorithmManager] 任务完成" << taskId << "耗时:" << elapsedMs << "ms";

    QString algorithmName = getTaskAlgorithmName(taskId);
    if (algorithmName.isEmpty()) {
        return;
    }

    AlgorithmResult algorithmResult = result.value<AlgorithmResult>();

    if (algorithmResult.isSuccess()) {
        handleAlgorithmResult(algorithmResult);
        emit algorithmFinished(taskId, algorithmName, algorithmResult, elapsedMs);
        emit algorithmResultReady(algorithmName, algorithmResult);
    } else {
        emit algorithmFailed(taskId, algorithmName, algorithmResult.errorMessage());
        emit algorithmExecutionFailed(algorithmName, algorithmResult.errorMessage());
    }

    cleanupTask(taskId);
}

void AlgorithmManager::onWorkerFailed(const QString& taskId, const QString& errorMessage)
{
    qWarning() << "[AlgorithmManager] 任务失败" << taskId << "错误:" << errorMessage;

    QString algorithmName = getTaskAlgorithmName(taskId);
    if (algorithmName.isEmpty()) {
        return;
    }

    emit algorithmFailed(taskId, algorithmName, errorMessage);

    cleanupTask(taskId);
}

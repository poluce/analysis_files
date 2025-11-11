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

AlgorithmManager* AlgorithmManager::instance()
{
    static AlgorithmManager service;
    return &service;
}

AlgorithmManager::AlgorithmManager(QObject* parent)
    : QObject(parent)
{
    qDebug() << "构造:    AlgorithmManager";

    // 注册元类型，用于跨线程信号传递
    qRegisterMetaType<AlgorithmTaskPtr>("AlgorithmTaskPtr");
    qRegisterMetaType<AlgorithmResult>("AlgorithmResult");
    qRegisterMetaType<IThermalAlgorithm*>("IThermalAlgorithm*");  // 注册算法指针类型（用于 QMetaObject::invokeMethod）

    // 连接线程管理器的 workerReleased 信号，用于处理队列
    connect(AlgorithmThreadManager::instance(), &AlgorithmThreadManager::workerReleased,
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

// ==================== 上下文驱动执行实现 ====================

void AlgorithmManager::executeWithContext(const QString& name, AlgorithmContext* context)
{
    if (!context) {
        qWarning() << "算法执行失败：上下文为空。";
        return;
    }

    if (!m_curveManager) {
        qWarning() << "算法执行失败：CurveManager 未设置。";
        return;
    }

    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "算法执行失败：找不到算法" << name;
        return;
    }

    // 从上下文获取活动曲线
    auto activeCurve = context->get<ThermalCurve*>(ContextKeys::ActiveCurve);
    if (!activeCurve.has_value()) {
        qWarning() << "算法执行失败：上下文中缺少活动曲线 (activeCurve)。";
        return;
    }

    ThermalCurve* curve = activeCurve.value();
    if (!curve) {
        qWarning() << "算法执行失败：活动曲线为空指针。";
        return;
    }

    qDebug() << "正在执行算法" << name << "（上下文驱动）于曲线" << curve->name();
    qDebug() << "输入类型:" << static_cast<int>(algorithm->inputType());
    qDebug() << "输出类型:" << static_cast<int>(algorithm->outputType());

    // ==================== 两阶段执行机制 ====================
    // 阶段1：准备上下文并验证数据完整性
    bool isReady = algorithm->prepareContext(context);

    if (!isReady) {
        qWarning() << "算法" << name << "数据不完整，无法执行";
        qWarning() << "  可能原因：缺少必需的用户交互数据（如选点、参数）";
        return;
    }

    qDebug() << "算法" << name << "数据就绪，开始执行";

    // 阶段2：执行算法（算法从上下文拉取完整数据）
    AlgorithmResult result = algorithm->executeWithContext(context);

    // 检查执行状态
    if (result.hasError()) {
        qWarning() << "算法" << name << "执行失败:" << result.errorMessage();
        emit algorithmExecutionFailed(name, result.errorMessage());
        return;
    }

    qDebug() << result.toString();

    // 根据输出类型处理结果
    handleAlgorithmResult(result);

    // 发出成功信号
    emit algorithmResultReady(name, result);
}

void AlgorithmManager::handleAlgorithmResult(const AlgorithmResult& result)
{
    if (!result.isSuccess() || !m_curveManager) {
        return;
    }

    auto resultType = result.type();

    switch (resultType) {
    case ResultType::Curve: {
        // 输出为新曲线（最常见情况）
        if (!result.hasCurves()) {
            qWarning() << "算法结果中没有曲线数据";
            return;
        }

        // 添加所有输出曲线
        for (const ThermalCurve& curve : result.curves()) {
            addCurveWithHistory(curve);
        }
        break;
    }

    case ResultType::Marker: {
        // 输出为标注点
        qDebug() << "标注点数量:" << result.markerCount();
        for (int i = 0; i < result.markerCount(); ++i) {
            qDebug() << "  标注点" << i << ":" << result.markers()[i];
        }

        // 发送标注点到 ChartView
        if (result.hasMarkers()) {
            QColor markerColor = result.metaValue<QColor>("markerColor", QColor(Qt::red));  // 默认红色，与用户选点颜色一致
            emit markersGenerated(result.parentCurveId(), result.markers(), markerColor);
        }
        break;
    }

    case ResultType::Region: {
        // 输出为区域
        qDebug() << "区域数量:" << result.regionCount();
        // TODO: 发送区域到 ChartView（用于阴影填充）
        break;
    }

    case ResultType::ScalarValue: {
        // 输出为标量值
        qDebug() << "标量结果:";
        for (auto it = result.allMeta().constBegin(); it != result.allMeta().constEnd(); ++it) {
            qDebug() << "  " << it.key() << ":" << it.value();
        }
        // TODO: 显示结果对话框或状态栏
        break;
    }

    case ResultType::Composite: {
        // 混合输出：依次处理所有输出
        qDebug() << "混合结果:";

        if (result.hasCurves()) {
            qDebug() << "  包含" << result.curveCount() << "条曲线";
            for (const ThermalCurve& curve : result.curves()) {
                addCurveWithHistory(curve);
            }
        }

        if (result.hasMarkers()) {
            qDebug() << "  包含" << result.markerCount() << "个标注点";

            // 发送标注点到 ChartView（关联到生成的第一条曲线）
            QString targetCurveId = result.parentCurveId();
            if (result.hasCurves() && !result.curves().isEmpty()) {
                // 如果生成了新曲线，标注点关联到新曲线
                targetCurveId = result.curves().first().id();
            }

            QColor markerColor = result.metaValue<QColor>("markerColor", QColor(Qt::red));  // 默认红色，与用户选点颜色一致
            emit markersGenerated(targetCurveId, result.markers(), markerColor);
        }

        if (result.hasRegions()) {
            qDebug() << "  包含" << result.regionCount() << "个区域";
            // TODO: 发送区域到 ChartView
        }

        if (result.hasMeta("area")) {
            qDebug() << "  面积:" << result.area() << result.meta("unit").toString();

            // 如果有标签文本和位置，创建 FloatingLabel
            if (result.hasMeta("label") && result.hasMeta("labelPosition")) {
                QString labelText = result.meta("label").toString();
                QPointF labelPos = result.metaValue<QPointF>("labelPosition");
                QString targetCurveId = result.parentCurveId();

                qDebug() << "  发出 FloatingLabel 请求：" << labelText << "位置：" << labelPos;
                emit floatingLabelRequested(labelText, labelPos, targetCurveId);
            }
        }
        break;
    }

    default:
        qWarning() << "未知的结果类型:" << static_cast<int>(resultType);
        break;
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

void AlgorithmManager::createAndAddOutputCurve(
    IThermalAlgorithm* algorithm,
    ThermalCurve* parentCurve,
    const QVector<ThermalDataPoint>& outputData,
    bool useHistoryManager)
{
    if (!algorithm || !parentCurve || !m_curveManager) {
        qWarning() << "创建输出曲线失败：参数无效";
        return;
    }

    // 创建新曲线
    const QString newId = QUuid::createUuid().toString();
    const QString newName = algorithm->displayName();
    ThermalCurve newCurve(newId, newName);

    // 填充数据和元数据
    newCurve.setProcessedData(outputData);
    newCurve.setMetadata(parentCurve->getMetadata());
    newCurve.setParentId(parentCurve->id());
    newCurve.setProjectName(parentCurve->projectName());

    // 设置类型
    newCurve.setInstrumentType(parentCurve->instrumentType());
    newCurve.setSignalType(algorithm->getOutputSignalType(parentCurve->signalType()));

    // 添加到管理器（根据是否使用历史管理）
    if (useHistoryManager) {
        const QString description = QStringLiteral("执行 %1 算法").arg(algorithm->displayName());
        auto command = std::make_unique<AddCurveCommand>(m_curveManager, newCurve, description);
        if (!HistoryManager::instance()->executeCommand(std::move(command))) {
            qWarning() << "算法结果入栈失败，放弃添加新曲线";
        }
    } else {
        m_curveManager->addCurve(newCurve);
        m_curveManager->setActiveCurve(newId);
    }
}

// ==================== 异步执行实现 ====================

QString AlgorithmManager::executeAsync(const QString& name, AlgorithmContext* context)
{
    // 1. 验证算法
    IThermalAlgorithm* algorithm = getAlgorithm(name);
    if (!algorithm) {
        qWarning() << "[AlgorithmManager] executeAsync: 算法不存在:" << name;
        return QString();
    }

    // 2. 验证上下文
    if (!context) {
        qWarning() << "[AlgorithmManager] executeAsync: 上下文为空";
        return QString();
    }

    // 3. 调用 prepareContext() 验证数据完整性
    if (!algorithm->prepareContext(context)) {
        qWarning() << "[AlgorithmManager] executeAsync: prepareContext 失败，数据不完整";
        return QString();
    }

    // 4. 创建上下文快照
    AlgorithmContext* contextSnapshot = context->clone();
    if (!contextSnapshot) {
        qWarning() << "[AlgorithmManager] executeAsync: 上下文克隆失败";
        return QString();
    }

    // 5. 创建任务（使用 QSharedPointer）
    AlgorithmTaskPtr task = QSharedPointer<AlgorithmTask>::create(name, contextSnapshot);
    QString taskId = task->taskId();

    qDebug() << "[AlgorithmManager] executeAsync: 创建任务" << taskId
             << "算法:" << name;

    // 6. 记录活跃任务
    m_activeTasks[taskId] = task;

    // 7. 尝试获取工作线程
    auto [worker, thread] = AlgorithmThreadManager::instance()->acquireWorker();
    Q_UNUSED(thread);  // 标记未使用的变量（避免编译警告）

    if (!worker) {
        // 所有线程都忙，加入队列
        QueuedTask queuedTask{task, algorithm, name};
        m_taskQueue.enqueue(queuedTask);

        qDebug() << "[AlgorithmManager] 所有线程忙，任务" << taskId << "加入队列"
                 << "队列长度:" << m_taskQueue.size();

        emit algorithmQueued(taskId, name);
        emit queuedTaskCountChanged(m_taskQueue.size());

        return taskId;
    }

    // 8. 有空闲线程，立即执行
    submitTaskToWorker(task, algorithm, worker);

    return taskId;
}

void AlgorithmManager::submitTaskToWorker(AlgorithmTaskPtr task,
                                         IThermalAlgorithm* algorithm,
                                         AlgorithmWorker* worker)
{
    QString taskId = task->taskId();

    qDebug() << "[AlgorithmManager] submitTaskToWorker: 任务" << taskId
             << "提交给 worker" << worker;

    // 1. 确保信号已连接（只连接一次）
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

        qDebug() << "[AlgorithmManager] 已连接 worker" << worker << "的信号";
    }

    // 2. 记录任务-工作线程映射
    m_taskWorkers[taskId] = worker;

    // 3. 异步调用 worker->executeTask()
    QMetaObject::invokeMethod(worker, "executeTask", Qt::QueuedConnection,
                             Q_ARG(AlgorithmTaskPtr, task),
                             Q_ARG(IThermalAlgorithm*, algorithm));
}

void AlgorithmManager::processQueue()
{
    if (m_taskQueue.isEmpty()) {
        return;
    }

    qDebug() << "[AlgorithmManager] processQueue: 队列长度" << m_taskQueue.size();

    // 尝试获取空闲线程
    auto [worker, thread] = AlgorithmThreadManager::instance()->acquireWorker();
    Q_UNUSED(thread);  // 标记未使用的变量（避免编译警告）

    if (!worker) {
        qDebug() << "[AlgorithmManager] processQueue: 没有空闲线程，等待下次";
        return;
    }

    // 从队列中取出第一个任务
    QueuedTask queuedTask = m_taskQueue.dequeue();

    qDebug() << "[AlgorithmManager] processQueue: 从队列取出任务"
             << queuedTask.task->taskId()
             << "剩余队列:" << m_taskQueue.size();

    emit queuedTaskCountChanged(m_taskQueue.size());

    // 提交任务到工作线程
    submitTaskToWorker(queuedTask.task, queuedTask.algorithm, worker);
}

bool AlgorithmManager::cancelTask(const QString& taskId)
{
    // 1. 验证任务存在
    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[AlgorithmManager] cancelTask: 任务不存在" << taskId;
        return false;
    }

    AlgorithmTaskPtr task = m_activeTasks[taskId];
    QString algorithmName = task->algorithmName();

    qDebug() << "[AlgorithmManager] cancelTask: 取消任务" << taskId
             << "算法:" << algorithmName;

    // 2. 检查任务是否正在执行
    if (m_taskWorkers.contains(taskId)) {
        // 正在执行的任务：请求取消
        AlgorithmWorker* worker = m_taskWorkers[taskId];
        QMetaObject::invokeMethod(worker, "requestCancellation", Qt::QueuedConnection);

        qDebug() << "[AlgorithmManager] 请求 worker" << worker << "取消任务" << taskId;

        emit algorithmCancelled(taskId, algorithmName);
        return true;
    }

    // 3. 检查任务是否在队列中
    for (int i = 0; i < m_taskQueue.size(); ++i) {
        if (m_taskQueue[i].task->taskId() == taskId) {
            // 从队列中移除
            m_taskQueue.removeAt(i);
            m_activeTasks.remove(taskId);

            qDebug() << "[AlgorithmManager] 从队列中移除任务" << taskId
                     << "剩余队列:" << m_taskQueue.size();

            emit queuedTaskCountChanged(m_taskQueue.size());
            emit algorithmCancelled(taskId, algorithmName);
            return true;
        }
    }

    qWarning() << "[AlgorithmManager] cancelTask: 任务" << taskId << "既不在执行也不在队列中";
    return false;
}

// ==================== 工作线程信号处理槽函数 ====================

void AlgorithmManager::onWorkerStarted(const QString& taskId, const QString& algorithmName)
{
    qDebug() << "[AlgorithmManager] onWorkerStarted: 任务" << taskId
             << "算法:" << algorithmName;

    emit algorithmStarted(taskId, algorithmName);
}

void AlgorithmManager::onWorkerProgress(const QString& taskId, int percentage, const QString& message)
{
    // 转发进度信号
    emit algorithmProgress(taskId, percentage, message);
}

void AlgorithmManager::onWorkerFinished(const QString& taskId, const QVariant& result, qint64 elapsedMs)
{
    qDebug() << "[AlgorithmManager] onWorkerFinished: 任务" << taskId
             << "耗时:" << elapsedMs << "ms";

    // 1. 获取任务信息
    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[AlgorithmManager] onWorkerFinished: 任务不存在" << taskId;
        return;
    }

    AlgorithmTaskPtr task = m_activeTasks[taskId];
    QString algorithmName = task->algorithmName();

    // 2. 释放工作线程
    if (m_taskWorkers.contains(taskId)) {
        AlgorithmWorker* worker = m_taskWorkers[taskId];
        AlgorithmThreadManager::instance()->releaseWorker(worker);
        m_taskWorkers.remove(taskId);
    }

    // 3. 处理结果
    AlgorithmResult algorithmResult = result.value<AlgorithmResult>();

    if (algorithmResult.isSuccess()) {
        // 成功：处理结果并发出信号
        handleAlgorithmResult(algorithmResult);

        // 发出异步执行完成信号
        emit algorithmFinished(taskId, algorithmName, algorithmResult, elapsedMs);

        // 同时发出旧的兼容信号，供 AlgorithmCoordinator 监听
        emit algorithmResultReady(algorithmName, algorithmResult);
    } else {
        // 失败：发出失败信号
        emit algorithmFailed(taskId, algorithmName, algorithmResult.errorMessage());

        // 同时发出旧的兼容信号
        emit algorithmExecutionFailed(algorithmName, algorithmResult.errorMessage());
    }

    // 4. 清理任务记录
    m_activeTasks.remove(taskId);

    qDebug() << "[AlgorithmManager] 任务" << taskId << "已清理，剩余活跃任务:" << m_activeTasks.size();
}

void AlgorithmManager::onWorkerFailed(const QString& taskId, const QString& errorMessage)
{
    qWarning() << "[AlgorithmManager] onWorkerFailed: 任务" << taskId
               << "失败:" << errorMessage;

    // 1. 获取任务信息
    if (!m_activeTasks.contains(taskId)) {
        qWarning() << "[AlgorithmManager] onWorkerFailed: 任务不存在" << taskId;
        return;
    }

    AlgorithmTaskPtr task = m_activeTasks[taskId];
    QString algorithmName = task->algorithmName();

    // 2. 释放工作线程
    if (m_taskWorkers.contains(taskId)) {
        AlgorithmWorker* worker = m_taskWorkers[taskId];
        AlgorithmThreadManager::instance()->releaseWorker(worker);
        m_taskWorkers.remove(taskId);
    }

    // 3. 发出失败信号
    emit algorithmFailed(taskId, algorithmName, errorMessage);

    // 4. 清理任务记录
    m_activeTasks.remove(taskId);

    qDebug() << "[AlgorithmManager] 任务" << taskId << "已清理，剩余活跃任务:" << m_activeTasks.size();
}

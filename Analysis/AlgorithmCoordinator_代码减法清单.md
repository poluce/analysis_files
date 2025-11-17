# AlgorithmCoordinator 代码减法优化清单

## 目标
- **消除冗余代码**
- **简化复杂逻辑**
- **保持功能不变**
- **提升可读性**

## 快速改进列表（按优先级排序）

### 1. 消除重复代码（高优先级）

#### 1.1 提取结果保存逻辑
**当前问题**：`onAlgorithmResultReady()` 和 `onAsyncAlgorithmFinished()` 中重复保存结果到上下文

**重复代码位置**：
- `algorithm_coordinator.cpp:267-277`（同步路径）
- `algorithm_coordinator.cpp:421-428`（异步路径）

**优化方案**：
```cpp
// 新增私有方法
void AlgorithmCoordinator::saveResultToContext(
    const QString& algorithmName, const AlgorithmResult& result)
{
    m_context->setValue(
        OutputKeys::latestResult(algorithmName),
        QVariant::fromValue(result),
        QStringLiteral("AlgorithmCoordinator"));

    m_context->setValue(
        OutputKeys::resultType(algorithmName),
        QVariant::fromValue(static_cast<int>(result.type())),
        QStringLiteral("AlgorithmCoordinator"));
}

// 在两处调用
void AlgorithmCoordinator::onAlgorithmResultReady(...) {
    saveResultToContext(algorithmName, result);
    emit algorithmSucceeded(algorithmName);
}

void AlgorithmCoordinator::onAsyncAlgorithmFinished(...) {
    // 清除任务ID
    if (m_currentTaskId == taskId) {
        m_currentTaskId.clear();
    }

    saveResultToContext(algorithmName, result);  // 复用
    emit algorithmSucceeded(algorithmName);
}
```

**收益**：
- 代码减少：10 行
- 维护成本降低：修改保存逻辑只需改一处

---

#### 1.2 简化上下文清空逻辑
**当前问题**：`executeAlgorithm()` 中手动清空多个键，然后逐一设置新值

**冗余代码位置**：`algorithm_coordinator.cpp:324-330`
```cpp
// 清空旧数据
m_context->remove(ContextKeys::ActiveCurve);
m_context->remove(ContextKeys::BaselineCurves);
m_context->remove(ContextKeys::SelectedPoints);
QStringList paramKeys = m_context->keys("param.");
for (const QString& key : paramKeys) {
    m_context->remove(key);  // O(n) 遍历删除
}
```

**优化方案A**：直接覆盖（推荐）
```cpp
// 删除清空逻辑，直接设置新值（QHash 自动覆盖）
m_context->setValue(ContextKeys::ActiveCurve, QVariant::fromValue(*curve), ...);
m_context->setValue(ContextKeys::BaselineCurves, ..., ...);
m_context->setValue(ContextKeys::SelectedPoints, ..., ...);
// 参数也是直接覆盖
for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
    m_context->setValue(QString("param.%1").arg(it.key()), it.value(), ...);
}
```

**优化方案B**：批量清空（备选）
```cpp
// 在 AlgorithmContext 中添加方法
void AlgorithmContext::removeByPrefix(const QString& prefix) {
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if (it.key().startsWith(prefix)) {
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
}

// 使用
m_context->removeByPrefix("param.");  // 批量删除参数
```

**推荐**：方案A（直接覆盖）
**收益**：
- 代码减少：7 行
- 性能提升：避免遍历删除

---

#### 1.3 统一状态清理逻辑
**当前问题**：多处调用 `resetPending()` 和 `m_currentTaskId.clear()`，分散且容易遗漏

**分散位置**：
- `cancelPendingRequest()` - 清理 pending 和 taskId
- `onAsyncAlgorithmFinished()` - 只清理 taskId
- `onAsyncAlgorithmFailed()` - 只清理 taskId
- 各个错误分支 - 只调用 `resetPending()`

**优化方案**：
```cpp
// 替换 resetPending()，统一清理所有状态
void AlgorithmCoordinator::resetState() {
    m_pending.reset();
    m_currentTaskId.clear();
    // 未来可扩展：清理其他临时状态
}

// 在所有需要清理的地方调用 resetState()
void AlgorithmCoordinator::cancelPendingRequest() {
    if (m_pending.has_value()) {
        const QString algorithmName = m_pending->descriptor.name;
        resetState();  // 统一清理
        emit showMessage(QStringLiteral("已取消算法 %1").arg(algorithmName));
    }
    // ...
}

void AlgorithmCoordinator::onAsyncAlgorithmFinished(...) {
    qDebug() << "异步任务完成:" << algorithmName;
    resetState();  // 统一清理（替代单独的 m_currentTaskId.clear()）
    saveResultToContext(algorithmName, result);
    emit algorithmSucceeded(algorithmName);
}
```

**收益**：
- 代码统一：所有状态清理逻辑一致
- 减少bug：避免遗漏清理某个状态

---

### 2. 改进错误处理（中优先级）

#### 2.1 统一错误处理流程
**当前问题**：错误处理分散，只有 `qWarning()` 日志，UI 无法感知

**分散位置**：
- `handleAlgorithmTriggered()` - 5 处不同的失败场景
- `handleParameterSubmission()` - 2 处
- `handlePointSelectionResult()` - 3 处
- `executeAlgorithm()` - 2 处

**优化方案**：
```cpp
// 添加统一错误处理方法
void AlgorithmCoordinator::handleError(
    const QString& algorithmName,
    const QString& reason)
{
    qWarning() << "[AlgorithmCoordinator] 错误:" << algorithmName << "-" << reason;
    resetState();  // 自动清理状态
    emit algorithmFailed(algorithmName, reason);
}

// 使用示例（替换所有手动 emit + return）
void AlgorithmCoordinator::handleAlgorithmTriggered(...) {
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        handleError(algorithmName, "没有选中的曲线");  // 统一处理
        return;
    }
    // ...
}
```

**收益**：
- 代码简化：每个错误点减少 2-3 行
- 一致性：所有错误都会清理状态并通知UI

---

### 3. 简化复杂逻辑（低优先级）

#### 3.1 添加辅助方法简化 `handleAlgorithmTriggered()`
**当前问题**：`handleAlgorithmTriggered()` 有 88 行，包含 4 个复杂分支

**优化方案**：提取分支逻辑为独立方法
```cpp
// 提取交互类型分支
void AlgorithmCoordinator::handleNoneInteraction(
    const AlgorithmDescriptor& descriptor,
    ThermalCurve* curve,
    const QVariantMap& parameters)
{
    QVariantMap effectiveParams = parameters;
    populateDefaultParameters(descriptor, effectiveParams);
    executeAlgorithm(descriptor, curve, effectiveParams, {});
}

void AlgorithmCoordinator::handleParameterDialogInteraction(
    const AlgorithmDescriptor& descriptor,
    ThermalCurve* curve,
    const QVariantMap& parameters,
    bool hasPresetParameters)
{
    QVariantMap effectiveParams = parameters;
    const bool autoExecutable = populateDefaultParameters(descriptor, effectiveParams);

    if ((autoExecutable && !hasPresetParameters) || hasPresetParameters) {
        executeAlgorithm(descriptor, curve, effectiveParams, {});
    } else {
        // 需要用户输入参数
        PendingRequest request;
        request.descriptor = descriptor;
        request.curveId = curve->id();
        request.parameters = effectiveParams;
        request.phase = PendingPhase::AwaitParameters;
        m_pending = request;
        emit requestParameterDialog(descriptor.name, descriptor.parameters, effectiveParams);
    }
}

// 主函数变简洁
void AlgorithmCoordinator::handleAlgorithmTriggered(...) {
    // 验证和准备工作
    ThermalCurve* activeCurve = m_curveManager->getActiveCurve();
    if (!activeCurve) {
        handleError(algorithmName, "没有选中的曲线");
        return;
    }

    auto descriptorOpt = descriptorFor(algorithmName);
    if (!descriptorOpt.has_value()) {
        handleError(algorithmName, "找不到算法描述");
        return;
    }

    const AlgorithmDescriptor& descriptor = descriptorOpt.value();
    if (!ensurePrerequisites(descriptor, activeCurve)) {
        handleError(algorithmName, "前置条件未满足");
        return;
    }

    // 根据交互类型分发
    switch (descriptor.interaction) {
    case AlgorithmInteraction::None:
        handleNoneInteraction(descriptor, activeCurve, presetParameters);
        break;
    case AlgorithmInteraction::ParameterDialog:
        handleParameterDialogInteraction(descriptor, activeCurve, presetParameters, !presetParameters.isEmpty());
        break;
    // ... 其他分支
    }
}
```

**收益**：
- 主函数从 88 行 → 约 40 行
- 每个分支逻辑清晰独立
- 易于单独测试每个交互类型

**权衡**：
- 增加 4 个私有方法（约 100 行）
- 总代码量略增，但可读性大幅提升

**建议**：可选优化，根据实际需求决定

---

## 优化前后对比

### 代码量变化
| 优化项 | 当前行数 | 优化后 | 减少 |
|--------|---------|--------|------|
| 结果保存逻辑（重复） | 22 | 12 | -10 |
| 上下文清空逻辑 | 7 | 0 | -7 |
| 状态清理（统一） | 分散 | 集中 | -5 |
| 错误处理（统一） | 分散 | 集中 | -15 |
| **总计** | **571** | **534** | **-37** |

### 性能提升
- 上下文清空：避免 O(n) 遍历删除
- 状态管理：减少状态不一致风险

### 可维护性提升
- 结果保存逻辑：修改一处，所有路径生效
- 错误处理：统一流程，一致的用户体验
- 状态清理：集中管理，减少遗漏

---

## 实施步骤（保守策略）

### 第1步：提取结果保存逻辑（2小时）
1. 创建 `saveResultToContext()` 私有方法
2. 替换 `onAlgorithmResultReady()` 中的保存逻辑
3. 替换 `onAsyncAlgorithmFinished()` 中的保存逻辑
4. 手动测试所有算法（微分、积分、移动平均、基线校正、峰面积）
5. 提交：`git commit -m "重构：提取结果保存逻辑"`

### 第2步：统一状态清理（1小时）
1. 将 `resetPending()` 重命名为 `resetState()`
2. 在 `resetState()` 中添加 `m_currentTaskId.clear()`
3. 替换所有 `m_currentTaskId.clear()` 调用为 `resetState()`
4. 手动测试取消操作和错误场景
5. 提交：`git commit -m "重构：统一状态清理逻辑"`

### 第3步：统一错误处理（3小时）
1. 创建 `handleError()` 私有方法
2. 逐个替换所有 `emit algorithmFailed()` 调用
3. 手动测试所有错误场景（无活动曲线、算法未找到、参数不足等）
4. 提交：`git commit -m "重构：统一错误处理流程"`

### 第4步：简化上下文清空（1小时）
1. 移除 `executeAlgorithm()` 中的清空逻辑
2. 测试算法连续执行（验证覆盖逻辑正确）
3. 提交：`git commit -m "优化：简化上下文清空逻辑"`

### 第5步：添加注释（2小时）
1. 为所有公有方法添加 Doxygen 注释
2. 为关键私有方法添加注释
3. 提交：`git commit -m "文档：添加函数注释"`

**总时间**：约 9 小时（1-2 个工作日）

---

## 验收标准

### 功能测试
所有现有功能必须正常工作：
- [ ] 微分算法（None 交互）
- [ ] 积分算法（None 交互）
- [ ] 移动平均算法（None 交互）
- [ ] 基线校正算法（PointSelection 交互）
- [ ] 峰面积算法（PointSelection 交互）
- [ ] 取消操作（待处理请求 + 正在执行任务）
- [ ] 错误处理（无活动曲线、算法未找到等）

### 代码质量
- [ ] 代码行数减少 > 30 行
- [ ] 无重复代码（通过 CPD 工具检查）
- [ ] 所有公有方法有注释
- [ ] 编译无警告

### 性能
- [ ] 算法执行时间无明显增加（< 5%）
- [ ] 上下文操作优化生效

---

## 风险评估

| 风险 | 影响 | 概率 | 应对措施 |
|------|------|------|---------|
| 破坏现有功能 | 高 | 低 | 充分的手动测试 + 回滚准备 |
| 引入新bug | 中 | 中 | 每步提交独立分支，可回滚 |
| 性能下降 | 中 | 低 | 性能基准测试对比 |
| 代码冲突 | 低 | 低 | 短期重构，避免长期分支 |

---

## 总结

这个清单专注于**代码减法**和**功能不变**，通过5个小步骤在1-2天内完成：

1. ✅ 消除重复代码（-10 行）
2. ✅ 统一状态清理（-5 行）
3. ✅ 统一错误处理（-15 行）
4. ✅ 简化上下文清空（-7 行）
5. ✅ 添加注释（提升可读性）

**关键原则**：
- 每次改动小，立即测试
- 每步独立提交，可随时回滚
- 不改变外部行为
- 先减法，后加法

**预期收益**：
- 代码量减少 6%
- 可维护性提升 30%
- 风险极低

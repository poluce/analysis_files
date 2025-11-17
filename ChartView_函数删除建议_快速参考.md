# ChartView 函数删除建议 - 快速参考

## 概览：15 个函数分析

**总体统计**:
- ✅ 被调用的函数：9 个（都不能删除）
- ❌ 未被调用的函数：6 个（可考虑删除）

---

## 🚀 立即删除（2 个函数，风险极低）

### 1. selectedPoints()
```cpp
// chart_view.h:121 - 删除这行
const QVector<ThermalDataPoint>& selectedPoints() const { return m_selectedPoints; }
```
- **理由**: 信号 `algorithmInteractionCompleted()` 已包含所有选点数据
- **调用**: 0 次
- **风险**: 极低
- **工作量**: 2 分钟

### 2. selectedPointsCurveId()
```cpp
// chart_view.h:126 - 删除这行
const QString& selectedPointsCurveId() const { return m_selectedPointsCurveId; }
```
- **理由**: 曲线 ID 可从信号上下文推导
- **调用**: 0 次
- **风险**: 极低
- **工作量**: 2 分钟

**验证命令**:
```bash
grep -r "selectedPoints()\|selectedPointsCurveId()" Analysis/src --include="*.cpp" --include="*.h"
# 应该返回空结果
```

---

## ⚠️ 可以删除（4 个函数，优先级低）

### 3. verticalCrosshairEnabled()
```cpp
// chart_view.h:151 - 删除声明
bool ChartView::verticalCrosshairEnabled() const;

// chart_view.cpp:121-124 - 删除实现
bool ChartView::verticalCrosshairEnabled() const
{
    return m_chart->verticalCrosshairEnabled();
}
```
- **理由**: 只有设置器被使用，查询器完全未调用
- **调用**: 0 次
- **风险**: 低（可能的外部依赖）
- **备选方案**: 用 [[deprecated]] 标记
- **工作量**: 5 分钟

### 4. horizontalCrosshairEnabled()
```cpp
// chart_view.h:152 - 删除声明
bool ChartView::horizontalCrosshairEnabled() const;

// chart_view.cpp:126-129 - 删除实现
bool ChartView::horizontalCrosshairEnabled() const
{
    return m_chart->horizontalCrosshairEnabled();
}
```
- **理由**: 与 verticalCrosshairEnabled() 完全对称，都未被调用
- **调用**: 0 次
- **风险**: 低
- **备选方案**: 用 [[deprecated]] 标记
- **工作量**: 5 分钟

### 5. interactionState()
```cpp
// chart_view.h:111 - 删除这行
InteractionState interactionState() const { return m_interactionState; }
```
- **理由**: 使用信号驱动 (interactionStateChanged)，无需同步查询
- **调用**: 0 次（信号在 main_controller.cpp:81 被连接）
- **风险**: 低
- **备选方案**: 用 [[deprecated]] 标记
- **工作量**: 2 分钟

### 6. activeAlgorithm()
```cpp
// chart_view.h:116 - 删除这行
const ActiveAlgorithmInfo& activeAlgorithm() const { return m_activeAlgorithm; }
```
- **理由**: 无外部调用，内部也不需要
- **调用**: 0 次
- **风险**: 低
- **备选方案**: 用 [[deprecated]] 标记
- **工作量**: 2 分钟

**验证命令**:
```bash
grep -r "verticalCrosshairEnabled()\|horizontalCrosshairEnabled()\|interactionState()\|activeAlgorithm()" \
  Analysis/src --include="*.cpp" --include="*.h" | grep -v "^[^:]*:\s*//"
```

---

## ⚠️ 谨慎删除（1 个函数，需要评估）

### 7. cancelAlgorithmInteraction()
```cpp
// chart_view.h:106
void cancelAlgorithmInteraction();

// chart_view.cpp:230-254
void ChartView::cancelAlgorithmInteraction()
{
    // ... 25 行清理代码
}
```

- **理由待评估**:
  - ✅ 当前未被调用
  - ✅ 设计完善，职责清晰
  - ❓ 基线校正功能完成时可能需要
  - ❓ 用户需要"取消选点"的方式

- **建议**:
  1. 保留，计划在基线校正完成后评估
  2. 考虑添加 UI 支持：
     - Escape 键中止选点
     - "取消选点"按钮
  3. 如果确认不需要 → 删除

- **工作量**: 需要决策

---

## ❌ 不能删除（9 个函数）

这 9 个函数都在代码库中被调用，不能删除：

### 测量工具（6 个）- 都是活跃功能
- ✅ `addMassLossTool()` - thermal_chart_view.cpp:450
- ✅ `removeMassLossTool()` - thermal_chart.cpp:793 (lambda)
- ✅ `clearAllMassLossTools()` - thermal_chart.cpp:472
- ✅ `addPeakAreaTool()` - thermal_chart_view.cpp:601
- ✅ `removePeakAreaTool()` - thermal_chart.cpp:853 (lambda)
- ✅ `clearAllPeakAreaTools()` - thermal_chart.cpp:473

### 标记管理（3 个）- 都是活跃功能
- ✅ `clearAllMarkers()` - thermal_chart.cpp:471
- ✅ `removeCurveMarkers()` - thermal_chart.cpp:444, 664

---

## 📋 实施计划

### 第 1 天：删除明显冗余的函数（15 分钟）

编辑 `/home/user/analysis_files/Analysis/src/ui/chart_view.h`：

**删除行 121 和 126**:
```diff
- const QVector<ThermalDataPoint>& selectedPoints() const { return m_selectedPoints; }
- const QString& selectedPointsCurveId() const { return m_selectedPointsCurveId; }
```

验证和提交：
```bash
cd /home/user/analysis_files/Analysis
grep -r "selectedPoints()\|selectedPointsCurveId()" src --include="*.cpp" --include="*.h"
# 应该无结果

git add src/ui/chart_view.h
git commit -m "refactor: 删除冗余的 getter 函数 (selectedPoints, selectedPointsCurveId)

ChartView 的这两个 getter 完全冗余，因为：
1. algorithmInteractionCompleted 信号已包含所有选点数据
2. 外部代码无需同步查询这些信息
3. 当前代码库无任何调用"
```

### 第 2 周：评估十字线函数（1 小时）

检查是否有外部依赖：
```bash
grep -r "verticalCrosshairEnabled\|horizontalCrosshairEnabled" . \
  --include="*.cpp" --include="*.h" --include="*.md"
```

如果无依赖 → 删除；否则 → 用 [[deprecated]] 标记

### 第 3 周：基线校正完成后（TBD）

重新评估 cancelAlgorithmInteraction()：
1. 检查是否真的需要"取消"功能
2. 如果需要 → 添加 UI 支持（快捷键、按钮）
3. 如果不需要 → 删除

---

## 代码审查检查清单

删除这些函数时检查：

- [ ] 已验证没有任何调用
- [ ] 已搜索整个代码库（.cpp, .h, 注释）
- [ ] 已检查是否有 UI 显示这些函数的状态
- [ ] 已更新 CLAUDE.md 文档
- [ ] 已提交 git commit 说明删除原因
- [ ] 已更新相关的设计文档（如有）

---

## 关键发现

### 模式 1：设置器有调用，查询器无调用

```cpp
// 有调用
setVerticalCrosshairEnabled(bool enabled);

// 无调用
bool verticalCrosshairEnabled() const;
```

这种模式下：
- 设置器：保留（有功能需求）
- 查询器：可删除（无查询需求）

### 模式 2：信号驱动架构中的冗余 getter

```cpp
// 信号已包含数据
void algorithmInteractionCompleted(const QString& algorithmName, const QVector<ThermalDataPoint>& points);

// getter 完全冗余
const QVector<ThermalDataPoint>& selectedPoints() const;
```

解决方案：删除冗余 getter，使用信号驱动架构

### 模式 3：完整的转发函数（都是必需的）

```cpp
void ChartView::addMassLossTool(...)
{
    m_chart->addMassLossTool(...);  // 直接转发
}
```

这些看似"无用"的转发函数实际上被调用：
- ThermalChartView 调用 ChartView 的 add* 函数
- ChartView 转发给 ThermalChart
- 不能删除

---

## 质量指标

| 指标 | 数值 |
|------|------|
| 分析的函数总数 | 15 |
| 可删除函数（极低风险） | 2 |
| 可删除函数（低风险） | 4 |
| 需要评估的函数 | 1 |
| 活跃函数（不能删除） | 9 |
| 总删除潜力 | ~40 行代码 |
| 估计删除时间 | 30 分钟 |

---

## 相关文档

- 详细分析：`/home/user/analysis_files/ChartView_函数使用分析.md`
- 项目说明：`/home/user/analysis_files/CLAUDE.md`


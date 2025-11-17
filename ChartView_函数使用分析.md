# ChartView 未使用函数分析报告

## 概述

本报告分析了 ChartView 中 15 个可能未使用的函数，判断其是否可以删除。分析维度包括：
1. 代码库中的实际调用情况
2. 是否属于旧交互体系遗留
3. 新交互体系是否已实现相同功能
4. 删除的风险评估

---

## 第一部分：质量损失工具函数

### 1.1 `addMassLossTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)`

**位置**: chart_view.h:137, chart_view.cpp:154-157

**调用情况**: ✅ 被调用 (thermal_chart_view.cpp:450)
- ThermalChartView::handleMassLossToolClick() 调用此函数

**分析**:
- 这是一个完全的转发函数，将调用传递给 ThermalChart
- 被 ThermalChartView 的工具逻辑调用
- 是活跃的测量工具功能的一部分

**删除建议**: ❌ 不能删除

**风险**: 删除会破坏质量损失测量工具功能的完整调用链

---

### 1.2 `removeMassLossTool(QGraphicsObject* tool)`

**位置**: chart_view.h:138, chart_view.cpp:159-162

**调用情况**: ✅ 被调用 (thermal_chart.cpp:793，通过 lambda)
- TrapezoidMeasureTool 发出 removeRequested 信号时触发

**分析**:
- 转发函数
- 通过信号连接被调用（当用户点击工具的关闭按钮时）

**删除建议**: ❌ 不能删除

**风险**: 删除会导致无法移除已添加的测量工具

---

### 1.3 `clearAllMassLossTools()`

**位置**: chart_view.h:139, chart_view.cpp:164-167

**调用情况**: ✅ 被调用 (thermal_chart.cpp:472)
- ThermalChart::clearCurves() 调用

**分析**:
- 转发函数
- 清空项目时调用
- 确保清空项目时所有测量工具都被清理

**删除建议**: ❌ 不能删除

**风险**: 删除会导致内存泄漏或孤立的测量工具对象

---

## 第二部分：峰面积工具函数

### 2.1 `addPeakAreaTool(const ThermalDataPoint& point1, const ThermalDataPoint& point2, const QString& curveId)`

**位置**: chart_view.h:143, chart_view.cpp:176-179

**调用情况**: ✅ 被调用 (thermal_chart_view.cpp:601)
- ThermalChartView::handlePeakAreaToolClick() 调用此函数

**分析**:
- 转发函数
- 被 ThermalChartView 的峰面积工具逻辑调用
- 是活跃的峰面积测量功能的一部分

**删除建议**: ❌ 不能删除

**风险**: 删除会破坏峰面积测量功能

---

### 2.2 `removePeakAreaTool(QGraphicsObject* tool)`

**位置**: chart_view.h:144, chart_view.cpp:181-184

**调用情况**: ✅ 被调用 (thermal_chart.cpp:853，通过 lambda)
- PeakAreaTool 发出 removeRequested 信号时触发

**分析**:
- 转发函数
- 通过信号连接被调用
- 对称于 removeMassLossTool()

**删除建议**: ❌ 不能删除

**风险**: 删除会导致无法移除峰面积工具

---

### 2.3 `clearAllPeakAreaTools()`

**位置**: chart_view.h:145, chart_view.cpp:186-189

**调用情况**: ✅ 被调用 (thermal_chart.cpp:473)
- ThermalChart::clearCurves() 调用

**分析**:
- 转发函数
- clearCurves() 清理时调用
- 对称于 clearAllMassLossTools()

**删除建议**: ❌ 不能删除

**风险**: 删除会导致内存泄漏

---

## 第三部分：标记线 / 十字线工具函数

### 3.1 `cancelAlgorithmInteraction()`

**位置**: chart_view.h:106, chart_view.cpp:230-254

**调用情况**: ❌ 未被调用（仅定义，无任何调用）

**分析**:
- 是新的状态机设计的一部分
- 目的：允许用户中断正在进行的算法交互（如选点过程）
- 功能完善：清除选点、重置状态、回到视图模式
- 当前没有被调用原因：
  - 新的架构尚未集成"取消"功能的 UI 元素（按钮、快捷键）
  - 用户界面还没有提供"取消选点"的交互入口

**删除建议**: ⚠️ 谨慎删除

**理由**:
1. 设计完整性：函数在状态机中有明确的职责
2. 架构一致性：与 startAlgorithmInteraction() 成对出现
3. 未来需求：基线校正等交互算法可能需要"取消"功能

**风险评估**:
- 低风险删除：当前确实没有调用
- 中风险保留：基线校正完成时可能需要此函数

**建议**: 在"取消"功能真正需要时再删除，或添加快捷键支持（Escape 中止选点）

---

### 3.2 `verticalCrosshairEnabled() const`

**位置**: chart_view.h:151, chart_view.cpp:121-124

**调用情况**: ❌ 未被调用（仅定义，无外部调用）

**分析**:
- 纯转发函数，查询十字线状态
- 对应的设置函数 setVerticalCrosshairEnabled() 被使用
- 十字线功能本身是活跃的，但状态查询函数没有被使用

**删除建议**: ⚠️ 可以删除

**理由**:
1. 当前代码库中没有任何地方需要查询十字线状态
2. 是"信息隐藏"的典型例子（设置器被用，查询器未被用）
3. 删除不会影响十字线功能的正常运行

**风险**:
- 低风险：可能的外部使用者（如插件或扩展）
- 如果有人依赖此函数进行 UI 绑定，删除会导致编译错误

---

### 3.3 `horizontalCrosshairEnabled() const`

**位置**: chart_view.h:152, chart_view.cpp:126-129

**调用情况**: ❌ 未被调用（仅定义，无外部调用）

**分析**:
- 与 verticalCrosshairEnabled() 完全对称
- 同样的设置函数被使用，但查询函数未被使用

**删除建议**: ⚠️ 可以删除

**理由与风险**: 与 verticalCrosshairEnabled() 相同

---

### 3.4 `clearAllMarkers()`

**位置**: chart_view.h:133, chart_view.cpp:144-147

**调用情况**: ✅ 被调用 (thermal_chart.cpp:471)
- ThermalChart::clearCurves() 调用

**分析**:
- 转发函数
- 在清空所有曲线时调用
- 确保标注点被清理

**删除建议**: ❌ 不能删除

**风险**: 删除会导致清空项目时标注点没有被清理

---

### 3.5 `removeCurveMarkers(const QString& curveId)`

**位置**: chart_view.h:132, chart_view.cpp:139-142

**调用情况**: ✅ 被调用 (thermal_chart.cpp:444 和 664)
- 删除曲线时：removeCurve()
- 隐藏曲线时：setCurveVisible()

**分析**:
- 转发函数
- 当删除或隐藏曲线时调用
- 清理该曲线关联的标注点

**删除建议**: ❌ 不能删除

**风险**: 删除会导致孤立的标注点

---

## 第四部分：状态/交互相关函数（新设计）

### 4.1 `interactionState() const`

**位置**: chart_view.h:111（inline 定义）

**调用情况**: ❌ 未被调用
- 对应信号 interactionStateChanged() 在 main_controller.cpp:81 被连接
- 但 getter 函数本身没有被调用

**分析**:
- 新的算法状态机的一部分
- 目的：允许外部代码查询当前交互状态
- 相应的信号提供了异步通知机制

**删除建议**: ⚠️ 可以删除

**理由**:
1. 当前无任何调用
2. 相应的信号 interactionStateChanged() 提供了异步通知
3. 架构选择：信号驱动而非同步查询

**风险**:
- 低风险删除
- 如果有人需要同步查询状态，会导致功能丢失

---

### 4.2 `activeAlgorithm() const`

**位置**: chart_view.h:116（inline 定义）

**调用情况**: ❌ 未被调用

**分析**:
- 返回当前活动算法的元数据（名称、显示名、所需点数、提示文本、目标曲线 ID）
- 仅在内部使用（onValueClicked 时检查 isValid()）
- 外部代码没有地方需要查询完整的算法元数据

**删除建议**: ⚠️ 可以删除

**理由**:
1. 无外部调用
2. 如需通知外部关于活动算法，应使用信号而非同步 getter

**风险**:
- 低风险删除
- 可能的 UI 显示需求会导致需要重新实现

---

### 4.3 `selectedPoints() const`

**位置**: chart_view.h:121（inline 定义）

**调用情况**: ❌ 未被调用

**分析**:
- 返回用户已选择的点（完整的 ThermalDataPoint）
- 当 algorithmInteractionCompleted 信号发出时，已自动包含选点数据
- 所以外部代码直接从信号参数获得选点，无需查询

**删除建议**: ✅ 可以删除

**理由**:
1. 完全冗余：信号中已包含所有选点数据
2. 架构不一致：应该使用信号驱动，而非同步查询
3. 无任何代码调用此函数

**风险**: 极低风险 - 当前无调用

---

### 4.4 `selectedPointsCurveId() const`

**位置**: chart_view.h:126（inline 定义）

**调用情况**: ❌ 未被调用

**分析**:
- 返回选中点所属的曲线 ID
- 当 algorithmInteractionCompleted 信号发出时：
  - 算法名称已知
  - 选点数据已知
  - 曲线 ID 可从信号的上下文推导

**删除建议**: ✅ 可以删除

**理由**:
1. 完全冗余
2. 曲线 ID 应该从信号的上下文中获取
3. 无任何调用

**风险**: 极低风险 - 当前无调用

---

## 总结表

| 函数 | 被调用 | 类型 | 删除建议 | 优先级 |
|------|--------|--------|---------|--------|
| addMassLossTool() | ✅ | 活跃转发 | ❌ 保留 | - |
| removeMassLossTool() | ✅ | 活跃转发 | ❌ 保留 | - |
| clearAllMassLossTools() | ✅ | 活跃转发 | ❌ 保留 | - |
| addPeakAreaTool() | ✅ | 活跃转发 | ❌ 保留 | - |
| removePeakAreaTool() | ✅ | 活跃转发 | ❌ 保留 | - |
| clearAllPeakAreaTools() | ✅ | 活跃转发 | ❌ 保留 | - |
| cancelAlgorithmInteraction() | ❌ | 新设计，未实装 | ⚠️ 谨慎 | 中 |
| verticalCrosshairEnabled() | ❌ | 冗余查询 | ⚠️ 可删 | 低 |
| horizontalCrosshairEnabled() | ❌ | 冗余查询 | ⚠️ 可删 | 低 |
| clearAllMarkers() | ✅ | 活跃转发 | ❌ 保留 | - |
| removeCurveMarkers() | ✅ | 活跃转发 | ❌ 保留 | - |
| interactionState() | ❌ | 状态查询 | ⚠️ 可删 | 低 |
| activeAlgorithm() | ❌ | 状态查询 | ⚠️ 可删 | 低 |
| selectedPoints() | ❌ | 冗余查询 | ✅ 删除 | 中 |
| selectedPointsCurveId() | ❌ | 冗余查询 | ✅ 删除 | 中 |

---

## 建议删除清单

### 立即删除（优先级中，风险极低）

1. **selectedPoints()**
   - 位置：chart_view.h:121
   - 理由：信号已包含所有数据，完全冗余
   - 影响：无任何调用

2. **selectedPointsCurveId()**
   - 位置：chart_view.h:126
   - 理由：曲线 ID 可从上下文获取，完全冗余
   - 影响：无任何调用

### 考虑删除（优先级低）

3. **verticalCrosshairEnabled()**
   - 位置：chart_view.h:151, chart_view.cpp:121-124
   - 理由：查询器无调用，只有设置器被使用
   - 备选：标记为 [[deprecated]]

4. **horizontalCrosshairEnabled()**
   - 位置：chart_view.h:152, chart_view.cpp:126-129
   - 理由：查询器无调用，只有设置器被使用
   - 备选：标记为 [[deprecated]]

5. **interactionState()**
   - 位置：chart_view.h:111
   - 理由：信号驱动架构中无需同步查询
   - 备选：标记为 [[deprecated]]

6. **activeAlgorithm()**
   - 位置：chart_view.h:116
   - 理由：无外部调用
   - 备选：标记为 [[deprecated]]

### 谨慎处理（优先级中）

7. **cancelAlgorithmInteraction()**
   - 位置：chart_view.h:106, chart_view.cpp:230-254
   - 建议：保留，等待基线校正功能完成后评估
   - 可以考虑添加 UI 支持（快捷键 Escape、取消按钮）

### 绝不删除

保留以下 9 个函数（都被调用）:
- addMassLossTool, removeMassLossTool, clearAllMassLossTools
- addPeakAreaTool, removePeakAreaTool, clearAllPeakAreaTools
- clearAllMarkers, removeCurveMarkers

---

## 实施步骤

### 第 1 步：删除最明显的冗余函数

编辑 `Analysis/src/ui/chart_view.h`，删除这两行（第 121 和 126 行）：

```cpp
const QVector<ThermalDataPoint>& selectedPoints() const { return m_selectedPoints; }
const QString& selectedPointsCurveId() const { return m_selectedPointsCurveId; }
```

验证：
```bash
grep -r "selectedPoints()" Analysis/src --include="*.cpp" --include="*.h"
grep -r "selectedPointsCurveId()" Analysis/src --include="*.cpp" --include="*.h"
```
应该没有结果（除了声明和内部使用）。

### 第 2 步：评估十字线查询函数

检查是否有外部代码依赖：
```bash
grep -r "verticalCrosshairEnabled()" . --exclude-dir=.git
grep -r "horizontalCrosshairEnabled()" . --exclude-dir=.git
```

如果没有依赖 → 删除

### 第 3 步：基线校正完成后评估

重新评估 cancelAlgorithmInteraction() 的必要性。

---

## 架构建议

1. **明确信号驱动设计**:
   - 如果完全使用信号，删除所有 getter 函数
   - 如果需要同步查询，明确说明原因

2. **简化转发函数**:
   - 考虑是否真的需要所有转发函数
   - 或直接暴露底层组件的 public API

3. **版本管理**:
   - 使用 [[deprecated]] 标记无用函数
   - 在更新日志中说明计划删除时间
   - 给用户足够的迁移时间

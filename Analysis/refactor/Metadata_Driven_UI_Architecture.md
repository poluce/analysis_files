# 架构重构总结：元数据驱动的 UI 方案（方案 B）

## 1. 目标与动机
- 降低 UI 与算法实现耦合，杜绝 UI 直接感知算法内部细节。
- 统一“参数收集 + 交互选点”的执行入口，减少重复 UI 代码。
- 以元数据（AlgorithmDescriptor）作为单一信息源，提升新增/改造算法效率与一致性。
- 符合既有四层架构与命令模式，不破坏 `MainWindow` 发信号、`MainController/AlgorithmCoordinator` 编排的职责边界。

## 2. 设计原则
- 分层清晰：
  - UI 只渲染与发信号；
  - Application 编排流程、参数校验、选点管理；
  - Domain/Infrastructure 仅承载算法逻辑，不含 UI 语义；
  - 数据修改一律通过命令对象由 `HistoryManager` 执行。
- 单一信息源：所有算法的参数/交互元数据只维护在 `AlgorithmDescriptor`。
- 渐进式演进：默认支持“无参数/无选点”，先接入 1–2 个算法打通全链路，再逐步迁移。

## 3. 核心构件
- AlgorithmDescriptor（Application 层）
  - 描述算法的参数清单、默认值、约束、枚举项，以及是否需要图上选点（数量与提示）。
  - UI 动态渲染与 Coordinator 编排流程的唯一数据源。
- GenericAlgorithmDialog（UI 层）
  - 根据 AlgorithmDescriptor 动态生成参数表单（整型/浮点/布尔/字符串/枚举/区间等）。
  - 内建基础校验（必填、范围、枚举合法性），产出 `QVariant` 参数包。
- AlgorithmCoordinator（Application 层）
  - 决定是否弹参窗、是否进入选点模式；将参数/选点写入 `AlgorithmContext`；最终调用 `AlgorithmManager::executeWithInputs()`。
  - 所有曲线修改通过命令模式落地到 `HistoryManager`。
- CurveViewController（UI 控制器）
  - 仅负责切换拾取模式与回传选点（`QPointF` 序列）；不参与算法编排与数据修改。

## 4. 元数据模型（AlgorithmDescriptor）
参数类型枚举建议：
- Integer / Double / Boolean / String / Enum
- DoubleRange（双端浮点区间）
- PointsOnChart（占位符，用于流程控制与 UI 提示，不渲染为可编辑控件）

参数约束：
- 整型/浮点：最小、最大、步长、单位（可选）。
- 枚举：值-标签对列表。
- 必填：空值校验。

选点规范：
- `minCount`/`maxCount`（`-1` 为无限制）、交互提示 `hint`。

详见代码骨架：`refactor/application/algorithm/algorithm_descriptor.h`。

## 5. 端到端流程
无选点：
1) `MainWindow` 发出 `algorithmRequested(name)`；
2) `MainController` 转交 `AlgorithmCoordinator::runByName(name)`；
3) Coordinator 读取 `AlgorithmDescriptor` → 弹出 `GenericAlgorithmDialog` 收集参数；
4) 组装 inputs → `AlgorithmManager::executeWithInputs()` → 命令模式落地 → UI 同步。

含选点：
1) 与上相同，若描述包含 `PointsOnChart` 需求；
2) Coordinator 切换 `CurveViewController` 至拾取模式，等待 `pointsPicked`；
3) 将参数与选点写入 `AlgorithmContext`；
4) 调用 `AlgorithmManager::executeWithInputs()` 并记录历史。

## 6. 职责边界与约束
- UI：只渲染/发信号，不做业务判断，不触碰数据模型。
- Application：Coordinator 统筹参数/选点/校验/执行；通过命令模式修改曲线。
- Domain/Infrastructure：只实现算法逻辑，不携带 UI 语义或组件名。
- 历史：所有会修改曲线的操作必须封装为命令并由 `HistoryManager::executeCommand()` 执行。

## 7. 迁移策略
- 第 1 阶段：为“移动平均滤波”“基线校正”编写 `AlgorithmDescriptor`，打通全链路；
- 第 2 阶段：逐步为其他算法补齐描述，下线老的专用参数窗；
- 提供默认描述（无参数/无选点），避免一次性修改全部算法。

## 8. 风险与规避
- 双源描述冲突：仅以 `AlgorithmDescriptor` 为真，不在算法实现中重复声明 UI 元数据。
- 领域层污染：描述仅用抽象类型与约束，禁止出现控件名/布局细节。
- 绕过命令模式：Coordinator/对话框都不得直接改曲线，统一通过命令对象与 `HistoryManager`。

## 9. 示例：移动平均滤波 & 基线校正
- 移动平均滤波（无选点）
  - 参数：`windowSize`（必填，整数，建议奇数），`passes`（可选，整数）。
  - 输出：`AppendCurve`（新增曲线）。
- 基线校正（需要 2 个选点）
  - 参数：`method`（枚举：线性/多项式），`order`（整数，仅多项式时有效）。
  - 选点：`minCount=2, maxCount=2`，提示“请在主曲线上选择两个基线参考点”。
  - 输出：`ReplaceCurve`（替换处理数据）。
  - 详见：`refactor/application/algorithm/register_default_descriptors.cpp`。

## 10. 与 AlgorithmCoordinator 的集成要点
- 新增入口 `runByName(const QString& name)`：按描述驱动“参数窗→选点→执行”。
- 选点对接：发出“进入拾取模式”的信号给 `CurveViewController`，等待 `pointsPicked`；所得点写入 `AlgorithmContext`。
- 执行：将 `params` 与 `pickedPoints` 一并传入 `AlgorithmManager::executeWithInputs()`；生成的命令由 `HistoryManager` 记录并触发 UI 更新。

## 11. 目录结构（本重构提案的示例与骨架）
- `refactor/Metadata_Driven_UI_Architecture.md`（本文档）
- `refactor/application/algorithm/algorithm_descriptor.h`
- `refactor/application/algorithm/algorithm_descriptor_registry.h/.cpp`
- `refactor/ui/generic_algorithm_dialog.h/.cpp`
- `refactor/application/algorithm/register_default_descriptors.cpp`


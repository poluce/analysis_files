# 执行计划（彻底改造版）：围绕三类一次性切换

范围仅限三条最高优先级问题对应的三类，但在该范围内做“彻底、无兼容分叉”的一次性切换：
- AlgorithmExecutionController（算法执行控制：点选/参数/进度/完成/失败/工作流回调）
- MessagePresenter（统一的消息呈现实现，不引入接口层）
- DeleteCurveUseCase（删除/级联删除的业务用例）

不引入其它控制器/Builder/Presenter 接口；但上述三类覆盖到的现有路径全部迁移，删除旧实现，避免“新旧两套并行”。

---

## 背景与问题（摘要）

1) 算法执行链路紧耦合在 MainController：点选启动、动态参数表单、进度/取消、完成/失败/工作流回调均在一个类中实现，导致体积大、耦合重、扩展困难。
2) 删除流程业务与 UI 混杂：在单一槽函数中完成校验、子项罗列、确认与命令执行，可复用性与可测试性差。
3) 消息呈现散落在控制器：直接使用 `QMessageBox::`，控制器与 UI 呈现强绑定。

---

## 目标

- MainController 仅保留 orchestrator 与入口日志；不再承载算法链路细节、删除业务细节或直接弹窗。
- AlgorithmExecutionController 完整承接算法执行全链路；DeleteCurveUseCase 承接删除业务；MessagePresenter 统一呈现。
- 控制器层不再直接出现 `QMessageBox::`。

---

## 执行步骤（一次性切换，保证每步提交可回滚）

1) 接线与实例化
- 在 `ApplicationContext` 实例化并注入：AlgorithmExecutionController、MessagePresenter、DeleteCurveUseCase。
- 将 `AlgorithmCoordinator` 与 `ChartView` 的相关信号全部连接至 AlgorithmExecutionController。
- MainController 的 `onAlgorithmRequested` 仅保留 run/cancel 入口与日志。

2) 算法链路整体迁移
- 将以下内容从 MainController 迁入 AlgorithmExecutionController：
  - 点选交互启动与结果处理
  - 参数表单创建与参数提取
  - 进度对话框生命周期与取消逻辑
  - 算法完成/失败与工作流回调
- 删除 MainController 中对应方法与成员，保持日志输出一致。

3) 删除用例整体迁移
- 将删除流程中的：校验、子曲线汇总、确认（改由 MessagePresenter）、命令执行，迁入 DeleteCurveUseCase。
- MainController 仅收集输入、调用用例并转达反馈；移除业务细节实现。

4) 控制器层弹窗统一替换
- 在所有控制器（MainController、AlgorithmExecutionController、CurveViewController 如有）统一替换 `QMessageBox::` 为 MessagePresenter。
- 确保控制器层源码不再直接出现 `QMessageBox::`。

5) 清理与回归
- 删除冗余 include/成员/信号连接。
- 全量走查：导入 → 删除 → 算法执行（点选/参数/进度/完成/失败）与相关菜单/快捷键。

---

## 验收标准

- MainController 不含参数表单、进度框、点选状态或删除业务细节；相应成员/方法移除。
- AlgorithmExecutionController 覆盖点选/参数/进度/完成/失败/工作流回调，行为与原先一致或更清晰。
- DeleteCurveUseCase 覆盖所有删除路径（含级联），并通过 MessagePresenter 进行确认与提示。
- 所有控制器源码不再直接出现 `QMessageBox::` 字样（搜索验证）。

---

## 风险、校验与回退

- 风险：接线遗漏导致链路断裂；参数/进度对话框生命周期管理错误。
- 校验：迁移后执行关键词搜索，确保“彻底”达成。
  - `rg -n "QMessageBox::" src/ui/controller`
  - `rg -n "onRequestParameterDialog|createParameterWidget|extractParameters" src/ui/controller/main_controller.cpp`
  - `rg -n "onAlgorithmStarted|onAlgorithmProgress" src/ui/controller/main_controller.cpp`
  - `rg -n "onCurveDeleteRequested" src/ui/controller/main_controller.cpp`
- 回退：各步骤分别提交；必要时回滚到上一个提交，不保留“双路径兼容”的长期状态。


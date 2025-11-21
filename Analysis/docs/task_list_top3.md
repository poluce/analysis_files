# 任务清单（彻底改造版）：围绕三类一次性切换

范围：仅新增 AlgorithmExecutionController、MessagePresenter、DeleteCurveUseCase 三类，但对其涉及路径“全量替换+删除旧实现”，不保留双路径兼容。

---

- [ ] T1 接线与实例化
  - 目标：在 ApplicationContext 实例化并接线三类，主控仅保留入口。
  - 触达：
    - `src/application/application_context.cpp`（创建并注入三类；连接 `AlgorithmCoordinator`/`ChartView` → `AlgorithmExecutionController`）
    - `src/ui/controller/main_controller.cpp`（`onAlgorithmRequested` 仅保留入口与日志）
  - 交付：能编译运行；算法请求到达 AlgorithmExecutionController（日志验证）。
  - 验收：旧的参数/进度/点选代码将于后续步骤彻底移除。

- [ ] T2 算法链路整体迁移
  - 目标：将点选、参数、进度/取消、完成/失败/工作流全部迁至 `AlgorithmExecutionController`。
  - 触达：
    - 从 `src/ui/controller/main_controller.cpp` 迁移以下方法/逻辑至新控制器并删除原实现：
      - `onCoordinatorRequestPointSelection` 相关
      - `onRequestParameterDialog` / `createParameterWidget` / `extractParameters`
      - `onAlgorithmStarted` / `onAlgorithmProgress` / 取消处理及进度框成员
      - `onAlgorithmCompleted` / `onWorkflowCompleted` / `onWorkflowFailed`
  - 交付：新控制器内实现完整链路；保留原日志文本与用户可见行为。
  - 验收：算法执行流程（点选 → 参数 → 进度 → 完成/失败）跑通；MainController 内对应成员/方法删除。

- [ ] T3 删除用例整体迁移
  - 目标：将删除/级联删除的业务校验、子曲线汇总、命令执行迁入 `DeleteCurveUseCase`。
  - 触达：
    - 新建 `src/application/usecase/delete_curve_use_case.*`
    - 改造 `src/ui/controller/main_controller.cpp: onCurveDeleteRequested`：变为“询问 → 调用用例 → 呈现结果”，移除业务细节。
  - 交付：用例类与最小单测（非 GUI）。
  - 验收：带级联确认的删除行为与提示一致；MainController 不再包含删除业务细节。

- [ ] T4 控制器层弹窗统一替换
  - 目标：控制器层不再直接使用 `QMessageBox::`，改为 `MessagePresenter`。
  - 触达：
    - 新建 `src/ui/presenter/message_presenter.*`
    - 替换控制器层（MainController、AlgorithmExecutionController、CurveViewController 如有）所有 `QMessageBox::` 调用。
  - 交付：具体实现（不抽接口）。
  - 验收：在 `src/ui/controller` 路径搜索不到 `QMessageBox::`。

- [ ] T5 清理冗余与依赖
  - 目标：删除已无用的 include、成员、信号连接与注入代码。
  - 触达：`src/ui/controller/main_controller.cpp`、`src/application/application_context.cpp` 全文查验。
  - 交付：更小、更清晰的 MainController 与 ApplicationContext。
  - 验收：编译通过；无未使用符号；无残留旧实现调用点。

- [ ] T6 回归验证与检索校验
  - 目标：完成全量走查与关键词检索，确保“彻底切换”。
  - 触达：手工走查导入/删除/算法执行（点选/参数/进度/完成/失败）与相关菜单/快捷键。
  - 交付：回归结果记录（简要）。
  - 验收（检索命令建议）：
    - `rg -n "QMessageBox::" src/ui/controller`
    - `rg -n "onRequestParameterDialog|createParameterWidget|extractParameters" src/ui/controller/main_controller.cpp`
    - `rg -n "onAlgorithmStarted|onAlgorithmProgress" src/ui/controller/main_controller.cpp`
    - `rg -n "onCurveDeleteRequested" src/ui/controller/main_controller.cpp`


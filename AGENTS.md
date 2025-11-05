# AGENTS.md

本文件面向在本仓库执行自动化修改的编码助手（Claude Code、Cursor、GitHub Copilot、Codex 等），提供项目背景、核心约束与常用工作流，帮助在不破坏架构的前提下高效完成任务。

## 文档目的
- 让智能助手快速掌握项目的现状与关键模块，避免做出与设计相悖的修改。
- 明确构建、运行、调试所需的基本命令与环境要求。
- 罗列必须遵守的开发约束和常见坑点，降低引入回归的风险。

## 项目概览
- 基于 Qt 5.14.2 + Qt Charts 的桌面热分析数据处理工具，支持 TGA、DSC、ARC 等多种仪器数据。
- 采用四层分层架构（UI / Application / Domain / Infrastructure），遵循领域驱动设计，数据由 `CurveManager` 统一管理。
- 图表子系统重写为 `ChartView` + 控制器组合，支持多曲线显示、曲线选择、高亮与注释线等交互。
- 算法体系由 `AlgorithmManager` 统一注册与调度，支持传统单输入算法与需要交互选点的 B 类算法（如基线校正）。
- 撤销/重做通过命令模式落地，`HistoryManager` 维护命令栈并向 UI 发信号更新工具栏状态。

## 构建与运行
- 批处理脚本（Windows）：位于 `Analysis/build-scripts/`
  ```bash
  cd Analysis/build-scripts
  build.bat         # 首次 Debug 构建
  rebuild.bat       # 清理后快速重建
  build-release.bat # Release 构建
  clean.bat         # 清理所有构建产物
  ```
- 手动命令行构建（MinGW + qmake）：
  ```bash
  cd Analysis
  mkdir build-debug && cd build-debug
  qmake ../Analysis.pro
  mingw32-make -j4
  ```
- 运行调试版：`Analysis/build-debug/debug/Analysis.exe`
- 运行发布版（若使用脚本生成）：`Analysis/build-release/release/Analysis.exe`
- 环境要求：Qt ≥ 5.14.2、MinGW 7.3.0、C++17、启用 Qt Charts 模块。

## 代码结构与职责

### 表示层（`src/ui/`）
- `main_window.*`：主窗口，负责菜单、工具栏、停靠面板、Ribbon 初始化与信号转发，接收预构造的 `ChartView` 与 `ProjectExplorerView` 并仅处理布局与 UI 事件；通过 `dataImportRequested`、`algorithmRequested*`、`undoRequested` 等信号通知控制器，避免直接持有控制器指针。
- `chart_view.*`：基于 Qt Charts 的曲线视图组件，支持多轴、多曲线、命中检测与注释线管理。
- `project_explorer_view.*`：封装 `QTreeView`，展示由 `ProjectTreeManager` 提供的曲线树。
- `data_import_widget.*`：数据导入与预览对话框。
- （已移除）`peak_area_dialog.*`：峰面积计算改为消息提示流程，暂不支持图上点拾取。
- `controller/`：
  - `main_controller.*`：协调 UI 与应用层服务，集中处理导入、算法执行、撤销/重做等业务流程。
  - `curve_view_controller.*`：连接 `CurveManager`、`ChartView`、`ProjectExplorerView`、`ProjectTreeManager`，管理曲线显示、高亮与树视图同步。

### 应用层（`src/application/`）
- `application_context.*`：统一初始化入口，按模块顺序创建 Model / View / Controller，并集中注册算法后启动主窗口。
- `curve/curve_manager.*`：曲线生命周期管理、活动曲线维护、信号广播。
- `project/project_tree_manager.*`：基于 `QStandardItemModel` 管理项目/曲线树，处理复选框状态并发射 `curveCheckStateChanged`。
- `algorithm/algorithm_manager.*`：算法注册与调度，支持 `execute()`（A 类算法）与 `executeWithInputs()`（B-E 类算法），根据输出类型创建新曲线或返回数据。
- `history/history_manager.*` 与命令类：实现命令模式，所有会修改数据的操作必须封装为命令并进入历史栈。

### 领域层（`src/domain/`）
- `model/thermal_curve.*`：核心业务对象，维护 `rawData`（不可变）与 `processedData`（可变），持有仪器类型 `InstrumentType` 与信号类型 `SignalType`。
- `model/thermal_data_point.h`：温度、时间、测量值等基础数据结构。
- `algorithm/i_thermal_algorithm.h`：算法抽象，定义输入/输出类型、参数配置、交互提示等接口。
- `algorithm/i_command.h`：命令接口，统一 `execute/undo/redo`。

### 基础设施层（`src/infrastructure/`）
- `io/text_file_reader.*`：按行读取文本/CSV 数据并解析为 `ThermalCurve`。
- `algorithm/`：
  - `differentiation_algorithm.*`：微分（大窗口平滑中心差分）。
  - `integration_algorithm.*`：积分（梯形法）。
  - `moving_average_filter_algorithm.*`：移动平均滤波。
  - `baseline_correction_algorithm.*`：基线校正算法逻辑保留，但当前 UI 入口因点拾取功能移除而禁用。

### 分层交互准则
- **视图层（UI）**：只负责发射信号与直接的界面响应，不包含业务逻辑；需要的数据与动作由控制层驱动。
- **控制层（Controller）**：接收来自视图层与数据模型的信号，解析成业务语义后调用应用服务或回写视图，承担协调与调度职责。
- **数据模型层（Manager / Domain Model）**：通过信号向控制层报告状态变化，接受控制层或其他模型的事件驱动；禁止模型间直接调用，只允许事件监听关系。

### 其他目录
- `build-scripts/`：集中存放构建脚本与 `BUILD.md` 说明。
- `设计文档/`、`新设计文档/`、`微分算法/`、`调试日志说明.md`：补充架构、交互、算法与调试说明。

## 核心流程

### 数据导入
```
MainWindow → MainController::onShowDataImport()
  → DataImportWidget 预览/确认
  → TextFileReader 读取文件
  → CurveManager::addCurve()
  → 信号广播：
      curveAdded → ProjectTreeManager 构建树节点
                → CurveViewController 调用 ChartView::addCurve()
```

### 算法执行（含交互场景）
```
MainWindow 菜单/按钮
  → MainController::onAlgorithmRequested*(name, params)
  → AlgorithmManager 调度算法
      A 类：直接处理活动曲线并生成新曲线
      B 类：原交互式算法（需点拾取）入口已禁用，待重新设计交互流程后再启用
  → AlgorithmManager 根据 OutputType
      - AppendCurve: 构造新曲线 → CurveManager::addCurve()
      - ReplaceCurve: 更新已有曲线 → CurveManager::updateCurve()
      - DataOnly: 通过 algorithmResultReady 信号交回结果
  → HistoryManager 记录命令，更新撤销/重做状态
```

### 撤销 / 重做 与可见性同步
- `HistoryManager::instance()` 维护命令栈，`MainController` 通过信号联动菜单/工具栏。
- `CurveManager` 发射 `curveRemoved`、`curveDataChanged`、`curvesCleared` 等信号，`ProjectTreeManager` 与 `CurveViewController` 监听并同步 UI。
- `ChartView` 负责曲线命中与高亮，`CurveViewController` 负责转换勾选状态为可见性并触发轴重算。

## 开发约束与注意事项
- 严格遵守分层依赖：UI → Application → Domain → Infrastructure。下层绝不能依赖上层，跨层交流使用信号或接口。
- 变更 `ThermalCurve` 时保持 `rawData` 不可变；所有写入必须作用于 `processedData` 并通过命令模式记录。
- 曲线增删改必须走 `CurveManager`，不得直接操作 `ChartView` 或 `ProjectTreeManager` 内部模型。
- 新增算法时：
  1. 在 `infrastructure/algorithm/` 实现 `IThermalAlgorithm`。
  2. 明确 `InputType` / `OutputType` / `SignalType`。
  3. 在 `AlgorithmManager::registerAlgorithm()` 注册。
  4. 在 `MainWindow` 中添加 UI 操作并连接至 `MainController`。
  5. 若需恢复交互式选点，请先重新设计 `ChartView` 点拾取接口并更新相关控制器。
- UI 层不包含业务逻辑，只发射信号；业务处理统一进入 `MainController` 或对应的 Manager。
- 所有修改曲线数据的操作必须封装为命令（派生自 `ICommand`）并通过 `HistoryManager::executeCommand()` 执行。
- 引入新文件格式需实现 `IFileReader`，并在 `CurveManager::registerDefaultReaders()` 中注册。
- 字符串默认使用 UTF-8；界面文案可用中文，源码注释保持清晰简洁。
- 断言和条件判断策略：对与主程序同生命周期或在构造过程中一次性完成初始化的实例，不要重复作无意义的空指针判断；仅在可能发生生命周期错配、指针可能失效或外部输入驱动的场景使用断言或保护性判断，确保真正需要的边界得到覆盖。

## 调试与排障
- 微分算法调试：在参数中设置 `params["enableDebug"] = true`、`params["halfWin"] = 50`、`params["dt"] = 0.1`，详见 `微分算法/微分算法使用说明.md`。
- 调试日志说明请参考根目录的 `调试日志说明.md`。
- 点拾取相关调试入口已移除，如需重建请先恢复 `ChartView` 交互接口。
- 若图表显示异常，确认 `ProjectTreeManager::curveCheckStateChanged` 与 `ChartView::setCurveVisible` 的连接是否完整。
- Windows 平台下若找不到 `qmake` 或 `mingw32-make`，根据 `Analysis/build-scripts/BUILD.md` 调整 PATH。

## 已知限制与路线图
- 当前仍为单项目模式，重新导入会清空已有曲线。
- 图表平移/缩放交互待进一步打磨（尤其是混合坐标轴场景）。
- 曲线导出、项目保存/加载、归一化、动力学分析等高级功能尚未完成。
- 基线校正已提供首版实现，仍需补齐更多交互提示与结果可视化选项。
- 中长期计划参考 `Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md` 与 `设计文档/曲线交互功能实现计划.md`。

## 参考资料
- 架构概览：`设计文档/01_主架构设计.md`、`设计文档/02_四层架构详解.md`
- 功能说明：`Analysis/功能说明.md`、`Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md`
- 交互设计：`设计文档/曲线交互功能实现计划.md`、`新设计文档/交互类.md`
- 项目树方案：`新设计文档/迁移项目树指导.md`、`新设计文档/优化项目浏览器的对比.md`
- 初始化与配置：`新设计文档/统一初始化.md`、`新设计文档/注释语法.md`
- 算法行为分类：`新设计文档/抽象算法行为类型.md`

## AI 助手工作提示
- 变更前优先使用 `rg`、`ls` 等命令定位代码，避免全仓库盲目搜索。
- 编辑文件时保持 ASCII 兼容，除非原文件已使用 Unicode 字符。
- 遵循命令模式：任何会修改曲线或状态的操作，需要有对应命令类并接入 `HistoryManager`。
- 更新算法或交互逻辑时，同时检查 `MainWindow`、`MainController`、`AlgorithmManager`、`ProjectTreeManager`、`CurveViewController` 的联动是否需要同步修改。
- 若引入新依赖或大改结构，请先更新设计文档并在回复中说明影响面。
- 默认使用中文与用户交流，提交前检查文档拼写与格式是否符合仓库风格。
- 确认生存周期，减少if 的使用。
- `MainWindow` 请通过信号向控制器发布事件，控制器在 `ApplicationContext` 中注册这些槽函数；不要重新引入 `attachControllers` 或直接持有控制器指针。

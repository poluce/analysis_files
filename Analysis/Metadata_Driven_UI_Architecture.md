# 架构重构总结：元数据驱动的UI方案 (方案B)

## 1. 问题陈述

在重构之前，软件架构存在以下主要问题：

- **紧密耦合**: UI层（特别是 `Tool` 类）与算法的实现细节紧密耦合。UI代码需要手动创建算法实例并设置其参数，违反了分层架构的原则。
- **可扩展性差**: 每当新增一个算法或修改现有算法的参数时，都必须编写或修改一个与之对应的、特定的UI `Tool` 类，开发效率低下，维护成本高。
- **状态管理复杂**: 对于需要多步骤交互（如在图表上选点）的复杂算法，其交互状态逻辑散落在UI代码中，缺乏统一的管理机制，容易出错。

## 2. 重构目标

本次重构的核心目标是解决上述问题，达成以下效果：

- **完全解耦**: 实现UI层与算法层的完全分离。UI层不应再关心任何算法的实现细节。
- **隔离变化**: 将算法参数、逻辑等方面的变化完全隔离在算法模块内部，实现“即插即用”的算法集成。
- **提升扩展性**: 建立一套可扩展的、通用的算法执行框架，使得添加新算法的流程标准化、简单化。

## 3. 核心理念：元数据驱动的UI

为了实现完全解耦，我们将架构模式从“为每个任务定制UI”演进为“**根据元数据动态生成UI**”。

其核心思想是：算法自己负责“说”出它需要什么参数，而UI层则负责“听懂”这些描述，并动态地、通用地生成所需的交互界面。

## 4. 核心组件解析

新架构由以下几个关键组件构成：

### a. `ParameterDescriptor` (参数描述符)
- **位置**: `src/domain/algorithm/parameter_descriptor.h`
- **职责**: 作为算法与UI之间沟通的“契约”或“语言”。它是一个结构体，详细定义了一个参数的全部元信息，如：
    - `name`: 内部程序名 (e.g., "window_size")
    - `label`: 用于UI显示的标签 (e.g., "窗口大小")
    - `type`: 参数类型 (e.g., `Integer`, `Enum`, `PointsOnChart`)
    - `defaultValue`: 默认值
    - `options`: 其他约束，如最大/最小值、枚举选项列表、所需点的数量等。

### b. `IThermalAlgorithm::getParameterDescriptors()`
- **位置**: `src/domain/algorithm/i_thermal_algorithm.h`
- **职责**: 在算法的基类接口中新增的纯虚函数。它强制所有具体的算法子类都必须实现该方法，返回一个 `ParameterDescriptorList`（参数描述符列表），从而实现“自我描述”的功能。

### c. `GenericAlgorithmDialog` (通用参数对话框)
- **位置**: `src/ui/`
- **职责**: 一个智能的、可复用的对话框。它接收一个参数描述符列表，然后根据描述动态地创建对应的UI控件（如 `QSpinBox`, `QComboBox`, `QLineEdit`），组合成一个参数输入表单。

### d. `GenericAlgorithmTool` (通用交互工具)
- **位置**: `src/ui/`
- **职责**: 整个前台工作流的“总指挥”，一个管理复杂交互的状态机。它负责：
    1.  启动工作流，并根据算法元数据，调用 `GenericAlgorithmDialog` 让用户填写简单参数。
    2.  在对话框关闭后，检查是否还需要更复杂的交互（如 `PointsOnChart`）。
    3.  如果需要，则进入相应的交互模式（如“图上选点”），并收集用户输入。
    4.  在收集完所有参数后，调用后台的 `AlgorithmCoordinator` 执行算法。
    5.  监听后台任务结果并进行后续处理。

## 5. 新工作流程

1.  用户从主菜单点击一个算法功能（如“基线校正”）。
2.  `MainWindow` 捕获信号，激活 `GenericAlgorithmTool`，并告知要执行的算法名称（如 `"BaselineCorrection"`）。
3.  `GenericAlgorithmTool` 根据算法名称获取算法实例，并调用其 `getParameterDescriptors()` 方法。
4.  `GenericAlgorithmTool` 将获取到的参数描述符列表传递给 `GenericAlgorithmDialog` 并显示它。
5.  用户在对话框中填写参数后点击“确定”。
6.  `GenericAlgorithmTool` 收回对话框中的参数，并检查是否还有交互式参数（如 `PointsOnChart`）。
7.  如需要，`GenericAlgorithmTool` 进入“选点模式”，等待并收集用户在图表上的点击坐标。
8.  所有参数收集完毕后，`GenericAlgorithmTool` 将整合好的完整参数集，连同算法名称，一同传递给 `AlgorithmCoordinator`。
9.  `AlgorithmCoordinator` 及后续的后台模块执行计算，流程不变。

## 6. 如何基于新架构添加一个全新的算法

得益于新架构，添加一个全新算法的流程变得极其简单和标准化：

1.  **实现算法逻辑**:
    在 `src/infrastructure/algorithm/` 目录下创建你的算法类（如 `MyNewAlgorithm.cpp/.h`），并继承 `IThermalAlgorithm`。

2.  **实现参数描述**:
    在新算法类中，完整实现 `getParameterDescriptors()` 方法。这是最关键的一步，你需要在这里精确描述你的算法所需要的所有参数，无论是简单的整数，还是复杂的图上选点。

3.  **注册算法**:
    打开 `src/application/algorithm/algorithm_manager.cpp`，在 `AlgorithmManager::init()` 方法中，仿照现有代码，使用 `addAlgorithm(new MyNewAlgorithm());` 来注册你的新算法实例。

4.  **添加UI入口**:
    打开 `src/ui/main_window.cpp`，在 `MainWindow::createAlgorithmMenu()` (或类似的方法) 中，仿照现有代码，创建一个新的 `QAction`。最关键的一步是通过 `action->setData("MyNewAlgorithm");` 将算法的唯一注册名称与这个菜单项绑定。

**完成！** 你不需要编写任何新的UI类或修改任何现有的UI逻辑。通用的 `GenericAlgorithmTool` 会自动处理后续的一切。

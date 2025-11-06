# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 Qt 5.14.2 开发的热分析数据处理工具,支持 TGA(热重分析)、DSC(差示扫描量热)、ARC(加速量热仪)等热分析数据的导入、可视化和分析处理。

## 构建和开发命令

### 编译项目

在 Windows 上使用批处理脚本:
```bash
# 首次编译 Debug 版本
cd Analysis
build.bat

# 重新编译(快速)
rebuild.bat

# 编译 Release 版本
build-release.bat

# 清理编译产物
clean.bat
```

手动编译:
```bash
cd Analysis
mkdir build-debug
cd build-debug
qmake ../Analysis.pro
mingw32-make
```

### 运行程序
```bash
cd Analysis/build-debug/release
Analysis.exe
```

### 开发环境要求
- Qt 5.14.2 或更高版本
- MinGW 7.3.0 编译器
- C++17 标准
- Qt Charts 模块

## 架构设计

项目采用清晰的**四层分层架构**,遵循领域驱动设计(DDD)原则:

### 1. 表示层 (Presentation Layer) - `src/ui/`
- **MainWindow**: 主窗口,管理整体布局(菜单、工具栏、停靠面板),接收预构造的 ChartView 与 ProjectExplorerView,仅负责布局与信号转发
- **ChartView**: 基于 Qt Charts 的图表组件,支持多曲线显示、缩放、交互选择
- **ProjectExplorerView**: 项目浏览器视图,树形结构展示曲线及其衍生关系(模型由 ProjectTreeManager 提供)
- **DataImportWidget**: 数据导入对话框,智能列识别和预览

**控制器 (Controller)** - `src/ui/controller/`
- **MainController**: 主控制器(MVC中的C),协调UI和业务逻辑,处理应用级事件
- **CurveViewController**: 负责协调 CurveManager、ProjectTreeManager、ChartView、ProjectExplorerView 的状态同步

**重要**: UI 层组件只发射信号,不包含业务逻辑。所有业务处理由 Controller 协调。

### 2. 应用层 (Application Layer) - `src/application/`
协调业务流程,连接UI和领域逻辑:

**统一初始化** - `src/application/`
- **ApplicationContext**: 应用上下文,负责统一初始化和管理所有 MVC 实例的构造顺序,确保依赖关系正确

**曲线管理** - `src/application/curve/`
- **CurveManager**: 曲线生命周期管理,提供增删改查,管理活动曲线,发射数据变化信号

**算法服务** - `src/application/algorithm/`
- **AlgorithmManager**: 算法管理器,管理算法注册表,执行算法并创建新曲线,自动设置父子关系
- **AlgorithmContext**: 🆕 算法运行时上下文容器,统一管理算法执行所需的数据(参数、点选结果、曲线引用等),提供时间戳和数据来源追踪
- **AlgorithmCoordinator**: 🆕 算法执行流程协调器,负责调度算法执行流程(参数收集、点选请求、算法执行、结果通知)
- **AlgorithmDescriptor**: 🆕 算法参数定义和元数据描述

**项目管理** - `src/application/project/`
- **ProjectTreeManager**: 项目树管理器,管理树形视图结构,维护曲线的父子关系层级

**历史管理** - `src/application/history/`
- **HistoryManager**: 历史管理器,实现撤销/重做功能(命令模式)
- **ICommand**: 命令接口,定义 `execute()`, `undo()`, `redo()`
- **AlgorithmCommand**: 算法执行命令
- **BaselineCommand**: 基线校正命令
- **AddCurveCommand**: 🆕 添加曲线命令

**关键设计**: 使用信号槽机制实现松耦合,Manager 通过信号通知状态变化。

### 3. 领域层 (Domain Layer) - `src/domain/`
定义核心业务对象和规则,不依赖任何其他层:

#### 数据模型 - `src/domain/model/`
- **ThermalDataPoint**: 数据点结构(温度、时间、测量值、元数据)
- **ThermalCurve**: 核心业务对象,维护原始数据和处理数据的双份数据
  - `rawData`: 加载后不可变(支持撤销恢复)
  - `processedData`: 可修改(用于显示和计算)
  - **双枚举设计**:
    - `InstrumentType`: 仪器类型(TGA/DSC/ARC)
    - `SignalType`: 信号类型(Raw/Derivative/Baseline/PeakArea)
  - **显示样式**:
    - `PlotStyle`: 图表样式(Line/Scatter/Both)
  - 动态单位推断: `getYAxisLabel()` 根据类型自动生成Y轴标签

#### 算法接口 - `src/domain/algorithm/`
- **IThermalAlgorithm**: 算法接口,定义 `process()`, `name()`, `category()`, `getOutputSignalType()`
- **AlgorithmDescriptor**: 算法描述符,定义算法的参数元数据和配置
- **ICommand**: 命令模式接口,定义 `execute()`, `undo()`, `redo()`

### 4. 基础设施层 (Infrastructure Layer) - `src/infrastructure/`
提供技术支持,不包含业务逻辑:

#### 文件IO - `src/infrastructure/io/`
- **IFileReader**: 文件读取接口
- **TextFileReader**: 文本文件读取器(支持 .txt, .csv)

#### 算法实现 - `src/infrastructure/algorithm/`
- **DifferentiationAlgorithm**: 微分算法(大窗口平滑中心差分法)
- **IntegrationAlgorithm**: 积分算法(梯形法则)
- **MovingAverageFilterAlgorithm**: 移动平均滤波算法
- **BaselineCorrectionAlgorithm**: 基线校正算法

## 核心设计模式和原则

### 1. 双枚举分离设计
传统热分析软件常用单一枚举(TGA/DSC/DTG),导致语义混乱。本项目分离为:
```cpp
enum class InstrumentType { TGA, DSC, ARC };              // 仪器类型
enum class SignalType { Raw, Derivative, Baseline, PeakArea }; // 信号处理状态
enum class PlotStyle { Line, Scatter, Both };             // 图表显示样式
```

**优势**:
- 语义清晰,概念不混淆
- 易于扩展新仪器类型或处理方法
- 算法通用化,无需针对特定仪器编写特殊逻辑
- 支持多种信号类型(原始、微分、基线校正、峰面积)

### 2. 数据不可变性
曲线维护原始数据和处理数据两份:
- `rawData`: 只读,不可变,支持撤销恢复
- `processedData`: 可修改,用于显示和计算

### 3. 父子关系追踪
算法生成的曲线自动关联到父曲线:
```cpp
newCurve.setParentId(parentCurve->id());
```
在 ProjectExplorer 中以树形结构展示数据血缘关系。

### 4. 命令模式实现撤销/重做
所有修改数据的操作封装为 Command:
- HistoryManager 维护 undoStack 和 redoStack
- 执行新操作时清空 redoStack
- 限制历史栈深度防止内存溢出

### 5. 信号槽通信机制
- UI → Controller: 用户操作信号
- Manager → Controller: 状态变化通知
- Controller → UI: 更新界面

### 6. 算法通用化设计
所有算法适用于所有仪器类型:
```cpp
// 微分算法
SignalType getOutputSignalType(SignalType input) {
    return (input == Raw) ? Derivative : input;
}
// 对 TGA/DSC/ARC 均适用,无需特殊处理
```

### 7. AlgorithmContext 上下文模式 🆕
统一的算法运行时数据容器,解决算法执行时的数据传递和追踪问题:
```cpp
// 上下文值结构
struct ContextValue {
    QVariant value;       // 实际值
    QDateTime timestamp;  // 更新时间戳
    QString source;       // 数据来源(UI/Algorithm/File)
};

class AlgorithmContext : public QObject {
    QMap<QString, ContextValue> m_data;  // 键值对存储

    // 类型安全的访问
    template<typename T>
    T get(const QString& key, const T& defaultValue = T());

    // 设置值并记录来源
    void set(const QString& key, const QVariant& value, const QString& source);

signals:
    void valueChanged(const QString& key);  // 值变化信号
};
```

**优势**:
- 统一数据管理: 所有算法参数、点选结果、曲线引用等都存储在上下文中
- 时间戳追踪: 每个值都有更新时间,便于调试和审计
- 来源追踪: 记录数据来自 UI 输入、算法计算还是文件加载
- 信号通知: 值变化时自动发出信号,支持响应式编程
- 类型安全: 模板方法提供类型安全的访问接口
- 批量操作: 支持 `merge()` 合并多个上下文

**数据分类** (详见 `新设计文档/AlgorithmContext_数据清单.md`):
- 基础曲线数据(5项): 活动曲线、输入曲线、输出曲线等
- 通用算法参数(10项): 窗口大小、阈值、步长等
- 峰面积参数(8项): 积分范围、基线类型、起止点等
- 基线校正参数(6项): 校正类型、多项式次数、锚点等
- 15个类别,共60+数据项

### 8. 统一初始化模式 🆕
使用 ApplicationContext 统一管理所有 MVC 实例的构造顺序:
```cpp
class ApplicationContext {
    // 按正确的依赖顺序创建实例
    void initialize() {
        // 1. 核心数据管理
        m_curveManager = new CurveManager();

        // 2. 算法服务
        m_algorithmManager = new AlgorithmManager(m_curveManager);
        m_algorithmContext = new AlgorithmContext();
        m_algorithmCoordinator = new AlgorithmCoordinator(...);

        // 3. 项目管理
        m_projectTreeManager = new ProjectTreeManager(m_curveManager);

        // 4. UI 组件
        m_mainWindow = new MainWindow(...);
        m_chartView = new ChartView(...);
        m_projectExplorerView = new ProjectExplorerView(...);

        // 5. 控制器
        m_mainController = new MainController(...);
        m_curveViewController = new CurveViewController(...);
    }
};
```

**优势**:
- 依赖注入: 清晰的依赖关系,避免循环依赖
- 可测试性: 易于替换为 Mock 对象进行单元测试
- 生命周期管理: 统一管理对象的创建和销毁
- 配置集中: 所有初始化逻辑集中在一处

## 重要的数据流向

### 导入数据流
```
用户选择文件
  → DataImportWidget (预览)
  → TextFileReader (读取)
  → ThermalCurve (创建)
  → CurveManager (管理)
  → 信号: curveAdded
  → ├─ ProjectTreeManager (更新树)
    └─ ChartView (显示图表)
```

### 算法执行流 (新架构) 🆕
```
用户选择算法
  → MainWindow (菜单触发)
  → MainController (协调)
  → AlgorithmCoordinator (流程编排)
  → ├─ 创建 AlgorithmContext (上下文)
    ├─ 设置活动曲线到上下文
    ├─ 触发参数收集对话框 (如需要)
    │   → 用户输入参数 → 保存到上下文
    ├─ 请求点选 (如需要)
    │   → 用户在图表上点选 → 保存到上下文
    ├─ AlgorithmManager::execute(context)
    │   → 从上下文获取参数和曲线
    │   → 执行算法处理
    │   → 创建新曲线
    │   → 设置 parentId 和 signalType
    │   → 添加到 CurveManager
    └─ 发出算法完成信号
  → CurveManager 发出 curveAdded 信号
  → ├─ ProjectTreeManager (添加为子节点)
    ├─ ChartView (显示新曲线)
    └─ CurveViewController (同步状态)
```

**关键改进**:
- 使用 AlgorithmContext 统一管理所有算法数据
- AlgorithmCoordinator 负责流程编排,解耦算法执行和UI交互
- 支持灵活的参数收集和点选流程
- 所有数据变化都有时间戳和来源追踪

## 编码约定和注意事项

### 层间依赖规则
- ✅ 上层可以依赖下层
- ❌ 下层不能依赖上层
- ✅ 同层之间通过接口依赖

### 添加新算法
1. 在 `Analysis/src/infrastructure/algorithm/` 实现 `IThermalAlgorithm` 接口
2. 实现 `process()` 方法(核心算法逻辑)
3. 实现 `getOutputSignalType()` 定义信号类型转换规则
4. 在 `AlgorithmService` 中注册算法
5. 在 `MainWindow` 中添加菜单项并连接信号

### 添加新文件格式
1. 在 `Analysis/src/infrastructure/io/` 实现 `IFileReader` 接口
2. 实现 `canRead()` 和 `read()` 方法
3. 在 `CurveManager::registerDefaultReaders()` 中注册

### 修改数据模型
- 修改 `ThermalCurve` 时注意保持原始数据不可变性
- 添加新的 `InstrumentType` 时需更新 `getYAxisLabel()` 方法
- 添加新的 `SignalType` 时需更新算法的 `getOutputSignalType()` 方法

### UTF-8 编码支持
项目配置了 UTF-8 支持:
```qmake
win32:msvc: QMAKE_CXXFLAGS += /utf-8
win32:g++:  QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
```
注释和字符串可以使用中文。

## 调试和诊断

### 微分算法调试
微分算法支持调试输出:
```cpp
params["enableDebug"] = true;  // 启用调试输出
params["halfWin"] = 50;        // 半窗口大小
params["dt"] = 0.1;            // 虚拟时间步长
```
调试信息会输出到控制台,包括窗口大小、数据点数等。

详细说明见: `微分算法/微分算法使用说明.md`

### 调试日志说明
项目包含调试日志说明文档: `调试日志说明.md`

## 已知限制和后续计划

### 最近完成 ✅
- ✅ 统一初始化机制 (ApplicationContext)
- ✅ 算法上下文容器 (AlgorithmContext)
- ✅ 算法流程协调器 (AlgorithmCoordinator)
- ✅ 项目树管理器 (ProjectTreeManager 替换 CurveTreeModel)
- ✅ 基线校正算法实现
- ✅ 命令模式框架 (AddCurveCommand 等)
- ✅ 双枚举扩展 (SignalType 支持 Baseline 和 PeakArea)

### 当前限制
1. 仅支持单项目模式(导入新数据会清空已有曲线)
2. 图表缩放和平移功能待完善
3. 算法参数收集对话框尚未实现
4. 点选交互功能尚未完整实现
5. 撤销/重做功能框架已建立,但集成尚未完成
6. 动力学计算功能待实现
7. 项目保存/加载功能待实现
8. 曲线导出功能待实现

### 开发计划 (详见 `Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md`)

**短期 (Phase 1 - 命令模式和历史管理)**:
- 完善 HistoryManager 和命令模式集成
- 实现所有修改操作的 Command 封装
- 完成撤销/重做功能测试

**中期 (Phase 2 - 项目管理)**:
- ProjectManager 多项目支持
- 项目保存/加载功能
- 曲线导出和图表导出

**中期 (Phase 3 - 算法交互)**:
- 算法参数收集对话框 (基于 AlgorithmDescriptor)
- 点选交互功能完善 (基于 AlgorithmContext)
- 峰值检测、面积计算、归一化

**长期 (Phase 4 - 高级功能)**:
- 动力学分析(Kissinger、Ozawa、Flynn-Wall-Ozawa 等)
- 数据库支持
- 批处理和脚本系统
- 插件系统

## 相关文档

### 架构设计文档
- **主架构设计**: `设计文档/01_主架构设计.md`
- **四层架构详解**: `设计文档/02_四层架构详解.md`
- **架构案例分析**: `设计文档/03_架构案例分析.md`
- **架构优化计划**: `Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md` (四阶段路线图)

### 功能设计文档
- **功能说明**: `Analysis/功能说明.md` (v0.1.0-alpha)
- **UI设计文档**: `设计文档/UI 设计文档 — 仿 AKTS-Thermokinetics.md`
- **撤销重做功能**: `设计文档/撤销重做功能实现文档.md`
- **曲线交互功能**: `设计文档/曲线交互功能实现计划.md`

### 最新设计文档 🆕 (`新设计文档/`)
- **AlgorithmContext 类设计**: `新设计文档/📘 AlgorithmContext 类设计文档.md` (核心上下文容器设计)
- **AlgorithmContext 数据清单**: `新设计文档/AlgorithmContext_数据清单.md` (60+ 数据项, 15 个类别)
- **MVC 层次划分总览**: `新设计文档/一、MVC 层次划分总览.md` (最新架构分层)
- **统一初始化**: `新设计文档/统一初始化.md` (ApplicationContext 设计)
- **交互类**: `新设计文档/交互类.md`
- **抽象算法行为类型**: `新设计文档/抽象算法行为类型.md`

### 算法文档
- **微分算法使用说明**: `微分算法/微分算法使用说明.md` (大窗口平滑中心差分法)

### 其他文档
- **信号流设计**: `设计图/信号流设计.md`

## 代码质量要求

- 遵循 C++17 标准
- 保持层次分离,不违反依赖规则
- UI 层不包含业务逻辑
- 算法实现应该通用化,支持所有仪器类型
- 修改数据的操作应封装为 Command 以支持撤销
- 使用信号槽机制实现松耦合
- 注释应清晰说明类的职责和方法的用途

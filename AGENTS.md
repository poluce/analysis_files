# AGENTS.md

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
负责用户界面显示和交互,不包含业务逻辑:
- **MainWindow**: 主窗口,管理整体布局(菜单、工具栏、停靠面板)
- **PlotWidget**: 基于 Qt Charts 的图表组件,支持多曲线显示、缩放、交互选择
- **ProjectExplorer**: 项目浏览器,树形结构展示曲线及其衍生关系
- **CurveTreeModel**: 自定义树模型,支持父子关系和 checkbox 控制显示
- **DataImportWidget**: 数据导入对话框,智能列识别和预览
- **PeakAreaDialog**: 峰面积计算对话框
- **MainController**: 主控制器(MVC中的C),协调UI和业务逻辑

**重要**: UI 层组件只发射信号,不包含业务逻辑。所有业务处理由 Controller 协调。

### 2. 应用层 (Application Layer) - `src/application/`
协调业务流程,连接UI和领域逻辑:
- **CurveManager**: 曲线生命周期管理,提供增删改查,管理活动曲线,发射数据变化信号
- **AlgorithmService**: 算法服务,管理算法注册表,执行算法并创建新曲线,自动设置父子关系
- **HistoryManager**: 历史管理,实现撤销/重做功能(命令模式)
- **AlgorithmCommand/BaselineCommand**: 具体的命令实现

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
    - `SignalType`: 信号类型(Raw/Derivative)
  - 动态单位推断: `getYAxisLabel()` 根据类型自动生成Y轴标签

#### 算法接口 - `src/domain/algorithm/`
- **IThermalAlgorithm**: 算法接口,定义 `process()`, `name()`, `category()`, `getOutputSignalType()`
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

## 核心设计模式和原则

### 1. 双枚举分离设计
传统热分析软件常用单一枚举(TGA/DSC/DTG),导致语义混乱。本项目分离为:
```cpp
enum class InstrumentType { TGA, DSC, ARC };  // 仪器类型
enum class SignalType { Raw, Derivative };     // 信号处理状态
```

**优势**:
- 语义清晰,概念不混淆
- 易于扩展新仪器类型或处理方法
- 算法通用化,无需针对特定仪器编写特殊逻辑

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

## 重要的数据流向

### 导入数据流
```
用户选择文件
  → DataImportWidget (预览)
  → TextFileReader (读取)
  → ThermalCurve (创建)
  → CurveManager (管理)
  → 信号: curveAdded
  → ├─ CurveTreeModel (更新树)
    └─ PlotWidget (显示图表)
```

### 算法执行流
```
用户选择算法
  → MainWindow (触发)
  → MainController (协调)
  → AlgorithmService (执行)
  → ├─ 获取活动曲线
    ├─ 执行算法处理
    ├─ 创建新曲线
    ├─ 设置 parentId
    ├─ 推导曲线类型(signalType)
    └─ 添加到 CurveManager
  → 信号: curveAdded
  → ├─ CurveTreeModel (添加为子节点)
    └─ PlotWidget (显示新曲线)
```

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

### 当前限制
1. 仅支持单项目模式(导入新数据会清空已有曲线)
2. 图表缩放和平移功能待完善
3. 基线校正、归一化等高级功能待实现
4. 动力学计算功能待实现
5. 项目保存/加载功能待实现
6. 曲线导出功能待实现

### 开发计划
- 短期: 项目保存/加载、曲线导出、图表导出、撤销/重做完善
- 中期: 基线校正、归一化、峰值检测、面积计算、多项目支持
- 长期: 动力学分析(Kissinger、Ozawa等)、数据库支持、批处理、插件系统

## 相关文档

- **架构文档**: `设计文档/01_主架构设计.md`
- **四层架构详解**: `设计文档/02_四层架构详解.md`
- **架构案例分析**: `设计文档/03_架构案例分析.md`
- **功能说明**: `Analysis/功能说明.md`
- **架构优化计划**: `Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md`
- **撤销重做功能**: `设计文档/撤销重做功能实现文档.md`
- **曲线交互功能**: `设计文档/曲线交互功能实现计划.md`
- **UI设计文档**: `设计文档/UI 设计文档 — 仿 AKTS-Thermokinetics .md`
- **新设计文档**: `新设计文档/抽象算法行为类型.md`

## 代码质量要求

- 遵循 C++17 标准
- 保持层次分离,不违反依赖规则
- UI 层不包含业务逻辑
- 算法实现应该通用化,支持所有仪器类型
- 修改数据的操作应封装为 Command 以支持撤销
- 使用信号槽机制实现松耦合
- 注释应清晰说明类的职责和方法的用途

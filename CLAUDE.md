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
- **ChartView**: 🆕 基于 Qt Charts 的图表组件,支持多曲线显示、缩放、交互选择
  - **活动算法状态机**: 管理用户与算法的交互流程
    - `InteractionState`: 交互状态枚举 (Idle/WaitingForPoints/PointsCompleted/Executing)
    - `ActiveAlgorithmInfo`: 当前活动算法信息 (名称/显示名/所需点数/提示)
    - `startAlgorithmInteraction()`: 启动算法交互,进入等待选点状态
    - `cancelAlgorithmInteraction()`: 取消当前交互,回到空闲状态
    - `algorithmInteractionCompleted(algorithmName, points)`: 当用户完成选点后自动发出,触发算法执行
  - **交互模式**: InteractionMode (View/Pick)
  - **横轴模式**: XAxisMode (Temperature/Time)
  - **交互功能**: 右键拖动平移、Ctrl+滚轮缩放、多Y轴支持
- **FloatingLabel**: 🆕 浮动标签组件,可拖动、可缩放的文本标签
  - **锚定模式**: DataAnchored (跟随数据) / ViewAnchored (固定HUD)
  - **交互功能**: 拖动移动、Ctrl+滚轮缩放、关闭按钮、锁定/解锁
  - **应用场景**: 峰面积数值显示、测量结果、注释标签
- **TrapezoidMeasureTool**: 🆕 直角梯形测量工具
  - **功能**: 测量曲线上两点之间的垂直高度差（质量损失）
  - **特性**: 可拖动端点、自动吸附曲线、实时显示测量值
  - **支持**: 横轴切换同步（温度/时间模式）
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
- **AlgorithmCoordinator**: 🆕🧪 算法执行流程协调器,负责调度算法执行流程(参数收集、点选请求、算法执行、结果通知)
  - ⚠️ **实验性组件**: 主要用于未来的交互式算法（基线绘制、峰面积计算等）
  - 当前简单算法（微分、积分、移动平均）通过此组件统一调度
  - 提供扩展性：支持参数对话框、点选交互、多步骤流程
- **AlgorithmContext**: 🆕🧪 算法运行时上下文容器,统一管理算法执行所需的数据(参数、点选结果、曲线引用等),提供时间戳和数据来源追踪
- **AlgorithmDescriptor**: 🆕🧪 算法参数定义和元数据描述

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
    - `PlotStyle`: 图表样式(Line/Scatter/Area)
  - **曲线属性**: 🆕
    - `isAuxiliaryCurve`: 是否为辅助曲线（决定Y轴继承策略）
    - `isStronglyBound`: 是否强绑定（决定树中显示策略）
    - `isMainCurve`: 是否为主曲线（数据源，不可删除）
  - 动态单位推断: `getYAxisLabel()` 根据类型自动生成Y轴标签

#### 算法接口 - `src/domain/algorithm/`
- **IThermalAlgorithm**: 算法接口,定义核心方法
  - `executeWithContext(AlgorithmContext*)`: 上下文驱动执行（纯虚函数）
  - `prepareContext(AlgorithmContext*)`: 数据完整性验证（两阶段执行-阶段1）
  - `name()`, `displayName()`, `category()`: 基本信息
  - `getOutputSignalType(SignalType)`: 信号类型转换规则
  - `inputType()`: 🆕 输入类型（None/PointSelection/DualCurve/Intersect/MultiPoint）
  - `outputType()`: 🆕 输出类型（Curve/Marker/Region/ScalarValue/Composite）
  - `isAuxiliaryCurve()`: 🆕 声明是否为辅助曲线
  - `isStronglyBound()`: 🆕 声明是否强绑定
- **AlgorithmDescriptor**: 算法描述符,定义算法的参数元数据和交互配置
- **AlgorithmResult**: 🆕 统一输出容器,结构化封装算法执行结果
  - `ResultType`: 结果类型枚举（Curve/Marker/Region/ScalarValue/Composite）
  - 支持多种输出：曲线、标注点、区域、数值、混合
  - 内置成功/失败状态和错误处理
  - 元数据存储（area, peakHeight, unit等）
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
- **PeakAreaAlgorithm**: 🆕 峰面积计算算法(梯形积分法)
  - **类型**: B类算法（交互式，需要用户选点）
  - **输入**: 用户选择2个点定义积分范围
  - **输出**: Composite (数值 + FloatingLabel)
  - **应用**: TGA质量损失、DSC焓变计算

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

### 8. AlgorithmResult 统一输出容器 🆕✨

**设计目标**: 创建统一的结果容器类，封装所有算法输出类型，提供结构化、类型安全的结果访问接口。

**核心理念**:
```
算法输入：AlgorithmContext (拉取数据)
           ↓
      算法执行逻辑
           ↓
算法输出：AlgorithmResult (结构化结果)
```

**结果类型**:
```cpp
enum class ResultType {
    Curve,          // 输出曲线（如微分、积分、滤波后的曲线）
    Marker,         // 标注点（如峰值点、外推点、拐点）
    Region,         // 区域（如峰面积、积分区域）
    ScalarValue,    // 单个数值（如外推温度、峰高、斜率）
    Composite       // 混合结果（包含多种类型）
};
```

**关键特性**:
- ✅ **结构化**: 所有输出封装在单一对象中，清晰明确
- ✅ **类型安全**: 强类型字段，编译时检查
- ✅ **多输出支持**: `Composite` 类型支持同时返回多种结果
- ✅ **易于解析**: 专门的处理方法针对不同结果类型
- ✅ **可扩展**: 通过 `meta` 字段灵活扩展
- ✅ **错误处理**: 内置成功/失败状态和错误信息
- ✅ **可追溯**: 包含时间戳和来源信息

**使用示例**:
```cpp
// 简单曲线输出（微分）
AlgorithmResult result = AlgorithmResult::success("differentiation", curveId, ResultType::Curve);
result.setCurve(outputCurve);
result.setMeta("unit", "mg/min");
return result;

// 混合输出（峰面积）
AlgorithmResult result = AlgorithmResult::success("peakArea", curveId, ResultType::Composite);
result.addCurve(baselineCurve);        // 添加基线曲线
result.setArea(area, "J/g");           // 设置面积数值
result.addRegion(areaPolygon);         // 添加区域（用于阴影填充）
result.addMarker(startPoint, "起点"); // 添加标注点
return result;
```

详见: `新设计文档/统一输出算法_AlgorithmResult设计.md`

### 9. 纯上下文驱动执行模式 🆕✨

**核心理念**：算法采用**拉取 (Pull)** 模式从上下文获取数据，彻底消除参数传递。

#### 设计原则

本项目采用**纯上下文驱动**设计，**无向后兼容层**：

❌ **已移除**：
- `process(QVector<ThermalDataPoint>)` - 旧的数据处理接口
- `setParameter(QString, QVariant)` - 旧的参数设置方法
- `execute(QVariantMap)` - 旧的参数推送接口
- `executeWithInputs(QVariantMap)` - AlgorithmManager 旧方法

✅ **唯一接口**：
- `executeWithContext(AlgorithmContext*)` - 算法从上下文拉取数据
- `prepareContext(AlgorithmContext*)` - 注入默认参数（可选）

#### 算法实现模板

```cpp
class XXXAlgorithm : public IThermalAlgorithm {
public:
    // 核心接口方法
    QString name() const override;
    QString displayName() const override;
    QString category() const override;
    SignalType getOutputSignalType(SignalType) const override;
    AlgorithmDescriptor descriptor() const override;

    // 上下文驱动执行接口
    void prepareContext(AlgorithmContext* context) override {
        // 注入默认参数（如果上下文中不存在）
        if (!context->contains("param.windowSize")) {
            context->setValue("param.windowSize", m_defaultWindowSize);
        }
    }

    QVariant executeWithContext(AlgorithmContext* context) override {
        // 1. 验证上下文
        if (!context) return QVariant();

        // 2. 拉取曲线
        auto curve = context->get<ThermalCurve*>("activeCurve");
        if (!curve.has_value() || !curve.value()) return QVariant();

        // 3. 拉取参数（value_or 提供 fallback）
        int windowSize = context->get<int>("param.windowSize").value_or(m_defaultWindowSize);

        // 4. 获取输入数据
        const auto& inputData = curve.value()->getProcessedData();

        // 5. 执行核心算法逻辑
        QVector<ThermalDataPoint> outputData;
        // ... 算法实现 ...

        // 6. 返回结果
        return QVariant::fromValue(outputData);
    }

private:
    int m_defaultWindowSize = 50;  // 仅作为 fallback
};
```

#### 已迁移的算法（100%完成）

✅ **DifferentiationAlgorithm** - 微分（大窗口平滑中心差分法）
✅ **IntegrationAlgorithm** - 积分（梯形法则）
✅ **MovingAverageFilterAlgorithm** - 移动平均滤波
✅ **BaselineCorrectionAlgorithm** - 基线校正（需要点选交互）

所有算法均已完全迁移到纯上下文驱动模式，**无向后兼容层**。

#### 核心优势

- ✅ **零参数传递**: 不再需要在调用点构造 QVariantMap
- ✅ **单一数据源**: AlgorithmContext 是唯一的数据容器
- ✅ **类型安全**: `context->get<T>()` 提供编译时类型检查
- ✅ **默认值支持**: `value_or(default)` 简化参数处理
- ✅ **调用顺序无关**: 数据在上下文中全局可用
- ✅ **易于扩展**: 添加新参数无需修改函数签名
- ✅ **架构统一**: 所有算法遵循相同的执行模式

#### 两阶段执行机制 🔐

**问题**：算法被选中时，必需的数据（如图表上的选点）可能不完整。如果算法在数据不完整时执行，会导致结果不准确或资源浪费。

**解决方案**：采用两阶段执行确保算法只在接收到完整数据后才开始计算。

| 阶段 | 方法 | 职责 | 数据状态 | 返回值 |
|------|------|------|----------|--------|
| **阶段1** | `prepareContext()` | 验证数据完整性，注入默认参数 | 可能不完整 | `bool` - 是否就绪 |
| **阶段2** | `executeWithContext()` | 使用完整数据执行计算 | 完整且最终 | `QVariant` - 计算结果 |

**阶段1示例（prepareContext）**：
```cpp
bool XXXAlgorithm::prepareContext(AlgorithmContext* context) {
    // 1. 验证上下文本身
    if (!context) return false;

    // 2. 验证必需数据（activeCurve）
    auto curve = context->get<ThermalCurve*>("activeCurve");
    if (!curve.has_value() || !curve.value()) return false;

    // 3. [交互算法] 验证用户交互数据（如选点）
    auto points = context->get<QVector<QPointF>>("selectedPoints");
    if (!points.has_value() || points.value().size() < 2) {
        return false;  // 数据不完整，等待用户选点
    }

    // 4. 注入默认参数
    if (!context->contains("param.xxx")) {
        context->setValue("param.xxx", m_default);
    }

    return true;  // 数据完整，可以执行
}
```

**核心优势**：
- ✅ **防止过早执行**：算法只在数据完整时才执行计算
- ✅ **明确的状态检查**：`prepareContext()` 返回值明确表示就绪状态
- ✅ **保证数据一致性**：算法接收到的一定是用户完整交互后的数据
- ✅ **易于调试**：清晰的日志输出指示数据缺失原因

#### 执行流程

```
AlgorithmCoordinator::executeAlgorithm()
  → 清空上下文旧数据
  → context->setValue("activeCurve", curve)
  → context->setValue("param.*", values)
  → context->setValue("selectedPoints", points)  // 如需要
  ↓
AlgorithmManager::executeWithContext(name, context)
  ↓
  【阶段1：数据完整性验证】
  → bool isReady = algorithm->prepareContext(context)
  → if (!isReady) {
       qWarning() << "数据不完整，无法执行";
       return;  // 提前终止，等待用户输入
    }
  ↓
  【阶段2：执行计算】
  → QVariant result = algorithm->executeWithContext(context)
    → auto curve = context->get<ThermalCurve*>("activeCurve")
    → int param = context->get<int>("param.xxx").value_or(default)
    → 执行核心算法逻辑
    → return QVariant::fromValue(result)
  ↓
  → handleAlgorithmResult(...)  // 处理结果
  → emit algorithmResultReady(...)
```

### 10. 统一初始化模式 🆕
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

### 算法执行流 (上下文驱动架构) 🆕✨
```
用户选择算法
  → MainWindow (菜单触发)
  → MainController (协调)
  → AlgorithmCoordinator::handleAlgorithmTriggered (流程编排)
  → ├─ 获取活动曲线
    ├─ 触发参数收集对话框 (如需要)
    │   → 用户输入参数 → 保存到 m_pending
    ├─ 请求点选 (如需要)
    │   → 用户在图表上点选 → 保存到 m_pending
    └─ AlgorithmCoordinator::executeAlgorithm
      → 清空上下文中的旧数据
      → 设置 activeCurve 到上下文
      → 设置参数到上下文 (param.*)
      → 设置选择的点到上下文 (selectedPoints)
      → AlgorithmManager::executeWithContext(name, context)
        ├─ 验证上下文中的 activeCurve
        ├─ 调用 algorithm->prepareContext(context) [注入默认参数]
        └─ 调用 algorithm->executeWithContext(context)
          → 算法从上下文拉取 activeCurve
          → 算法从上下文拉取参数 (param.*)
          → 执行核心算法逻辑
          → 返回结果
        → handleAlgorithmResult(algorithm, curve, result)
          → 根据 outputType 处理结果
          → 创建新曲线 (OutputType::Curve)
          → 设置 parentId 和 signalType
          → 添加到 CurveManager (通过 HistoryManager)
        → 发出 algorithmResultReady 信号
  → CurveManager 发出 curveAdded 信号
  → ├─ ProjectTreeManager (添加为子节点)
    ├─ ChartView (显示新曲线)
    └─ CurveViewController (同步状态)
```

**关键特性**:
- ✅ **上下文驱动**: 算法从 AlgorithmContext 拉取数据，避免繁琐的参数传递
- ✅ **单一数据源**: 所有运行时数据集中在上下文中管理
- ✅ **流程编排**: AlgorithmCoordinator 负责交互流程和数据收集
- ✅ **类型安全**: `context->get<T>()` 提供类型安全的访问
- ✅ **默认参数**: `prepareContext()` 注入算法默认值
- ✅ **历史追踪**: 所有数据变化都有时间戳和来源记录

### 🆕 活动算法状态机 (ChartView 交互管理) ✨

**设计理念**: ChartView 维护"当前活动算法"状态，跟踪用户交互进度，当交互完成时自动触发算法执行。

**状态机流程**:
```
用户选择算法 (如"基线校正")
  → MainController::onAlgorithmRequested()
  → AlgorithmCoordinator::handleAlgorithmTriggered()
  → 发出 requestPointSelection(algorithmName, curveId, requiredPoints, hint)
  → MainController::onCoordinatorRequestPointSelection()
  → ChartView::startAlgorithmInteraction(algorithmName, displayName, requiredPoints, hint)
    ├─ 设置 m_activeAlgorithm (记录当前算法信息)
    ├─ 状态转换: Idle → WaitingForPoints
    ├─ 发出 interactionStateChanged(WaitingForPoints)
    └─ 切换到 Pick 模式 (十字光标)

用户在图表上点击选点
  → ChartView::handlePointSelectionClick()
    ├─ 检查 m_activeAlgorithm.isValid() && state == WaitingForPoints
    ├─ 添加点到 m_selectedPoints
    ├─ 检查是否收集够所需点数 (size >= requiredPointCount)
    └─ 如果完成:
      ├─ 状态转换: WaitingForPoints → PointsCompleted
      ├─ 发出 interactionStateChanged(PointsCompleted)
      ├─ 发出 algorithmInteractionCompleted(algorithmName, selectedPoints) ⭐ 自动触发执行
      └─ 切换回 View 模式

MainController 接收信号
  → 连接 ChartView::algorithmInteractionCompleted()
  → 调用 AlgorithmCoordinator::handlePointSelectionResult(points)
  → 算法执行流程继续 (见上方"算法执行流")
```

**核心优势**:
- ✅ **状态追踪**: 始终知道哪个算法正在等待交互，进度如何
- ✅ **自动执行**: 用户完成选点后无需手动点击"执行"按钮
- ✅ **清晰反馈**: 可显示提示文本 "已选 1/2 点，请继续选择"
- ✅ **可取消性**: `cancelAlgorithmInteraction()` 可随时中止交互
- ✅ **松耦合**: ChartView 只负责交互管理，不知道算法具体逻辑

**集成要点** (✅ 已完成):
1. ✅ MainController 连接了 `ChartView::algorithmInteractionCompleted()` 信号（在 `setPlotWidget()` 中）
2. ✅ 修改了 `onCoordinatorRequestPointSelection()` 调用 `ChartView::startAlgorithmInteraction()`
3. ✅ 信号处理器自动调用 `coordinator->handlePointSelectionResult(points)` 继续执行
4. ✅ 添加了 `interactionStateChanged` 信号监听（用于调试和未来的UI反馈）

**代码位置**:
- 信号连接: `main_controller.cpp:62-95` (setPlotWidget 方法)
- 状态机启动: `main_controller.cpp:266-296` (onCoordinatorRequestPointSelection 方法)
- 旧路径标记: `main_controller.cpp:257-270` (onPointsPickedFromView 保留用于向后兼容)

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

## AlgorithmCoordinator 使用策略 🧪

### 设计理念

**AlgorithmCoordinator** 是为未来的**交互式算法**设计的协调器，采用实验性架构。

### 当前状态

**✅ 已统一使用**:
- 所有算法执行统一通过 `AlgorithmCoordinator::handleAlgorithmTriggered()`
- MainController 中移除了旧的直接调用路径
- 确保架构一致性，避免双重路径

**⚠️ 实验性质**:
- 当前简单算法（微分、积分、移动平均）**不需要**复杂的参数对话框和点选交互
- AlgorithmCoordinator 为这些简单算法提供了统一入口，但其核心价值尚未完全体现

### 何时体现价值

AlgorithmCoordinator 将在以下场景中发挥关键作用：

1. **基线校正** (Phase 3)
   - 需要用户在图表上点选基线点
   - 利用 `requestPointSelection()` 信号
   - 使用 PendingRequest 管理点选状态

2. **峰面积计算** (Phase 3)
   - 需要参数对话框收集积分范围、基线类型等
   - 需要点选峰的起止点
   - 利用 ParameterThenPoint 流程

3. **多步骤算法** (Phase 4)
   - 如动力学分析需要多次用户交互
   - 状态机管理复杂流程

### 技术债务说明

**保留原因**:
- ✅ 为即将实现的基线和峰面积功能做准备
- ✅ 避免重复设计和重构
- ✅ 架构清晰，扩展性强

**成本**:
- 约 540 行代码 (AlgorithmCoordinator + Context + Descriptor)
- 当前简单算法未充分利用其能力

**建议**:
- 保持现状，继续使用统一架构
- Phase 3 实现交互功能时验证设计
- 如发现设计问题，及时调整

## 已知限制和后续计划

### 最近完成 ✅
- ✅ 统一初始化机制 (ApplicationContext)
- ✅ 算法上下文容器 (AlgorithmContext)
- ✅ 算法流程协调器 (AlgorithmCoordinator)
- ✅ 项目树管理器 (ProjectTreeManager 替换 CurveTreeModel)
- ✅ 基线校正算法实现
- ✅ 命令模式框架 (AddCurveCommand 等)
- ✅ 双枚举扩展 (SignalType 支持 Baseline 和 PeakArea)
- ✅ 代码重构优化 (消除 150+ 行冗余代码，统一技术路线)
- ✅ 性能优化 (HistoryManager O(n)→O(1)，AlgorithmCoordinator O(n²)→O(n))
- ✅ 架构简化 (统一使用 AlgorithmCoordinator，移除双重路径)
- ✅ **纯上下文驱动执行模式** (executeWithContext, prepareContext)
  - 彻底移除旧接口和向后兼容层（无 process/setParameter/execute）
  - IThermalAlgorithm 只保留 `executeWithContext(AlgorithmContext*)` 纯虚函数
  - AlgorithmManager 只保留 `executeWithContext(name, context)` 唯一执行接口
  - 所有4个算法完成迁移：Differentiation, Integration, MovingAverage, BaselineCorrection
  - 算法从上下文拉取数据，零参数传递，单一数据源
- ✅ **两阶段执行机制** (prepareContext 返回 bool)
  - 阶段1（prepareContext）：验证数据完整性，返回就绪状态
  - 阶段2（executeWithContext）：只在数据完整时执行计算
  - AlgorithmManager 检查就绪状态，数据不完整时提前终止
  - 防止算法在用户未完成交互时过早执行
  - BaselineCorrectionAlgorithm 验证至少2个选点
  - 所有算法实现数据完整性验证逻辑
- ✅ **AlgorithmResult 统一输出容器** (v1.0)
  - 结构化算法输出：Curve, Marker, Region, ScalarValue, Composite
  - 类型安全的结果访问接口
  - 内置成功/失败状态和错误处理
  - 支持混合输出（如峰面积=数值+曲线+区域+标注）
  - 所有算法迁移到新的返回值类型
- ✅ **峰面积计算算法** (PeakAreaAlgorithm)
  - B类交互算法，用户选择2个点定义积分范围
  - 使用梯形积分法计算峰面积
  - 混合输出：数值结果 + FloatingLabel 显示
  - 支持所有仪器类型（TGA/DSC/ARC）
- ✅ **浮动标签组件** (FloatingLabel)
  - 数据锚定模式：跟随图表缩放
  - 视图锚定模式：HUD固定位置
  - 拖动、缩放（Ctrl+滚轮）、锁定功能
  - 关闭按钮、锁定按钮
  - 应用于峰面积、测量工具等场景
- ✅ **测量工具** (TrapezoidMeasureTool)
  - 直角梯形测量工具，测量垂直高度差（质量损失）
  - 两个可拖动端点，自动吸附曲线
  - 实时显示测量值
  - 支持横轴切换（温度/时间）同步
- ✅ **曲线级联删除功能** (v1.0.0)
  - 主曲线保护：从文件导入的原始数据不能删除
  - 派生曲线级联删除：自动递归删除所有子曲线
  - 确认对话框：显示将删除的子曲线列表
  - 深度优先遍历：先删除子曲线，再删除父曲线
- ✅ **图表交互增强**
  - 右键拖动平移功能
  - Ctrl+滚轮缩放功能
  - 横轴切换：温度/时间模式（XAxisMode）
  - 修复多Y轴缩放导致曲线丢失的问题
  - 修复右键拖动Y方向相反的问题
- ✅ **ChartView 三层架构重构** (Phase 1-5 完成)
  - 将单体 ChartView (2207行) 重构为三层架构 (2553行)
  - **ThermalChart** (1064行): 数据管理层 - 曲线/坐标轴/叠加物管理
  - **ThermalChartView** (493行): 交互处理层 - 鼠标/键盘事件、碰撞检测、缩放平移
  - **ChartView** (693行): 容器层 + 算法状态机 - 转发调用、管理交互状态
  - 代码简化 68.6%，职责分离清晰，易于维护和测试
  - 单向依赖：ChartView → ThermalChartView → ThermalChart
  - 保持向后兼容，外部 API 不变
- ✅ **撤销/重做功能完全集成** (HistoryManager + Command Pattern)
  - HistoryManager 单例管理命令历史 (支持50步撤销/重做)
  - ICommand 接口定义命令模式（execute/undo/redo）
  - AddCurveCommand、AlgorithmCommand、BaselineCommand 已实现
  - AlgorithmManager 使用 AddCurveCommand 通过 HistoryManager 添加曲线
  - MainWindow 添加撤销/重做按钮和快捷键 (Ctrl+Z/Ctrl+Y)
  - MainController 实现 onUndo/onRedo 槽函数
  - 所有算法执行自动支持撤销/重做

### 当前限制
1. 仅支持单项目模式(导入新数据会清空已有曲线)
2. 算法参数收集对话框尚未实现
3. 动力学计算功能待实现
4. 项目保存/加载功能待实现
5. 曲线导出功能待实现
6. 峰值检测算法待实现
7. 归一化算法待实现

### 开发计划 (详见 `Analysis/ARCHITECTURE_OPTIMIZATION_PLAN.md`)

**短期 (Phase 1 - 命令模式和历史管理)** ✅ 已完成:
- ✅ HistoryManager 和命令模式集成完成
- ✅ 所有修改操作已使用 Command 封装 (AddCurveCommand 等)
- ✅ 撤销/重做功能已集成并测试

**中期 (Phase 2 - 项目管理)**:
- ProjectManager 多项目支持
- 项目保存/加载功能
- 曲线导出和图表导出

**中期 (Phase 3 - 算法扩展)**:
- 算法参数收集对话框 (基于 AlgorithmDescriptor)
- 峰值检测算法 (Marker输出)
- 归一化算法
- 外推法（Onset/Endset温度）
- 切线法

**长期 (Phase 4 - 高级功能)**:
- 动力学分析(Kissinger、Ozawa、Flynn-Wall-Ozawa 等)
- 数据库支持
- 批处理和脚本系统
- 插件系统
- 报告生成系统

## UI组件使用指南

### FloatingLabel 浮动标签

**功能**: 可拖动、可缩放、悬浮在图表上的文本标签

**特性**:
- 数据锚定模式 (DataAnchored): 锚定到数据坐标，随图表缩放移动
- 视图锚定模式 (ViewAnchored): 锚定到视图像素位置（HUD），不受图表缩放影响
- 拖动移动（鼠标左键拖拽）
- 缩放（Ctrl+滚轮）
- 关闭按钮（右上角）
- 固定/解锁（锁定后禁止拖动）

**使用示例**:
```cpp
// 添加数据锚定标签
FloatingLabel* label = chartView->addFloatingLabel(
    "温度=350°C\n值=1.23 mg",  // 支持多行文本
    QPointF(350.0, 1.23),       // 数据坐标
    curveId                     // 曲线ID
);

// 添加视图锚定标签（HUD）
FloatingLabel* hud = chartView->addFloatingLabelHUD(
    "实验条件\nT=25°C",
    QPointF(60, 30)  // 视图坐标（像素）
);
```

详见: `Analysis/FloatingLabel_使用示例.md`

### TrapezoidMeasureTool 测量工具

**功能**: 测量曲线上两点之间的垂直高度差（质量损失）

**特性**:
- 两个可拖动的端点（自动吸附到曲线）
- 绘制直角梯形（两条水平线 + 一条竖直线）
- 实时显示垂直距离测量值
- 可以删除（右上角关闭按钮）
- 支持横轴切换（温度/时间）同步

**使用方式**:
1. 用户点击"质量损失"按钮
2. 在图表上拖动鼠标选择两个点
3. 工具自动绘制并显示测量值
4. 可以拖动端点重新调整测量范围

### ChartView 交互功能

**交互模式**:
- `View` 模式: 缩放、平移、区域选择
- `Pick` 模式: 选择数据点、测量、显示坐标

**横轴模式**:
- `Temperature` 模式: 以温度为横轴（默认）
- `Time` 模式: 以时间为横轴

**鼠标操作**:
- **右键拖动**: 平移图表
- **Ctrl + 滚轮**: 缩放图表
- **左键点击**: 选择曲线/选点（根据当前模式）

### 曲线管理功能

**曲线属性**:
- `isMainCurve`: 主曲线（从文件导入的原始数据，不可删除）
- `isAuxiliaryCurve`: 辅助曲线（继承父曲线的Y轴）
- `isStronglyBound`: 强绑定曲线（不在树中独立显示，随父曲线隐藏）

**删除策略**:
- 主曲线保护：不能删除主曲线，只能通过"清空项目"移除
- 级联删除：删除派生曲线时自动递归删除所有子曲线
- 确认对话框：显示将删除的子曲线列表

详见: `Analysis/曲线级联删除功能说明.md`

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
- **AlgorithmResult 统一输出**: `新设计文档/统一输出算法_AlgorithmResult设计.md` (结构化算法输出容器)
- **MVC 层次划分总览**: `新设计文档/一、MVC 层次划分总览.md` (最新架构分层)
- **统一初始化**: `新设计文档/统一初始化.md` (ApplicationContext 设计)
- **交互类**: `新设计文档/交互类.md`
- **抽象算法行为类型**: `新设计文档/抽象算法行为类型.md`

### 功能说明文档
- **FloatingLabel 使用示例**: `Analysis/FloatingLabel_使用示例.md`
- **曲线级联删除功能**: `Analysis/曲线级联删除功能说明.md` (v1.0.0)

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

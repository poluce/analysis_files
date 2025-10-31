# 架构优化计划

## 📊 当前架构分析

### ✅ 已实现的部分

1. **四层架构基础** ✓
   - Domain Layer: `ThermalDataPoint`, `ThermalCurve`, `IThermalAlgorithm`
   - Infrastructure Layer: `TextFileReader`, `DifferentiationAlgorithm`, etc.
   - Application Layer: `CurveManager`, `AlgorithmService`
   - Presentation Layer: `MainWindow`, `PlotWidget`, `MainController`

2. **核心功能模块** ✓
   - 数据模型定义完整
   - 文件读取接口清晰
   - 曲线管理基本完成
   - 算法服务框架建立

### ❌ 缺失的关键组件

1. **ProjectManager** - 项目生命周期管理
2. **HistoryManager** + `ICommand`接口 - 撤销/重做功能
3. **命令模式实现** - 所有数据修改操作需要Command封装
4. **信号流向规范** - 命令路径和通知路径未明确分离

### ⚠️ 需要改进的部分

1. **依赖注入不完整** - MainController 依赖注入不够彻底
2. **缺少历史记录** - 用户操作无法撤销
3. **项目管理缺失** - 无法保存/加载项目
4. **信号连接混乱** - 没有明确区分命令信号和通知信号

---

## 🎯 优化目标

按照优先级和复杂度,分阶段实施优化:

### 阶段1: 添加Command模式和历史管理 (核心功能)

**优先级: 🔥🔥🔥 HIGH**

这是支持撤销/重做功能的基础,也是完善架构的关键。

#### 1.1 创建ICommand接口

```
src/domain/algorithm/ICommand.h
```

定义命令接口:
- `execute()` - 执行操作
- `undo()` - 撤销操作
- `redo()` - 重做操作
- `description()` - 操作描述

#### 1.2 创建HistoryManager

```
src/application/history/HistoryManager.h
src/application/history/HistoryManager.cpp
```

功能:
- 维护undo/redo栈
- 执行命令并记录历史
- 提供撤销/重做接口
- 限制历史记录数量

#### 1.3 实现具体Command类

```
src/application/history/AlgorithmCommand.h
src/application/history/AlgorithmCommand.cpp
```

封装算法执行操作:
- 保存执行前的数据状态
- 执行算法
- 支持撤销回到原状态

**收益:**
- ✅ 用户可以撤销/重做操作
- ✅ 操作历史可追踪
- ✅ 增强用户体验

**工作量:** 约4-6小时

---

### 阶段2: 添加ProjectManager (项目管理)

**优先级: 🔥🔥 MEDIUM-HIGH**

支持保存/加载项目,管理多条曲线的工作流程。

#### 2.1 创建ProjectManager

```
src/application/project/ProjectManager.h
src/application/project/ProjectManager.cpp
```

功能:
- 创建/打开/保存项目
- 管理项目元数据
- 项目修改状态跟踪
- 最近项目列表

#### 2.2 创建ThermalProject模型

```
src/domain/model/ThermalProject.h
src/domain/model/ThermalProject.cpp
```

项目数据结构:
- 项目ID和名称
- 曲线ID列表
- 项目设置
- 处理历史记录

**收益:**
- ✅ 支持项目的保存和恢复
- ✅ 多个项目之间切换
- ✅ 工作流程持久化

**工作量:** 约6-8小时

---

### 阶段3: 优化信号流向架构 (规范通信)

**优先级: 🔥 MEDIUM**

明确区分命令路径(UI→Controller→Service)和通知路径(Service→UI)。

#### 3.1 重构MainController

- 明确哪些槽函数处理命令 (写操作)
- 移除不必要的信号转发
- 使用依赖注入传入所有Manager

#### 3.2 UI组件直接监听Service

- PlotWidget 直接监听 `CurveManager::curveDataChanged`
- ToolPanel 直接监听 `AlgorithmService::algorithmFinished`
- 避免通过Controller转发只读信号

#### 3.3 所有写操作走Command

- 算法执行 → AlgorithmCommand
- 基线校正 → BaselineCommand
- 数据导入 → ImportCommand

**收益:**
- ✅ 代码更清晰,职责明确
- ✅ 减少不必要的信号转发
- ✅ 更容易维护和测试

**工作量:** 约4-6小时

---

### 阶段4: 完善依赖注入 (可选优化)

**优先级: 🆗 LOW**

提高代码的可测试性和灵活性。

#### 4.1 创建Application类

在`main.cpp`中创建统一的初始化入口:
- 初始化所有单例Service
- 创建Controller并注入依赖
- 建立信号连接
- 启动主窗口

#### 4.2 测试支持

更容易编写单元测试:
- 可以注入Mock对象
- 独立测试每个模块
- 不依赖真实的Manager实例

**收益:**
- ✅ 更好的可测试性
- ✅ 更灵活的依赖管理
- ✅ 更清晰的启动流程

**工作量:** 约3-4小时

---

## 📝 实施建议

### 推荐方案A: 完整优化 (如果时间充足)

按照阶段1→2→3→4 依次实施,构建完整的架构:

**总工作量:** 约17-24小时
**适用场景:** 长期项目,需要完整的架构支持

### 推荐方案B: 渐进优化 (推荐)

优先实施阶段1和阶段2,保证核心功能:

1. **本次实施:** 阶段1 (Command + HistoryManager)
2. **下次迭代:** 阶段2 (ProjectManager)
3. **后续优化:** 阶段3 + 阶段4

**首次工作量:** 约4-6小时
**适用场景:** 渐进式改进,快速见效

### 推荐方案C: 最小改动 (如果时间紧张)

仅实施阶段1的核心部分:

- 添加 `ICommand`接口
- 添加 `HistoryManager`
- 实现一个 `AlgorithmCommand`示例
- 在MainController中集成

**工作量:** 约3-4小时
**适用场景:** 快速增加撤销功能,后续再扩展

---

## 🎬 下一步行动

### 立即可以开始的工作:

1. **创建ICommand接口**
   ```
   新建: src/domain/algorithm/ICommand.h
   ```

2. **创建HistoryManager**
   ```
   新建: src/application/history/HistoryManager.h
   新建: src/application/history/HistoryManager.cpp
   ```

3. **创建AlgorithmCommand**
   ```
   新建: src/application/history/AlgorithmCommand.h
   新建: src/application/history/AlgorithmCommand.cpp
   ```

4. **更新MainController**
   ```
   修改: src/ui/controller/MainController.h
   修改: src/ui/controller/MainController.cpp
   - 添加 HistoryManager 依赖
   - 添加 undo/redo 槽函数
   - 算法执行改为创建Command
   ```

5. **更新MainWindow**
   ```
   修改: src/ui/mainwindow.h
   修改: src/ui/mainwindow.cpp
   - 添加 Undo/Redo 菜单项
   - 连接到 MainController
   ```

6. **更新构建配置**
   ```
   修改: Analysis.pro
   - 添加新的头文件和源文件
   ```

### 验证步骤:

1. ✅ 编译通过
2. ✅ 执行算法操作
3. ✅ 点击"撤销"能恢复数据
4. ✅ 点击"重做"能重新应用
5. ✅ 多次撤销/重做正常工作

---

## 💡 关键设计点

### 1. Command的数据保存策略

**问题:** Command需要保存哪些数据?

**方案:**
- **保存完整数据副本** (推荐) - 简单可靠,但占内存
- **保存差异数据** - 节省内存,但实现复杂

**建议:** 先用完整副本,后续优化

### 2. HistoryManager的栈深度

**问题:** 历史记录保存多少步?

**方案:**
- 默认 50 步
- 可在设置中配置
- 超过限制时删除最老记录

### 3. 信号连接的时机

**问题:** 什么时候建立信号连接?

**方案:**
- UI组件自身的`setupConnections()`中监听Service
- `main.cpp`的Application类中连接UI→Controller

### 4. 单例的生命周期

**问题:** 单例何时创建和销毁?

**方案:**
- 在`main()`中显式创建
- 程序退出时自动销毁
- 避免静态初始化顺序问题

---

## 📚 参考文档

- [01_主架构设计.md](../01_主架构设计.md) - 四层架构详解
- [02_四层架构详解.md](../02_四层架构详解.md) - 各层职责说明
- [信号流设计.md](../设计图/信号流设计.md) - 信号流向规范

---

## ❓ 需要决策的问题

在开始实施前,请确认:

1. **选择哪个实施方案?**
   - [ ] 方案A: 完整优化
   - [x] 方案B: 渐进优化 (推荐)
   - [ ] 方案C: 最小改动

2. **优先实施哪些功能?**
   - [x] Command + HistoryManager (撤销/重做)
   - [ ] ProjectManager (项目管理)
   - [ ] 信号流向优化
   - [ ] 依赖注入完善

3. **是否需要同时实施?**
   - [ ] 一次性完成所有
   - [x] 分阶段逐步实施

---

**建议:** 我可以先帮你实施 **阶段1: Command模式和历史管理**,这是最核心也是收益最大的改进。完成后你可以立即看到撤销/重做功能的效果,然后再决定是否继续其他阶段。

**预计时间:** 约4-6小时的开发工作

**需要我现在开始实施吗?**

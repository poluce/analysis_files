# GEMINI 项目说明

## 目录概述

本目录包含 **DSC/TGA 热分析软件**的架构和设计文档。该项目是使用 Qt 框架构建的 C++ 应用程序。这些文档提供了软件设计的全面概述，从高层次的四层架构到详细的类设计和未来改进计划。

核心设计是四层架构：
1.  **表示层（Presentation Layer）：** 使用 Qt Widgets 构建的用户界面。
2.  **应用层（Application Layer）：** 协调业务逻辑、服务和控制器。
3.  **领域层（Domain Layer）：** 定义核心业务模型和接口。
4.  **基础设施层（Infrastructure Layer）：** 处理技术实现，如文件 I/O 和特定算法。

## 关键文件

*   `🏗️ DSCTGA热分析软件 - 模块化架构设计.md`：主要的架构文档。概述了四层架构、模块划分、核心模块设计和设计原则。
*   `1023四层架构详细解析.md`：深入探讨四层架构，详细解释每一层和模块的职责。
*   `数据架构分析案例.md`：使用简化的实际案例来解释架构中的数据流和职责。
*   `展示/`：该目录包含 PlantUML 图表和其他可视化文档。
    *   `架构层级图.puml` 和 `架构图.puml`：详细和简化架构图的 PlantUML 源文件。
    *   `信号流向设计说明.md`：解释应用程序不同层之间的信号/槽通信模式。
    *   `MDI多文档架构改进方案.md`、`MDI架构对比图.puml`、`MDI架构详细设计.puml`：一组提出并详细说明重大架构改进的文档，支持多文档界面（MDI），允许用户同时处理多个数据集。

## 使用说明

本目录作为"热分析（ThermalAnalysis）"软件项目的主要技术参考资料。应用于：

*   理解整体软件架构和设计模式。
*   指导新功能开发，确保与现有结构保持一致。
*   为新开发人员提供入职培训的背景资料。
*   审查和讨论未来的架构改进，例如提议的 MDI 架构。

## 项目编译

### 快速开始

项目源代码位于 `Analysis/` 目录，提供了便捷的批处理脚本用于编译：

#### 方法一：使用批处理脚本（推荐）

```bash
# 进入项目目录
cd Analysis/

# 首次编译 - 双击运行或命令行执行
build.bat              # 编译 Debug 版本

# 日常开发 - 快速重编译
rebuild.bat            # 清理并重新编译

# 发布版本
build-release.bat      # 编译优化的 Release 版本

# 清理编译产物
clean.bat              # 删除所有 build 目录
```

#### 方法二：在现有 build 目录编译

如果已经存在 `Analysis/build/MinGW_32_Qt_15_14_2-Debug/` 目录：

```bash
# 进入 build 目录
cd Analysis/build/MinGW_32_Qt_15_14_2-Debug/

# 编译项目
mingw32-make -j4

# 如果修改了 .pro 文件，需要先运行 qmake
qmake ../../Analysis.pro
mingw32-make -j4

# 运行程序
debug\Analysis.exe
```

#### 方法三：创建新的 build 目录

```bash
# 进入项目根目录
cd Analysis/

# 创建并进入 build 目录
mkdir build-debug
cd build-debug

# 生成 Makefile
qmake ../Analysis.pro

# 编译
mingw32-make -j4

# 运行
debug\Analysis.exe
```

### 环境要求

*   **Qt 版本**：5.14.2 或更高
*   **编译器**：MinGW 7.3.0
*   **C++ 标准**：C++17
*   **构建工具**：qmake + mingw32-make

### 详细文档

更多编译选项和问题排查，请参考：
*   `Analysis/BUILD.md` - 详细的命令行编译指南
*   `Analysis/README.md` - 项目概览和快速开始

### 项目结构

```
analysis_files/
├── GEMINI.md                    # 本文档
├── Analysis/                    # 源代码目录
│   ├── Analysis.pro            # Qt 项目文件
│   ├── BUILD.md                # 编译指南
│   ├── README.md               # 项目说明
│   ├── build.bat               # 编译脚本
│   ├── src/                    # 源代码（四层架构）
│   │   ├── ui/                # 表示层
│   │   ├── application/       # 应用层
│   │   ├── domain/            # 领域层
│   │   └── infrastructure/    # 基础设施层
│   └── build/                  # 编译输出目录
│       └── MinGW_32_Qt_15_14_2-Debug/
│           ├── debug/Analysis.exe
│           └── release/Analysis.exe
└── 展示/                        # 架构文档和图表

## 开发规范

*   **语言要求**：所有对话、文档、代码注释及提交信息必须使用**中文**。
*   **代码注释**：所有代码注释都应使用中文。
*   **验证**：完成代码修改后，必须主动运行编译和测试。

```

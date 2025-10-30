# Analysis - 热分析数据处理工具

基于 Qt 5.14.2 开发的热分析数据处理和可视化工具。

## 快速开始

### 使用批处理脚本编译（Windows）

项目提供了便捷的批处理脚本，双击即可使用：

#### 1. 首次编译（Debug 版本）
```
双击运行: build.bat
```
- 自动创建 build-debug 目录
- 生成 Makefile
- 编译项目
- 编译完成后可直接运行程序

#### 2. 重新编译（使用现有 build 目录）
```
双击运行: rebuild.bat
```
- 清理编译产物
- 快速重新编译
- 适合日常开发使用

#### 3. 编译 Release 版本
```
双击运行: build-release.bat
```
- 编译优化后的 Release 版本
- 文件更小，运行更快
- 适合发布使用

#### 4. 清理所有编译产物
```
双击运行: clean.bat
```
- 删除所有 build 目录
- 清理编译生成的文件

### 使用命令行编译

详细的命令行编译说明请参考：[BUILD.md](BUILD.md)

## 项目结构

```
Analysis/
├── Analysis.pro           # Qt 项目文件
├── README.md             # 本文档
├── BUILD.md              # 详细编译指南
├── build.bat             # 编译脚本（Debug）
├── rebuild.bat           # 快速重编译脚本
├── build-release.bat     # 编译脚本（Release）
├── clean.bat             # 清理脚本
├── src/                  # 源代码目录
│   ├── ui/              # UI 层
│   ├── application/     # 应用层
│   ├── domain/          # 领域层
│   └── infrastructure/  # 基础设施层
└── build/               # 构建目录
```

## 架构说明

项目采用分层架构设计：

- **UI 层** (`src/ui/`): 用户界面相关代码
  - MainWindow: 主窗口
  - DataImportWidget: 数据导入界面
  - PlotWidget: 图表绘制组件
  - ProjectExplorer: 项目资源管理器

- **应用层** (`src/application/`): 应用服务
  - CurveManager: 曲线管理器
  - AlgorithmService: 算法服务

- **领域层** (`src/domain/`): 核心业务逻辑
  - ThermalCurve: 热分析曲线模型
  - ThermalDataPoint: 数据点模型
  - IThermalAlgorithm: 算法接口

- **基础设施层** (`src/infrastructure/`): 技术实现
  - TextFileReader: 文本文件读取器
  - DifferentiationAlgorithm: 微分算法实现

## 开发环境

- **Qt 版本**: 5.14.2 或更高
- **编译器**: MinGW 7.3.0
- **C++ 标准**: C++17
- **构建工具**: qmake + mingw32-make

## 常见问题

### Q: 双击批处理脚本后提示"找不到 qmake"？

**A**: 打开批处理脚本（右键 -> 编辑），修改 Qt 安装路径：
```batch
set QT_DIR=你的Qt路径\Qt5.14.2\5.14.2\mingw73_32
set MINGW_DIR=你的Qt路径\Qt5.14.2\Tools\mingw730_32
```

### Q: 如何使用 Qt Creator 打开项目？

**A**:
1. 启动 Qt Creator
2. 文件 -> 打开文件或项目
3. 选择 `Analysis.pro` 文件
4. 配置构建目录
5. 点击"运行"按钮

### Q: 编译时出现头文件找不到的错误？

**A**:
1. 运行 `clean.bat` 清理所有编译产物
2. 重新运行 `build.bat` 编译

### Q: 如何调试程序？

**A**:
- 使用 Qt Creator 打开项目，按 F5 开始调试
- 或使用 GDB 命令行调试：
  ```bash
  gdb build-debug/debug/Analysis.exe
  ```

## 功能特性

- ✅ 多格式数据导入
- ✅ 实时数据预览
- ✅ 交互式图表绘制
- ✅ 曲线数据管理
- ✅ 算法处理服务
- 🚧 数据导出功能（开发中）
- 🚧 高级数据分析（规划中）

## 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m '添加一些很棒的特性'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 许可证

本项目采用 MIT 许可证 - 详见 LICENSE 文件

## 联系方式

如有问题或建议，请提交 Issue 或 Pull Request。

---

**注意**: 首次使用前，请确保已正确安装 Qt 5.14.2 和 MinGW 编译器。

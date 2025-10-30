# Analysis 项目编译指南

本文档介绍如何使用命令行在 build 目录中编译 Analysis 项目。

## 环境要求

- Qt 5.14.2 或更高版本
- MinGW 7.3.0 编译器
- qmake 和 mingw32-make 工具

## 编译步骤

### 方法一：使用现有 build 目录编译（推荐）

如果你已经有 build 目录（如 `build/MinGW_32_Qt_15_14_2-Debug`），可以直接在该目录下编译：

```bash
# 1. 进入 build 目录
cd "f:\B_My_Document\analysis\analysis_files\Analysis\build\MinGW_32_Qt_15_14_2-Debug"

# 2. 重新生成 Makefile（当 .pro 文件有修改时需要）
qmake "f:\B_My_Document\analysis\analysis_files\Analysis\Analysis.pro"

# 3. 编译项目（Debug 版本）
mingw32-make

# 4. 清理编译产物（需要时）
mingw32-make clean

# 5. 重新完整编译
mingw32-make
```

### 方法二：创建新的 build 目录

如果需要从零开始创建 build 目录：

```bash
# 1. 进入项目根目录
cd "f:\B_My_Document\analysis\analysis_files\Analysis"

# 2. 创建 build 目录
mkdir build-debug
cd build-debug

# 3. 运行 qmake 生成 Makefile
qmake ..\Analysis.pro

# 4. 编译项目
mingw32-make

# 5. 运行程序
debug\Analysis.exe
```

### 方法三：Release 版本编译

编译 Release 优化版本（体积更小，性能更好）：

```bash
# 1. 创建 Release build 目录
cd "f:\B_My_Document\analysis\analysis_files\Analysis"
mkdir build-release
cd build-release

# 2. 运行 qmake 指定 Release 配置
qmake ..\Analysis.pro CONFIG+=release

# 3. 编译 Release 版本
mingw32-make

# 4. 运行程序
release\Analysis.exe
```

## 常用命令

### 清理编译产物

```bash
# 清理所有编译生成的文件
mingw32-make clean

# 清理所有文件（包括 Makefile）
mingw32-make distclean
```

### 并行编译（加快编译速度）

```bash
# 使用 4 个线程并行编译
mingw32-make -j4
```

### 查看编译详细信息

```bash
# 显示详细的编译命令
mingw32-make VERBOSE=1
```

### 仅检查是否需要重新编译

```bash
# 不执行编译，仅检查
mingw32-make -q
```

## 编译输出

### Debug 版本
- 可执行文件：`build-debug/debug/Analysis.exe`
- 包含调试信息，文件较大
- 适合开发和调试

### Release 版本
- 可执行文件：`build-release/release/Analysis.exe`
- 优化编译，文件较小，运行更快
- 适合发布和生产使用

## 常见问题

### 1. qmake 命令找不到

**问题**：`'qmake' 不是内部或外部命令`

**解决方法**：
```bash
# 方法1：添加 Qt 到 PATH 环境变量
set PATH=E:\Qt\Qt5.14.2\5.14.2\mingw73_32\bin;%PATH%
set PATH=E:\Qt\Qt5.14.2\Tools\mingw730_32\bin;%PATH%

# 方法2：使用完整路径
E:\Qt\Qt5.14.2\5.14.2\mingw73_32\bin\qmake.exe Analysis.pro
```

### 2. mingw32-make 命令找不到

**问题**：`'mingw32-make' 不是内部或外部命令`

**解决方法**：
```bash
# 添加 MinGW 到 PATH
set PATH=E:\Qt\Qt5.14.2\Tools\mingw730_32\bin;%PATH%
```

### 3. 编译错误：找不到头文件

**问题**：`fatal error: xxx.h: No such file or directory`

**解决方法**：
```bash
# 1. 删除 build 目录
cd ..
rmdir /s /q build-debug

# 2. 重新创建并编译
mkdir build-debug
cd build-debug
qmake ..\Analysis.pro
mingw32-make
```

### 4. 链接错误

**问题**：`undefined reference to ...`

**解决方法**：
```bash
# 清理后重新完整编译
mingw32-make clean
mingw32-make
```

### 5. 修改 .pro 文件后需要重新生成 Makefile

**解决方法**：
```bash
# 在 build 目录中重新运行 qmake
qmake ..\Analysis.pro
mingw32-make
```

## 自动化编译脚本

### Windows 批处理脚本 (build.bat)

创建一个 `build.bat` 文件在项目根目录：

```batch
@echo off
echo ====================================
echo   Analysis 项目编译脚本
echo ====================================

REM 设置 Qt 环境变量
set QT_DIR=E:\Qt\Qt5.14.2\5.14.2\mingw73_32
set MINGW_DIR=E:\Qt\Qt5.14.2\Tools\mingw730_32
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

REM 创建 build 目录
if not exist build-debug mkdir build-debug
cd build-debug

REM 运行 qmake
echo.
echo [1/3] 正在生成 Makefile...
qmake ..\Analysis.pro
if errorlevel 1 (
    echo qmake 失败！
    pause
    exit /b 1
)

REM 编译项目
echo.
echo [2/3] 正在编译项目...
mingw32-make -j4
if errorlevel 1 (
    echo 编译失败！
    pause
    exit /b 1
)

echo.
echo [3/3] 编译完成！
echo 可执行文件位置: %cd%\debug\Analysis.exe
echo.
pause
```

使用方法：
```bash
# 双击运行或在命令行中执行
build.bat
```

### 快速重新编译脚本 (rebuild.bat)

```batch
@echo off
echo ====================================
echo   快速重新编译
echo ====================================

set QT_DIR=E:\Qt\Qt5.14.2\5.14.2\mingw73_32
set MINGW_DIR=E:\Qt\Qt5.14.2\Tools\mingw730_32
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

cd build-debug

echo 正在清理...
mingw32-make clean

echo 正在编译...
mingw32-make -j4

if errorlevel 1 (
    echo 编译失败！
    pause
    exit /b 1
)

echo 编译成功！
pause
```

## 项目结构

```
Analysis/
├── Analysis.pro          # Qt 项目文件
├── BUILD.md             # 本文档
├── build/               # 现有 build 目录
│   └── MinGW_32_Qt_15_14_2-Debug/
│       ├── Makefile
│       ├── debug/
│       │   └── Analysis.exe
│       └── release/
│           └── Analysis.exe
├── build-debug/         # 新建 Debug build 目录
└── build-release/       # 新建 Release build 目录
```

## 推荐工作流程

### 日常开发

```bash
# 1. 修改代码后，在 build 目录编译
cd build/MinGW_32_Qt_15_14_2-Debug
mingw32-make -j4

# 2. 如果修改了 .pro 文件或头文件依赖
qmake ../../Analysis.pro
mingw32-make -j4

# 3. 运行程序测试
debug\Analysis.exe
```

### 发布版本

```bash
# 1. 清理所有 build 目录
# 2. 创建 Release build
mkdir build-release
cd build-release
qmake ..\Analysis.pro CONFIG+=release
mingw32-make -j4

# 3. 测试 Release 版本
release\Analysis.exe

# 4. 部署（将 exe 和必要的 dll 打包）
```

## 注意事项

1. **路径中的空格**：如果路径包含空格，必须使用引号括起来
2. **工作目录**：确保在正确的目录执行命令
3. **环境变量**：确保 Qt 和 MinGW 的 bin 目录在 PATH 中
4. **并行编译**：使用 `-j4` 可以加快编译速度（数字根据 CPU 核心数调整）
5. **增量编译**：修改少量文件后直接运行 `mingw32-make` 只会编译修改的文件

## 参考资料

- [Qt qmake 文档](https://doc.qt.io/qt-5/qmake-manual.html)
- [Qt 编译指南](https://doc.qt.io/qt-5/build-sources.html)
- [MinGW 使用指南](http://www.mingw.org/)

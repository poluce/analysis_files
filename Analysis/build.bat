@echo off
chcp 65001 >nul
echo ====================================
echo   Analysis 项目编译脚本
echo ====================================

REM 设置 Qt 环境变量（根据你的 Qt 安装路径修改）
set QT_DIR=E:\Qt\Qt5.14.2\5.14.2\mingw73_32
set MINGW_DIR=E:\Qt\Qt5.14.2\Tools\mingw730_32
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

REM 检查 qmake 是否可用
where qmake >nul 2>&1
if errorlevel 1 (
    echo 错误: 找不到 qmake 命令！
    echo 请检查 Qt 安装路径是否正确
    echo 当前设置: %QT_DIR%
    pause
    exit /b 1
)

REM 创建 build 目录
if not exist build-debug mkdir build-debug
cd build-debug

REM 运行 qmake
echo.
echo [1/3] 正在生成 Makefile...
qmake ..\Analysis.pro
if errorlevel 1 (
    echo qmake 失败！
    cd ..
    pause
    exit /b 1
)

REM 编译项目
echo.
echo [2/3] 正在编译项目...
mingw32-make -j4
if errorlevel 1 (
    echo 编译失败！
    cd ..
    pause
    exit /b 1
)

echo.
echo [3/3] 编译完成！
echo 可执行文件位置: %cd%\debug\Analysis.exe
echo.
echo 按任意键运行程序...
pause >nul

REM 运行程序
debug\Analysis.exe

cd ..

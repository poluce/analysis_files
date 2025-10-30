@echo off
chcp 65001 >nul
echo ====================================
echo   快速重新编译
echo ====================================

REM 设置 Qt 环境变量
set QT_DIR=E:\Qt\Qt5.14.2\5.14.2\mingw73_32
set MINGW_DIR=E:\Qt\Qt5.14.2\Tools\mingw730_32
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

REM 检查 build 目录是否存在
if not exist build\MinGW_32_Qt_15_14_2-Debug (
    echo 错误: 找不到 build 目录！
    echo 请先使用 build.bat 进行初始编译
    pause
    exit /b 1
)

cd build\MinGW_32_Qt_15_14_2-Debug

echo [1/2] 正在清理编译产物...
mingw32-make clean

echo.
echo [2/2] 正在重新编译...
mingw32-make -j4

if errorlevel 1 (
    echo 编译失败！
    cd ..\..
    pause
    exit /b 1
)

echo.
echo 编译成功！
echo 可执行文件: %cd%\debug\Analysis.exe
echo.
pause

cd ..\..

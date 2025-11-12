@echo off
REM ============================================
REM 修复链接错误 - 完全清理重新编译脚本
REM ============================================

echo ========================================
echo 外推温度功能编译修复脚本
echo ========================================
echo.

echo [1/5] 清理旧的构建目录...
if exist build-debug (
    rmdir /s /q build-debug
    echo   - 已删除 build-debug
)
if exist build-release (
    rmdir /s /q build-release
    echo   - 已删除 build-release
)
if exist build (
    rmdir /s /q build
    echo   - 已删除 build
)

echo.
echo [2/5] 清理 qmake 缓存...
if exist Makefile (
    del /q Makefile
    echo   - 已删除 Makefile
)
if exist Makefile.Debug (
    del /q Makefile.Debug
    echo   - 已删除 Makefile.Debug
)
if exist Makefile.Release (
    del /q Makefile.Release
    echo   - 已删除 Makefile.Release
)
if exist .qmake.stash (
    del /q .qmake.stash
    echo   - 已删除 .qmake.stash
)

echo.
echo [3/5] 创建 Release 构建目录...
mkdir build-release
cd build-release

echo.
echo [4/5] 运行 qmake（Release 配置）...
qmake ..\Analysis.pro -spec win32-g++ "CONFIG+=release"
if errorlevel 1 (
    echo 错误：qmake 执行失败！
    echo 请检查 Qt 环境变量是否正确配置
    pause
    exit /b 1
)

echo.
echo [5/5] 编译项目（使用 4 个并行任务）...
mingw32-make -j4
if errorlevel 1 (
    echo.
    echo ========================================
    echo 编译失败！
    echo ========================================
    echo.
    echo 可能的原因：
    echo 1. 代码存在语法错误
    echo 2. 缺少必要的库文件
    echo 3. Qt 版本不匹配
    echo.
    echo 请查看上方的错误信息进行排查
    pause
    exit /b 1
)

echo.
echo ========================================
echo 编译成功！
echo ========================================
echo.
echo 可执行文件位置：
cd
echo   %cd%\release\Analysis.exe
echo.
echo 运行程序：
echo   cd release
echo   Analysis.exe
echo.
pause

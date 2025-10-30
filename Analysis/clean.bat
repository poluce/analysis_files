@echo off
chcp 65001 >nul
echo ====================================
echo   清理编译产物
echo ====================================
echo.
echo 这将删除以下目录:
echo   - build-debug
echo   - build-release
echo   - build/MinGW_32_Qt_15_14_2-Debug/debug
echo   - build/MinGW_32_Qt_15_14_2-Debug/release
echo.
echo 按任意键继续，或关闭窗口取消...
pause >nul

REM 清理 build-debug 目录
if exist build-debug (
    echo 正在清理 build-debug...
    rmdir /s /q build-debug
    echo   ✓ build-debug 已删除
)

REM 清理 build-release 目录
if exist build-release (
    echo 正在清理 build-release...
    rmdir /s /q build-release
    echo   ✓ build-release 已删除
)

REM 清理现有 build 目录中的编译产物
if exist build\MinGW_32_Qt_15_14_2-Debug\debug (
    echo 正在清理 build/MinGW_32_Qt_15_14_2-Debug/debug...
    rmdir /s /q build\MinGW_32_Qt_15_14_2-Debug\debug
    echo   ✓ debug 已删除
)

if exist build\MinGW_32_Qt_15_14_2-Debug\release (
    echo 正在清理 build/MinGW_32_Qt_15_14_2-Debug/release...
    rmdir /s /q build\MinGW_32_Qt_15_14_2-Debug\release
    echo   ✓ release 已删除
)

REM 清理 .o 文件和 Makefile
if exist build\MinGW_32_Qt_15_14_2-Debug (
    cd build\MinGW_32_Qt_15_14_2-Debug

    set QT_DIR=E:\Qt\Qt5.14.2\5.14.2\mingw73_32
    set MINGW_DIR=E:\Qt\Qt5.14.2\Tools\mingw730_32
    set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

    if exist Makefile (
        echo 正在运行 make clean...
        mingw32-make clean >nul 2>&1
        echo   ✓ 编译产物已清理
    )
    cd ..\..
)

echo.
echo 清理完成！
echo.
pause

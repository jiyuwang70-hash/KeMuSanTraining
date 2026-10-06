@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
title 科目三模拟训练 - 一键编译与运行检验
color 0B

echo =======================================================================
echo          考驾照科目三实机驾驶系统（UE 5.7）编译与运行工具
echo =======================================================================
echo.

cd /d "%~dp0"
set "PROJECT_FILE=%~dp0KeMuSanTraining.uproject"

if not exist "%PROJECT_FILE%" (
    echo [错误] 未在当前目录找到 KeMuSanTraining.uproject 工程文件！
    pause
    exit /b 1
)

echo [项目工程路径] %PROJECT_FILE%

set "UE_PATH=D:\UE_5.7"
if not "%UE_ROOT%"=="" if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" set "UE_PATH=%UE_ROOT%"
if not exist "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" if exist "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" set "UE_PATH=C:\Program Files\Epic Games\UE_5.7"

if not exist "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" (
    echo [错误] 找不到 "%UE_PATH%\Engine\Build\BatchFiles\Build.bat"，请设置 UE_ROOT 环境变量！
    pause
    exit /b 1
)

set "UNREAL_EDITOR=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UNREAL_EDITOR%" (
    echo [错误] 找不到 "%UNREAL_EDITOR%"，请检查 Unreal Engine 安装！
    pause
    exit /b 1
)

echo [虚幻引擎路径] %UE_PATH%
echo.
echo =======================================================================
echo [步骤 1/2] 正在调用 UnrealBuildTool 执行编译构建...
echo =======================================================================
echo.

call "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" KeMuSanTrainingEditor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex
set "BUILD_EXIT=%ERRORLEVEL%"

if %BUILD_EXIT% NEQ 0 (
    echo.
    echo [编译失败] 代码编译未通过，退出码: %BUILD_EXIT%，请查看上方编译报错！
    pause
    exit /b %BUILD_EXIT%
)

echo.
echo =======================================================================
echo [步骤 2/2] 编译通过！准备启动游戏实机...
echo =======================================================================
echo.
echo 操作指南：
echo   - [Enter] 键：开启【推荐自由练习】模式
echo   - [F1] 键：开启【C1 手动挡模拟考试】
echo   - [F2] 键：开启【C2 自动挡模拟考试】
echo   - [F3] 键：学员档案与错题复盘；上下选择，PageUp/PageDown翻页，Enter看详情
echo   - [T] 键：开启/关闭【光学后视镜微调控制台】
echo   - [V] 键：切换【第一人称座舱视点 / 第三人称追尾跟车】
echo   - [Tab] 键：调镜模式下切换镜面；自动挡下循环切挡
echo   - [方向键]：微调后视镜光学角度
echo   - [R] 键：调镜模式下一键复位为出厂默认角度（非调镜时为倒挡）
echo.

start "" /D "%~dp0" "%UNREAL_EDITOR%" "%PROJECT_FILE%" /Engine/Maps/Entry -game -log -WINDOWED -ResX=1280 -ResY=720
set "LAUNCH_EXIT=%ERRORLEVEL%"
if %LAUNCH_EXIT% NEQ 0 (
    echo [启动失败] 未能启动游戏，退出码: %LAUNCH_EXIT%，请检查引擎与工程路径！
    pause
    exit /b %LAUNCH_EXIT%
)

echo 游戏启动命令已成功执行！
pause
exit /b 0
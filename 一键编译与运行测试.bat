@echo off
chcp 65001 >nul
title 科目三模拟训练 - 一键本地编译与运行检验
color 0B

echo =======================================================================
echo          考驾照科目三模拟系统（UE 5.7）- 本地编译与运行检验工具
echo =======================================================================
echo.

cd /d "%~dp0"

:: 1. 查找项目工程文件
set PROJECT_FILE=
if exist "KeMuSanTraining.uproject" (
    set PROJECT_FILE=%cd%\KeMuSanTraining.uproject
) else if exist "..\KeMuSanTraining.uproject" (
    pushd ..
    set PROJECT_FILE=%cd%\KeMuSanTraining.uproject
    popd
) else if exist "KeMuSanTraining\KeMuSanTraining.uproject" (
    set PROJECT_FILE=%cd%\KeMuSanTraining\KeMuSanTraining.uproject
)

if "%PROJECT_FILE%"=="" (
    echo [错误] 未在当前目录找到 KeMuSanTraining.uproject 工程文件！
    echo 正在从 GitHub 克隆项目...
    git clone https://github.com/jiyuwang70-hash/KeMuSanTraining.git
    if exist "KeMuSanTraining\KeMuSanTraining.uproject" (
        set PROJECT_FILE=%cd%\KeMuSanTraining\KeMuSanTraining.uproject
    ) else (
        echo [错误] 依然未能找到工程文件，请确认网络连接！
        pause
        exit /b 1
    )
)

echo [项目工程路径] %PROJECT_FILE%

:: 2. 确保升级后的源码已同步至工程目录
if exist "%~dp0Source\KeMuSanTraining" if not "%PROJECT_FILE%"=="%cd%\KeMuSanTraining.uproject" (
    xcopy /y /e /i "%~dp0Source\KeMuSanTraining" "%cd%\KeMuSanTraining\Source\KeMuSanTraining" >nul 2>&1
)

:: 3. 自动检测 Unreal Engine 5.7 安装路径
set UE_PATH=
if exist "D:\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=D:\UE_5.7
if "%UE_PATH%"=="" if exist "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=C:\Program Files\Epic Games\UE_5.7
if "%UE_PATH%"=="" if exist "D:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=D:\Program Files\Epic Games\UE_5.7
if "%UE_PATH%"=="" if exist "E:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=E:\Program Files\Epic Games\UE_5.7
if "%UE_PATH%"=="" if exist "E:\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=E:\UE_5.7
if "%UE_PATH%"=="" if exist "C:\UE_5.7\Engine\Build\BatchFiles\Build.bat" set UE_PATH=C:\UE_5.7

if "%UE_PATH%"=="" (
    echo [提示] 未在默认目录检测到 UE 5.7。
    set /p UE_PATH="请输入你的 UE 5.7 安装目录 (如 D:\UE_5.7 或 C:\Program Files\Epic Games\UE_5.7): "
)

if not exist "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" (
    echo [错误] 找不到 "%UE_PATH%\Engine\Build\BatchFiles\Build.bat"，请检查引擎路径！
    pause
    exit /b 1
)

echo [虚幻引擎路径] %UE_PATH%
echo.
echo =======================================================================
echo [步骤 1/2] 正在调用 UnrealBuildTool 执行严苛编译与链接检查...
echo =======================================================================
echo.

call "%UE_PATH%\Engine\Build\BatchFiles\Build.bat" KeMuSanTrainingEditor Win64 Development -Project="%PROJECT_FILE%" -WaitMutex

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo =======================================================================
    echo ❌ [编译失败] 代码编译未通过，请查看上方编译输出报错！
    echo =======================================================================
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo =======================================================================
echo 🎉 [步骤 2/2 编译通过] 0 错误！准备启动游戏进入实机测试...
echo =======================================================================
echo.
echo  即将启动独立测试窗口 (-game)...
echo  进入游戏后操作提示：
echo    - 屏幕中央将显示【小白智能按键向导】
echo    - 按 [Enter] 键直接一键开启 C2 自动挡考试
echo    - 按 [O] 键唤起【真实光学物理后视镜微调控制台】
echo    - 按 [V] 键切换【第一人称主驾驶沉浸座舱视角】
echo    - 按 [Tab] 切换后视镜，[方向键] 微调光学反射，[R] 键一键回正
echo.

start "" "%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJECT_FILE%" -game -log

echo 游戏已启动！可直接在画面中体验调镜与驾驶。
pause

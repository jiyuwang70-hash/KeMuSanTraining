@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
title 科目三项目 - GitHub 提交与推送工具
color 0A

cd /d "%~dp0"

echo =======================================================================
echo         KeMuSan Training - GitHub 规范提交与推送工具
echo =======================================================================
echo.

if not exist ".git" (
    echo [错误] 当前目录不是 Git 仓库！
    pause
    exit /b 1
)

echo [1/4] 检查当前改动状态...
git status -s
if %ERRORLEVEL% NEQ 0 (
    echo [错误] git status 执行失败！
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [2/4] 获取当前工作分支与远端状态...
set "CURRENT_BRANCH="
for /f "tokens=*" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "CURRENT_BRANCH=%%b"
if "%CURRENT_BRANCH%"=="" (
    echo [错误] 无法获取当前 Git 分支！
    pause
    exit /b 1
)
echo [当前分支] %CURRENT_BRANCH%

echo.
echo [3/4] 规范添加项目源码、常驻规则与必要脚本...
git add Source/
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add Source/ 失败！ & pause & exit /b %ERRORLEVEL% )

git add Config/
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add Config/ 失败！ & pause & exit /b %ERRORLEVEL% )

git add scripts/
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add scripts/ 失败！ & pause & exit /b %ERRORLEVEL% )

git add .agents/rules/project-quality.md
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add .agents/rules/project-quality.md 失败！ & pause & exit /b %ERRORLEVEL% )

git add *.bat
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add *.bat 失败！ & pause & exit /b %ERRORLEVEL% )

git add README.md
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add README.md 失败！ & pause & exit /b %ERRORLEVEL% )

git add KeMuSanTraining.uproject
if %ERRORLEVEL% NEQ 0 ( echo [错误] git add KeMuSanTraining.uproject 失败！ & pause & exit /b %ERRORLEVEL% )

echo.
git diff --cached --quiet
set "DIFF_EXIT=%ERRORLEVEL%"
if %DIFF_EXIT% EQU 0 (
    echo [提示] 暂存区无新增修改，跳过提交并继续推送当前分支。
    goto PushChanges
)
if %DIFF_EXIT% NEQ 1 (
    echo [错误] 无法检查暂存区修改，退出码: %DIFF_EXIT%。
    pause
    exit /b %DIFF_EXIT%
)

set "COMMIT_MSG="
set /p COMMIT_MSG="请输入提交信息 (直接回车默认: feat: complete driving journey, optical mirrors, and quality rules): "
if "%COMMIT_MSG%"=="" set "COMMIT_MSG=feat: complete driving journey, optical mirrors, and quality rules"

git commit -m "%COMMIT_MSG%"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [提交失败] git commit 未成功执行，中止推送！
    pause
    exit /b %ERRORLEVEL%
)

echo.
:PushChanges
echo [4/4] 推送到远程仓库 origin %CURRENT_BRANCH% ...
git push origin "%CURRENT_BRANCH%"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [推送受阻] git push 退出码非零，请检查网络或认证凭据。
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo =======================================================================
echo   [成功] 代码已成功推送到远程分支 origin/%CURRENT_BRANCH%！
echo =======================================================================
echo.
pause
exit /b 0

@echo off
title KeMuSan Git Push Tool
color 0A

cd /d "%~dp0"

echo =======================================================================
echo         KeMuSan Training - GitHub Auto Push Tool
echo =======================================================================
echo.

set REPO_DIR=
if exist ".git" (
    set REPO_DIR=%cd%
) else if exist "..\.git" (
    cd ..
    set REPO_DIR=%cd%
) else if exist "KeMuSanTraining\.git" (
    cd KeMuSanTraining
    set REPO_DIR=%cd%
) else (
    echo [Info] Cloning repository from GitHub...
    git clone https://github.com/jiyuwang70-hash/KeMuSanTraining.git
    if not exist "KeMuSanTraining\.git" (
        echo [Error] Git clone failed. Please check network.
        pause
        exit /b 1
    )
    cd KeMuSanTraining
    set REPO_DIR=%cd%
)

echo [Working Directory] %REPO_DIR%
echo.

if exist "%~dp0Source\KeMuSanTraining" (
    echo [1/3] Syncing source files...
    xcopy /y /e /i "%~dp0Source\KeMuSanTraining" "%REPO_DIR%\Source\KeMuSanTraining" >nul 2>&1
)
if exist "%~dp0KeMuSanTraining\Source\KeMuSanTraining" (
    echo [1/3] Syncing source files...
    xcopy /y /e /i "%~dp0KeMuSanTraining\Source\KeMuSanTraining" "%REPO_DIR%\Source\KeMuSanTraining" >nul 2>&1
)

cd /d "%REPO_DIR%"

echo [2/3] Adding files to git...
git add Source/KeMuSanTraining/ExamTypes.h
git add Source/KeMuSanTraining/KeMuSanHUD.h
git add Source/KeMuSanTraining/KeMuSanHUD.cpp
git add Source/KeMuSanTraining/KeMuSanPawn.h
git add Source/KeMuSanTraining/KeMuSanPawn.cpp
git add Source/KeMuSanTraining/KeMuSanPlayerController.h
git add Source/KeMuSanTraining/KeMuSanPlayerController.cpp
git add .

echo.
echo Committing changes...
git commit -m "feat: add physical optical rearview mirrors, novice HUD guide, and fix ResetVehicle"

echo.
echo [3/3] Pushing to GitHub (origin main)...
git push origin main

if %ERRORLEVEL% EQU 0 (
    echo.
    echo =======================================================================
    echo   [SUCCESS] Code successfully pushed to GitHub!
    echo   Repository: https://github.com/jiyuwang70-hash/KeMuSanTraining
    echo =======================================================================
) else (
    echo.
    echo =======================================================================
    echo   [NOTICE] Git push needs credentials or network check.
    echo   Local commit is ready. Run: git push origin main
    echo =======================================================================
)

echo.
pause

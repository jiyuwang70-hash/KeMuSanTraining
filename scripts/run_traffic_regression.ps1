[CmdletBinding()]
param(
    [string]$UnrealEditor,
    [string[]]$Scenario = @("empty", "crosswalk_yield", "follow_and_meet", "full_mix"),
    [int]$Seed = 20260823,
    [int]$TimeoutSeconds = 80,
    [int]$MinScreenshots = 2,
    [switch]$KeepProcess
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot ".."))
$ProjectFile = Join-Path $ProjectRoot "KeMuSanTraining.uproject"
$SavedRoot = Join-Path $ProjectRoot "Saved"
$LogRoot = Join-Path $SavedRoot "Logs"
$ShotRoot = Join-Path $SavedRoot "Screenshots\WindowsEditor"

if (-not $UnrealEditor) {
    if ($env:UE_ROOT) {
        $UnrealEditor = Join-Path $env:UE_ROOT "Engine\Binaries\Win64\UnrealEditor.exe"
    } else {
        $UnrealEditor = "D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
    }
}

if (-not (Test-Path -LiteralPath $UnrealEditor)) {
    throw "UnrealEditor.exe not found: $UnrealEditor. Pass -UnrealEditor or set UE_ROOT."
}
if (-not (Test-Path -LiteralPath $ProjectFile)) {
    throw "Project file not found: $ProjectFile"
}

New-Item -ItemType Directory -Force -Path $LogRoot, $ShotRoot | Out-Null
$runStamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runResults = @()

function Get-NewScreenshots([datetime]$Since) {
    if (-not (Test-Path -LiteralPath $ShotRoot)) { return @() }
    return @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File |
        Where-Object { $_.LastWriteTime -ge $Since } |
        Sort-Object LastWriteTime)
}

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw "ASSERTION FAILED: $Message`nExpected pattern: $Pattern"
    }
}

foreach ($scenarioName in $Scenario) {
    $startedAt = Get-Date
    $safeScenario = ($scenarioName -replace "[^A-Za-z0-9_-]", "_")
    $logPath = Join-Path $LogRoot "traffic-regression-$runStamp-$safeScenario.log"
    $stdoutPath = "$logPath.stdout"
    $stderrPath = "$logPath.stderr"
    $args = @(
        $ProjectFile,
        "/Engine/Maps/Entry",
        "-game",
        "-autotest",
        "-Unattended",
        "-NoSplash",
        "-NoScreenMessages",
        "-WINDOWED",
        "-ResX=1280",
        "-ResY=720",
        "-traffic-scenario=$scenarioName",
        "-traffic-seed=$Seed",
        "-traffic-regression",
        "-ExecCmds=viewmode lit,showflag.Game 1,showflag.Materials 1,showflag.Lighting 1,showflag.Wireframe 0,showflag.BSP 0,showflag.BSPTriangles 0,showflag.BSPSplit 0,showflag.Brushes 0,showflag.BuilderBrush 0,showflag.Collision 0,showflag.Bounds 0"
    )

    Write-Host "[traffic] starting scenario=$scenarioName seed=$Seed"
    $beforeShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File -ErrorAction SilentlyContinue)
    $proc = Start-Process -FilePath $UnrealEditor -ArgumentList $args -WorkingDirectory $ProjectRoot `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -WindowStyle Hidden -PassThru
    $procId = $proc.Id
    $passed = $false
    try {
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        $regressionComplete = $false
        do {
            Start-Sleep -Seconds 2
            $proc.Refresh()
            $combined = ""
            if (Test-Path -LiteralPath $stdoutPath) { $combined += Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue }
            if (Test-Path -LiteralPath $stderrPath) { $combined += Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue }
            $ueLogs = @(Get-ChildItem -LiteralPath $LogRoot -Filter "KeMuSanTraining.log" -File -ErrorAction SilentlyContinue)
            if ($ueLogs.Count -gt 0) { $combined += Get-Content -LiteralPath $ueLogs[0].FullName -Raw -ErrorAction SilentlyContinue }
            if ($combined -match "\[KeMuSanTraffic\] regression_complete") {
                $regressionComplete = $true
                break
            }
        } while ((Get-Date) -lt $deadline -and -not $proc.HasExited)

        if (-not $proc.HasExited -and -not $regressionComplete -and (Get-Date) -ge $deadline) {
            throw "scenario timed out after $TimeoutSeconds seconds (pid=$procId)"
        }
        if (-not $proc.HasExited -and $regressionComplete) {
            $proc.WaitForExit(10) | Out-Null
            $proc.Refresh()
        }

        # RequestExit(false) may leave Start-Process.ExitCode unset even after a clean run.
        # Treat the explicit regression marker as the completion signal and reject only a
        # known non-zero process code.
        if ($proc.HasExited) {
            $proc.Refresh()
            $exitCode = $null
            try { $exitCode = $proc.ExitCode } catch { $exitCode = $null }
            if ($null -ne $exitCode -and $exitCode -ne 0) {
                throw "scenario exited with code $exitCode (pid=$procId)"
            }
        } elseif (-not $regressionComplete) {
            throw "scenario process ended without a completion marker (pid=$procId)"
        }

        $combined = ""
        if (Test-Path -LiteralPath $stdoutPath) { $combined += Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue }
        if (Test-Path -LiteralPath $stderrPath) { $combined += Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue }
        $ueLog = Join-Path $LogRoot "KeMuSanTraining.log"
        if (Test-Path -LiteralPath $ueLog) { $combined += Get-Content -LiteralPath $ueLog -Raw -ErrorAction SilentlyContinue }
        $combined | Set-Content -LiteralPath $logPath -Encoding utf8

        Assert-Contains $combined "\[KeMuSanTraffic\] regression_complete" "scenario did not emit the completion marker"
        Assert-Contains $combined "scenario=$([regex]::Escape($scenarioName)) seed=$Seed" "scenario and deterministic seed were not logged"
        Assert-Contains $combined "presentation_state view=Lit Wireframe=0 BSP=0 BSPTriangles=0 BSPSplit=0" "runtime presentation guard did not report a clean Lit/CSG state"
        Assert-Contains $combined "viewmode lit|view=Lit" "Lit view was not requested"
        Assert-Contains $combined "Wireframe=0" "Wireframe was not disabled"
        Assert-Contains $combined "BSP=0" "BSP/CSG was not disabled"
        Assert-Contains $combined "road builder done" "road was not built"

        $shots = @(Get-NewScreenshots $startedAt | Where-Object { $beforeShots.Name -notcontains $_.Name })
        if ($shots.Count -lt $MinScreenshots) {
            throw "expected at least $MinScreenshots screenshots, found $($shots.Count)"
        }
        if ($combined -match "Fatal error|Assertion failed|Unhandled Exception|Error:.*Failed") {
            throw "fatal runtime error found in scenario log"
        }

        if ($scenarioName -in @("crosswalk_yield", "pedestrian_yield")) {
            Assert-Contains $combined "pedestrian_prepare" "pedestrian scenario did not prepare a crossing"
            Assert-Contains $combined "pedestrian_release" "pedestrian scenario did not release the waiting pedestrian"
            Assert-Contains $combined "crosswalk_traffic_activated" "crosswalk scenario did not activate its deterministic vehicle conflict"
            Assert-Contains $combined "vehicle_policy.*yielding=1" "traffic did not yield to the pedestrian"
        }
        if ($scenarioName -in @("follow_and_meet", "full_mix", "road_test_default")) {
            Assert-Contains $combined "scripted_vehicle|ambient_spawn|vehicle_policy" "vehicle behavior scenario did not create a traffic event"
        }

        $passed = $true
        $runResults += [pscustomobject]@{ Scenario = $scenarioName; Seed = $Seed; Screenshots = $shots.Count; Status = "PASS"; Log = $logPath }
        Write-Host "[traffic] PASS scenario=$scenarioName screenshots=$($shots.Count) log=$logPath" -ForegroundColor Green
    } finally {
        if (-not $KeepProcess) {
            $proc.Refresh()
            if (-not $proc.HasExited) {
                Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
                Start-Sleep -Milliseconds 500
            }
        }
        if (-not $passed) {
            if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath -Tail 80 -ErrorAction SilentlyContinue | Add-Content -LiteralPath $logPath -Encoding utf8 }
            if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath -Tail 80 -ErrorAction SilentlyContinue | Add-Content -LiteralPath $logPath -Encoding utf8 }
        }
    }
}

Write-Host "[traffic] all scenarios passed: $($runResults.Count)" -ForegroundColor Green
$runResults | Format-Table -AutoSize
exit 0

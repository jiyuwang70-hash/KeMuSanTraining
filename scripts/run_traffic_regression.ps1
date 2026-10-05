[CmdletBinding()]
param(
    [string]$UnrealEditor,
    [string[]]$Scenario = @("empty", "crosswalk_yield", "follow_and_meet", "full_mix"),
    [int]$Seed = 20260823,
    [int]$TimeoutSeconds = 80,
    [int]$MinScreenshots = 2,
    [switch]$KeepProcess,
    [string]$ProjectFile
)

$ErrorActionPreference = "Stop"
$allowedScenarios = @("empty", "crosswalk_yield", "follow_and_meet", "full_mix", "pedestrian_yield", "road_test_default")
$normalizedScenarios = @()
foreach ($scenarioElement in $Scenario) {
    foreach ($scenarioPart in ([string]$scenarioElement -split ",")) {
        $scenarioName = $scenarioPart.Trim()
        if (-not $scenarioName) {
            throw "Scenario contains an empty name. Allowed values: $($allowedScenarios -join ', ')."
        }
        if ($allowedScenarios -cnotcontains $scenarioName) {
            throw "Unknown scenario '$scenarioName'. Allowed values: $($allowedScenarios -join ', ')."
        }
        $normalizedScenarios += $scenarioName
    }
}
if ($normalizedScenarios.Count -eq 0) {
    throw "At least one scenario is required."
}
if ($TimeoutSeconds -le 0) { throw "TimeoutSeconds must be greater than zero." }
if ($MinScreenshots -lt 0) { throw "MinScreenshots cannot be negative." }

if (-not $ProjectFile) {
    $ProjectFile = Join-Path (Join-Path $PSScriptRoot "..") "KeMuSanTraining.uproject"
}
if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) {
    throw "Project file not found: $ProjectFile"
}
$ProjectFile = (Resolve-Path -LiteralPath $ProjectFile).ProviderPath
$ProjectRoot = [System.IO.Path]::GetDirectoryName($ProjectFile)
$SavedRoot = Join-Path $ProjectRoot "Saved"
$LogRoot = Join-Path $SavedRoot "Logs"
$ShotRoot = Join-Path $SavedRoot "Screenshots\WindowsEditor"

if (-not $UnrealEditor) {
    $editorCandidates = @()
    if ($env:UE_ROOT) {
        $editorCandidates += Join-Path $env:UE_ROOT "Engine\Binaries\Win64\UnrealEditor.exe"
    }
    $editorCandidates += "D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
    $UnrealEditor = $editorCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $UnrealEditor -or -not (Test-Path -LiteralPath $UnrealEditor -PathType Leaf)) {
    throw "UnrealEditor.exe not found: $UnrealEditor. Pass -UnrealEditor or set UE_ROOT."
}
$UnrealEditor = (Resolve-Path -LiteralPath $UnrealEditor).ProviderPath

New-Item -ItemType Directory -Force -Path $LogRoot, $ShotRoot | Out-Null
$runResults = @()

function Get-NewScreenshots([datetime]$Since) {
    if (-not (Test-Path -LiteralPath $ShotRoot)) { return @() }
    return @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File |
        Where-Object { $_.LastWriteTime -ge $Since } |
        Sort-Object LastWriteTime)
}

function Read-RunLogs([string[]]$Paths) {
    $parts = foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Get-Content -LiteralPath $path -Raw -Encoding UTF8 -ErrorAction Stop
        }
    }
    return ($parts -join "`r`n")
}

function Assert-Contains([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw "ASSERTION FAILED: $Message`nExpected pattern: $Pattern"
    }
}

$failurePattern = "(?-i:\bFAIL\b)|Fatal error|Assertion failed|Unhandled Exception|Error:.*Failed"
foreach ($scenarioName in $normalizedScenarios) {
    $startedAt = Get-Date
    $runId = "$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')-$([guid]::NewGuid().ToString('N'))"
    $logPath = Join-Path $LogRoot "traffic-regression-$runId-$scenarioName.log"
    $ueLogPath = "$logPath.ue.log"
    $stdoutPath = "$logPath.stdout"
    $stderrPath = "$logPath.stderr"
    $runLogPaths = @($ueLogPath, $stdoutPath, $stderrPath)
    $launchArgs = @(
        "`"$ProjectFile`"",
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
        "-AbsLog=`"$ueLogPath`"",
        "-ExecCmds=`"viewmode lit,showflag.Game 1,showflag.Materials 1,showflag.Lighting 1,showflag.Wireframe 0,showflag.BSP 0,showflag.BSPTriangles 0,showflag.BSPSplit 0,showflag.Brushes 0,showflag.BuilderBrush 0,showflag.Collision 0,showflag.Bounds 0`""
    )

    Write-Host "[traffic] starting scenario=$scenarioName seed=$Seed"
    $beforeShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File -ErrorAction SilentlyContinue)
    $proc = $null
    $procId = $null
    $passed = $false
    try {
        $proc = Start-Process -FilePath $UnrealEditor -ArgumentList $launchArgs -WorkingDirectory $ProjectRoot `
            -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -WindowStyle Hidden -PassThru
        $procId = $proc.Id
        # Retain the process handle so Start-Process can report its natural exit code.
        $null = $proc.Handle
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Seconds 2
            $proc.Refresh()
            $combined = Read-RunLogs $runLogPaths
            if ($combined -match $failurePattern) {
                throw "runtime failure found in scenario log (pid=$procId)"
            }
        } while (-not $proc.HasExited -and (Get-Date) -lt $deadline)

        if (-not $proc.HasExited) {
            throw "scenario timed out after $TimeoutSeconds seconds (pid=$procId)"
        }
        # A completion marker can precede RequestExit(false). Await the natural
        # exit and reject a known non-zero code; the marker and assertions are
        # still required below, even when the process exits with code zero.
        if ($proc.HasExited) {
            $proc.WaitForExit()
            $proc.Refresh()
            $exitCode = $null
            try { $exitCode = $proc.ExitCode } catch { $exitCode = $null }
            if ($null -ne $exitCode -and $exitCode -ne 0) {
                throw "scenario exited with code $exitCode (pid=$procId)"
            }
        }

        if (-not (Test-Path -LiteralPath $ueLogPath -PathType Leaf)) {
            throw "scenario produced no session UE log: $ueLogPath (pid=$procId)"
        }
        if ((Get-Item -LiteralPath $ueLogPath).LastWriteTime -lt $startedAt) {
            throw "scenario UE log is older than this run: $ueLogPath"
        }
        $combined = Read-RunLogs $runLogPaths
        $combined | Set-Content -LiteralPath $logPath -Encoding utf8
        if ($combined -match $failurePattern) {
            throw "runtime failure found in scenario log (pid=$procId)"
        }

        $ueText = Get-Content -LiteralPath $ueLogPath -Raw -Encoding UTF8 -ErrorAction Stop
        Assert-Contains $ueText "\[KeMuSanTraffic\] regression_complete(?:\s|$)" "scenario did not emit the completion marker"
        Assert-Contains $ueText "\[KeMuSanTraffic\] scenario=$([regex]::Escape($scenarioName)) seed=$Seed(?:\s|$)" "traffic manager did not log the exact scenario and deterministic seed"
        Assert-Contains $combined "presentation_state view=Lit Wireframe=0 BSP=0 BSPTriangles=0 BSPSplit=0" "runtime presentation guard did not report a clean Lit/CSG state"
        Assert-Contains $combined "viewmode lit|view=Lit" "Lit view was not requested"
        Assert-Contains $combined "Wireframe=0" "Wireframe was not disabled"
        Assert-Contains $combined "BSP=0" "BSP/CSG was not disabled"
        Assert-Contains $combined "road builder done" "road was not built"

        $shots = @(Get-NewScreenshots $startedAt | Where-Object { $beforeShots.Name -notcontains $_.Name })
        if ($shots.Count -lt $MinScreenshots) {
            throw "expected at least $MinScreenshots screenshots, found $($shots.Count)"
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
        $runResults += [pscustomobject]@{ Scenario = $scenarioName; Seed = $Seed; Screenshots = $shots.Count; Status = "PASS"; Log = $logPath; UeLog = $ueLogPath }
        Write-Host "[traffic] PASS scenario=$scenarioName screenshots=$($shots.Count) log=$logPath" -ForegroundColor Green
    } finally {
        # KeepProcess applies to successful runs only. Failed and timed-out runs
        # always clean up the one process started by this scenario.
        if ($null -ne $proc -and (-not $KeepProcess -or -not $passed)) {
            $proc.Refresh()
            if (-not $proc.HasExited) {
                Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
                Start-Sleep -Milliseconds 500
            }
        }
        try {
            (Read-RunLogs $runLogPaths) | Set-Content -LiteralPath $logPath -Encoding utf8
        } catch {
            Write-Warning "Could not save combined scenario log: $($_.Exception.Message)"
        }
    }
}

Write-Host "[traffic] all scenarios passed: $($runResults.Count)" -ForegroundColor Green
$runResults | Format-Table -AutoSize
exit 0

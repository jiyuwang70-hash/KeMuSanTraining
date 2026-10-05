[CmdletBinding()]
param(
    [string]$UnrealEditor,
    [string]$ProjectFile,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 150,
    [ValidateRange(7, 1000)][int]$MinScreenshots = 7
)

$ErrorActionPreference = 'Stop'
$ScriptProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ProjectFile) { $ProjectFile = Join-Path $ScriptProjectRoot 'KeMuSanTraining.uproject' }
if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) { throw "Project file not found: $ProjectFile" }
$ProjectFile = (Resolve-Path -LiteralPath $ProjectFile).Path
$ProjectRoot = Split-Path -Parent $ProjectFile

if (-not $UnrealEditor) {
    $envEditor = if ($env:UE_ROOT) { Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor.exe' } else { $null }
    if ($envEditor -and (Test-Path -LiteralPath $envEditor -PathType Leaf)) {
        $UnrealEditor = $envEditor
    } elseif (Test-Path -LiteralPath 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' -PathType Leaf) {
        $UnrealEditor = 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'
    } else {
        throw 'UnrealEditor.exe not found. Set UE_ROOT or pass -UnrealEditor.'
    }
}
if (-not (Test-Path -LiteralPath $UnrealEditor -PathType Leaf)) { throw "UnrealEditor.exe not found: $UnrealEditor" }
$UnrealEditor = (Resolve-Path -LiteralPath $UnrealEditor).Path
$LogRoot = Join-Path $ProjectRoot 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
$RunStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$RuntimeFailurePattern = 'Fatal error|Assertion failed|Unhandled Exception|Error:.*Failed'

function Quote-Argument([string]$Value) { return ('"' + $Value + '"') }

function Read-LogText([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $value = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 -ErrorAction Stop
        if ($null -ne $value) { return $value }
    }
    return ''
}

function Invoke-LoggedCase {
    param(
        [string]$Name,
        [string[]]$CaseArguments,
        [string]$PassPattern,
        [string]$FailPattern,
        [int]$CaseTimeoutSeconds
    )
    $caseId = [guid]::NewGuid().ToString('N')
    $logPath = Join-Path $LogRoot "$Name-$RunStamp-$caseId.log"
    $stdoutPath = "$logPath.stdout"
    $stderrPath = "$logPath.stderr"
    if (Test-Path -LiteralPath $logPath) { throw "Case log already exists: $logPath" }
    $arguments = @((Quote-Argument $ProjectFile), '/Engine/Maps/Entry', '-game', '-Unattended', '-NoSplash', '-WINDOWED', '-ResX=1280', '-ResY=720')
    $arguments += $CaseArguments
    $arguments += ('-AbsLog=' + (Quote-Argument $logPath))
    $arguments += ('-ExecCmds=' + (Quote-Argument 'viewmode lit,showflag.Game 1,showflag.Materials 1,showflag.Lighting 1'))
    Write-Host "[$Name] Starting; log: $logPath"
    $proc = $null
    try {
        $proc = Start-Process -FilePath $UnrealEditor -ArgumentList $arguments -WorkingDirectory $ProjectRoot `
            -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru
        # Keep the process handle so its original exit status remains available.
        try { $null = $proc.Handle } catch { }
        $deadline = (Get-Date).AddSeconds($CaseTimeoutSeconds)
        while ($true) {
            $proc.Refresh()
            $text = (Read-LogText $logPath) + "`n" + (Read-LogText $stdoutPath) + "`n" + (Read-LogText $stderrPath)
            if ($text -match $RuntimeFailurePattern -or ($FailPattern -and $text -match $FailPattern)) {
                throw "[$Name] Failure recorded; inspect $logPath"
            }
            if ($proc.HasExited) { break }
            if ((Get-Date) -ge $deadline) { throw "[$Name] Timed out after $CaseTimeoutSeconds seconds; inspect $logPath" }
            Start-Sleep -Milliseconds 250
        }
        $proc.WaitForExit()
        $proc.Refresh()
        $exitCode = $null
        try { $exitCode = $proc.ExitCode } catch { }
        if ($null -ne $exitCode -and $exitCode -ne 0) { throw "[$Name] Process exited with code $exitCode; inspect $logPath" }
        if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) { throw "[$Name] No case log was produced: $logPath" }
        $text = (Read-LogText $logPath) + "`n" + (Read-LogText $stdoutPath) + "`n" + (Read-LogText $stderrPath)
        if ($text -match $RuntimeFailurePattern -or ($FailPattern -and $text -match $FailPattern)) {
            throw "[$Name] Failure recorded; inspect $logPath"
        }
        if ($text -notmatch $PassPattern) { throw "[$Name] Required PASS marker missing; inspect $logPath" }
        return [pscustomobject]@{ Name = $Name; Log = $logPath; Text = $text; ExitCode = $exitCode }
    } finally {
        if ($null -ne $proc) {
            $proc.Refresh()
            if (-not $proc.HasExited) {
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
                $null = $proc.WaitForExit(5000)
            }
        }
    }
}

$ShotRoot = Join-Path $ProjectRoot 'Saved\Screenshots\WindowsEditor'
New-Item -ItemType Directory -Force -Path $ShotRoot | Out-Null
$beforeShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter '*.png' -File | Select-Object -ExpandProperty FullName)
$startedAt = Get-Date
try {
    $capture = Invoke-LoggedCase -Name 'showcase-capture' -CaseArguments @('-capture-showcase') `
        -PassPattern '\[KeMuSanShowcase\] capture_complete PASS(?:\s|$)' `
        -FailPattern '\[KeMuSanShowcase\][^\r\n]*\bFAIL\b' -CaseTimeoutSeconds $TimeoutSeconds
    $newShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter '*.png' -File |
        Where-Object { $_.LastWriteTime -ge $startedAt -and ($beforeShots -notcontains $_.FullName) } |
        Sort-Object LastWriteTime)
    if ($newShots.Count -lt $MinScreenshots) {
        throw "[Showcase] Expected at least $MinScreenshots new screenshots, found $($newShots.Count); inspect $($capture.Log)"
    }
    foreach ($shotNumber in @('1', '2', '3', '4a', '4b', '5', '6')) {
        if ($capture.Text -notmatch "\[KeMuSanShowcase\] Shot ${shotNumber}:") {
            throw "[Showcase] Capture stage $shotNumber missing; inspect $($capture.Log)"
        }
    }
    $newShots | ForEach-Object { Write-Host "[Showcase] $($_.FullName) ($($_.Length) bytes)" }
    Write-Host "[Showcase] PASS: $($newShots.Count) new screenshots; log: $($capture.Log)"
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}

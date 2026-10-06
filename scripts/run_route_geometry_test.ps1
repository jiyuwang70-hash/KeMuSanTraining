[CmdletBinding()]
param([string]$UnrealEditor, [string]$ProjectFile, [ValidateRange(1,3600)][int]$TimeoutSeconds=60)
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ProjectFile) { $ProjectFile = Join-Path $ProjectRoot 'KeMuSanTraining.uproject' }
$ProjectFile = (Resolve-Path -LiteralPath $ProjectFile).Path
$ProjectRoot = Split-Path -Parent $ProjectFile
if (-not $UnrealEditor) {
    if ($env:UE_ROOT) { $UnrealEditor = Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor.exe' }
    elseif (Test-Path -LiteralPath 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe') { $UnrealEditor = 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' }
}
if (-not $UnrealEditor -or -not (Test-Path -LiteralPath $UnrealEditor -PathType Leaf)) { throw '找不到引擎，请设置 UE_ROOT 或传入 -UnrealEditor。' }
$testId = [guid]::NewGuid().ToString('N')
$TestRoot = Join-Path $ProjectRoot "Saved\Automation\Route-$testId"
New-Item -ItemType Directory -Force -Path $TestRoot | Out-Null
$LogPath = Join-Path $TestRoot 'route.log'
$slot = "KeMuSanTest_Route_$testId"
$jsonPath = Join-Path $TestRoot 'history.json'
$ScreenshotPath = Join-Path $TestRoot 'route.png'
$arguments = @(('"'+$ProjectFile+'"'),'/Engine/Maps/Entry','-game','-Unattended','-NoSplash','-WINDOWED','-ResX=1280','-ResY=720','-test-route-geometry',("-exam-save-slot=$slot"),('-exam-history-path="'+$jsonPath+'"'),('-AbsLog="'+$LogPath+'"'),('-route-screenshot="'+$ScreenshotPath+'"'))
$proc = $null
try {
    $proc = Start-Process -FilePath $UnrealEditor -ArgumentList $arguments -WorkingDirectory $ProjectRoot -PassThru
    $null = $proc.Handle
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $proc.Refresh()
        if ($proc.HasExited) { break }
        if ((Get-Date) -ge $deadline) { throw "路线验证超时，日志：$LogPath" }
        Start-Sleep -Milliseconds 200
    } while ($true)
    $proc.WaitForExit()
    if ($proc.ExitCode -ne 0) { throw "路线验证异常退出：$($proc.ExitCode)" }
    $text = Get-Content -LiteralPath $LogPath -Raw -Encoding UTF8
    if ($text -match 'Fatal error|Assertion failed|Unhandled Exception|\[KeMuSanRouteTest\][^\r\n]*\bFAIL\b') { throw "路线验证失败，日志：$LogPath" }
    foreach ($marker in @('rebuild_resets_length','computed_mileage','samples_connected','tangents_continuous','projection_round_trip','return_flag_after_turn','pawn_ready','no_premature_turn_completion','left_turn_angle_unwrapped','turn_completes_on_return','complete')) {
        if ($text -notmatch "\[KeMuSanRouteTest\] $marker PASS") { throw "缺少路线验证完成项：$marker，日志：$LogPath" }
    }
    if (-not (Test-Path -LiteralPath $ScreenshotPath -PathType Leaf)) { throw "本次路线截图未生成：$ScreenshotPath" }
    Write-Output "路线俯视图：$ScreenshotPath"
    $text -split "`r?`n" | Where-Object { $_ -match '\[KeMuSanRouteTest\]' } | Write-Output
    Write-Output "路线几何、投影及掉头评分回归通过；此项不代表全程驾驶通过。日志：$LogPath"
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
} finally {
    if ($proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
    $testSlotFile = Join-Path $ProjectRoot "Saved\SaveGames\$slot.sav"
    if (Test-Path -LiteralPath $testSlotFile) { Remove-Item -LiteralPath $testSlotFile -Force }
}

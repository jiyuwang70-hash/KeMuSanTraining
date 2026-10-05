param(
    [string]$UnrealEditor = "D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
    [string]$ProjectFile = "C:\Users\honor\Projects\KeMuSanTraining\KeMuSanTraining.uproject",
    [int]$TimeoutSeconds = 40
)

$ProjectRoot = Split-Path -Parent $ProjectFile
$LogRoot = Join-Path $ProjectRoot "Saved\Logs"
if (!(Test-Path -LiteralPath $LogRoot)) { New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null }

$startedAt = Get-Date
$stdoutPath = Join-Path $LogRoot "input-chain-test.stdout"
$stderrPath = Join-Path $LogRoot "input-chain-test.stderr"

$argsList = @(
    $ProjectFile,
    "/Engine/Maps/Entry",
    "-game",
    "-Unattended",
    "-NoSplash",
    "-WINDOWED",
    "-ResX=1280",
    "-ResY=720",
    "-test-input-chain",
    "-test-input-chain-exit",
    "-ExecCmds=viewmode lit,showflag.Game 1"
)

Write-Host "[InputChain] Starting automated input chain test..."
$proc = Start-Process -FilePath $UnrealEditor -ArgumentList $argsList -WorkingDirectory $ProjectRoot `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru

$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$complete = $false

do {
    Start-Sleep -Seconds 1
    $proc.Refresh()
    $logContent = ""
    $ueLog = Join-Path $LogRoot "KeMuSanTraining.log"
    if (Test-Path -LiteralPath $ueLog) {
        $logContent = Get-Content -LiteralPath $ueLog -Raw -ErrorAction SilentlyContinue
    }
    if ($logContent -match "\[KeMuSanInputChain\] test_complete") {
        $complete = $true
        break
    }
} while ((Get-Date) -lt $deadline -and -not $proc.HasExited)

if (-not $proc.HasExited) {
    Write-Host "[InputChain] Process still running, stopping..."
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

Start-Sleep -Seconds 1
Write-Host "=== Input Chain Verification Results ==="
$allLines = @()
$ueLog = Join-Path $LogRoot "KeMuSanTraining.log"
if (Test-Path -LiteralPath $ueLog) {
    $allLines += Get-Content -LiteralPath $ueLog -ErrorAction SilentlyContinue | Where-Object { $_ -match "\[KeMuSanInputChain\]" }
}
if (Test-Path -LiteralPath $stdoutPath) {
    $allLines += Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue | Where-Object { $_ -match "\[KeMuSanInputChain\]" }
}
$allLines | Select-Object -Unique | ForEach-Object { Write-Host $_ }

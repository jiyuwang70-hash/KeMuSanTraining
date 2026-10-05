param(
    [string]$UnrealEditor = "D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
    [string]$ProjectFile = "C:\Users\honor\Projects\KeMuSanTraining\KeMuSanTraining.uproject",
    [int]$TimeoutSeconds = 30
)

$ProjectRoot = Split-Path -Parent $ProjectFile
$LogRoot = Join-Path $ProjectRoot "Saved\Logs"
$ShotRoot = Join-Path $ProjectRoot "Saved\Screenshots\WindowsEditor"
if (!(Test-Path -LiteralPath $LogRoot)) { New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null }
if (!(Test-Path -LiteralPath $ShotRoot)) { New-Item -ItemType Directory -Force -Path $ShotRoot | Out-Null }

$startedAt = Get-Date
$beforeShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File -ErrorAction SilentlyContinue)

$stdoutPath = Join-Path $LogRoot "showcase-capture.stdout"
$stderrPath = Join-Path $LogRoot "showcase-capture.stderr"

$argsList = @(
    $ProjectFile,
    "/Engine/Maps/Entry",
    "-game",
    "-Unattended",
    "-NoSplash",
    "-WINDOWED",
    "-ResX=1280",
    "-ResY=720",
    "-capture-showcase",
    "-ExecCmds=viewmode lit,showflag.Game 1,showflag.Materials 1,showflag.Lighting 1"
)

Write-Host "[Showcase] Starting automated showcase capture..."
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
    if ($logContent -match "\[KeMuSanShowcase\] capture_complete") {
        $complete = $true
        break
    }
} while ((Get-Date) -lt $deadline -and -not $proc.HasExited)

if (-not $proc.HasExited) {
    Write-Host "[Showcase] Stopping process..."
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

Start-Sleep -Seconds 1
$afterShots = @(Get-ChildItem -LiteralPath $ShotRoot -Filter "*.png" -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $startedAt })

Write-Host "=== Showcase Capture Complete ==="
Write-Host "New screenshots generated: $($afterShots.Count)"
$afterShots | ForEach-Object { Write-Host "$($_.Name) ($($_.Length) bytes) -> $($_.FullName)" }

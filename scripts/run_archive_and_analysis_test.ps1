[CmdletBinding()]
param(
    [string]$UnrealEditor,
    [string]$ProjectFile,
    [ValidateRange(30, 3600)][int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$defaultProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ProjectFile) { $ProjectFile = Join-Path $defaultProjectRoot 'KeMuSanTraining.uproject' }
if (-not (Test-Path -LiteralPath $ProjectFile -PathType Leaf)) { throw "Project file not found: $ProjectFile" }
$ProjectFile = (Resolve-Path -LiteralPath $ProjectFile).Path
$projectRoot = Split-Path -Parent $ProjectFile
if (-not $UnrealEditor) {
    $envEditor = if ($env:UE_ROOT) { Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor.exe' } else { $null }
    if ($envEditor -and (Test-Path -LiteralPath $envEditor -PathType Leaf)) {
        $UnrealEditor = $envEditor
    } elseif (Test-Path -LiteralPath 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe' -PathType Leaf) {
        $UnrealEditor = 'D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'
    } else { throw 'UnrealEditor.exe not found. Set UE_ROOT or pass -UnrealEditor.' }
}
if (-not (Test-Path -LiteralPath $UnrealEditor -PathType Leaf)) { throw "UnrealEditor.exe not found: $UnrealEditor" }
$UnrealEditor = (Resolve-Path -LiteralPath $UnrealEditor).Path
$caseId = [guid]::NewGuid().ToString('N')
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$runDirectory = Join-Path $projectRoot "Saved\Automation\ArchiveAnalysis\$stamp-$caseId"
$shotDirectory = Join-Path $runDirectory 'screenshots'
New-Item -ItemType Directory -Force -Path $shotDirectory | Out-Null
$runDirectory = (Resolve-Path -LiteralPath $runDirectory).Path
$shotDirectory = (Resolve-Path -LiteralPath $shotDirectory).Path
$jsonPath = Join-Path $runDirectory 'ExamHistory.json'
$slotName = "KeMuSanTest_$caseId"
$testSavPath = Join-Path $projectRoot "Saved\SaveGames\$slotName.sav"
$learnerSavPath = Join-Path $projectRoot 'Saved\SaveGames\KeMuSanExamSave.sav'
$learnerJsonPath = Join-Path $projectRoot 'Saved\SaveGames\ExamHistory.json'
$dllPath = Join-Path $projectRoot 'Binaries\Win64\UnrealEditor-KeMuSanTraining.dll'
if (-not (Test-Path -LiteralPath $dllPath -PathType Leaf)) { throw "Compiled project DLL not found: $dllPath" }
$dllHashBefore = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash

function Quote-Argument([string]$Value) { return ('"' + $Value + '"') }
function Read-LogText([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
        if ($null -ne $text) { return $text }
    }
    return ''
}
function Get-Fingerprint([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Leaf) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
    return 'FILE_ABSENT'
}
$learnerSavBefore = Get-Fingerprint $learnerSavPath
$learnerJsonBefore = Get-Fingerprint $learnerJsonPath

function Get-RuntimeProblem([string]$Text, [string]$Mode) {
    $lines = $Text -split "`r?`n"
    foreach ($line in $lines) {
        if ($line -match '\[KeMuSanArchiveTest\]\s+\w+\s+FAIL\b') { return $line }
        if ($line -match 'Fatal error|Assertion failed|Unhandled Exception') { return $line }
        if ($line -match 'Error:.*Failed') {
            # The save process intentionally writes four invalid bytes to its isolated slot,
            # asserts refusal, then restores the original bytes. That exact guard is expected.
            if ($Mode -eq 'save' -and $line -match '\[KeMuSanArchive\] Existing save failed to load; preserved and refusing overwrite\.') { continue }
            return $line
        }
    }
    return $null
}

function Invoke-ArchiveProcess([string]$Mode, [string[]]$RequiredMarkers) {
    $logPath = Join-Path $runDirectory "$Mode.log"
    $stdoutPath = "$logPath.stdout"
    $stderrPath = "$logPath.stderr"
    $testFlag = if ($Mode -eq 'save') { '-test-archive-analysis' } else { '-test-archive-reload' }
    $arguments = @((Quote-Argument $ProjectFile), '/Engine/Maps/Entry', '-game', '-Unattended', '-NoSplash', '-WINDOWED', '-ResX=1280', '-ResY=720', $testFlag)
    $arguments += "-exam-save-slot=$slotName"
    $arguments += ('-exam-history-path=' + (Quote-Argument $jsonPath))
    $arguments += "-archive-test-id=$caseId"
    $arguments += ('-archive-test-shot-dir=' + (Quote-Argument $shotDirectory))
    $arguments += ('-AbsLog=' + (Quote-Argument $logPath))
    $arguments += ('-ExecCmds=' + (Quote-Argument 'viewmode lit,showflag.Wireframe 0'))
    Write-Host "[ArchiveAnalysisTest] Starting isolated $Mode process; log: $logPath"
    $proc = $null
    try {
        $started = Get-Date
        $proc = Start-Process -FilePath $UnrealEditor -ArgumentList $arguments -WorkingDirectory $projectRoot -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru
        try { $null = $proc.Handle } catch { }
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        while ($true) {
            $proc.Refresh()
            $text = (Read-LogText $logPath) + "`n" + (Read-LogText $stdoutPath) + "`n" + (Read-LogText $stderrPath)
            $problem = Get-RuntimeProblem $text $Mode
            if ($problem) { throw "Runtime failure in $Mode process: $problem" }
            if ($proc.HasExited) { break }
            if ((Get-Date) -ge $deadline) { throw "$Mode process timed out after $TimeoutSeconds seconds; inspect $logPath" }
            Start-Sleep -Milliseconds 200
        }
        $proc.WaitForExit()
        $proc.Refresh()
        $exitCode = $proc.ExitCode
        if ($null -eq $exitCode -or $exitCode -ne 0) { throw "$Mode process exited with unexpected code '$exitCode'; inspect $logPath" }
        $text = (Read-LogText $logPath) + "`n" + (Read-LogText $stdoutPath) + "`n" + (Read-LogText $stderrPath)
        $problem = Get-RuntimeProblem $text $Mode
        if ($problem) { throw "Runtime failure in $Mode process: $problem" }
        foreach ($marker in $RequiredMarkers) {
            $pattern = '\[KeMuSanArchiveTest\] ' + [regex]::Escape($marker) + ' PASS test_id=' + [regex]::Escape($caseId) + '\b'
            if ($text -notmatch $pattern) { throw "Missing fresh $Mode marker: $marker ($caseId)" }
        }
        $text -split "`r?`n" | Where-Object { $_ -match '\[KeMuSanArchiveTest\]' } | ForEach-Object { Write-Host $_ }
        return [pscustomobject]@{ mode = $Mode; log = $logPath; process_id = $proc.Id; exit_code = $exitCode; started_at = $started.ToUniversalTime().ToString('o'); markers = $RequiredMarkers }
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

$saveMarkers = @('isolated_parameters', 'empty_isolated_archive', 'native_f1_start', 'three_category_deductions', 'native_finish_analysis', 'binary_and_json_saved',
    'summary_screenshot_written', 'positive_exam_record', 'fatal_violation_analysis', 'fatal_exam_record', 'practice_record', 'pagination_fixture_record',
    'practice_excluded_from_exam_statistics', 'duplicate_record_idempotent', 'topn_zero_and_negative', 'other_category_reports_weakness',
    'json_unwritable_target_rejected', 'corrupt_slot_refused_and_restored', 'save_complete')
$reloadMarkers = @('isolated_parameters', 'cross_process_sav_reload', 'menu_f3_visible', 'history_second_page_selects_sixth', 'native_enter_opens_details',
    'native_enter_returns_to_list', 'native_esc_closes_archive', 'native_f1_after_archive', 'native_esc_pauses_before_archive', 'f3_opens_over_existing_pause',
    'closing_preserves_existing_pause', 'native_esc_resumes_existing_pause', 'f3_introduces_own_pause', 'archive_pauses_simulation_clock_and_car',
    'closing_restores_running_state', 'reload_complete')

try {
    $saveResult = Invoke-ArchiveProcess 'save' $saveMarkers
    if (-not (Test-Path -LiteralPath $testSavPath -PathType Leaf)) { throw "Isolated binary save was not written: $testSavPath" }
    if (-not (Test-Path -LiteralPath $jsonPath -PathType Leaf)) { throw "Isolated JSON was not written: $jsonPath" }
    $savAfterSave = Get-Fingerprint $testSavPath
    $jsonAfterSave = Get-Fingerprint $jsonPath
    $reloadResult = Invoke-ArchiveProcess 'reload' $reloadMarkers
    if ($savAfterSave -ne (Get-Fingerprint $testSavPath)) { throw 'Reload process unexpectedly changed its saved binary archive.' }
    if ($jsonAfterSave -ne (Get-Fingerprint $jsonPath)) { throw 'Reload process unexpectedly changed its exported JSON archive.' }

    $json = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($json.schema_version -ne 2 -or $json.total_sessions_count -ne 6 -or $json.total_exams_count -ne 3 -or $json.total_practice_count -ne 3 -or
        $json.passed_exams_count -ne 1 -or $json.best_score -ne 100 -or $json.best_duration_seconds -ne 42 -or @($json.recent_sessions).Count -ne 6) {
        throw 'Persisted JSON statistics do not match the six isolated fixtures.'
    }
    if ([math]::Abs($json.pass_rate_percent - (100.0 / 3.0)) -gt 0.0001) { throw 'Practice records changed the examination pass rate.' }
    $records = @($json.recent_sessions)
    if ($records[0].session_id -ne "${caseId}_practice_extra_1" -or $records[0].scored_exam -or $records[0].passed -or
        $records[3].session_id -ne "${caseId}_fatal" -or $records[3].final_score -ne 0 -or $records[3].passed -or @($records[3].deductions).Count -ne 1 -or
        $records[3].deductions[0].points -ne 100 -or $records[3].deductions[0].time_seconds -ne 5.5 -or
        $records[4].session_id -ne "${caseId}_pass" -or -not $records[4].scored_exam -or -not $records[4].passed -or
        $records[5].final_score -ne 70 -or @($records[5].deductions).Count -ne 3 -or -not $records[5].coach_advice) {
        throw 'Reloaded session IDs, scoring, timestamps or deductions do not match the fresh test.'
    }
    if (@($records | Select-Object -ExpandProperty session_id -Unique).Count -ne 6) { throw 'Session IDs were duplicated.' }
    $distanceSum = ($records | Measure-Object -Property distance_meters -Sum).Sum
    if ([math]::Abs($json.total_distance_driven_meters - $distanceSum) -gt 0.01) { throw 'Archived total distance does not equal session distances.' }

    Add-Type -AssemblyName System.Drawing
    $screenshots = @()
    foreach ($name in @('summary', 'history', 'details')) {
        $path = Join-Path $shotDirectory "$name.png"
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Fresh screenshot missing: $path" }
        $image = [System.Drawing.Image]::FromFile($path)
        try {
            if ($image.Width -ne 1280 -or $image.Height -ne 720) { throw "Wrong screenshot dimensions: $name ($($image.Width)x$($image.Height))" }
            $screenshots += [pscustomobject]@{ name = $name; path = $path; width = $image.Width; height = $image.Height; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        } finally { $image.Dispose() }
    }
    if ($learnerSavBefore -ne (Get-Fingerprint $learnerSavPath) -or $learnerJsonBefore -ne (Get-Fingerprint $learnerJsonPath)) {
        throw 'The real learner archive changed during isolated verification.'
    }
    if ($dllHashBefore -ne (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash) { throw 'The project DLL changed while the test was running.' }
    $report = [ordered]@{
        result = 'PASS'; captured_at_utc = (Get-Date).ToUniversalTime().ToString('o'); test_id = $caseId; project = $ProjectFile;
        scope = 'Archive persistence, analysis, keyboard UI and modal pause; synthetic isolated fixtures, not a completed road examination.';
        dll_sha256 = $dllHashBefore; slot = $slotName; binary_save = $testSavPath; binary_sha256 = $savAfterSave; json_path = $jsonPath; json_sha256 = $jsonAfterSave;
        save_process = $saveResult; reload_process = $reloadResult; screenshots = $screenshots;
        real_learner_binary_sha256 = $learnerSavBefore; real_learner_json_sha256 = $learnerJsonBefore; real_learner_archive_unchanged = $true;
        statistics = [ordered]@{ total_sessions = 6; simulated_exams = 3; guided_practices = 3; passed_exams = 1; best_score = 100; pass_rate_percent = $json.pass_rate_percent }
    }
    $reportPath = Join-Path $runDirectory 'verification.json'
    $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding UTF8
    Write-Host "[ArchiveAnalysisTest] Two-process persistence, positive/negative analysis, native UI and archive isolation PASS."
    Write-Host "[ArchiveAnalysisTest] Report: $reportPath"
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
} finally {
    # Preserve isolated evidence for inspection; never clear or restore over learner data.
    if ($learnerSavBefore -ne (Get-Fingerprint $learnerSavPath) -or $learnerJsonBefore -ne (Get-Fingerprint $learnerJsonPath)) {
        [Console]::Error.WriteLine('Learner archive fingerprint changed; no automatic overwrite or deletion was performed.')
    }
}

[CmdletBinding()]
param([string]$UnrealEditor, [string]$ProjectFile, [int]$TimeoutSeconds=120)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if(-not $ProjectFile){$ProjectFile=Join-Path $root 'KeMuSanTraining.uproject'}
$ProjectFile=(Resolve-Path -LiteralPath $ProjectFile).Path
$root=Split-Path -Parent $ProjectFile
if(-not $UnrealEditor){
 if($env:UE_ROOT){$UnrealEditor=Join-Path $env:UE_ROOT 'Engine\Binaries\Win64\UnrealEditor.exe'}
 else{$UnrealEditor='D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe'}
}
$id=[guid]::NewGuid().ToString('N')
$dir=Join-Path $root "Saved\Automation\Physics-$id"
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$log=Join-Path $dir 'physics.log'
$history=Join-Path $dir 'history.json'
$shot=Join-Path $dir 'road-heavy-truck.png'
$dll=Join-Path $root 'Binaries\Win64\UnrealEditor-KeMuSanTraining.dll'
$before=(Get-FileHash -LiteralPath $dll).Hash
$real=@('KeMuSanExamSave.sav','ExamHistory.json')
$saved=@{}
foreach($name in $real){$file=Join-Path $root "Saved\SaveGames\$name";if(Test-Path -LiteralPath $file){$saved[$name]=(Get-FileHash -LiteralPath $file).Hash}else{$saved[$name]='MISSING'}}
$arguments=@(('"'+$ProjectFile+'"'),'/Engine/Maps/Entry','-game','-Unattended','-NoSplash','-WINDOWED','-ResX=1280','-ResY=720','-test-vehicle-physics',('-damage-screenshot-dir="'+$dir+'"'),('-physics-screenshot="'+$shot+'"'),"-exam-save-slot=KeMuSanTest_Physics_$id",('-exam-history-path="'+$history+'"'),('-AbsLog="'+$log+'"'))
$proc=$null
try{
 $proc=Start-Process -FilePath $UnrealEditor -ArgumentList $arguments -WorkingDirectory $root -PassThru
 $null=$proc.Handle
 if(-not $proc.WaitForExit($TimeoutSeconds*1000)){throw "Physics runtime timed out: $log"}
 $proc.WaitForExit()
 if($proc.ExitCode -ne 0){throw "Physics runtime exit=$($proc.ExitCode): $log"}
 $text=Get-Content -LiteralPath $log -Raw -Encoding UTF8
 if($text -match 'Fatal error|Assertion failed|Unhandled Exception|\[KeMuSanPhysicsTest\][^\r\n]*\bFAIL\b'){throw "Physics runtime failed: $log"}
 foreach($case in 0..10){if($text -notmatch "\[KeMuSanPhysicsTest\] case=$case PASS"){throw "Missing fresh impact case $case"}}
 foreach($marker in @('\[KeMuSanDamageTest\] player_crash PASS','\[KeMuSanDamageTest\] repair PASS','\[KeMuSanDamageTest\] recycle_collision PASS','\[KeMuSanDamageTest\] pool_reactivate PASS','\[KeMuSanDamageTest\] roof_contact PASS','\[KeMuSanDynamicsTest\] case=7 PASS')){if($text -notmatch $marker){throw "Missing deformation/roll marker $marker"}}
 foreach($photo in @('dent-before.png','dent-after.png','rollover.png')){if(-not(Test-Path -LiteralPath (Join-Path $dir $photo))){throw "Fresh physics photo missing: $photo"}}
 if($text -notmatch '\[KeMuSanPhysicsTest\] complete PASS failures=0'){throw 'Missing successful completion'}
 foreach($name in $real){$file=Join-Path $root "Saved\SaveGames\$name";$after='MISSING';if(Test-Path -LiteralPath $file){$after=(Get-FileHash -LiteralPath $file).Hash};if($after -ne $saved[$name]){throw "Learner archive changed: $name"}}
 if((Get-FileHash -LiteralPath $dll).Hash -ne $before){throw 'DLL changed during test'}
 if(-not (Test-Path -LiteralPath $shot -PathType Leaf)){throw 'Fresh road/truck screenshot missing'}
 $markers=@($text -split "`r?`n" | Where-Object {$_ -match '\[KeMuSan(Physics|Dynamics|Damage)Test\]'})
 $markers | Write-Output
 @{status='PASS';dll_sha256=$before;log=$log;screenshot=$shot;test_id=$id;cases=$markers;learner_archive_unchanged=$true;scope='Actual Chaos impacts, permanent surface dents, repair/recycle, suspension and turn-induced rollover; controlled initial velocities, native driving verified separately';deformation_photos=@('dent-before.png','dent-after.png','rollover.png')} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dir 'verification.json') -Encoding UTF8
 Write-Output "Physics verification PASS: $dir"
 exit 0
}catch{
 [Console]::Error.WriteLine($_.Exception.Message)
 exit 1
}finally{
 if($proc -and -not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue}
}

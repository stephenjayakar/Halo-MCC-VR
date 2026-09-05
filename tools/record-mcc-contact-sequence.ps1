param(
    [Parameter(Mandatory)][ValidatePattern('^[a-z0-9-]+$')][string]$Name,
    [ValidateSet('W','A','S','D')][string]$ApproachKey='D',
    [ValidateSet('W','A','S','D')][string]$RetreatKey='A',
    [ValidateRange(50,5000)][int]$ApproachMilliseconds=3000,
    [ValidateRange(50,5000)][int]$PressureMilliseconds=1500,
    [ValidateRange(50,5000)][int]$RetreatMilliseconds=4000
)
$ErrorActionPreference='Stop'
$mcc=Get-Process MCC-Win64-Shipping -ErrorAction Stop
if (@($mcc).Count -ne 1) { throw 'Exactly one MCC process is required.' }
$expectedPid=$mcc.Id
$demoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../out/demos')).Path
$started=[DateTime]::UtcNow
$phases=[System.Collections.Generic.List[object]]::new()
$recordScript=Join-Path $PSScriptRoot 'record-mcc-demo.ps1'
$job=Start-Job -ScriptBlock { param($script,$name) & $script -Name $name -Seconds 40 } -ArgumentList $recordScript,$Name
$video=$null
$sequenceError=$null
function Move-Recorded([string]$phase,[string]$key,[int]$duration) {
    $live=Get-Process MCC-Win64-Shipping -ErrorAction Stop
    if (@($live).Count -ne 1 -or $live.Id -ne $expectedPid) { throw 'MCC process changed during recording.' }
    $phases.Add([ordered]@{phase=$phase;key=$key;hold_ms=$duration;started_utc=[DateTime]::UtcNow.ToString('o')})
    & (Join-Path $PSScriptRoot 'send-mcc-keys.ps1') -Background -Keys $key -HoldMilliseconds $duration
}
try {
    $deadline=[DateTime]::UtcNow.AddSeconds(15)
    do {
        $video=Get-ChildItem -LiteralPath $demoRoot -Directory | Where-Object { $_.Name.EndsWith('-'+$Name) -and $_.CreationTimeUtc -ge $started.AddSeconds(-1) } |
            ForEach-Object { Get-Item -LiteralPath (Join-Path $_.FullName 'raw.mp4') -ErrorAction SilentlyContinue } |
            Where-Object Length -GT 0 | Select-Object -First 1
        if ($job.State -ne 'Running') { throw 'Recorder ended before readiness.' }
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Recorder did not become ready.' }
        if (-not $video) { Start-Sleep -Milliseconds 100 }
    } until ($video)
    $phases.Add([ordered]@{phase='recording-file-ready';started_utc=[DateTime]::UtcNow.ToString('o')})
    Start-Sleep -Seconds 2
    Move-Recorded 'approach' $ApproachKey $ApproachMilliseconds
    Start-Sleep -Seconds 3
    Move-Recorded 'continued-pressure' $ApproachKey $PressureMilliseconds
    Start-Sleep -Seconds 3
    Move-Recorded 'retreat' $RetreatKey $RetreatMilliseconds
    Start-Sleep -Seconds 3
    $phases.Add([ordered]@{phase='sequence-complete';started_utc=[DateTime]::UtcNow.ToString('o')})
} catch { $sequenceError=$_.Exception.Message }
finally {
    # Let the bounded recorder finish normally so it restores window state.
    $null=Wait-Job $job -Timeout 55
    $recordOutput=Receive-Job $job -ErrorAction Continue
    if ($job.State -ne 'Completed' -and -not $sequenceError) { $sequenceError='Recorder did not complete successfully.' }
    if ($video) {
        [ordered]@{mcc_pid=$expectedPid;phases=$phases;failure=$sequenceError;functionality_verified=$false;headset_acceptance=$false} |
            ConvertTo-Json -Depth 6 | Set-Content (Join-Path $video.DirectoryName 'sequence.json')
    }
    if ($job.State -in @('Completed','Failed','Stopped')) { Remove-Job $job }
    $recordOutput
}
if ($sequenceError) { throw $sequenceError }

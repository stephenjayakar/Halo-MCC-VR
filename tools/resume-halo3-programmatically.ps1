[CmdletBinding()]
param([ValidateRange(30,300)][int]$TimeoutSeconds = 120,[switch]$MenuOnly)
# State-driven shell navigation, using captured-frame OCR and addressed window
# messages. Unknown pages receive no input. This never chooses a new mission.
$ErrorActionPreference = 'Stop'
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$lastState = ''
while ([DateTime]::UtcNow -lt $deadline) {
    $capture = & "$PSScriptRoot/capture-mcc-window-sequence.ps1" -DurationSeconds 1 -FramesPerSecond 1 -OutputWidth 1280 -Prefix menu-ocr
    $frame = (Get-ChildItem -LiteralPath $capture -Filter *.jpg | Select-Object -First 1).FullName
    $lines = & "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe" -NoProfile -File "$PSScriptRoot/read-mcc-menu-text.ps1" -ImagePath $frame
    if ($LASTEXITCODE -ne 0) { throw 'MCC menu OCR failed.' }
    $text = $lines -join "`n"
    $state = ''
    $keys = @()
    if ($text -match 'MESSAGE OF THE DAY') { $state='message-of-the-day'; $keys=@('Escape') }
    elseif ($text -match 'PRESS.*Enter.*TO START') { $state='start'; $keys=@('Enter') }
    elseif ($text -match 'CAMPAIGNS' -and $text -match 'MULTIPLAYER' -and $text -match 'CREATIVE') {
        $state='main'; $keys=@('Up','Up','Up','Up','Up','Up','Up','Enter')
    }
    elseif ($text -match 'CAMPAIGNS' -and $text -match 'HALO.*REACH' -and $text -match 'HALO.*CE.*ANNIVERSARY' -and $text -match 'HALO.*O[DO]ST') {
        $state='campaign-titles'; $keys=@('Up','Up','Up','Up','Up','Up','Up','Down','Down','Down','Enter')
    }
    elseif ($text -match 'HALO 3' -and $text -match 'BUILT IN') {
        $state='halo3-built-in'; $keys=@('Up','Up','Up','Up','Up','Up','Up','Enter')
    }
    elseif ($lastState -eq 'halo3-built-in' -and $text -match 'QUICKSTART' -and $text -match 'MISSIONS' -and $text -match 'HALO 3') {
        if ($text -match 'RESUME') {
            if ($MenuOnly) {
                Write-Host "Observed Halo 3 Resume available ($frame). MenuOnly: no mission started."
                exit 0
            }
            $state='halo3-resume'; $keys=@('Up','Up','Up','Up','Enter')
        }
        else {
            throw "Observed Halo 3 Campaign menu without Resume ($frame). No mission started; check checkpoint files and Steam cached metadata."
        }
    }
    if ($state -and $state -ne $lastState) {
        Write-Host "Observed MCC menu: $state ($frame)"
        & "$PSScriptRoot/send-mcc-keys.ps1" -Keys $keys -Background
        $lastState = $state
        if ($state -eq 'halo3-resume') { Write-Host 'Halo 3 Resume requested; gameplay readiness must be verified from the runtime log.'; exit 0 }
    }
    Start-Sleep -Seconds 2
}
throw "No recognized next MCC menu before timeout; last state=$lastState. No input was sent to unrecognized pages."

[CmdletBinding()]
param(
    [ValidateRange(30,300)][int]$TimeoutSeconds = 180,
    [switch]$LobbyOnly
)
# State-driven navigation to the currently selected Halo 3 Forge map.
# Every transition requires OCR evidence; unfamiliar pages receive no input.
$ErrorActionPreference = 'Stop'
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$lastState = ''
while ([DateTime]::UtcNow -lt $deadline) {
    $capture = & "$PSScriptRoot/capture-mcc-window-sequence.ps1" -DurationSeconds 1 -FramesPerSecond 1 -OutputWidth 1280 -Prefix forge-menu-ocr
    $frame = (Get-ChildItem -LiteralPath $capture -Filter *.jpg | Select-Object -First 1).FullName
    $lines = & "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe" -NoProfile -File "$PSScriptRoot/read-mcc-menu-text.ps1" -ImagePath $frame
    if ($LASTEXITCODE -ne 0) { throw 'MCC menu OCR failed.' }
    $text = $lines -join "`n"
    $state = ''
    $keys = @()
    if ($text -match 'MESSAGE OF THE DAY') { $state='message-of-the-day'; $keys=@('Escape') }
    elseif ($text -match 'PRESS.*Enter.*TO START') { $state='start'; $keys=@('Enter') }
    elseif ($text -match 'CAMPAIGNS' -and $text -match 'MULTIPLAYER' -and $text -match 'CREATIVE') {
        $state='main'; $keys=@('Up','Up','Up','Up','Up','Up','Up','Down','Down','Enter')
    }
    elseif ($lastState -eq 'main' -and $text -match 'CREATIVE' -and $text -match 'FORGE' -and $text -match 'THEATER' -and $text -notmatch 'HALO.*REACH') {
        $state='creative'; $keys=@('Up','Up','Enter')
    }
    elseif ($text -match 'FORGE' -and $text -match 'HALO.*REACH' -and $text -match 'HALO.*ANNIVERSARY' -and $text -match 'HALO 3') {
        $state='forge-titles'; $keys=@('Up','Up','Up','Up','Down','Down','Enter')
    }
    elseif ($lastState -eq 'forge-titles' -and $text -match 'HALO 3 FORGE' -and $text -match 'CHOOSE MAP' -and $text -match 'START') {
        $state='halo3-forge-lobby'; $keys=@('Right','Right','Right','Enter')
    }
    if ($state -and $state -ne $lastState) {
        Write-Host "Observed MCC menu: $state ($frame)"
        if ($state -eq 'halo3-forge-lobby' -and $LobbyOnly) {
            Write-Host 'Stopped at the verified Forge lobby for visible map selection; Start was not requested.'
            exit 0
        }
        & "$PSScriptRoot/send-mcc-keys.ps1" -Keys $keys -Background
        $lastState = $state
        if ($state -eq 'halo3-forge-lobby') { Write-Host 'Halo 3 Forge Start requested; gameplay readiness must be verified from the runtime log.'; exit 0 }
    }
    Start-Sleep -Seconds 2
}
throw "No recognized next MCC menu before timeout; last state=$lastState. No input was sent to unrecognized pages."

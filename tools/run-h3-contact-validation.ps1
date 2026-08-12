[CmdletBinding()]
param(
    [ValidateSet(
        'weapon-scoop',
        'equipment-scoop',
        'left-grab',
        'crate-nudge',
        'vehicle-nudge',
        'visible-weapon-nudge',
        'visible-weapon-gap',
        'wall',
        'decorator-wall',
        'melee')]
    [string]$Test = 'equipment-scoop',

    [ValidateRange(30, 300)]
    [int]$ValidationTimeoutSeconds = 120,

    [ValidateRange(30, 600)]
    [int]$MenuControlTimeoutSeconds = 300,

    [ValidateRange(0, 300)]
    [int]$PostPassHoldSeconds = 0,

    [ValidateSet('Auto', 'Construct', 'HighGround', 'Valhalla')]
    [string]$ForgeMap = 'Auto',

    [switch]$ExternalMenuControl
)

# Runs one Halo 3 physical-contact transaction through SteamVR's null driver.
# Visible external menu control is mandatory: MCC remembers the selected title,
# so a fixed blind key sequence can enter another game even after returning to
# the shell. The user's exact SteamVR settings are copied before any change and
# restored in finally, including launch or menu-control failures. SteamVR stays
# stopped after restoration so a late null-driver process cannot rewrite the
# restored file. This is a diagnostic tool only. A passing null-driver run is
# never headset acceptance.

$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot = Join-Path $repoRoot 'out\debug-openxr'
$steamVrSettings =
    'C:\Program Files (x86)\Steam\config\steamvr.vrsettings'
$vrMonitor =
    'C:\Program Files (x86)\Steam\steamapps\common\SteamVR\bin\win64\vrmonitor.exe'
$mccProcessName = 'MCC-Win64-Shipping'
$steamInstallRoots = @(
    'E:\SteamLibrary\steamapps\common\Halo The Master Chief Collection',
    'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection'
)

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Stop-SteamVr {
    $names = @(
        'vrmonitor',
        'vrserver',
        'vrcompositor',
        'vrdashboard',
        'vrwebhelper',
        'steamtours'
    )
    # vrmonitor can spawn vrserver or SteamVR Home (steamtours) a moment after
    # an apparently clean stop. Null-driver Home failures also remain alive
    # after vrserver exits and reserve about 380 MiB each. Require five
    # continuous seconds with every SteamVR-owned process absent before
    # restoring the user's exact settings file.
    $quietSamples = 0
    for ($attempt = 0; $attempt -lt 60; ++$attempt) {
        $processes = Get-Process -Name $names -ErrorAction SilentlyContinue
        if (-not $processes) {
            ++$quietSamples
            if ($quietSamples -ge 10) {
                return
            }
        }
        else {
            $quietSamples = 0
            $processes | Stop-Process -Force
        }
        Start-Sleep -Milliseconds 500
    }
    throw 'SteamVR processes did not stop.'
}

function Stop-Mcc {
    $processes = @(Get-Process $mccProcessName -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) { return }
    foreach ($process in $processes) {
        $null = $process.CloseMainWindow()
    }
    for ($attempt = 0; $attempt -lt 10; ++$attempt) {
        Start-Sleep -Seconds 1
        $processes = @(
            Get-Process $mccProcessName -ErrorAction SilentlyContinue)
        if ($processes.Count -eq 0) { return }
    }
    $processes | Stop-Process
    Start-Sleep -Seconds 3
    if (Get-Process $mccProcessName -ErrorAction SilentlyContinue) {
        throw 'MCC did not close.'
    }
}

function Get-UniqueMccWindowProcess {
    $processes = @(
        Get-Process $mccProcessName -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 })
    if ($processes.Count -ne 1) { return $null }
    return $processes[0]
}

function Save-DesktopScreenshot([string]$Path) {
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Windows.Forms
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen(
            $bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Wait-Until(
    [scriptblock]$Condition,
    [int]$TimeoutSeconds,
    [string]$FailureMessage) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (& $Condition) { return }
        Start-Sleep -Seconds 1
    } while ([DateTime]::UtcNow -lt $deadline)
    throw $FailureMessage
}

function Get-NewLogText([string]$Path, [DateTime]$StartedUtc) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
    $item = Get-Item -LiteralPath $Path
    if ($item.LastWriteTimeUtc -lt $StartedUtc.AddSeconds(-2)) { return '' }
    $stream = New-Object IO.FileStream(
        $Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        [IO.FileShare]::ReadWrite)
    $reader = New-Object IO.StreamReader($stream)
    try { return $reader.ReadToEnd() }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Test-Halo3ContactRuntimeReady([string]$Text) {
    return $Text -match
        'H3 physical contact: optional native bindings installed' -or
        $Text -match 'H3 physical contact status:'
}

function Test-Halo3LevelLoadGateStalled([string]$Text) {
    if (Test-Halo3ContactRuntimeReady $Text) { return $false }
    $matches = [regex]::Matches(
        $Text, 'Halo 3 level-load gate: holding install after ([0-9]+) ms')
    if ($matches.Count -eq 0) { return $false }
    return [int]$matches[$matches.Count - 1].Groups[1].Value -ge 45000
}

function Get-WrongSupportedTitle([string]$Text) {
    $matches = [regex]::Matches(
        $Text, 'Title adapter: detected supported title ([^\r\n]+)')
    foreach ($match in $matches) {
        $title = $match.Groups[1].Value
        if ($title -cne 'Halo 3 (halo3.dll)') { return $title }
    }
    return $null
}

function Get-LatestContactStatusLine([string]$Text, [int]$Kind = -1) {
    $lines = $Text -split "`r?`n" | Where-Object {
        $_ -like '*H3 physical contact status:*' -and
        $_ -like '*shapeSource=1*' -and
        $_ -match 'commandStatus=2' -and
        $_ -match 'impulses=([1-9][0-9]*)'
    }
    if ($Kind -ge 0) {
        $lines = $lines | Where-Object { $_ -match "kind=$Kind(\s|$)" }
    }
    return $lines | Select-Object -Last 1
}

function Test-ContactHaptic([string]$Line) {
    if ($Line -notmatch 'contactHaptic=([0-9]+(?:\.[0-9]+)?)') {
        return $false
    }
    return [double]$Matches[1] -gt 0.0
}

function Test-DetailedTargetGeometrySeen([string]$Text) {
    return $Text -match
        'H3 physical contact status:.*targetShapeSource=1.*targetDetailed=([1-9][0-9]*)'
}

function Test-DynamicBodyConstraintSeen([string]$Text) {
    return $Text -match
        'H3 physical contact status:.*bodyConstraints=([1-9][0-9]*).*bodyPeak=(?!0\.000m)([0-9]+\.[0-9]{3})m'
}

function Test-SlowResult([string]$Text, [int]$Kind, [bool]$RequireScoop) {
    if ($Text -match 'H3 physical contact status:.*melees=[1-9][0-9]*') {
        return $false
    }
    $status = ($Text -split "`r?`n") |
        Where-Object {
            $_ -match 'H3 physical contact status:' -and
            $_ -match "kind=$Kind(\s|$)" -and
            $_ -match 'melees=0' -and
            (Test-ContactHaptic $_)
        } |
        Select-Object -Last 1
    if (-not $status) { return $false }
    if (-not $RequireScoop) { return $true }
    return $Text -match
        "H3 physical contact DEBUG RIG:.*kind=$Kind.*validated=1"
}

function Test-VehicleReleaseResult([string]$Text) {
    if ($Text -match 'H3 physical contact status:.*melees=[1-9][0-9]*') {
        return $false
    }
    if (-not (Test-DetailedTargetGeometrySeen $Text)) {
        return $false
    }
    # Do not accept an earlier clean line while a later contact has queued a
    # native melee. This caught a real slow Mongoose shove where target rebound
    # raised relative speed to 1.68 m/s while the weapon itself moved 0.88 m/s.
    $latestStatus = ($Text -split "`r?`n") | Where-Object {
        $_ -match 'H3 physical contact status:' -and
        $_ -match 'target=0x(?!FFFFFFFF)[0-9A-F]+ kind=1 '
    } | Select-Object -Last 1
    if (-not $latestStatus -or
        $latestStatus -notmatch 'melees=0 ' -or
        $latestStatus -notmatch 'meleeStatus=0 ') {
        return $false
    }
    $lines = $Text -split "`r?`n" | Where-Object {
        $_ -match 'H3 physical contact status:' -and
        $_ -match 'impulses=([1-9][0-9]*) releases=([1-9][0-9]*) melees=0 ' -and
        $_ -match 'command=([1-9][0-9]*) applied=([1-9][0-9]*) commandStatus=2' -and
        $_ -match 'target=0x(?!FFFFFFFF)[0-9A-F]+ kind=1 ' -and
        $_ -match 'targetMass=([0-9]+(?:\.[0-9]+)?) targetMotion=4 ' -and
        $_ -match 'targetDetailed=([1-9][0-9]*) targetFallback=0' -and
        $_ -match 'lastImpulse=\((-?[0-9]+(?:\.[0-9]+)?) (-?[0-9]+(?:\.[0-9]+)?) (-?[0-9]+(?:\.[0-9]+)?)\)'
    }
    foreach ($line in $lines) {
        $null = $line -match 'command=([1-9][0-9]*) applied=([1-9][0-9]*) commandStatus=2'
        $command = [int]$Matches[1]
        $applied = [int]$Matches[2]
        $null = $line -match 'targetMass=([0-9]+(?:\.[0-9]+)?) targetMotion=4 '
        $mass = [double]$Matches[1]
        $null = $line -match 'lastImpulse=\((-?[0-9]+(?:\.[0-9]+)?) (-?[0-9]+(?:\.[0-9]+)?) (-?[0-9]+(?:\.[0-9]+)?)\)'
        $x = [double]$Matches[1]
        $y = [double]$Matches[2]
        $z = [double]$Matches[3]
        $magnitude = [math]::Sqrt($x * $x + $y * $y + $z * $z)
        if ($command -eq $applied -and $mass -gt 0.0 -and
            $magnitude -gt 0.0 -and $magnitude -le 1.0001) {
            return $true
        }
    }
    return $false
}

function Test-LeftGrabResult([string]$Text) {
    if ($Text -notmatch 'H3 physical contact DEBUG LEFT GRAB enabled') {
        return $false
    }
    # Acquisition publishes the exact active handle and native mass. Release
    # deliberately clears both fields, so those two states must be proven by
    # separate status samples instead of one impossible post-release line.
    $held = ($Text -split "`r?`n") | Where-Object {
        $_ -match 'H3 left grab status:' -and
        $_ -match 'bindings=1' -and
        $_ -match 'active=0x(?!FFFFFFFF)[0-9A-F]+' -and
        $_ -match 'mass=(?!0\.000)([0-9]+\.[0-9]{3})' -and
        $_ -match 'acquisitions=([1-9][0-9]*)'
    } | Select-Object -Last 1
    $released = ($Text -split "`r?`n") | Where-Object {
        $_ -match 'H3 left grab status:' -and
        $_ -match 'bindings=1' -and
        $_ -match 'acquisitions=([1-9][0-9]*)' -and
        $_ -match 'commands=([1-9][0-9]*)' -and
        $_ -match 'applied=([1-9][0-9]*)' -and
        $_ -match 'releases=([1-9][0-9]*)' -and
        $_ -match 'serial=([1-9][0-9]*) appliedSerial=([1-9][0-9]*)'
    } | Select-Object -Last 1
    if (-not $held -or -not $released) { return $false }
    return $Text -match
        'H3 physical contact DEBUG RIG:.*kind=3.*validated=1.*peakLift=(?!0\.000m)([0-9]+\.[0-9]{3})m.*peakCarry=(?!0\.000m)([0-9]+\.[0-9]{3})m.*releaseSpeed=(?!0\.000m/s)([0-9]+\.[0-9]{3})m/s'
}

function Test-VisibleWeaponGapResult([string]$Text) {
    # These counters are cumulative. Judge only the newest sample and require
    # a sustained replay. An earlier zero-overlap line is not a completed pass.
    $line = ($Text -split "`r?`n") | Where-Object {
        $_ -match 'H3 physical contact DEBUG VISIBLE REPLAY:'
    } | Select-Object -Last 1
    if (-not $line) { return $false }
    if ($line -notmatch
        'exactPalettes=([0-9]+).*correctedPalettes=([0-9]+).*approvedPalettes=([0-9]+) heldPalettes=([0-9]+).*directOverlaps=([0-9]+) directSeparations=([0-9]+).*geometryOverlaps=([0-9]+) geometrySeparations=([0-9]+).*solidOverlaps=([0-9]+) solidSeparations=([0-9]+)') {
        return $false
    }
    $exactPalettes = [int]$Matches[1]
    $correctedPalettes = [int]$Matches[2]
    $approvedPalettes = [int]$Matches[3]
    $heldPalettes = [int]$Matches[4]
    $directOverlaps = [int]$Matches[5]
    $directSeparations = [int]$Matches[6]
    $geometryOverlaps = [int]$Matches[7]
    $geometrySeparations = [int]$Matches[8]
    $solidOverlaps = [int]$Matches[9]
    return $exactPalettes -ge 8000 -and
        $correctedPalettes -ge 2500 -and
        $approvedPalettes -ge 2500 -and
        $heldPalettes -ge 8000 -and
        $directSeparations -ge 2500 -and
        $geometrySeparations -ge 2500 -and
        $geometryOverlaps -eq 0
}

function Test-VisibleWeaponGapFailure([string]$Text) {
    # The physical query has a 1.25 mm contact skin, so direct overlap includes
    # legitimate near-touch. The convex volume is also deliberately conservative
    # and fills concave authored gaps. Only zero-radius authored-triangle overlap
    # is proof that the visible surfaces entered one another.
    $line = ($Text -split "`r?`n") | Where-Object {
        $_ -match 'H3 physical contact DEBUG VISIBLE REPLAY:'
    } | Select-Object -Last 1
    return $line -and $line -match 'geometryOverlaps=([1-9][0-9]*)'
}

function Test-ValidationResult([string]$Text, [string]$Name) {
    switch ($Name) {
        'weapon-scoop' {
            return (Test-DetailedTargetGeometrySeen $Text) -and
                (Test-DynamicBodyConstraintSeen $Text) -and
                (Test-SlowResult $Text 2 $true)
        }
        'equipment-scoop' {
            return Test-SlowResult $Text 3 $true
        }
        'left-grab' {
            return Test-LeftGrabResult $Text
        }
        'crate-nudge' {
            return Test-SlowResult $Text 10 $false
        }
        'vehicle-nudge' {
            return (Test-DynamicBodyConstraintSeen $Text) -and
                (Test-VehicleReleaseResult $Text)
        }
        'visible-weapon-nudge' {
            return (Test-DetailedTargetGeometrySeen $Text) -and
                (Test-DynamicBodyConstraintSeen $Text) -and
                (Test-SlowResult $Text 2 $false) -and
                $Text -match
                    'H3 physical contact DEBUG VISIBLE REPLAY: palettes=([1-9][0-9]*)'
        }
        'visible-weapon-gap' {
            return (Test-DetailedTargetGeometrySeen $Text) -and
                (Test-VisibleWeaponGapResult $Text)
        }
        'wall' {
            return $Text -match
                'H3 physical contact DEBUG WALL:.*structureValidated=1.*objectValidated=1'
        }
        'decorator-wall' {
            return $Text -match
                'H3 physical contact status:.*decoratorSelfTest=([1-9][0-9]*)'
        }
        'melee' {
            $status = Get-LatestContactStatusLine $Text
            return $status -and $status -match 'melees=([1-9][0-9]*)' -and
                $status -match 'meleeStatus=2' -and
                (Test-ContactHaptic $status)
        }
    }
    return $false
}

if (-not (Test-Path -LiteralPath $steamVrSettings -PathType Leaf)) {
    throw "SteamVR settings were not found: $steamVrSettings"
}
if (-not (Test-Path -LiteralPath $vrMonitor -PathType Leaf)) {
    throw "SteamVR vrmonitor was not found: $vrMonitor"
}
if (-not $ExternalMenuControl) {
    throw ('Blind MCC menu control is disabled because the shell remembers ' +
        'the selected title. Re-run with -ExternalMenuControl and navigate ' +
        'from visible Windows state.')
}
if (Get-Process $mccProcessName -ErrorAction SilentlyContinue) {
    throw 'Close MCC before starting unattended validation.'
}

$installRoot = $steamInstallRoots | Where-Object {
    Test-Path -LiteralPath (Join-Path $_ 'MCC\Binaries\Win64\MCC-Win64-Shipping.exe')
} | Select-Object -First 1
if (-not $installRoot) {
    throw 'No supported Steam MCC installation was found.'
}
$modRoot = Join-Path $installRoot 'Halo_MCC_VR'
$launcher = Join-Path $modRoot 'halo3xr_launcher.exe'
$runtimeLog = Join-Path $modRoot 'halo3xr.log'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) {
    throw "The installed MCC VR launcher was not found: $launcher"
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
$stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmssfff'Z'")
$backupPath = Join-Path $outputRoot "steamvr-before-null-$stamp.json"
$savedLogPath = Join-Path $outputRoot "$stamp-$Test.log"
$resultPath = Join-Path $outputRoot "$stamp-$Test-result.json"
$failureScreenshot = Join-Path $outputRoot "$stamp-$Test-failure.png"
$successScreenshot = Join-Path $outputRoot "$stamp-$Test-success.png"
$backupHash = $null
$startedUtc = $null
$passed = $false
$failure = $null

$debugVariables = @(
    'HALOMCCVR_H3_CONTACT_DEBUG_RIG',
    'HALOMCCVR_H3_CONTACT_DEBUG_SCOOP',
    'HALOMCCVR_H3_CONTACT_DEBUG_LEFT_GRAB',
    'HALOMCCVR_H3_CONTACT_DEBUG_KIND',
    'HALOMCCVR_H3_CONTACT_DEBUG_MELEE',
    'HALOMCCVR_H3_CONTACT_DEBUG_WALL',
    'HALOMCCVR_H3_CONTACT_DEBUG_VISIBLE',
    'HALOMCCVR_H3_CONTACT_DEBUG_VISIBLE_EXACT'
)
$savedEnvironment = @{}
foreach ($name in $debugVariables) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable(
        $name, [EnvironmentVariableTarget]::Process)
}

try {
    Stop-SteamVr
    Copy-Item -LiteralPath $steamVrSettings -Destination $backupPath
    $backupHash = Get-Sha256 $backupPath

    $settings = Get-Content -Raw -LiteralPath $steamVrSettings |
        ConvertFrom-Json
    $settings.driver_null.enable = $true
    $settings.steamvr.forcedDriver = 'null'
    $settings.steamvr.requireHmd = $false
    $settings | ConvertTo-Json -Depth 32 |
        Set-Content -LiteralPath $steamVrSettings -Encoding UTF8
    $nullSettings = Get-Content -Raw -LiteralPath $steamVrSettings |
        ConvertFrom-Json
    if ($nullSettings.driver_null.enable -ne $true -or
        [string]$nullSettings.steamvr.forcedDriver -cne 'null' -or
        $nullSettings.steamvr.requireHmd -ne $false) {
        throw 'SteamVR null-driver settings did not verify.'
    }

    Start-Process -FilePath $vrMonitor -WindowStyle Hidden
    Wait-Until {
        [bool](Get-Process vrserver -ErrorAction SilentlyContinue)
    } 30 'SteamVR null driver did not start.'

    foreach ($name in $debugVariables) {
        [Environment]::SetEnvironmentVariable(
            $name, $null, [EnvironmentVariableTarget]::Process)
    }
    $env:HALOMCCVR_H3_CONTACT_DEBUG_RIG = '1'
    switch ($Test) {
        'weapon-scoop' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_SCOOP = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '2'
        }
        'equipment-scoop' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_SCOOP = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '3'
        }
        'left-grab' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_SCOOP = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_LEFT_GRAB = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '3'
        }
        'crate-nudge' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_SCOOP = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '10'
        }
        'vehicle-nudge' {
            # A vehicle is a heavy nudge target, not a scoop-and-toss target.
            # The scoop path crosses the full vehicle and then rejects it for
            # failing loose-prop lift thresholds, creating artificial 1 m
            # constraint peaks and target-to-target teleports.
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '1'
        }
        'visible-weapon-nudge' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_VISIBLE = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '2'
        }
        'visible-weapon-gap' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_VISIBLE_EXACT = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '2'
        }
        'wall' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_WALL = '1'
        }
        'decorator-wall' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_WALL = '1'
        }
        'melee' {
            $env:HALOMCCVR_H3_CONTACT_DEBUG_MELEE = '1'
            $env:HALOMCCVR_H3_CONTACT_DEBUG_KIND = '2'
        }
    }

    $startedUtc = [DateTime]::UtcNow
    $maxLaunchAttempts = if ($ExternalMenuControl) { 1 } else { 2 }
    for ($launchAttempt = 1;
         $launchAttempt -le $maxLaunchAttempts;
         ++$launchAttempt) {
        Start-Process -FilePath $launcher
        Wait-Until {
            [bool](Get-UniqueMccWindowProcess)
        } 45 'MCC did not open one unique controllable window.'
        Start-Sleep -Seconds 15

        if (-not $ExternalMenuControl) {
            if (-not ([System.Management.Automation.PSTypeName]'HaloMccVrContactInput').Type) {
                Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class HaloMccVrContactInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct KeyboardInput {
        public ushort virtualKey, scanCode;
        public uint flags, time;
        public UIntPtr extraInfo;
    }
    [StructLayout(LayoutKind.Explicit, Size=40)]
    public struct Input {
        [FieldOffset(0)] public uint type;
        [FieldOffset(8)] public KeyboardInput keyboard;
    }
    [DllImport("user32.dll")]
    static extern uint SendInput(uint count, Input[] inputs, int size);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int left, top, right, bottom; }
    [DllImport("user32.dll")]
    static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")]
    static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")]
    static extern void mouse_event(
        uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);
    public static uint Key(ushort scanCode, bool extended) {
        var inputs = new Input[2];
        inputs[0].type = 1;
        inputs[0].keyboard.scanCode = scanCode;
        inputs[0].keyboard.flags = (uint)(8 | (extended ? 1 : 0));
        inputs[1].type = 1;
        inputs[1].keyboard.scanCode = scanCode;
        inputs[1].keyboard.flags = (uint)(10 | (extended ? 1 : 0));
        return SendInput(2, inputs, 40);
    }
    public static bool ClickRelative(IntPtr window, double x, double y) {
        Rect rect;
        if (!GetWindowRect(window, out rect)) return false;
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) return false;
        int px = rect.left + (int)Math.Round(width * x);
        int py = rect.top + (int)Math.Round(height * y);
        SetForegroundWindow(window);
        if (!SetCursorPos(px, py)) return false;
        mouse_event(0x0002, 0, 0, 0, UIntPtr.Zero);
        mouse_event(0x0004, 0, 0, 0, UIntPtr.Zero);
        return true;
    }
}
'@
            }
        function Send-ScanCode([uint16]$ScanCode, [bool]$Extended = $false) {
            $written = [HaloMccVrContactInput]::Key($ScanCode, $Extended)
            if ($written -ne 2) {
                throw "SendInput wrote $written of 2 events."
            }
            Start-Sleep -Milliseconds 350
        }
        function Send-Enter { Send-ScanCode 0x1C }
        function Send-Escape { Send-ScanCode 0x01 }
        function Send-Down { Send-ScanCode 0x50 $true }
        function Send-Right { Send-ScanCode 0x4D $true }

        $mcc = Get-UniqueMccWindowProcess
        if (-not $mcc) {
            throw 'MCC did not expose one unique controllable window.'
        }
        $null = [HaloMccVrContactInput]::SetForegroundWindow(
            $mcc.MainWindowHandle)
        # MCC remembers the last selected title and activity. This legacy path
        # is retained for known menu state only. ExternalMenuControl lets an
        # observer read the visible menu before acting.
        for ($attempt = 0; $attempt -lt 8; ++$attempt) {
            Send-Escape
            Start-Sleep -Milliseconds 650
        }
        Start-Sleep -Seconds 2
        Send-Enter
        Start-Sleep -Seconds 7
        Send-Down
        Send-Down
        Send-Enter
        Start-Sleep -Seconds 3
        Send-Enter
        Start-Sleep -Seconds 3
        Send-Down
        Send-Down
        Send-Enter
        Start-Sleep -Seconds 3

        $selectedMap = $ForgeMap
        if ($selectedMap -eq 'Auto') {
            $selectedMap = if ($Test -eq 'vehicle-nudge') {
                'HighGround'
            } else {
                'Construct'
            }
        }
        # Enter the map carousel explicitly. Construct is 1/27, High Ground
        # is 4/27, and Valhalla is 11/27 in the visible Halo 3 list.
        Send-Down
        $mapRightCount = switch ($selectedMap) {
            'Construct' { 0 }
            'HighGround' { 3 }
            'Valhalla' { 10 }
        }
        for ($mapIndex = 0; $mapIndex -lt $mapRightCount; ++$mapIndex) {
            Send-Right
        }
        # Confirm the map. MCC advances into the game-type carousel with the
        # built-in Forge type selected. Confirm it explicitly; moving Right
        # here selects Escalation Slayer and caused earlier false Forge runs.
        Send-Enter
        Start-Sleep -Seconds 3
        Send-Enter
        Start-Sleep -Seconds 3
        # The Forge confirmation advances to Options. Accepting the defaults
        # advances directly to Launch Game with its Start row selected. A
        # visible Valhalla replay on 2026-08-10 proved that exactly one Enter
        # starts loading; the previous Right + Enter + Enter sequence could
        # leave MCC in the shell instead of starting Halo 3.
        Send-Enter
        Start-Sleep -Seconds 2
        Send-Enter
        # MCC can visibly accept the Launch Game panel while dropping this
        # first scan-code during the shell transition.  Do not blindly send a
        # second Enter: if the first one worked, that input would land in the
        # loading game.  Wait long enough for the measured ~20 second Halo 3
        # module admission, then retry only when the runtime log proves that
        # the title never started.
        $halo3Started = $false
        for ($attempt = 0; $attempt -lt 50; ++$attempt) {
            Start-Sleep -Milliseconds 500
            $launchText = Get-NewLogText $runtimeLog $startedUtc
            if ($launchText -match
                'Title adapter: detected supported title Halo 3') {
                $halo3Started = $true
                break
            }
        }
        if (-not $halo3Started) {
            $mcc = Get-UniqueMccWindowProcess
            if (-not $mcc) {
                throw 'MCC did not expose one unique recovery window.'
            }
            $null = [HaloMccVrContactInput]::SetForegroundWindow(
                $mcc.MainWindowHandle)
            Send-Enter
        }
    }
    else {
        Write-Host 'MCC is ready for visible external Forge menu control.'
    }

        Wait-Until {
            $text = Get-NewLogText $runtimeLog $startedUtc
            $wrongTitle = Get-WrongSupportedTitle $text
            if ($wrongTitle) {
                throw "Menu control launched $wrongTitle; only Halo 3 Forge is allowed."
            }
            $text -match 'Title adapter: detected supported title Halo 3'
        } $MenuControlTimeoutSeconds 'Menu control did not start Halo 3.'

        if ($ExternalMenuControl) { break }

        $contactRuntimeReady = $false
        $levelLoadGateStalled = $false
        $runtimeDeadline = [DateTime]::UtcNow.AddSeconds(70)
        do {
            $text = Get-NewLogText $runtimeLog $startedUtc
            if (Test-Halo3ContactRuntimeReady $text) {
                $contactRuntimeReady = $true
                break
            }
            if (Test-Halo3LevelLoadGateStalled $text) {
                $levelLoadGateStalled = $true
                break
            }
            Start-Sleep -Seconds 1
        } while ([DateTime]::UtcNow -lt $runtimeDeadline)

        if ($contactRuntimeReady) { break }
        if ($launchAttempt -eq $maxLaunchAttempts) {
            if ($levelLoadGateStalled) {
                throw "Halo 3 remained in the level-load gate after $launchAttempt launch attempts."
            }
            throw "Halo 3 contact runtime did not become ready after $launchAttempt launch attempts."
        }

        Write-Host (
            "Halo 3 did not leave its level-load gate; restarting MCC " +
            "for launch attempt $($launchAttempt + 1) of $maxLaunchAttempts.")
        Stop-Mcc
        Start-Sleep -Seconds 5
    }

    Wait-Until {
        $text = Get-NewLogText $runtimeLog $startedUtc
        if ($Test -eq 'visible-weapon-gap' -and
            (Test-VisibleWeaponGapFailure $text)) {
            throw 'Halo 3 visible-weapon-gap recorded a cumulative direct overlap.'
        }
        Test-ValidationResult $text $Test
    } $ValidationTimeoutSeconds "Halo 3 $Test did not reach its pass condition."

    if ($PostPassHoldSeconds -gt 0) {
        Write-Host "Holding the validated Forge process for $PostPassHoldSeconds seconds."
        Start-Sleep -Seconds $PostPassHoldSeconds
    }
    $text = Get-NewLogText $runtimeLog $startedUtc
    if ($Test -eq 'visible-weapon-gap' -and
        (Test-VisibleWeaponGapFailure $text)) {
        throw 'Halo 3 visible-weapon-gap recorded a cumulative direct overlap during the post-pass hold.'
    }
    if (-not (Test-ValidationResult $text $Test)) {
        throw "Halo 3 $Test lost its pass condition during the post-pass hold."
    }
    [IO.File]::WriteAllText($savedLogPath, $text)
    if ($Test -in @('visible-weapon-nudge', 'visible-weapon-gap')) {
        Save-DesktopScreenshot $successScreenshot
    }
    $passed = $true
}
catch {
    $failure = $_.Exception.Message
    try { Save-DesktopScreenshot $failureScreenshot } catch {}
    if ($startedUtc) {
        try {
            $text = Get-NewLogText $runtimeLog $startedUtc
            if ($text) { [IO.File]::WriteAllText($savedLogPath, $text) }
        }
        catch {}
    }
}
finally {
    foreach ($name in $debugVariables) {
        [Environment]::SetEnvironmentVariable(
            $name, $savedEnvironment[$name],
            [EnvironmentVariableTarget]::Process)
    }
    try { Stop-Mcc } catch {
        if (-not $failure) { $failure = $_.Exception.Message }
        $passed = $false
    }
    try {
        Stop-SteamVr
        if (-not $backupHash -or
            (Get-Sha256 $backupPath) -cne $backupHash) {
            throw 'The SteamVR settings backup changed.'
        }
        Copy-Item -LiteralPath $backupPath -Destination $steamVrSettings -Force
        if ((Get-Sha256 $steamVrSettings) -cne $backupHash) {
            throw 'The restored SteamVR settings hash did not match.'
        }
        $restored = Get-Content -Raw -LiteralPath $steamVrSettings |
            ConvertFrom-Json
        if ($restored.driver_null.enable -ne $false -or
            [string]$restored.steamvr.forcedDriver -ne '' -or
            $restored.steamvr.requireHmd -ne $true) {
            throw 'Restored SteamVR settings are not in real-headset mode.'
        }
    }
    catch {
        if (-not $failure) { $failure = $_.Exception.Message }
        $passed = $false
    }
}

$runtimeSourceCommit = $null
if (Test-Path -LiteralPath $savedLogPath -PathType Leaf) {
    $savedText = [IO.File]::ReadAllText($savedLogPath)
    if ($savedText -match '\(source ([0-9a-f]{40}),') {
        $runtimeSourceCommit = $Matches[1]
    }
}
$result = [ordered]@{
    schema_version = 1
    test = $Test
    passed = $passed
    source_commit = $runtimeSourceCommit
    validator_source_commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    installed_dll_sha256 = Get-Sha256 (Join-Path $modRoot 'halo3xr.dll')
    installed_launcher_sha256 =
        Get-Sha256 (Join-Path $modRoot 'halo3xr_launcher.exe')
    mcc_edition = 'Steam'
    menu_control = $(if ($ExternalMenuControl) {
        'external-visible-state'
    } else {
        'legacy-fixed-sequence'
    })
    openxr_runtime = 'SteamVR null driver'
    headset = 'Null Model Number'
    started_utc = $(if ($startedUtc) { $startedUtc.ToString('o') } else { $null })
    completed_utc = [DateTime]::UtcNow.ToString('o')
    null_driver_result_is_headset_acceptance = $false
    steamvr_backup = $backupPath
    steamvr_backup_sha256 = $backupHash
    runtime_log = $(if (Test-Path -LiteralPath $savedLogPath) {
        $savedLogPath
    } else { $null })
    runtime_log_sha256 = $(if (Test-Path -LiteralPath $savedLogPath) {
        Get-Sha256 $savedLogPath
    } else { $null })
    failure_screenshot = $(if (Test-Path -LiteralPath $failureScreenshot) {
        $failureScreenshot
    } else { $null })
    success_screenshot = $(if (Test-Path -LiteralPath $successScreenshot) {
        $successScreenshot
    } else { $null })
    failure = $failure
}
$result | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath $resultPath -Encoding UTF8
$result | Format-List
"Result: $resultPath"

if (-not $passed) { exit 1 }

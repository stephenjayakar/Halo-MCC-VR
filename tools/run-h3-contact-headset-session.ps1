[CmdletBinding()]
param(
    [ValidateSet('Auto', 'Steam', 'MicrosoftStore')]
    [string]$Edition = 'Auto',

    [ValidateRange(15, 300)]
    [int]$LaunchTimeoutSeconds = 120,

    [ValidateRange(5, 360)]
    [int]$MaximumSessionMinutes = 180,

    [string]$OutputRoot,
    [switch]$VerifyOnly,
    [switch]$SelfTest
)

# Safe wrapper for a real-headset Halo 3 contact play session. It never edits
# SteamVR settings, game files, configuration, or Easy Anti-Cheat. It verifies
# that the null driver is off, identifies the exact installed candidate, starts
# or attaches to MCC, preserves the completed log, and invokes the read-only
# contact analyzer. The player still owns all menu and in-headset interaction.

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $repoRoot 'out\headset-sessions'
}
$steamVrSettings =
    'C:\Program Files (x86)\Steam\config\steamvr.vrsettings'

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Get-InstallDefinitions {
    return @(
        [pscustomobject]@{
            edition = 'Steam'
            root = 'E:\SteamLibrary\steamapps\common\Halo The Master Chief Collection'
            process = 'MCC-Win64-Shipping'
            executable = 'MCC\Binaries\Win64\MCC-Win64-Shipping.exe'
        },
        [pscustomobject]@{
            edition = 'Steam'
            root = 'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection'
            process = 'MCC-Win64-Shipping'
            executable = 'MCC\Binaries\Win64\MCC-Win64-Shipping.exe'
        },
        [pscustomobject]@{
            edition = 'MicrosoftStore'
            root = 'N:\XBOX\Halo- The Master Chief Collection\Content'
            process = 'MCCWinStore-Win64-Shipping'
            executable = 'MCC\Binaries\Win64\MCCWinStore-Win64-Shipping.exe'
        }
    )
}

function Get-PresentInstalls {
    $present = @()
    foreach ($definition in Get-InstallDefinitions) {
        $rootPresent = Test-Path -LiteralPath $definition.root -PathType Container -ErrorAction SilentlyContinue
        if (-not $rootPresent) {
            continue
        }
        $modRoot = Join-Path $definition.root 'Halo_MCC_VR'
        $gameExe = Join-Path $definition.root $definition.executable
        $launcher = Join-Path $modRoot 'halo3xr_launcher.exe'
        $dll = Join-Path $modRoot 'halo3xr.dll'
        if ((Test-Path -LiteralPath $gameExe -PathType Leaf) -and
            (Test-Path -LiteralPath $launcher -PathType Leaf) -and
            (Test-Path -LiteralPath $dll -PathType Leaf)) {
            $present += [pscustomobject]@{
                edition = $definition.edition
                root = $definition.root
                process = $definition.process
                game_exe = $gameExe
                mod_root = $modRoot
                launcher = $launcher
                dll = $dll
                config = Join-Path $modRoot 'halomccvr.cfg'
                log = Join-Path $modRoot 'halo3xr.log'
            }
        }
    }
    return $present
}

function Test-NormalSteamVrSettings([string]$Text) {
    try { $settings = $Text | ConvertFrom-Json }
    catch { return $false }
    if ($null -eq $settings.driver_null -or $null -eq $settings.steamvr) {
        return $false
    }
    return $settings.driver_null.enable -eq $false -and
        [string]$settings.steamvr.forcedDriver -eq '' -and
        $settings.steamvr.requireHmd -eq $true
}

function Find-CandidateManifest(
    [string]$DllHash, [string]$LauncherHash) {
    $roots = @(
        (Join-Path $repoRoot 'out\candidates'),
        (Join-Path $repoRoot 'out\package-worktrees')) |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container }
    $matches = @{}
    $manifestPaths = @(Get-ChildItem -Path $roots -Recurse -File -Filter 'CANDIDATE-MANIFEST.json' -ErrorAction SilentlyContinue)
    foreach ($manifestPath in $manifestPaths) {
        try {
            $manifest = Get-Content -LiteralPath $manifestPath.FullName -Raw |
                ConvertFrom-Json
            $manifestDll = [string]$manifest.files.'halo3xr.dll'.sha256
            $manifestLauncher =
                [string]$manifest.files.'halo3xr_launcher.exe'.sha256
            if ($manifestDll -ceq $DllHash -and
                $manifestLauncher -ceq $LauncherHash) {
                $id = [string]$manifest.package_id
                if (-not $matches.ContainsKey($id)) {
                    $matches[$id] = [pscustomobject]@{
                        package_id = $id
                        source_commit = [string]$manifest.source_commit
                        manifest_path = $manifestPath.FullName
                    }
                }
            }
        }
        catch {
            # An unrelated stale debug manifest must not hide a valid package.
        }
    }
    if ($matches.Count -ne 1) {
        throw "Installed hashes match $($matches.Count) unique candidate manifests; expected exactly one."
    }
    return @($matches.Values)[0]
}

function Get-RunningMcc {
    $found = @()
    foreach ($name in @('MCC-Win64-Shipping', 'MCCWinStore-Win64-Shipping')) {
        $found += @(Get-Process -Name $name -ErrorAction SilentlyContinue)
    }
    return $found
}

if ($SelfTest) {
    $normal = @'
{"driver_null":{"enable":false},"steamvr":{"forcedDriver":"","requireHmd":true}}
'@
    $nullEnabled = @'
{"driver_null":{"enable":true},"steamvr":{"forcedDriver":"null","requireHmd":false}}
'@
    if (-not (Test-NormalSteamVrSettings $normal) -or
        (Test-NormalSteamVrSettings $nullEnabled) -or
        (Test-NormalSteamVrSettings '{}') -or
        (Test-NormalSteamVrSettings 'invalid')) {
        throw 'Headset-session SteamVR safety self-test failed.'
    }
    Write-Output 'Halo 3 headset-session wrapper self-test passed.'
    exit 0
}

if (-not (Test-Path -LiteralPath $steamVrSettings -PathType Leaf)) {
    throw "SteamVR settings not found: $steamVrSettings"
}
$settingsBeforeHash = Get-Sha256 $steamVrSettings
$settingsText = [IO.File]::ReadAllText($steamVrSettings)
if (-not (Test-NormalSteamVrSettings $settingsText)) {
    throw 'SteamVR is not in normal headset mode. This wrapper will not change it.'
}

$installs = @(Get-PresentInstalls)
if ($Edition -ne 'Auto') {
    $installs = @($installs | Where-Object edition -eq $Edition)
}
if ($installs.Count -eq 0) {
    throw "No complete $Edition MCC VR installation was found."
}

$running = @(Get-RunningMcc)
if ($running.Count -gt 1) {
    throw 'More than one MCC process is running; refusing an ambiguous capture.'
}

$install = $null
$mcc = $null
if ($running.Count -eq 1) {
    $mcc = $running[0]
    $matchingInstalls = @(
        $installs | Where-Object process -eq $mcc.ProcessName)
    $runningPath = ''
    try { $runningPath = [string]$mcc.Path } catch {}
    if ($matchingInstalls.Count -gt 1 -and $runningPath) {
        $matchingInstalls = @($matchingInstalls | Where-Object {
            [IO.Path]::GetFullPath($_.game_exe) -ieq
                [IO.Path]::GetFullPath($runningPath)
        })
    }
    if ($matchingInstalls.Count -ne 1) {
        throw 'The running MCC edition does not match the requested installation.'
    }
    $install = $matchingInstalls[0]
}
elseif ($installs.Count -eq 1) {
    $install = $installs[0]
}
else {
    throw 'More than one MCC edition is installed. Pass -Edition Steam or -Edition MicrosoftStore.'
}

$dllHash = Get-Sha256 $install.dll
$launcherHash = Get-Sha256 $install.launcher
$candidate = Find-CandidateManifest $dllHash $launcherHash
if (-not (Test-Path -LiteralPath $install.config -PathType Leaf)) {
    throw "MCC VR configuration not found: $($install.config)"
}
$configText = [IO.File]::ReadAllText($install.config)
$contactMatches = [regex]::Matches(
    $configText, '(?im)^\s*physical_weapon_contact\s*=\s*([^\s#;]+)')
if ($contactMatches.Count -eq 0 -or
    $contactMatches[$contactMatches.Count - 1].Groups[1].Value -ne '1') {
    throw 'physical_weapon_contact is not enabled in halomccvr.cfg.'
}
$meleeMatches = [regex]::Matches(
    $configText, '(?im)^\s*physical_weapon_melee_speed\s*=\s*([^\s#;]+)')
$meleeSpeed = if ($meleeMatches.Count -gt 0) {
    $meleeMatches[$meleeMatches.Count - 1].Groups[1].Value
} else { '<default>' }

Write-Output "Edition: $($install.edition)"
Write-Output "Candidate: $($candidate.package_id)"
Write-Output "Source: $($candidate.source_commit)"
Write-Output "DLL SHA-256: $dllHash"
Write-Output "Launcher SHA-256: $launcherHash"
Write-Output "SteamVR settings SHA-256: $settingsBeforeHash"
Write-Output 'SteamVR mode: normal headset'
Write-Output 'Physical weapon contact: enabled'
Write-Output "Melee speed: $meleeSpeed m/s"

if ($VerifyOnly) {
    Write-Output 'Verification only: MCC was not launched or changed.'
    exit 0
}

$startedUtc = [DateTime]::UtcNow
$stamp = $startedUtc.ToString("yyyyMMdd-HHmmssfff'Z'")
$sessionName =
    "$stamp-$($candidate.source_commit.Substring(0, 7))-$($install.edition)"
$sessionRoot = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) $sessionName
[IO.Directory]::CreateDirectory($sessionRoot) | Out-Null
if (Test-Path -LiteralPath $install.log -PathType Leaf) {
    Copy-Item -LiteralPath $install.log -Destination
        (Join-Path $sessionRoot 'before-halo3xr.log')
}

if (-not $mcc) {
    Write-Output 'Starting the installed MCC VR launcher. Menu control stays with the player.'
    Start-Process -FilePath $install.launcher -WorkingDirectory $install.mod_root
    $launchDeadline = [DateTime]::UtcNow.AddSeconds($LaunchTimeoutSeconds)
    do {
        Start-Sleep -Seconds 1
        $matches = @(Get-Process -Name $install.process -ErrorAction SilentlyContinue)
        if ($matches.Count -gt 1) {
            throw 'More than one matching MCC process appeared.'
        }
        if ($matches.Count -eq 1) { $mcc = $matches[0]; break }
    } while ([DateTime]::UtcNow -lt $launchDeadline)
    if (-not $mcc) { throw 'MCC did not start before the launch timeout.' }
}
else {
    Write-Output "Attached to the open MCC process $($mcc.Id)."
}

Write-Output 'Recording until MCC closes.'
$sessionDeadline = [DateTime]::UtcNow.AddMinutes($MaximumSessionMinutes)
$nextProgress = [DateTime]::UtcNow.AddSeconds(30)
while (Get-Process -Id $mcc.Id -ErrorAction SilentlyContinue) {
    if ([DateTime]::UtcNow -ge $sessionDeadline) {
        throw 'MCC is still open after the session time limit. It was not stopped.'
    }
    if ([DateTime]::UtcNow -ge $nextProgress) {
        Write-Output 'MCC is still open; headset session recording continues.'
        $nextProgress = [DateTime]::UtcNow.AddSeconds(30)
    }
    Start-Sleep -Seconds 2
}
Start-Sleep -Seconds 3

if (-not (Test-Path -LiteralPath $install.log -PathType Leaf)) {
    throw 'MCC closed without producing halo3xr.log.'
}
$savedLog = Join-Path $sessionRoot 'halo3xr.log'
Copy-Item -LiteralPath $install.log -Destination $savedLog
$savedDllHash = Get-Sha256 $install.dll
$savedLauncherHash = Get-Sha256 $install.launcher
if ($savedDllHash -cne $dllHash -or $savedLauncherHash -cne $launcherHash) {
    throw 'Installed MCC VR files changed during the session.'
}

$analysisJson = Join-Path $sessionRoot 'contact-analysis.json'
$analyzerArguments = @{
    LogPath = $savedLog
    OutputPath = $analysisJson
}
$analyzer = Join-Path $PSScriptRoot 'analyze-h3-contact-session.ps1'
$analysisText = & $analyzer @analyzerArguments | Out-String
[IO.File]::WriteAllText(
    (Join-Path $sessionRoot 'contact-analysis.txt'), $analysisText)
Write-Output $analysisText.TrimEnd()
$analysis = Get-Content -LiteralPath $analysisJson -Raw | ConvertFrom-Json
if ($analysis.source_commit -cne $candidate.source_commit) {
    throw 'Saved runtime source does not match the installed candidate manifest.'
}

$settingsAfterHash = Get-Sha256 $steamVrSettings
$sessionManifest = [ordered]@{
    schema_version = 1
    started_utc = $startedUtc.ToString('o')
    completed_utc = [DateTime]::UtcNow.ToString('o')
    mcc_edition = $install.edition
    package_id = $candidate.package_id
    source_commit = $candidate.source_commit
    installed_dll_sha256 = $dllHash
    installed_launcher_sha256 = $launcherHash
    physical_weapon_contact = 1
    physical_weapon_melee_speed_mps = $meleeSpeed
    steamvr_settings_before_sha256 = $settingsBeforeHash
    steamvr_settings_after_sha256 = $settingsAfterHash
    runtime_log = $savedLog
    runtime_log_sha256 = Get-Sha256 $savedLog
    real_headset_session = [bool]$analysis.real_headset_session
    null_driver_session = [bool]$analysis.null_driver_session
    player_acceptance_recorded = $false
    acceptance_note =
        'The log proves observed runtime paths only. Record the player headset result separately.'
}
$manifestPath = Join-Path $sessionRoot 'session.json'
$sessionManifest | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Output "Saved headset session: $sessionRoot"

if (-not $analysis.real_headset_session -or $analysis.null_driver_session) {
    throw 'The session was preserved, but the runtime log did not identify a real headset.'
}

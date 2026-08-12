[CmdletBinding()]
param(
    [string]$LogPath,
    [string]$OutputPath,
    [switch]$SelfTest
)

# Read-only summary of one Halo 3 headset or null-driver session. This tool
# reports which runtime paths were observed. It deliberately does not call an
# unobserved path a failure, and it never treats log evidence as proof of visual
# alignment, force feel, haptic feel, or player acceptance.

$ErrorActionPreference = 'Stop'

function Get-ContactPairs([string]$Line) {
    $pairs = @{}
    foreach ($match in [regex]::Matches(
        $Line, '(?<![A-Za-z0-9_])([A-Za-z][A-Za-z0-9_]*)=([^\s]+)')) {
        $pairs[$match.Groups[1].Value] = $match.Groups[2].Value
    }
    return $pairs
}

function Get-Number($Pairs, [string]$Name, [double]$Default = 0.0) {
    if (-not $Pairs.ContainsKey($Name)) { return $Default }
    $text = $Pairs[$Name] -replace '(m/s|kg|m)$', ''
    $value = 0.0
    if ([double]::TryParse(
        $text, [Globalization.NumberStyles]::Float,
        [Globalization.CultureInfo]::InvariantCulture, [ref]$value)) {
        return $value
    }
    return $Default
}

function Update-Max($Table, [string]$Name, [double]$Value) {
    if (-not $Table.ContainsKey($Name) -or $Value -gt $Table[$Name]) {
        $Table[$Name] = $Value
    }
}

function Test-Positive($Table, [string]$Name) {
    return $Table.ContainsKey($Name) -and $Table[$Name] -gt 0
}

function Get-ObjectKindName([uint32]$Kind) {
    switch ($Kind) {
        0 { return 'biped' }
        1 { return 'vehicle' }
        2 { return 'weapon' }
        3 { return 'equipment/grenade' }
        10 { return 'crate' }
        11 { return 'garbage' }
        12 { return 'creature' }
        13 { return 'giant' }
        default { return "kind-$Kind" }
    }
}

function Analyze-Halo3ContactText([string]$Text, [string]$SourcePath) {
    $sourceCommit = ''
    $edition = ''
    $runtime = ''
    $headset = ''
    $panelHz = 0.0
    $maximum = @{}
    $kinds = [Collections.Generic.HashSet[uint32]]::new()
    $kindDetail = @{}
    $statusLines = 0
    $leftLines = 0
    $visibleLines = 0
    $bindingsInstalled = $false

    foreach ($line in ($Text -split "`r?`n")) {
        if (-not $sourceCommit -and
            $line -match 'HaloMCCVR loaded.*\(source ([0-9a-fA-F]{7,40})') {
            $sourceCommit = $Matches[1].ToLowerInvariant()
        }
        if (-not $edition -and $line -match 'MCC edition:\s*([^\r\n(]+)') {
            $edition = $Matches[1].Trim()
        }
        if (-not $runtime -and $line -match 'OpenXR runtime:\s*(.+)$') {
            $runtime = $Matches[1].Trim()
        }
        if (-not $headset -and $line -match "headset:\s*'([^']+)'") {
            $headset = $Matches[1].Trim()
        }
        if ($panelHz -eq 0.0 -and
            $line -match 'headset: panel is running at ([0-9.]+)Hz') {
            $panelHz = [double]::Parse(
                $Matches[1], [Globalization.CultureInfo]::InvariantCulture)
        }
        if ($line -like '*H3 physical contact: optional native bindings installed*') {
            $bindingsInstalled = $true
        }
        if ($line -like '*H3 physical contact status:*') {
            ++$statusLines
            $pairs = Get-ContactPairs $line
            foreach ($name in @(
                'sweeps', 'hits', 'impulses', 'releases', 'melees',
                'authoredShapeHits', 'animatedBodyHits', 'unsupportedShapes',
                'rejectNormal', 'enemySustainedMelees',
                'enemyFallbackNormalMelees', 'rejectPose', 'rejectVelocity',
                'rejectMeleeSpike', 'weaponTriangles', 'targetTriangles',
                'targetDetailed', 'targetFallback', 'nativeSamples',
                'wallBlocks', 'bodyConstraints', 'bodyGapHolds', 'wallRays',
                'wallMotionRays', 'wallObjectPlanes', 'wallVertices',
                'wallPlanes', 'decoratorSolidDraws', 'decoratorInstances',
                'decoratorPlanes', 'decoratorSelfTest', 'contactHaptic',
                'weaponMass', 'targetMass', 'bodyPeak')) {
                Update-Max $maximum $name (Get-Number $pairs $name)
            }

            $kindValue = [uint32](Get-Number $pairs 'kind' 4294967295)
            if ($kindValue -ne [uint32]::MaxValue) {
                $null = $kinds.Add($kindValue)
                $key = [string]$kindValue
                if (-not $kindDetail.ContainsKey($key)) {
                    $kindDetail[$key] = @{
                        name = Get-ObjectKindName $kindValue
                        detailed_geometry = $false
                        animated_body = $false
                        maximum_mass_kg = 0.0
                    }
                }
                $detail = $kindDetail[$key]
                $targetIdentityCurrent = $pairs.ContainsKey('target') -and
                    $pairs.ContainsKey('candidate') -and
                    $pairs['target'] -eq $pairs['candidate']
                if ($targetIdentityCurrent -and
                    (Get-Number $pairs 'targetShapeSource') -eq 1 -and
                    (Get-Number $pairs 'targetDetailed') -gt 0) {
                    $detail.detailed_geometry = $true
                }
                if ($targetIdentityCurrent -and
                    (Get-Number $pairs 'targetShapeSource') -eq 3) {
                    $detail.animated_body = $true
                }
                $detail.maximum_mass_kg = [math]::Max(
                    $detail.maximum_mass_kg,
                    (Get-Number $pairs 'targetMass'))
            }
        }
        elseif ($line -like '*H3 left grab status:*') {
            ++$leftLines
            $pairs = Get-ContactPairs $line
            foreach ($name in @(
                'bindings', 'acquisitions', 'commands', 'applied',
                'releases', 'mass')) {
                Update-Max $maximum "left_$name" (Get-Number $pairs $name)
            }
        }
        elseif ($line -like '*H3 physical contact visible IDs:*') {
            ++$visibleLines
            $pairs = Get-ContactPairs $line
            foreach ($name in @('slotMatches', 'slotMisses', 'submissions')) {
                Update-Max $maximum "visible_$name" (Get-Number $pairs $name)
            }
        }
    }

    $nullDriver = $headset -match '(?i)null' -or
        $runtime -match '(?i)null driver'
    $realHeadset = [bool]$headset -and -not $nullDriver
    $kindObjects = @()
    foreach ($kind in ($kinds | Sort-Object)) {
        $detail = $kindDetail[[string]$kind]
        $kindObjects += [pscustomobject]@{
            kind = $kind
            name = $detail.name
            detailed_geometry = $detail.detailed_geometry
            animated_body = $detail.animated_body
            maximum_mass_kg = [math]::Round($detail.maximum_mass_kg, 3)
        }
    }

    $checks = [ordered]@{
        native_bindings = $bindingsInstalled
        exact_visible_weapon =
            (Test-Positive $maximum 'weaponTriangles') -and
            (Test-Positive $maximum 'visible_submissions')
        authored_target_contact =
            (Test-Positive $maximum 'targetDetailed') -and
            (Test-Positive $maximum 'authoredShapeHits')
        gradual_rigid_body_response =
            (Test-Positive $maximum 'impulses') -and
            (Test-Positive $maximum 'bodyConstraints')
        dynamic_gap_hold = Test-Positive $maximum 'bodyGapHolds'
        contact_haptic = Test-Positive $maximum 'contactHaptic'
        structure_wall =
            (Test-Positive $maximum 'wallBlocks') -and
            (Test-Positive $maximum 'wallPlanes')
        placed_object_wall = Test-Positive $maximum 'wallObjectPlanes'
        decorator_wall =
            (Test-Positive $maximum 'decoratorSelfTest') -or
            (Test-Positive $maximum 'decoratorPlanes')
        animated_body = Test-Positive $maximum 'animatedBodyHits'
        native_melee = Test-Positive $maximum 'melees'
        left_hand_pickup =
            (Test-Positive $maximum 'left_acquisitions') -and
            (Test-Positive $maximum 'left_applied') -and
            (Test-Positive $maximum 'left_releases')
    }

    $counterObject = [ordered]@{}
    foreach ($key in ($maximum.Keys | Sort-Object)) {
        $counterObject[$key] = $maximum[$key]
    }

    return [pscustomobject]@{
        schema_version = 1
        log_path = $SourcePath
        source_commit = $sourceCommit
        mcc_edition = $edition
        openxr_runtime = $runtime
        headset = $headset
        panel_hz = $panelHz
        real_headset_session = $realHeadset
        null_driver_session = $nullDriver
        status_samples = $statusLines
        left_grab_samples = $leftLines
        visible_palette_samples = $visibleLines
        observed = [pscustomobject]$checks
        object_kinds = $kindObjects
        maxima = [pscustomobject]$counterObject
        acceptance_boundary =
            'Runtime evidence only; visual alignment, clipping, force feel, haptic feel, and player acceptance require a headset report.'
    }
}

function Write-ContactSummary($Report) {
    Write-Output 'Halo 3 physical-contact session'
    Write-Output "  Source:  $($Report.source_commit)"
    Write-Output "  Edition: $($Report.mcc_edition)"
    Write-Output "  Runtime: $($Report.openxr_runtime)"
    Write-Output "  Headset: $($Report.headset)"
    Write-Output "  Real headset: $($Report.real_headset_session)"
    Write-Output ''
    Write-Output 'Observed runtime paths:'
    foreach ($property in $Report.observed.PSObject.Properties) {
        $mark = if ($property.Value) { 'yes' } else { 'not observed' }
        Write-Output ('  {0,-28} {1}' -f
            ($property.Name -replace '_', ' '), $mark)
    }
    if ($Report.object_kinds.Count -gt 0) {
        Write-Output ''
        Write-Output 'Contacted object kinds:'
        foreach ($kind in $Report.object_kinds) {
            # Command and melee statuses are cumulative and may describe a
            # preceding target. Do not attribute them to the currently printed
            # kind. Geometry source and mass are target-local on this line.
            $format = '  {0,-20} detailed={1} animated-body={2} mass={3:N3}kg'
            Write-Output ($format -f
                $kind.name, $kind.detailed_geometry, $kind.animated_body,
                $kind.maximum_mass_kg)
        }
    }
    Write-Output ''
    Write-Output $Report.acceptance_boundary
}

if ($SelfTest) {
    $sample = @'
[10:00:00.000] HaloMCCVR loaded into pid 1 (source 0123456789abcdef, compiled now)
[10:00:00.001] MCC edition: Steam (sample.exe)
[10:00:00.002] OpenXR runtime: SteamVR/OpenXR sample
[10:00:00.003] headset: 'SteamVR/OpenXR : oculus' (vendor 0x28DE) on runtime sample
[10:00:00.004] headset: panel is running at 90.0Hz
[10:00:00.005] H3 physical contact: optional native bindings installed
[10:00:01.000] H3 physical contact status: sweeps=2 hits=1 impulses=1 releases=0 melees=0 commandStatus=2 meleeStatus=0 target=0x12340001 candidate=0x12340001 kind=3 weaponMass=2.764 targetMass=0.382 contactHaptic=0.250 authoredShapeHits=1 animatedBodyHits=0 weaponTriangles=36 targetShapeSource=1 targetTriangles=8 targetDetailed=1 targetFallback=0 wallBlocks=1 bodyConstraints=1 bodyGapHolds=1 wallRays=4 wallObjectPlanes=2 wallPlanes=3 decoratorPlanes=5 decoratorSelfTest=0
[10:00:01.001] H3 left grab status: bindings=1 acquisitions=1 commands=2 applied=2 releases=1 mass=0.382
[10:00:01.002] H3 physical contact visible IDs: slotMatches=3 slotMisses=0 submissions=3
'@
    $report = Analyze-Halo3ContactText $sample '<self-test>'
    if (-not $report.real_headset_session -or
        -not $report.observed.exact_visible_weapon -or
        -not $report.observed.authored_target_contact -or
        -not $report.observed.gradual_rigid_body_response -or
        -not $report.observed.structure_wall -or
        -not $report.observed.placed_object_wall -or
        -not $report.observed.decorator_wall -or
        -not $report.observed.left_hand_pickup -or
        $report.object_kinds.Count -ne 1 -or
        $report.object_kinds[0].name -ne 'equipment/grenade') {
        throw 'Halo 3 contact-session analyzer self-test failed.'
    }
    Write-Output 'Halo 3 contact-session analyzer self-test passed.'
    exit 0
}

if (-not $LogPath) {
    $candidates = @(
        'E:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\Halo_MCC_VR\halo3xr.log',
        'N:\SteamLibrary\steamapps\common\Halo The Master Chief Collection\Halo_MCC_VR\halo3xr.log',
        'N:\XBOX\Halo- The Master Chief Collection\Content\Halo_MCC_VR\halo3xr.log'
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    if ($candidates.Count -ne 1) {
        throw 'Pass -LogPath when exactly one installed Steam log cannot be found.'
    }
    $LogPath = $candidates[0]
}

$resolvedLog = [IO.Path]::GetFullPath($LogPath)
if (-not (Test-Path -LiteralPath $resolvedLog -PathType Leaf)) {
    throw "Halo 3 log not found: $resolvedLog"
}
$text = [IO.File]::ReadAllText($resolvedLog)
$report = Analyze-Halo3ContactText $text $resolvedLog
Write-ContactSummary $report

if ($OutputPath) {
    $resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
    $parent = Split-Path -Parent $resolvedOutput
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        [IO.Directory]::CreateDirectory($parent) | Out-Null
    }
    $report | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $resolvedOutput -Encoding utf8
    Write-Output "JSON report: $resolvedOutput"
}

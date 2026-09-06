[CmdletBinding()]
param([Parameter(Mandatory)][string]$LogPath)
$ErrorActionPreference = 'Stop'
$path = (Resolve-Path -LiteralPath $LogPath).Path
$pattern = 'H3 contact pose timing: proof=(\d+) corrected=(\d+) samples=(\d+) ms=\[(\d+) (\d+)\] originAgeMs\[p50=(\d+) p95=(\d+) max=(\d+)\] peakRootGap=([0-9.]+)m droppedTotal=(\d+)'
$rows = @(foreach ($line in [IO.File]::ReadLines($path)) {
    if ($line -match $pattern) {
        [pscustomobject]@{
            proof=[int]$Matches[1]; corrected=[int]$Matches[2]; samples=[int]$Matches[3]
            first_ms=[long]$Matches[4]; last_ms=[long]$Matches[5]
            p50_ms=[long]$Matches[6]; p95_ms=[long]$Matches[7]; max_ms=[long]$Matches[8]
            peak_gap_m=[double]::Parse($Matches[9], [Globalization.CultureInfo]::InvariantCulture)
            dropped=[long]$Matches[10]
        }
    }
})
if (!$rows.Count) { throw 'No paired pose timing records found.' }
$groups = @($rows | Group-Object proof,corrected | ForEach-Object {
    $g=$_.Group
    [ordered]@{
        proof=$g[0].proof; corrected=$g[0].corrected; windows=$g.Count
        samples=($g | Measure-Object samples -Sum).Sum
        min_window_p50_ms=($g | Measure-Object p50_ms -Minimum).Minimum
        max_window_p50_ms=($g | Measure-Object p50_ms -Maximum).Maximum
        max_window_p95_ms=($g | Measure-Object p95_ms -Maximum).Maximum
        max_sample_age_ms=($g | Measure-Object max_ms -Maximum).Maximum
        max_paired_root_gap_m=($g | Measure-Object peak_gap_m -Maximum).Maximum
    }
})
[ordered]@{
    log=$path; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    limits='Proposal age in GetTickCount64 units, not sensor or motion-to-photon latency. Samples are decimated palette submissions, not frames. Window percentiles cannot be combined into a session percentile. Corrected gaps include intentional contact displacement.'
    proof_names=@('guard inactive','approved','cached','raw/recovery','experimental empty region')
    dropped_total=($rows | Measure-Object dropped -Maximum).Maximum
    groups=$groups; windows=$rows
} | ConvertTo-Json -Depth 6

param(
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\out\debug-openxr'),
    [string]$Prefix = 'mcc-window-sequence',
    [ValidateRange(1, 300)]
    [int]$DurationSeconds = 40,
    [ValidateRange(1, 30)]
    [int]$FramesPerSecond = 5,
    [ValidateRange(320, 3840)]
    [int]$OutputWidth = 1280,
    [ValidateRange(1, 120)]
    [int]$WaitForProcessSeconds = 45
)

$ErrorActionPreference = 'Stop'
$processName = 'MCC-Win64-Shipping'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class HaloMccVrCaptureWindow
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int left;
        public int top;
        public int right;
        public int bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);
}
'@

$resolvedOutput = [IO.Path]::GetFullPath($OutputRoot)
[IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null
$stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmssfff'Z'")
$sequenceRoot = Join-Path $resolvedOutput "$stamp-$Prefix"
[IO.Directory]::CreateDirectory($sequenceRoot) | Out-Null

$deadline = [DateTime]::UtcNow.AddSeconds($WaitForProcessSeconds)
$mcc = $null
do {
    $mcc = Get-Process $processName -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1
    if ($mcc) { break }
    Start-Sleep -Milliseconds 250
} while ([DateTime]::UtcNow -lt $deadline)

if (-not $mcc) {
    throw "MCC did not expose a visible window within $WaitForProcessSeconds seconds."
}
$processId = $mcc.Id

$intervalMilliseconds = [Math]::Max(
    1, [int][Math]::Round(1000.0 / $FramesPerSecond))
$captureDeadline = [DateTime]::UtcNow.AddSeconds($DurationSeconds)
$frames = [Collections.Generic.List[object]]::new()
$frameIndex = 0

while ([DateTime]::UtcNow -lt $captureDeadline) {
    $mcc = Get-Process -Id $processId -ErrorAction SilentlyContinue
    if (-not $mcc -or $mcc.MainWindowHandle -eq 0) { break }

    $rect = New-Object HaloMccVrCaptureWindow+Rect
    if (-not [HaloMccVrCaptureWindow]::GetWindowRect(
            $mcc.MainWindowHandle, [ref]$rect)) {
        break
    }
    $sourceWidth = $rect.right - $rect.left
    $sourceHeight = $rect.bottom - $rect.top
    if ($sourceWidth -le 0 -or $sourceHeight -le 0) { break }

    $outputHeight = [Math]::Max(
        1, [int][Math]::Round(
            $OutputWidth * $sourceHeight / [double]$sourceWidth))
    $source = New-Object System.Drawing.Bitmap $sourceWidth, $sourceHeight
    $sourceGraphics = [System.Drawing.Graphics]::FromImage($source)
    try {
        $sourceGraphics.CopyFromScreen(
            $rect.left, $rect.top, 0, 0,
            (New-Object System.Drawing.Size $sourceWidth, $sourceHeight))
        $output = New-Object System.Drawing.Bitmap $OutputWidth, $outputHeight
        $outputGraphics = [System.Drawing.Graphics]::FromImage($output)
        try {
            $outputGraphics.InterpolationMode =
                [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $outputGraphics.DrawImage(
                $source, 0, 0, $OutputWidth, $outputHeight)
            $utc = [DateTime]::UtcNow
            $fileName = '{0:D4}-{1}.jpg' -f $frameIndex,
                $utc.ToString("HHmmssfff'Z'")
            $output.Save(
                (Join-Path $sequenceRoot $fileName),
                [System.Drawing.Imaging.ImageFormat]::Jpeg)
            $frames.Add([ordered]@{
                index = $frameIndex
                utc = $utc.ToString('o')
                file = $fileName
                source_rect = @(
                    $rect.left, $rect.top, $rect.right, $rect.bottom)
                output_size = @($OutputWidth, $outputHeight)
            })
            ++$frameIndex
        }
        finally {
            $outputGraphics.Dispose()
            $output.Dispose()
        }
    }
    finally {
        $sourceGraphics.Dispose()
        $source.Dispose()
    }
    Start-Sleep -Milliseconds $intervalMilliseconds
}

$manifest = [ordered]@{
    schema_version = 1
    process = $processName
    process_id = $processId
    started_utc = $stamp
    duration_seconds_requested = $DurationSeconds
    frames_per_second_requested = $FramesPerSecond
    frames_captured = $frames.Count
    completed_utc = [DateTime]::UtcNow.ToString('o')
    frames = $frames
}
[IO.File]::WriteAllText(
    (Join-Path $sequenceRoot 'manifest.json'),
    ($manifest | ConvertTo-Json -Depth 6))

Write-Output $sequenceRoot

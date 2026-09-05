param(
    [Parameter(Mandatory)][string]$Name,
    [ValidateRange(5,60)][int]$Seconds=20
)
$ErrorActionPreference='Stop'
if ($Name -notmatch '^[a-z0-9-]+$') { throw 'Use a simple demo name.' }
$mcc=Get-Process MCC-Win64-Shipping -ErrorAction Stop
if (@($mcc).Count -ne 1 -or $mcc.MainWindowHandle -eq 0) { throw 'MCC window unavailable.' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DemoWindow {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h,int index);
}
'@
$wasTopmost=([DemoWindow]::GetWindowLong($mcc.MainWindowHandle,-20) -band 8) -ne 0
$dpi=[DemoWindow]::SetThreadDpiAwarenessContext([IntPtr](-4))
$dir=Join-Path $PSScriptRoot ('../out/demos/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+$Name)
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$dir=(Resolve-Path $dir).Path
$video=Join-Path $dir 'raw.mp4'
$ffmpeg=Join-Path $PSScriptRoot '../out/video-deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
try {
    if (-not [DemoWindow]::SetWindowPos($mcc.MainWindowHandle,[IntPtr](-1),0,0,0,0,0x13)) { throw 'Could not expose MCC for recording.' }
    Start-Sleep -Milliseconds 300
    $rect=New-Object DemoWindow+Rect
    if (-not [DemoWindow]::GetWindowRect($mcc.MainWindowHandle,[ref]$rect)) { throw 'MCC capture bounds unavailable.' }
    $width=$rect.right-$rect.left; $height=$rect.bottom-$rect.top
    # D3D's window DC is blank with gdigrab title capture. Capture only the
    # exposed physical desktop rectangle, as the verified screenshot tool does.
    & $ffmpeg -hide_banner -loglevel error -f gdigrab -framerate 30 -draw_mouse 0 -offset_x $rect.left -offset_y $rect.top -video_size ($width.ToString()+'x'+$height) -i desktop -t $Seconds -vf 'scale=1280:-2' -c:v libx264 -preset veryfast -crf 20 -pix_fmt yuv420p -movflags +faststart $video
    if($LASTEXITCODE -ne 0) { throw 'MCC video capture failed.' }
    [ordered]@{video=$video;sha256=(Get-FileHash -LiteralPath $video).Hash;requested_seconds=$Seconds;requested_fps=30;source='Live MCC window; scripted null-driver demo; no generated imagery';headset_acceptance=$false} | ConvertTo-Json | Set-Content (Join-Path $dir 'recording.json')
    $video
}
finally {
    if (-not $wasTopmost) { $null=[DemoWindow]::SetWindowPos($mcc.MainWindowHandle,[IntPtr](-2),0,0,0,0,0x13) }
    if ($dpi -ne [IntPtr]::Zero) { $null=[DemoWindow]::SetThreadDpiAwarenessContext($dpi) }
}

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Up','Down','Left','Right','A','B','Start','X','Y')]
    [string[]]$Buttons,
    [ValidateRange(100,1500)][int]$HoldMilliseconds = 350,
    [ValidateRange(100,3000)][int]$DelayMilliseconds = 500
)
# Uses the existing opt-in keyboard gamepad bridge. These isolated keys have no
# native MCC binding; unlike arrow/Enter input they cannot navigate twice.
$ErrorActionPreference = 'Stop'
$mcc = @(Get-Process MCC-Win64-Shipping -ErrorAction Stop)
if ($mcc.Count -ne 1 -or $mcc[0].MainWindowHandle -eq 0) {
    throw 'Exactly one visible MCC process is required.'
}
$mcc = $mcc[0]
$install = Split-Path (Split-Path (Split-Path (Split-Path $mcc.Path)))
$log = Join-Path $install 'Halo_MCC_VR/halo3xr.log'
if (-not (Test-Path -LiteralPath $log) -or
    (Get-Item -LiteralPath $log).LastWriteTimeUtc -lt $mcc.StartTime.ToUniversalTime() -or
    [IO.File]::ReadAllText($log) -notmatch 'debug input: keyboard-to-gamepad bridge enabled \(isolated F13-F21 controls available\)') {
    throw 'The current MCC session has not enabled the F13-F21 test bridge.'
}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class MccDiagnosticPad {
 [StructLayout(LayoutKind.Sequential)] public struct Keyboard {
  public ushort vk, scan; public uint flags, time; public UIntPtr extra;
 }
 [StructLayout(LayoutKind.Explicit, Size=40)] public struct Input {
  [FieldOffset(0)] public uint type;
  [FieldOffset(8)] public Keyboard key;
 }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] static extern uint SendInput(uint n, Input[] input, int size);
 public static uint Pulse(ushort vk, int hold) {
  var input = new Input[1]; input[0].type=1; input[0].key.vk=vk;
  uint result=SendInput(1,input,40);
  try { System.Threading.Thread.Sleep(hold); }
  finally { input[0].key.flags=2; result+=SendInput(1,input,40); }
  return result;
 }
}
'@
$keys = @{Up=0x7C; Down=0x7D; Left=0x7E; Right=0x7F; A=0x80; B=0x81; Start=0x82; X=0x83; Y=0x84}
$null = [MccDiagnosticPad]::SetForegroundWindow($mcc.MainWindowHandle)
Start-Sleep -Milliseconds 250
foreach ($button in $Buttons) {
    if ([MccDiagnosticPad]::GetForegroundWindow() -ne $mcc.MainWindowHandle) {
        throw 'MCC is not foreground; no further buttons sent.'
    }
    $count = [MccDiagnosticPad]::Pulse($keys[$button], $HoldMilliseconds)
    if ($count -ne 2) { throw "Only $count of two input events delivered." }
    Write-Host "Sent diagnostic gamepad $button; verify the game response separately."
    Start-Sleep -Milliseconds $DelayMilliseconds
}

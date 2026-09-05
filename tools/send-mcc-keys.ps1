param(
    [Parameter(Mandatory)]
    [ValidateSet('Enter', 'Escape', 'Up', 'Down', 'Left', 'Right')]
    [string[]]$Keys,
    [ValidateRange(50, 5000)]
    [int]$DelayMilliseconds = 350,
    [switch]$Background
)

$ErrorActionPreference = 'Stop'
$mcc = Get-Process 'MCC-Win64-Shipping' -ErrorAction Stop
if ($mcc.MainWindowHandle -eq 0) { throw 'MCC has no visible window.' }

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class HaloMccVrVisibleInput {
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
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool PostMessage(IntPtr window, uint message, UIntPtr wParam, IntPtr lParam);
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
}
'@

$mapping = @{
    Enter = @(0x1C, $false)
    Escape = @(0x01, $false)
    Up = @(0x48, $true)
    Down = @(0x50, $true)
    Left = @(0x4B, $true)
    Right = @(0x4D, $true)
}

if (-not $Background) {
    $null = [HaloMccVrVisibleInput]::SetForegroundWindow($mcc.MainWindowHandle)
    Start-Sleep -Milliseconds 200
    if ([HaloMccVrVisibleInput]::GetForegroundWindow() -ne $mcc.MainWindowHandle) {
        throw 'MCC could not be focused.'
    }
}
foreach ($key in $Keys) {
    if (-not $Background -and [HaloMccVrVisibleInput]::GetForegroundWindow() -ne $mcc.MainWindowHandle) {
        throw 'MCC lost focus before input; no further keys sent.'
    }
    $value = $mapping[$key]
    if ($Background) {
        # UE4's menu consumes addressed key messages without foreground focus.
        # This does not inject keys into another foreground app or rely on
        # GetAsyncKeyState. Gameplay/raw-input paths may behave differently.
        $virtualKey = @{ Enter=0x0D; Escape=0x1B; Up=0x26; Down=0x28; Left=0x25; Right=0x27 }[$key]
        $flags = [int64](1 -bor ($value[0] -shl 16))
        if ($value[1]) { $flags = $flags -bor 0x01000000 }
        $down = [HaloMccVrVisibleInput]::PostMessage($mcc.MainWindowHandle, 0x100, [UIntPtr]$virtualKey, [IntPtr]$flags)
        $up = [HaloMccVrVisibleInput]::PostMessage($mcc.MainWindowHandle, 0x101, [UIntPtr]$virtualKey, [IntPtr]($flags -bor 0xC0000000L))
        if (-not $down -or -not $up) { throw 'MCC rejected a background key message.' }
        Start-Sleep -Milliseconds $DelayMilliseconds
        continue
    }
    $written = [HaloMccVrVisibleInput]::Key($value[0], $value[1])
    if ($written -ne 2) { throw "SendInput wrote $written of 2 events." }
    Start-Sleep -Milliseconds $DelayMilliseconds
}

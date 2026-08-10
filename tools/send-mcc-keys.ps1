param(
    [Parameter(Mandatory)]
    [ValidateSet('Enter', 'Escape', 'Up', 'Down', 'Left', 'Right')]
    [string[]]$Keys,
    [ValidateRange(50, 5000)]
    [int]$DelayMilliseconds = 350
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

if (-not [HaloMccVrVisibleInput]::SetForegroundWindow($mcc.MainWindowHandle)) {
    throw 'MCC could not be focused.'
}
Start-Sleep -Milliseconds 200
foreach ($key in $Keys) {
    $value = $mapping[$key]
    $written = [HaloMccVrVisibleInput]::Key($value[0], $value[1])
    if ($written -ne 2) { throw "SendInput wrote $written of 2 events." }
    Start-Sleep -Milliseconds $DelayMilliseconds
}

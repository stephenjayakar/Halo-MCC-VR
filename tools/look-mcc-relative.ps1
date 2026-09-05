param([ValidateRange(-600,600)][int]$X = 0, [ValidateRange(-600,600)][int]$Y = 0)
# Ordinary relative mouse look, only after verifying MCC owns foreground input.
$ErrorActionPreference = 'Stop'
$mcc = @(Get-Process MCC-Win64-Shipping -ErrorAction Stop)
if ($mcc.Count -ne 1 -or $mcc[0].MainWindowHandle -eq 0) { throw 'MCC window is not unique.' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class MccRelativeLook {
    [StructLayout(LayoutKind.Sequential)] public struct Mouse { public int x,y; public uint data,flags,time; public UIntPtr extra; }
    [StructLayout(LayoutKind.Explicit, Size=40)] public struct Input { [FieldOffset(0)] public uint type; [FieldOffset(8)] public Mouse mouse; }
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a,uint b,bool attach);
    [DllImport("user32.dll")] static extern uint SendInput(uint n, Input[] input, int size);
    public static bool Focus(IntPtr h) {
        uint pid;
        uint current = GetCurrentThreadId();
        uint foreground = GetWindowThreadProcessId(GetForegroundWindow(), out pid);
        bool attached = foreground != 0 && foreground != current && AttachThreadInput(current,foreground,true);
        try { return SetForegroundWindow(h); }
        finally { if(attached) AttachThreadInput(current,foreground,false); }
    }
    public static bool Look(IntPtr h,int x,int y) {
        if(GetForegroundWindow()!=h) return false;
        var input=new Input[1]; input[0].mouse.x=x; input[0].mouse.y=y; input[0].mouse.flags=1;
        return SendInput(1,input,40)==1;
    }
}
'@
$window=$mcc[0].MainWindowHandle
$null=[MccRelativeLook]::Focus($window)
Start-Sleep -Milliseconds 150
if (-not [MccRelativeLook]::Look($window,$X,$Y)) { throw 'MCC does not own foreground input; no mouse movement sent.' }

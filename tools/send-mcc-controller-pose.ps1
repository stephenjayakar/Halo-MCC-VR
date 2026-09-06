[CmdletBinding()]
param(
    [ValidateRange(-1.5,1.5)][float]$X=.18,
    [ValidateRange(-1.8,.8)][float]$Y=-.18,
    [ValidateRange(-1.6,.4)][float]$Z=-.65,
    [ValidateRange(-180,180)][float]$YawDegrees=0,
    [ValidateRange(-180,180)][float]$PitchDegrees=0,
    [ValidateRange(-180,180)][float]$RollDegrees=0,
    [ValidateRange(200,5000)][uint32]$DurationMilliseconds=1000,
    [switch]$Wait
)
$ErrorActionPreference='Stop'
$mcc=@(Get-Process MCC-Win64-Shipping -ErrorAction Stop)
if ($mcc.Count -ne 1 -or $mcc[0].MainWindowHandle -eq 0) { throw 'Exactly one visible MCC process is required.' }
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class MccControllerPoseSender {
 [StructLayout(LayoutKind.Sequential,Pack=4)]
 public struct Command { public uint version,duration; public float x,y,z,qx,qy,qz,qw; }
 [StructLayout(LayoutKind.Sequential)]
 public struct CopyData { public UIntPtr id; public int size; public IntPtr data; }
 [DllImport("user32.dll",SetLastError=true)]
 static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wp,ref CopyData data,uint flags,uint timeout,out UIntPtr result);
 public static bool Send(IntPtr window,float x,float y,float z,float yaw,float pitch,float roll,uint duration) {
  double f=Math.PI/360.0,sy=Math.Sin(yaw*f),cy=Math.Cos(yaw*f),sp=Math.Sin(pitch*f),cp=Math.Cos(pitch*f),sr=Math.Sin(roll*f),cr=Math.Cos(roll*f);
  var command=new Command {version=1,duration=duration,x=x,y=y,z=z,
   qx=(float)(cy*sp*cr+sy*cp*sr),qy=(float)(sy*cp*cr-cy*sp*sr),qz=(float)(cy*cp*sr-sy*sp*cr),qw=(float)(cy*cp*cr+sy*sp*sr)};
  if(Marshal.SizeOf(typeof(Command))!=36) throw new InvalidOperationException("Controller protocol layout mismatch.");
  IntPtr bytes=Marshal.AllocHGlobal(36);
  try {
   Marshal.StructureToPtr(command,bytes,false);
   var data=new CopyData {id=new UIntPtr(0x48335053),size=36,data=bytes};
   UIntPtr result;
   var sent=SendMessageTimeout(window,0x004A,UIntPtr.Zero,ref data,2,2000,out result);
   if(sent==IntPtr.Zero) throw new InvalidOperationException("MCC command delivery failed or timed out. Do not retry until the current motion is known complete.");
   return result.ToUInt64()==1;
  } finally { Marshal.FreeHGlobal(bytes); }
 }
}
'@
$started=[DateTime]::UtcNow
$accepted=[MccControllerPoseSender]::Send($mcc[0].MainWindowHandle,$X,$Y,$Z,$YawDegrees,$PitchDegrees,$RollDegrees,$DurationMilliseconds)
$record=[ordered]@{mcc_pid=$mcc[0].Id;utc=$started.ToString('o');accepted=$accepted;position_m=@($X,$Y,$Z);yaw_pitch_roll_degrees=@($YawDegrees,$PitchDegrees,$RollDegrees);duration_ms=$DurationMilliseconds;protocol=1;space='OpenXR LOCAL';source='Addressed WM_COPYDATA to opt-in Halo 3 null-controller path'}
$dir=Join-Path $PSScriptRoot '../out/debug-openxr/controller-commands'
$null=New-Item -ItemType Directory -Path $dir -Force
$path=Join-Path $dir ($started.ToString('yyyyMMdd-HHmmssfffffff')+'.json')
$record | ConvertTo-Json | Set-Content -LiteralPath $path
if (-not $accepted) { throw "MCC rejected the pose: diagnostic gameplay must be active, the previous motion complete, and requested peak speeds within bounds. Record: $path" }
if ($Wait) { Start-Sleep -Milliseconds ($DurationMilliseconds+100) }
$record | ConvertTo-Json

# Runs the PC build, optionally presses keys, and saves screenshots of its window.
#   pc_shot.ps1 -Dir <run folder> -Steps "wait:4","shot:menu.png","key:SPACE","wait:8","shot:level.png"
param(
    [string]$Exe = "$PSScriptRoot\..\..\build_pc\openclaw_s60.exe",
    [string]$Dir = ".",
    [string[]]$Steps = @("wait:5", "shot:shot.png")
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}
"@

$vk = @{ SPACE = 0x20; ENTER = 0x0D; ESC = 0x1B; LEFT = 0x25; UP = 0x26; RIGHT = 0x27; DOWN = 0x28;
         CTRL = 0x11; ALT = 0x12; SHIFT = 0x10 }

$p = Start-Process -FilePath (Resolve-Path $Exe) -WorkingDirectory (Resolve-Path $Dir) -PassThru
try {
    foreach ($s in $Steps) {
        $kind, $arg = $s.Split(":", 2)
        $p.Refresh()
        $h = $p.MainWindowHandle
        switch ($kind) {
            "wait" { Start-Sleep -Milliseconds ([double]$arg * 1000) }
            "mem" { Write-Output ("{0}: private {1:N0} KB" -f $arg, ($p.PrivateMemorySize64 / 1024)) }
            "key" {
                [W]::SetForegroundWindow($h) | Out-Null
                $code = [byte]$vk[$arg]
                [W]::keybd_event($code, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
                [W]::keybd_event($code, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
            }
            "hold" {
                $k, $ms = $arg.Split(",")
                [W]::SetForegroundWindow($h) | Out-Null
                $code = [byte]$vk[$k]
                [W]::keybd_event($code, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds ([int]$ms)
                [W]::keybd_event($code, 0, 2, [UIntPtr]::Zero)
            }
            "shot" {
                [W]::SetForegroundWindow($h) | Out-Null
                Start-Sleep -Milliseconds 200
                $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null
                $pt = New-Object W+POINT; [W]::ClientToScreen($h, [ref]$pt) | Out-Null
                $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, $bmp.Size)
                $bmp.Save((Join-Path (Resolve-Path $Dir) $arg))
                $g.Dispose(); $bmp.Dispose()
            }
        }
        if ($p.HasExited) { Write-Output "exited: $($p.ExitCode)"; break }
    }
} finally {
    if (!$p.HasExited) { $p.Kill() }
}

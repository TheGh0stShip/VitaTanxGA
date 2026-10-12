param(
    [Parameter(Mandatory = $true)]
    [string]$VpkPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$Vita3KPath = 'D:\Vita3K\Vita3K.exe',
    [int]$StartupTimeoutSeconds = 240
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class VitaTanxWindow {
    public delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct Point { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr window, ref Point point);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);
}
'@

function Find-GameWindow([int]$ProcessId) {
    $found = [IntPtr]::Zero
    $callback = [VitaTanxWindow+EnumWindowsProc]{
        param([IntPtr]$window, [IntPtr]$parameter)
        $owner = 0
        [void][VitaTanxWindow]::GetWindowThreadProcessId($window, [ref]$owner)
        if ($owner -eq $ProcessId -and [VitaTanxWindow]::IsWindowVisible($window)) {
            $title = New-Object System.Text.StringBuilder 512
            [void][VitaTanxWindow]::GetWindowText($window, $title, $title.Capacity)
            if ($title.ToString().StartsWith('VitaTanx GA (VTGA00001)')) {
                $script:foundWindow = $window
                return $false
            }
        }
        return $true
    }
    $script:foundWindow = [IntPtr]::Zero
    [void][VitaTanxWindow]::EnumWindows($callback, [IntPtr]::Zero)
    return $script:foundWindow
}

function Capture-Client([IntPtr]$Window, [string]$Path) {
    $rect = New-Object VitaTanxWindow+Rect
    $origin = New-Object VitaTanxWindow+Point
    if (-not [VitaTanxWindow]::GetClientRect($Window, [ref]$rect) -or
        -not [VitaTanxWindow]::ClientToScreen($Window, [ref]$origin)) {
        throw 'Unable to resolve the Vita3K client rectangle.'
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -ne 960 -or $height -ne 544) {
        throw "Unexpected Vita3K client size ${width}x${height}; expected 960x544."
    }
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $Vita3KPath -PathType Leaf)) {
    throw "Vita3K executable not found: $Vita3KPath"
}
if (-not (Test-Path -LiteralPath $VpkPath -PathType Leaf)) {
    throw "VPK not found: $VpkPath"
}
[void](New-Item -ItemType Directory -Force -Path $OutputDirectory)

$process = Start-Process -FilePath $Vita3KPath `
    -WorkingDirectory (Split-Path -Parent $Vita3KPath) `
    -ArgumentList @('-z', ('"' + $VpkPath + '"')) -PassThru
try {
    $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    $window = [IntPtr]::Zero
    $ready = $false
    do {
        if ($process.HasExited) {
            throw "Vita3K exited during startup with code $($process.ExitCode)."
        }
        Start-Sleep -Milliseconds 500
        $window = Find-GameWindow $process.Id
        if ($window -ne [IntPtr]::Zero) {
            $title = New-Object System.Text.StringBuilder 512
            [void][VitaTanxWindow]::GetWindowText($window, $title, $title.Capacity)
            if ($title.ToString() -notlike '*Please wait, loading*') {
                $ready = $true
                break
            }
        }
    } while ([DateTime]::UtcNow -lt $deadline)
    if (-not $ready) { throw 'Timed out waiting for the VitaTanx game window.' }

    Start-Sleep -Seconds 3
    $titlePath = Join-Path $OutputDirectory 'vita3k-title.png'
    Capture-Client $window $titlePath

    [void][VitaTanxWindow]::SetForegroundWindow($window)
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait('x')
    Start-Sleep -Seconds 12
    $gameplayPath = Join-Path $OutputDirectory 'vita3k-gameplay.png'
    Capture-Client $window $gameplayPath

    Write-Output $titlePath
    Write-Output $gameplayPath
} finally {
    if (-not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(5000)) { $process.Kill() }
    }
}

param(
    [string]$Label = "sample",
    [string]$ShotList = "",
    [string]$Save = "mini3d_pv.sav",
    [string]$Config = "openttd.cfg",
    [int]$Seed = -1,
    [int]$Speed = -1,
    [int]$Step = -1,
    [int]$TimeoutSec,
    [int]$StallSec = 60,
    [string]$DebugLevels = "3",
    [string]$Build = "",
    [switch]$Hud
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
$defaultWaitFrames = 60
$baseTimeoutSec = 240
$timeoutSecPerShot = 3
$timeoutSecPerFrame = 0.05
. (Join-Path $verify "slots.ps1")
. (Join-Path $verify "failure.ps1")

function Measure-ShotList([string]$Path) {
    $wait = $defaultWaitFrames
    $plan = @{ Pngs = 0; Frames = 0 }
    foreach ($line in Get-Content $Path | Where-Object { $_ -match '^\s*[^#\s]' }) {
        $fields = -split $line
        switch ($fields[0]) {
            "wait" { if ($fields.Count -gt 1) { $wait = [Math]::Max(1, [long]$fields[1]) } }
            "glide" { $plan.Frames += [Math]::Max(1, [long]$fields[1]) }
            default { $plan.Pngs++; $plan.Frames += $wait }
        }
    }
    $plan
}

function Get-ScaledTimeoutSec($Plan) {
    [int][Math]::Ceiling($baseTimeoutSec + $Plan.Pngs * $timeoutSecPerShot + $Plan.Frames * $timeoutSecPerFrame)
}
$repo = Split-Path $verify -Parent
$run = Join-Path $verify "run"
$buildDir = if ($Build -eq "") { Join-Path $repo "build" } else { (Resolve-Path -LiteralPath $Build).Path }
$exe = Join-Path $buildDir "openttd.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "no openttd.exe in $buildDir" }
$out = Join-Path $verify "shots\$Label"
$generating = $Seed -ge 0
if ($ShotList -eq "") { $ShotList = Join-Path $verify $(if ($generating) { "shots-idle.txt" } else { "shots-main.txt" }) }
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item $ShotList (Join-Path $out "shots.txt") -Force
$log = Join-Path $out "game.log"

if (-not ("WindowTitles" -as [type])) { Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class WindowTitles {
    delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
    public static string[] Of(int pid) {
        var titles = new List<string>();
        EnumWindows((hwnd, l) => {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            if (owner == (uint)pid && IsWindowVisible(hwnd)) {
                var text = new StringBuilder(256);
                GetWindowText(hwnd, text, 256);
                titles.Add(text.ToString());
            }
            return true;
        }, IntPtr.Zero);
        return titles.ToArray();
    }
}
"@ }

if (-not ("QuietLaunch" -as [type])) { Add-Type @"
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
public static class OffscreenWindow {
    static readonly string[] SENT_AWAY_CLASSES = { "OTTD", "ConsoleWindowClass" };
    const int CLASS_NAME_CAPACITY = 256;
    const uint SWEEP_MILLISECONDS = 250;
    const uint EVENT_OBJECT_CREATE = 0x8000;
    const uint EVENT_OBJECT_SHOW = 0x8002;
    const uint EVENT_OBJECT_LOCATIONCHANGE = 0x800B;
    const uint WINEVENT_OUTOFCONTEXT = 0x0;
    const uint ANY_PROCESS = 0;
    const int OBJID_WINDOW = 0;
    const int CHILDID_SELF = 0;
    const uint MONITOR_DEFAULTTONULL = 0x0;
    const int SM_XVIRTUALSCREEN = 76;
    const int SM_CXVIRTUALSCREEN = 78;
    const uint SWP_NOSIZE = 0x1;
    const uint SWP_NOACTIVATE = 0x10;
    static readonly IntPtr HWND_BOTTOM = new IntPtr(1);
    static readonly IntPtr DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = new IntPtr(-4);
    delegate void WinEventProc(IntPtr hook, uint kind, IntPtr hwnd, int objectId, int childId, uint thread, uint time);
    delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)]
    struct Rect { public int left; public int top; public int right; public int bottom; }
    [StructLayout(LayoutKind.Sequential)]
    struct Message { public IntPtr hwnd; public uint message; public UIntPtr wParam; public IntPtr lParam; public uint time; public int x; public int y; }
    [DllImport("user32.dll")] static extern IntPtr SetWinEventHook(uint first, uint last, IntPtr module, WinEventProc proc, uint process, uint thread, uint flags);
    [DllImport("user32.dll")] static extern bool UnhookWinEvent(IntPtr hook);
    [DllImport("user32.dll")] static extern UIntPtr SetTimer(IntPtr hwnd, UIntPtr id, uint milliseconds, IntPtr proc);
    [DllImport("user32.dll")] static extern bool KillTimer(IntPtr hwnd, UIntPtr id);
    [DllImport("user32.dll")] static extern int GetMessage(out Message message, IntPtr hwnd, uint first, uint last);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder name, int capacity);
    [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    public static int FarLeft(int width) {
        return GetSystemMetrics(SM_XVIRTUALSCREEN) - GetSystemMetrics(SM_CXVIRTUALSCREEN) - width;
    }
    public static void Hold(Process game) {
        using (var hooked = new ManualResetEvent(false)) {
            new Thread(() => Watch(game, hooked)) { IsBackground = true }.Start();
            hooked.WaitOne();
        }
    }
    static void Watch(Process game, ManualResetEvent hooked) {
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        uint pid = (uint)game.Id;
        WinEventProc changed = (hook, kind, hwnd, objectId, childId, thread, time) => {
            if (objectId == OBJID_WINDOW && childId == CHILDID_SELF) SendAway(hwnd, pid, kind == EVENT_OBJECT_SHOW);
        };
        IntPtr gameEvents = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_LOCATIONCHANGE, IntPtr.Zero, changed, pid, 0, WINEVENT_OUTOFCONTEXT);
        IntPtr consoleEvents = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, IntPtr.Zero, changed, ANY_PROCESS, 0, WINEVENT_OUTOFCONTEXT);
        UIntPtr sweep = SetTimer(IntPtr.Zero, UIntPtr.Zero, SWEEP_MILLISECONDS, IntPtr.Zero);
        hooked.Set();
        Message message;
        do SendAwayAll(pid); while (!game.HasExited && GetMessage(out message, IntPtr.Zero, 0, 0) > 0);
        KillTimer(IntPtr.Zero, sweep);
        UnhookWinEvent(consoleEvents);
        UnhookWinEvent(gameEvents);
        GC.KeepAlive(changed);
    }
    static void SendAwayAll(uint pid) {
        EnumWindows((hwnd, l) => { SendAway(hwnd, pid, false); return true; }, IntPtr.Zero);
    }
    static void SendAway(IntPtr hwnd, uint pid, bool shown) {
        uint owner;
        GetWindowThreadProcessId(hwnd, out owner);
        if (owner != pid || !IsSentAway(hwnd)) return;
        if (!shown && MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL) == IntPtr.Zero) return;
        Rect rect;
        GetWindowRect(hwnd, out rect);
        SetWindowPos(hwnd, HWND_BOTTOM, FarLeft(rect.right - rect.left), rect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
    static bool IsSentAway(IntPtr hwnd) {
        var name = new StringBuilder(CLASS_NAME_CAPACITY);
        GetClassName(hwnd, name, name.Capacity);
        return Array.IndexOf(SENT_AWAY_CLASSES, name.ToString()) >= 0;
    }
}
public static class QuietLaunch {
    const uint STARTF_USESHOWWINDOW = 0x1;
    const uint STARTF_USEPOSITION = 0x4;
    const uint STARTF_USESTDHANDLES = 0x100;
    const short SW_SHOWNOACTIVATE = 4;
    const uint CREATE_SUSPENDED = 0x4;
    const uint GENERIC_WRITE = 0x40000000;
    const uint FILE_SHARE_READ_WRITE = 0x3;
    const uint CREATE_ALWAYS = 2;
    const uint FILE_ATTRIBUTE_NORMAL = 0x80;
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct StartupInfo {
        public int cb; public string reserved; public string desktop; public string title;
        public int x; public int y; public int xSize; public int ySize; public int xCountChars; public int yCountChars; public int fillAttribute;
        public uint flags; public short showWindow; public short reserved2; public IntPtr reserved3;
        public IntPtr stdInput; public IntPtr stdOutput; public IntPtr stdError;
    }
    [StructLayout(LayoutKind.Sequential)]
    struct ProcessInformation { public IntPtr process; public IntPtr thread; public int processId; public int threadId; }
    [StructLayout(LayoutKind.Sequential)]
    struct SecurityAttributes { public int length; public IntPtr descriptor; public bool inherit; }
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CreateProcess(string application, string commandLine, IntPtr processAttributes, IntPtr threadAttributes, bool inheritHandles, uint flags, IntPtr environment, string directory, ref StartupInfo startup, out ProcessInformation information);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr CreateFile(string name, uint access, uint share, ref SecurityAttributes security, uint disposition, uint attributes, IntPtr template);
    [DllImport("kernel32.dll")] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    static IntPtr InheritableFile(string path) {
        var security = new SecurityAttributes { length = Marshal.SizeOf(typeof(SecurityAttributes)), inherit = true };
        return CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ_WRITE, ref security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, IntPtr.Zero);
    }
    public static Process Start(string exe, string arguments, string directory, string stdOut, string stdErr) {
        IntPtr output = InheritableFile(stdOut);
        IntPtr error = InheritableFile(stdErr);
        var startup = new StartupInfo {
            cb = Marshal.SizeOf(typeof(StartupInfo)),
            flags = STARTF_USESHOWWINDOW | STARTF_USEPOSITION | STARTF_USESTDHANDLES,
            showWindow = SW_SHOWNOACTIVATE,
            x = OffscreenWindow.FarLeft(0),
            stdOutput = output,
            stdError = error,
        };
        ProcessInformation information;
        bool started = CreateProcess(exe, "\"" + exe + "\" " + arguments, IntPtr.Zero, IntPtr.Zero, true, CREATE_SUSPENDED, IntPtr.Zero, directory, ref startup, out information);
        int failure = Marshal.GetLastWin32Error();
        CloseHandle(output);
        CloseHandle(error);
        if (!started) throw new Win32Exception(failure);
        Process process = Process.GetProcessById(information.processId);
        IntPtr held = process.Handle;
        OffscreenWindow.Hold(process);
        ResumeThread(information.thread);
        CloseHandle(information.thread);
        CloseHandle(information.process);
        return process;
    }
}
"@ }

$plan = Measure-ShotList $ShotList
$expected = if ($generating) { 0 } else { $plan.Pngs }
if (-not $PSBoundParameters.ContainsKey("TimeoutSec")) {
    $TimeoutSec = if ($generating) { $baseTimeoutSec } else { Get-ScaledTimeoutSec $plan }
}
$env:OTTD_MINI_CAPTURE = Join-Path $out "shots.txt"
$env:OTTD_MINI_CAPTURE_CLEAN = if ($Hud) { $null } else { "1" }
$env:OTTD_MINI_CAPTURE_SPEED = if ($Speed -ge 0) { $Speed } else { $null }
$env:OTTD_MINI_CAPTURE_STEP = if ($Step -ge 0) { $Step } else { $null }
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
$world = if ($generating) { "-G $Seed -g" } else { "-g save\$Save" }
$errLog = Join-Path $out "game.err"

function Stop-Game($reason) {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force; $proc.WaitForExit(5000) | Out-Null }
    $crashLog = Get-NewCrashLog $slot $knownCrashLogs
    if ($crashLog) { $reason = (@($reason, "crash: $(Get-CrashReason $crashLog)") | Where-Object { $_ }) -join "; " }
    Get-Content $errLog -ErrorAction SilentlyContinue | Set-Content $log
    Remove-Item $errLog -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $out "game.out") -Force -ErrorAction SilentlyContinue
    if ($reason) {
        Write-Output "FAILED: $reason"
        Get-Content $log -Tail 25
        exit 1
    }
}

$lease = Lock-GameSlot
try {
    $slot = Initialize-Slot $run $lease.Index
    $knownCrashLogs = Get-CrashLogNames $slot
    $arguments = "-c ""$(Join-Path $slot $Config)"" -s null -m null -v win32-opengl -d $DebugLevels $world"
    $proc = [QuietLaunch]::Start($exe, $arguments, $run, (Join-Path $out "game.out"), $errLog)
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $lastProgress = 0.0
    $lastCount = 0
    while (-not $proc.HasExited) {
        Start-Sleep -Milliseconds 500
        $titles = [WindowTitles]::Of($proc.Id)
        if ((Test-ErrorDialog $titles) -or (Get-NewCrashLog $slot $knownCrashLogs)) { Stop-Game "error dialog" }
        $count = @(Get-ChildItem $out -Filter *.png).Count
        if ($count -ne $lastCount) { $lastCount = $count; $lastProgress = $sw.Elapsed.TotalSeconds }
        if ($sw.Elapsed.TotalSeconds -gt $TimeoutSec) { Stop-Game "timeout ${TimeoutSec}s with $count of $expected shots" }
        if ($expected -gt 0 -and $sw.Elapsed.TotalSeconds - $lastProgress -gt $StallSec) { Stop-Game "stalled ${StallSec}s with $count of $expected shots" }
    }
    Stop-Game $null
} finally {
    if ($null -ne $proc -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    Unlock-Slots $lease
}
$count = @(Get-ChildItem $out -Filter *.png).Count
"exit=$($proc.ExitCode) seconds=$([int]$sw.Elapsed.TotalSeconds) shots=$count/$expected"
Get-ChildItem $out -Filter *.png | Select-Object Name, Length
if ($count -ne $expected) { Get-Content $log -Tail 25; exit 1 }
exit 0

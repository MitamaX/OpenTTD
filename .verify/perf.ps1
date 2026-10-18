param(
    [string]$Tag = "a",
    [string[]]$Run = @("std-downtown", "town", "downtown", "wide", "town-400", "town-1600", "glide"),
    [int]$Refresh = 120,
    [string]$Build = ""
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
. (Join-Path $verify "slots.ps1")
$userSave = "user-a.sav"
$userConfig = "openttd-user.cfg"
$recordSec = 1800
$debugLevels = "3,desync=0"
$python = "C:\msys64\ucrt64\bin\python.exe"

$scenarios = @{
    "std-downtown" = @{ List = "shots-hitch-std-downtown.txt"; Save = "mini3d_pv.sav" }
    "town"         = @{ List = "shots-hitch-town.txt" }
    "town-short"   = @{ List = "shots-hitch-town-short.txt" }
    "downtown"     = @{ List = "shots-hitch-downtown.txt" }
    "wide"         = @{ List = "shots-hitch-wide.txt" }
    "town-400"     = @{ List = "shots-hitch-town.txt"; Speed = 400 }
    "town-1600"    = @{ List = "shots-hitch-town.txt"; Speed = 1600 }
    "town-still"   = @{ List = "shots-hitch-town.txt"; Speed = 1 }
    "town-lowres"  = @{ List = "shots-hitch-town.txt"; Config = "openttd-user-lowres.cfg" }
    "town-clean"   = @{ List = "shots-hitch-town.txt"; Clean = $true }
    "glide"        = @{ List = "shots-hitch-glide.txt" }
    "autosave"     = @{ List = "shots-hitch-autosave.txt"; Config = "openttd-user-autosave.cfg" }
    "turn"         = @{ List = "shots-hitch-turn.txt"; Save = "mini3d_pv.sav" }
}

$timelines = @()
$gpu = Lock-Gpu
try {
    foreach ($name in $Run) {
        $scenario = $scenarios[$name]
        if ($null -eq $scenario) { throw "unknown scenario $name" }
        $label = "hitch-$name-$Tag"
        $capture = @{
            Label = $label
            ShotList = Join-Path $verify $scenario.List
            Save = if ($scenario.Save) { $scenario.Save } else { $userSave }
            Config = if ($scenario.Config) { $scenario.Config } else { $userConfig }
            TimeoutSec = $recordSec
            StallSec = $recordSec
            DebugLevels = $debugLevels
            Build = $Build
            Hud = -not $scenario.Clean
        }
        if ($scenario.Speed) { $capture.Speed = $scenario.Speed }

        $env:OTTD_MINI_TIMELINE = "1"
        & (Join-Path $verify "capture.ps1") @capture | Select-Object -First 1
        Remove-Item Env:OTTD_MINI_TIMELINE

        $out = Join-Path $verify "shots\$label"
        $timeline = Join-Path $out "timeline.txt"
        if (-not (Test-Path $timeline)) { Write-Output "no timeline for $label"; continue }
        & $python -I (Join-Path $verify "hitch.py") $timeline --refresh $Refresh | Set-Content (Join-Path $out "hitch.txt") -Encoding utf8
        $timelines += $timeline
    }
} finally {
    Unlock-Slots $gpu
}
if ($timelines.Count -gt 0) { & $python -I (Join-Path $verify "hitch.py") @timelines --refresh $Refresh --compare }

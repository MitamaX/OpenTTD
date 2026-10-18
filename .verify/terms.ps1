param(
    [string]$Label = "terms",
    [string[]]$Runs = @("user", "std"),
    [string[]]$Sections = @("frame", "world", "lay.structures", "draw.structures", "open.structures", "draw.terrain", "draw.forest", "draw.water"),
    [string]$Build = ""
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
. (Join-Path $verify "slots.ps1")
$plans = @{
    "user" = @{ List = "shots-terms-user.txt"; Save = "user-a.sav" }
    "std"  = @{ List = "shots-terms-std.txt"; Save = "mini3d_pv.sav" }
    "forest" = @{ List = "shots-terms-forest.txt"; Save = "mini3d_pv.sav" }
    "hill" = @{ List = "shots-terms-hill.txt"; Save = "mini3d_pv.sav" }
    "junction" = @{ List = "shots-terms-junction.txt"; Save = "mini3d_pv.sav" }
    "glass" = @{ List = "shots-terms-glass.txt"; Save = "mini3d_pv.sav" }
    "coast" = @{ List = "shots-terms-coast.txt"; Save = "mini3d_pv.sav" }
    "toycoast" = @{ List = "shots-terms-toycoast.txt"; Save = "climate-toyland.sav" }
}

function Read-Profile($path) {
    $blocks = @{}
    $name = $null
    foreach ($line in Get-Content $path) {
        if ($line.StartsWith("# ")) { $name = [IO.Path]::GetFileNameWithoutExtension($line.Substring(2)); $blocks[$name] = @{}; continue }
        $fields = $line -split "\s+"
        if ($null -ne $name -and $fields.Count -ge 7 -and $fields[0] -ne "section") { $blocks[$name][$fields[0]] = [double]$fields[1] }
    }
    return $blocks
}

$env:OTTD_MINI_PROFILE = "1"
$rows = @()
$gpu = Lock-Gpu
try {
    foreach ($runName in $Runs) {
        $plan = $plans[$runName]
        $runLabel = "terms-$Label-$runName"
        & (Join-Path $verify "capture.ps1") -Label $runLabel -ShotList (Join-Path $verify $plan.List) -Save $plan.Save -Config "openttd-user.cfg" -DebugLevels "3,desync=0" -Build $Build -Hud -TimeoutSec 900 -StallSec 300 | Select-Object -First 1
        $profilePath = Join-Path $verify "shots\$runLabel\profile.txt"
        if (-not (Test-Path $profilePath)) { Write-Output "no profile for $runLabel"; continue }
        $blocks = Read-Profile $profilePath
        foreach ($shot in $blocks.Keys | Sort-Object) {
            if ($shot -eq "warm") { continue }
            $cells = $Sections | ForEach-Object { "{0,10:F2}" -f $(if ($blocks[$shot].ContainsKey($_)) { $blocks[$shot][$_] } else { 0.0 }) }
            $rows += ("{0,-12}" -f $shot) + ($cells -join "")
        }
    }
} finally {
    Unlock-Slots $gpu
}
Remove-Item Env:OTTD_MINI_PROFILE
("{0,-12}" -f "shot") + (($Sections | ForEach-Object { "{0,10}" -f ($_ -replace "^draw\.", "d." -replace "^lay\.", "l." -replace "^open\.", "o." -replace "structures", "struct") }) -join "")
$rows

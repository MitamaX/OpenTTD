param(
    [string[]]$Climates = @("arctic", "tropic", "toyland"),
    [int]$Seed = 20261009,
    [int]$Months = 18,
    [int]$Speed = 0,
    [int]$TimeoutSec = 420
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
. (Join-Path $verify "slots.ps1")
$run = Join-Path $verify "run"

$terrainOf = @{ arctic = 3; tropic = 2; toyland = 2; temperate = 2 }
$common = @{ number_towns = 3; quantity_sea_lakes = 2; amount_of_rivers = 3; town_growth_rate = 0; show_finances = "false" }

function Step-Path($step) { Join-Path $scripts "climate_step$step.scr" }

function Write-Lines($path, $lines) {
    [IO.File]::WriteAllText($path, ($lines -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
}

function Write-ClimateConfig($climate) {
    $text = Get-Content (Join-Path $run "openttd.cfg") -Raw
    $settings = @{ landscape = $climate; terrain_type = $terrainOf[$climate] } + $common
    foreach ($key in $settings.Keys) { $text = $text -replace "(?m)^$key = .*$", "$key = $($settings[$key])" }
    $text = $text -replace "(?s)(\[game_scripts\]\r?\n)none = ", '${1}ClimateGrower = '
    [IO.File]::WriteAllText($config, $text, (New-Object Text.UTF8Encoding $false))
}

function Write-Scripts($climate) {
    Write-Lines $autoexec @("schedule on-next-calendar-month scripts/climate_step1.scr")
    for ($step = 1; $step -lt $Months; $step++) {
        Write-Lines (Step-Path $step) @("schedule on-next-calendar-month scripts/climate_step$($step + 1).scr")
    }
    Write-Lines (Step-Path $Months) @("save climate-$climate", "exit")
}

function Remove-IfPresent($path) { if (Test-Path $path) { Remove-Item $path -Force } }

$lease = Lock-GameSlot
try {
    $slot = Initialize-Slot $run $lease.Index
    $scripts = Join-Path $slot "scripts"
    $autoexec = Join-Path $scripts "autoexec.scr"
    $configName = "climate-slot$($lease.Index).cfg"
    $config = Join-Path $run $configName
    New-Item -ItemType Directory -Force $scripts | Out-Null
    foreach ($climate in $Climates) {
        $generated = Join-Path $slot "save\climate-$climate.sav"
        $saved = Join-Path $run "save\climate-$climate.sav"
        Remove-IfPresent $generated
        Remove-IfPresent $saved
        Write-ClimateConfig $climate
        Write-Scripts $climate
        & (Join-Path $verify "capture.ps1") -Label "gen-$climate" -Config $configName -Seed $Seed -Speed $Speed -TimeoutSec $TimeoutSec
        if ($LASTEXITCODE -ne 0) { throw "generation of $climate failed" }
        if (-not (Test-Path $generated)) { throw "$generated was not written" }
        Copy-Item $generated $saved
        Select-String -Path (Join-Path $verify "shots\gen-$climate\game.log") -Pattern "] VIEW " | ForEach-Object { "$climate $($_.Line)" }
    }
} finally {
    if ($scripts) {
        Remove-IfPresent $autoexec
        Remove-IfPresent $config
        1..$Months | ForEach-Object { Remove-IfPresent (Step-Path $_) }
        if (Test-Path $scripts) { Remove-Item $scripts -Force }
    }
    Unlock-Slots $lease
}

param(
    [string[]]$Saves = @("mini3d_pv", "climate-arctic", "climate-tropic", "climate-toyland"),
    [int]$TimeoutSec = 600
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
. (Join-Path $verify "slots.ps1")
$run = Join-Path $verify "run"
$liveSuffix = "-live"
$frozenName = "freeze-output"
$stepName = "freeze_step"
$neverEndingList = "wait 1000000", "freeze 0 0 6 0 40"

function Write-Lines($path, $lines) {
    [IO.File]::WriteAllText($path, ($lines -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
}

function Remove-IfPresent($path) { if (Test-Path $path) { Remove-Item $path -Force } }

$lease = Lock-GameSlot
try {
    $slot = Initialize-Slot $run $lease.Index
    $scripts = Join-Path $slot "scripts"
    $autoexec = Join-Path $scripts "autoexec.scr"
    $step = Join-Path $scripts "$stepName.scr"
    $generated = Join-Path $slot "save\$frozenName.sav"
    $list = Join-Path $verify "freeze-list-$PID.tmp"
    New-Item -ItemType Directory -Force $scripts | Out-Null
    $neverEndingList | Set-Content $list
    Write-Lines $autoexec @("schedule on-next-calendar-month scripts/$stepName.scr")
    Write-Lines $step @("pause", "save $frozenName", "exit")
    foreach ($name in $Saves) {
        $live = Join-Path $run "save\$name$liveSuffix.sav"
        $target = Join-Path $run "save\$name.sav"
        if (-not (Test-Path $live)) { Copy-Item $target $live }
        Remove-IfPresent $generated
        & (Join-Path $verify "capture.ps1") -Label "freeze-$name" -ShotList $list -Save "$name$liveSuffix.sav" -Speed 0 -TimeoutSec $TimeoutSec | Out-Null
        if (-not (Test-Path $generated)) { throw "$name was not frozen" }
        Copy-Item $generated $target -Force
    }
} finally {
    Remove-IfPresent $autoexec
    Remove-IfPresent $step
    Remove-IfPresent $list
    Remove-IfPresent $generated
    if ($scripts -and (Test-Path $scripts)) { Remove-Item $scripts -Force }
    Unlock-Slots $lease
}

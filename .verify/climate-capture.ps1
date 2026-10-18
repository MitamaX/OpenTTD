param(
    [string]$ShotList = "shots-climates.txt",
    [string[]]$Climates = @(),
    [string]$Group = "climates",
    [int]$Step = -1,
    [int]$Speed = -1,
    [string]$Build = ""
)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
$defaultClimate = "temperate"
$lines = Get-Content ([IO.Path]::Combine($verify, $ShotList)) | Where-Object { $_ -match '^\s*[^#\s]' }
if ($Climates.Count -eq 0) { $Climates = $lines | ForEach-Object { ($_ -split '\s+')[0] } | Select-Object -Unique }

foreach ($climate in $Climates) {
    $list = Join-Path $verify "shots-climate-$climate-$PID.tmp"
    $shots = @($lines | Where-Object { $_ -like "$climate *" } | ForEach-Object { $_.Substring($climate.Length + 1) })
    if ($shots.Count -eq 0) { throw "no shots for $climate in $ShotList" }
    $capture = @{ Label = "$Group\$climate"; ShotList = $list; Step = $Step; Speed = $Speed; Build = $Build }
    if ($climate -ne $defaultClimate) { $capture.Save = "climate-$climate.sav" }
    $shots | Set-Content $list
    try {
        & (Join-Path $verify "capture.ps1") @capture
        if ($LASTEXITCODE -ne 0) { throw "capture of $climate failed" }
    } finally {
        Remove-Item $list -Force
    }
}

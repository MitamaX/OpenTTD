param([string]$Group)
$ErrorActionPreference = "Stop"
$verify = $PSScriptRoot
$lines = Get-Content (Join-Path $verify "shots-climates.txt")
foreach ($climate in "arctic", "tropic", "toyland") {
    $list = Join-Path $verify "shots-climate-$climate.tmp"
    $lines | Where-Object { $_ -like "$climate *" } | ForEach-Object { $_.Substring($climate.Length + 1) } | Set-Content $list
    try {
        & (Join-Path $verify "capture.ps1") -Label "$Group\$climate" -ShotList $list -Save "climate-$climate.sav" -Step 16 -Speed 1 | Select-Object -First 1
    } finally {
        Remove-Item $list -Force
    }
}

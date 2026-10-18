param([string]$Label, [string]$Base = "det-base-a", [string]$Twin = "det-base-b", [string]$ShotList = "", [string]$Save = "mini3d_pv.sav")
$ErrorActionPreference = "Stop"
$v = $PSScriptRoot
$capture = @{ Label = $Label; Step = 16; Speed = 1; TimeoutSec = 900; Save = $Save }
if ($ShotList -ne "") { $capture.ShotList = $ShotList }
& "$v\capture.ps1" @capture | Select-Object -First 1
& "$v\maskcmp.ps1" -Base "$v\shots\$Base" -Twin "$v\shots\$Twin" -Right "$v\shots\$Label"

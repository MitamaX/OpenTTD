param([string]$Label)
$ErrorActionPreference = "Stop"
$v = $PSScriptRoot
& "$v\capture.ps1" -Label "det-e$Label" -ShotList "$v\shots-edge.txt" -Step 16 -Speed 1 | Select-Object -First 1
& "$v\capture.ps1" -Label "det-u$Label" -ShotList "$v\shots-terms-user.txt" -Save user-a.sav -Config openttd-user.cfg -Step 16 -Speed 1 -TimeoutSec 600 | Select-Object -First 1
& "$v\maskcmp.ps1" -Base "$v\shots\det-ebase-a" -Twin "$v\shots\det-ebase-b" -Right "$v\shots\det-e$Label"
& "$v\maskcmp.ps1" -Base "$v\shots\det-ubase-a" -Twin "$v\shots\det-ubase-b" -Right "$v\shots\det-u$Label"

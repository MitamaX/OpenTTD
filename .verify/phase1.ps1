$ErrorActionPreference = "Stop"
$v = $PSScriptRoot
& "$v\capture.ps1" -Label gpu-before | Select-Object -First 1
& "$v\capture.ps1" -Label gpu-before2 | Select-Object -First 1
$env:OTTD_MINI_PROFILE = "1"
& "$v\climate-capture.ps1" -Group gpu-climates-before | Where-Object { $_ -match "^exit=|FAILED" }
Remove-Item Env:OTTD_MINI_PROFILE
& "$v\terms.ps1" -Label base0a -Runs user,std
& "$v\terms.ps1" -Label base0b -Runs user,std
& "$v\perf.ps1" -Tag before -Run town-clean | Select-Object -Last 3

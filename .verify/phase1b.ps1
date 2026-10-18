$ErrorActionPreference = "Stop"
$v = $PSScriptRoot
& "$v\variants.ps1" -Names base,t1off,t5off -Runs user,std -Tag p1
& "$v\capture.ps1" -Label gpu-t15 | Select-Object -First 1

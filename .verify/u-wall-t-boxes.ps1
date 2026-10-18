param([string]$Tag, [string]$NewLabel)
$v = $PSScriptRoot
$boxes = @(
 @('e01_rend_103_23',520,590,370,655,3),
 @('u20_c_y270',620,700,385,495,4),
 @('u20_c_y270',910,1005,375,490,4),
 @('sc37_hills_rail',1320,1400,690,780,4),
 @('sc37_hills_rail',1380,1490,690,780,4),
 @('18_pv_bridgehead',640,760,530,650,4),
 @('b03_rbridge_115_89',820,1200,330,600,2),
 @('u22_head_close',930,965,575,600,8),
 @('b09_riverbend_bank',600,800,640,740,4)
)
$n = 0
foreach ($b in $boxes) {
  $out = Join-Path $v "shots\u-wall-t-cmp\$Tag-$n-$($b[0]).png"
  New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
  & "$v\u-wall-t-cmp.ps1" -BaseDir "$v\shots\u-wall-t-base" -NewDir "$v\shots\$NewLabel" -Shot $b[0] -X0 $b[1] -X1 $b[2] -Y0 $b[3] -Y1 $b[4] -Out $out -Scale $b[5]
  $n++
}

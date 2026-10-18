param([string]$Image, [int]$X0, [int]$Y0, [int]$X1, [int]$Y1)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Image
$found = @()
for ($y = $Y0 + 2; $y -lt $Y1 - 2; $y++) { for ($x = $X0 + 2; $x -lt $X1 - 2; $x++) {
  $c = $bmp.GetPixel($x, $y); $l = $c.R + $c.G + $c.B
  $ok = $true
  foreach ($d in @(@(-2,0),@(2,0),@(0,-2),@(0,2))) { $n = $bmp.GetPixel($x + $d[0], $y + $d[1]); if (($n.R + $n.G + $n.B) + 90 -gt $l) { $ok = $false; break } }
  if ($ok) { $found += ("{0},{1}:{2}/{3}/{4}" -f $x, $y, $c.R, $c.G, $c.B) }
} }
($found | Select-Object -First 18) -join '  '
"count " + $found.Count
$bmp.Dispose()

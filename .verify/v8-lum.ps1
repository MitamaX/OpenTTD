param([string]$Image, [string]$Rects)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Image
foreach ($r in $Rects.Split(';')) {
  $p = $r.Split(',') | ForEach-Object { [int]$_ }
  $sum = 0.0; $n = 0
  for ($y = $p[1]; $y -lt $p[3]; $y += 2) { for ($x = $p[0]; $x -lt $p[2]; $x += 2) {
    $c = $bmp.GetPixel($x, $y); $sum += (0.2126*$c.R + 0.7152*$c.G + 0.0722*$c.B)/255.0; $n++ } }
  "{0}: {1:N3}" -f $r, ($sum/$n)
}
$bmp.Dispose()

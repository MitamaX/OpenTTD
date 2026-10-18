param([string]$Image, [string]$Boxes)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Image
foreach ($box in $Boxes.Split(';')) {
    $name, $x, $y, $w, $h = $box.Split(',')
    $x = [int]$x; $y = [int]$y; $w = [int]$w; $h = [int]$h
    $r = 0.0; $gr = 0.0; $bl = 0.0; $n = 0
    for ($j = $y; $j -lt $y + $h; $j++) { for ($i = $x; $i -lt $x + $w; $i++) { $c = $bmp.GetPixel($i, $j); $r += $c.R; $gr += $c.G; $bl += $c.B; $n++ } }
    $r /= 255 * $n; $gr /= 255 * $n; $bl /= 255 * $n
    "{0,-12} rgb=({1:F2},{2:F2},{3:F2}) L={4:F2}" -f $name, $r, $gr, $bl, (0.2126 * $r + 0.7152 * $gr + 0.0722 * $bl)
}
$bmp.Dispose()

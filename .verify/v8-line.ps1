param([string]$Image, [int]$Y, [int]$X0, [int]$X1, [int]$Step = 4)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Image
$out = @()
for ($x = $X0; $x -lt $X1; $x += $Step) { $c = $bmp.GetPixel($x, $Y); $out += ("{0}:{1:N0}" -f $x, (0.2126*$c.R + 0.7152*$c.G + 0.0722*$c.B)) }
$out -join ' '
$bmp.Dispose()

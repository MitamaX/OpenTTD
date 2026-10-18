param([string]$Image, [int]$X, [int]$Y0, [int]$Y1, [int]$Step = 4)
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Image
$out = @()
for ($y = $Y0; $y -lt $Y1; $y += $Step) { $c = $bmp.GetPixel($X, $y); $out += ("{0}:{1:N0}" -f $y, (0.2126*$c.R + 0.7152*$c.G + 0.0722*$c.B)) }
$out -join ' '
$bmp.Dispose()

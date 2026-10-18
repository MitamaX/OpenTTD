param([string]$Left, [string]$Right, [string]$Out, [double]$Scale = 0.5)
Add-Type -AssemblyName System.Drawing
$imgL = New-Object System.Drawing.Bitmap $Left
$imgR = New-Object System.Drawing.Bitmap $Right
$w = [int]($imgL.Width * $Scale); $h = [int]($imgL.Height * $Scale)
$dst = New-Object System.Drawing.Bitmap ($w * 2 + 4), $h
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.Clear([System.Drawing.Color]::Black)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($imgL, 0, 0, $w, $h)
$g.DrawImage($imgR, $w + 4, 0, $w, $h)
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $dst.Dispose(); $imgL.Dispose(); $imgR.Dispose()

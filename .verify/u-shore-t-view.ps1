param([string]$Shot, [int]$X, [int]$Y, [int]$W, [int]$H, [int]$Scale = 2, [string]$A = "u-shore-t-base2", [string]$B = "u-shore-t-o", [string]$Out = "")
Add-Type -AssemblyName System.Drawing
$root = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\shots"
if ($Out -eq "") { $Out = "$root\u-shore-t-view-$Shot.png" }
$dst = New-Object System.Drawing.Bitmap ($W * $Scale * 2 + 6), ($H * $Scale)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.Clear([System.Drawing.Color]::Magenta)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$i = 0
foreach ($d in @($A, $B)) {
  $src = New-Object System.Drawing.Bitmap "$root\$d\$Shot.png"
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle ($i * ($W * $Scale + 6)), 0, ($W * $Scale), ($H * $Scale)), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
  $src.Dispose(); $i++
}
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $dst.Dispose()

param([string]$Dir, [string]$Prefix, [string]$Out, [int]$Columns = 4, [double]$Scale = 0.3)
Add-Type -AssemblyName System.Drawing
$files = @(Get-ChildItem $Dir -Filter "$Prefix*.png" | Sort-Object Name)
$first = New-Object System.Drawing.Bitmap $files[0].FullName
$w = [int]($first.Width * $Scale); $h = [int]($first.Height * $Scale)
$first.Dispose()
$rows = [math]::Ceiling($files.Count / $Columns)
$dst = New-Object System.Drawing.Bitmap ($w * $Columns + ($Columns - 1) * 4), ($h * $rows + ($rows - 1) * 4)
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.Clear([System.Drawing.Color]::Black)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
for ($i = 0; $i -lt $files.Count; $i++) {
    $img = New-Object System.Drawing.Bitmap $files[$i].FullName
    $g.DrawImage($img, ($i % $Columns) * ($w + 4), [math]::Floor($i / $Columns) * ($h + 4), $w, $h)
    $img.Dispose()
}
$dst.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $dst.Dispose()

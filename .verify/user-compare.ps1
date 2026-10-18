param(
    [string]$Base = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\shots\user-base",
    [string]$After = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\shots\user-after",
    [string]$Out = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\shots\user-compare",
    [string]$Boxes = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\user-compare-boxes.txt"
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class BoxStat {
    static byte[] Read(Bitmap b, Rectangle r, out int stride) {
        var d = b.LockBits(r, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
        stride = d.Stride; var buf = new byte[stride * r.Height];
        Marshal.Copy(d.Scan0, buf, 0, buf.Length); b.UnlockBits(d); return buf;
    }
    static bool Sky(int r, int g, int b) { return b >= 165 && b - r >= 45 && b - g >= 18 && r >= 95; }
    public static string Measure(Bitmap a, Bitmap c, Rectangle r) {
        int sa, sc; var pa = Read(a, r, out sa); var pc = Read(c, r, out sc);
        long skyA = 0, skyC = 0, changed = 0;
        for (int y = 0; y < r.Height; y++) for (int x = 0; x < r.Width; x++) {
            int i = y * sa + x * 3, j = y * sc + x * 3;
            if (Sky(pa[i+2], pa[i+1], pa[i])) skyA++;
            if (Sky(pc[j+2], pc[j+1], pc[j])) skyC++;
            int d = Math.Max(Math.Abs(pa[i]-pc[j]), Math.Max(Math.Abs(pa[i+1]-pc[j+1]), Math.Abs(pa[i+2]-pc[j+2])));
            if (d > 8) changed++;
        }
        return String.Format("sky {0}->{1} changed {2:F1}%", skyA, skyC, changed * 100.0 / (r.Width * r.Height));
    }
}
"@
$cropMaxW = 760; $cropMaxH = 430; $maxScale = 3.0; $gap = 6; $labelH = 18
New-Item -ItemType Directory -Force $Out | Out-Null
$rows = Get-Content $Boxes | Where-Object { $_ -match '\S' } | ForEach-Object {
    $f = -split $_
    [pscustomobject]@{ Id = $f[0]; Shot = $f[1]; X0 = [int]$f[2]; Y0 = [int]$f[3]; X1 = [int]$f[4]; Y1 = [int]$f[5] }
}
$font = New-Object System.Drawing.Font "Consolas", 10
foreach ($group in $rows | Group-Object Id) {
    $panels = foreach ($row in $group.Group) {
        $rect = New-Object System.Drawing.Rectangle $row.X0, $row.Y0, ($row.X1 - $row.X0), ($row.Y1 - $row.Y0)
        $scale = [Math]::Min($maxScale, [Math]::Min($cropMaxW / $rect.Width, $cropMaxH / $rect.Height))
        [pscustomobject]@{ Row = $row; Rect = $rect; Scale = $scale; W = [int]($rect.Width * $scale); H = [int]($rect.Height * $scale) }
    }
    $width = [int](($panels | Measure-Object W -Maximum).Maximum * 2 + $gap)
    $height = [int](($panels | ForEach-Object { $_.H + $labelH } | Measure-Object -Sum).Sum)
    $canvas = New-Object System.Drawing.Bitmap $width, $height
    $g = [System.Drawing.Graphics]::FromImage($canvas)
    $g.Clear([System.Drawing.Color]::Black)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
    $top = 0
    foreach ($p in $panels) {
        $before = New-Object System.Drawing.Bitmap (Join-Path $Base "$($p.Row.Shot).png")
        $later = New-Object System.Drawing.Bitmap (Join-Path $After "$($p.Row.Shot).png")
        $stat = [BoxStat]::Measure($before, $later, $p.Rect)
        $label = "$($p.Row.Shot) $($p.Row.X0)-$($p.Row.X1),$($p.Row.Y0)-$($p.Row.Y1)  $stat"
        $g.DrawString($label, $font, [System.Drawing.Brushes]::White, 2, $top + 1)
        $half = [int](($width - $gap) / 2)
        $g.DrawImage($before, (New-Object System.Drawing.Rectangle 0, ($top + $labelH), $p.W, $p.H), $p.Rect, [System.Drawing.GraphicsUnit]::Pixel)
        $g.DrawImage($later, (New-Object System.Drawing.Rectangle ($half + $gap), ($top + $labelH), $p.W, $p.H), $p.Rect, [System.Drawing.GraphicsUnit]::Pixel)
        "$($group.Name) $label"
        $top += $p.H + $labelH
        $before.Dispose(); $later.Dispose()
    }
    $canvas.Save((Join-Path $Out "$($group.Name).png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $canvas.Dispose()
}

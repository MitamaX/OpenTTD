param([string]$Base, [string]$Twin, [string]$Right, [double]$ChangedLevel = 0.03, [int]$Reach = 2)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class MaskCompare {
    static byte[] Pixels(Bitmap b) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var px = new byte[Math.Abs(d.Stride) * b.Height]; Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d); return px;
    }
    static double Luma(byte[] px, int i) { return (0.0722 * px[i] + 0.7152 * px[i + 1] + 0.2126 * px[i + 2]) / 255.0; }
    public static string Run(string basePath, string twinPath, string rightPath, double level, int reach) {
        using (var a = new Bitmap(basePath)) using (var t = new Bitmap(twinPath)) using (var b = new Bitmap(rightPath)) {
            int w = a.Width, h = a.Height;
            var pa = Pixels(a); var pt = Pixels(t); var pb = Pixels(b);
            var moving = new bool[w * h];
            for (int i = 0; i < w * h; i++) moving[i] = Math.Abs(Luma(pa, i * 4) - Luma(pt, i * 4)) > level;
            var masked = new bool[w * h];
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                if (!moving[y * w + x]) continue;
                for (int dy = -reach; dy <= reach; dy++) for (int dx = -reach; dx <= reach; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= 0 && yy < h && xx >= 0 && xx < w) masked[yy * w + xx] = true;
                }
            }
            double sum = 0, twinSum = 0; long kept = 0, changed = 0, twinChanged = 0, black = 0;
            for (int i = 0; i < w * h; i++) {
                double d = Math.Abs(Luma(pb, i * 4) - Luma(pa, i * 4));
                if (Luma(pb, i * 4) < 0.02) black++;
                if (masked[i]) continue;
                kept++; sum += d; if (d > level) changed++;
                double dt = Math.Abs(Luma(pt, i * 4) - Luma(pa, i * 4)); twinSum += dt; if (dt > level) twinChanged++;
            }
            return string.Format("moving={0:P1} static: meanDiff={1:F5} changed={2:P3} twinMean={3:F5} twinChanged={4:P3} black={5:P2}",
                1.0 - (double)kept / (w * h), sum / kept, (double)changed / kept, twinSum / kept, (double)twinChanged / kept, (double)black / (w * h));
        }
    }
}
"@
Get-ChildItem $Base -Filter *.png | ForEach-Object {
    $twinShot = Join-Path $Twin $_.Name
    $other = Join-Path $Right $_.Name
    $result = if ((Test-Path $twinShot) -and (Test-Path $other)) { [MaskCompare]::Run($_.FullName, $twinShot, $other, $ChangedLevel, $Reach) } else { "missing" }
    "{0,-22} {1}" -f $_.BaseName, $result
}

param([string]$Left, [string]$Right, [double]$ChangedLevel = 0.03, [double]$BlackLevel = 0.02)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class ShotCompare {
    static byte[] Pixels(Bitmap b) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var px = new byte[Math.Abs(d.Stride) * b.Height]; Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d); return px;
    }
    static double Luma(byte[] px, int i) { return (0.0722 * px[i] + 0.7152 * px[i + 1] + 0.2126 * px[i + 2]) / 255.0; }
    public static string Run(string left, string right, double changedLevel, double blackLevel) {
        using (var a = new Bitmap(left)) using (var b = new Bitmap(right)) {
            if (a.Width != b.Width || a.Height != b.Height) return "size differs";
            var pa = Pixels(a); var pb = Pixels(b);
            double sum = 0; long changed = 0, black = 0, count = pa.Length / 4;
            for (int i = 0; i < pa.Length; i += 4) {
                double la = Luma(pa, i), lb = Luma(pb, i), d = Math.Abs(lb - la);
                sum += d;
                if (d > changedLevel) changed++;
                if (lb < blackLevel) black++;
            }
            return string.Format("meanDiff={0:F4} changed={1:P2} black={2:P2}", sum / count, (double)changed / count, (double)black / count);
        }
    }
}
"@
Get-ChildItem $Left -Filter *.png | ForEach-Object {
    $other = Join-Path $Right $_.Name
    $result = if (Test-Path $other) { [ShotCompare]::Run($_.FullName, $other, $ChangedLevel, $BlackLevel) } else { "missing" }
    "{0,-22} {1}" -f $_.BaseName, $result
}

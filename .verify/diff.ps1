param([string]$Left, [string]$Right, [string]$Out, [double]$Gain = 8.0)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class Differ {
    static byte[] Bytes(Bitmap b, out int stride) {
        var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        stride = Math.Abs(d.Stride); var px = new byte[stride * b.Height]; Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d); return px;
    }
    public static string Run(string l, string r, string o, double gain) {
        using (var a = new Bitmap(l)) using (var b = new Bitmap(r)) {
            int s; var pa = Bytes(a, out s); var pb = Bytes(b, out s);
            var outb = new Bitmap(a.Width, a.Height, PixelFormat.Format32bppArgb);
            var d = outb.LockBits(new Rectangle(0, 0, a.Width, a.Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            var po = new byte[pa.Length]; double sum = 0;
            for (int i = 0; i < pa.Length; i += 4) {
                double la = 0.0722 * pa[i] + 0.7152 * pa[i + 1] + 0.2126 * pa[i + 2];
                double lb = 0.0722 * pb[i] + 0.7152 * pb[i + 1] + 0.2126 * pb[i + 2];
                double dv = (lb - la) * gain; sum += Math.Abs(lb - la);
                byte up = (byte)Math.Min(255, Math.Max(0, 128 + dv));
                po[i] = up; po[i + 1] = up; po[i + 2] = up; po[i + 3] = 255;
            }
            Marshal.Copy(po, 0, d.Scan0, po.Length); outb.UnlockBits(d); outb.Save(o, ImageFormat.Png); outb.Dispose();
            return string.Format("meanAbsDiff={0:F4}", sum / (pa.Length / 4) / 255.0);
        }
    }
}
"@
[Differ]::Run($Left, $Right, $Out, $Gain)

param([string]$Dir)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class ImageStats {
    static double Pct(double[] hist, double total, double p) { double acc = 0; for (int k = 0; k < 256; k++) { acc += hist[k]; if (acc >= p * total) return k / 255.0; } return 1.0; }
    static double Frac(double[] hist, double total, double lo, double hi) { double acc = 0; for (int k = 0; k < 256; k++) { double v = k / 255.0; if (v >= lo && v < hi) acc += hist[k]; } return acc / total; }
    public static string Of(string path) {
        using (var bmp = new Bitmap(path)) {
            int w = bmp.Width, h = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int bytes = Math.Abs(data.Stride) * h;
            byte[] px = new byte[bytes];
            Marshal.Copy(data.Scan0, px, 0, bytes);
            bmp.UnlockBits(data);
            int stride = Math.Abs(data.Stride);
            double[] luma = new double[w * h];
            double satSum = 0, greenShare = 0, redShare = 0, blueShare = 0;
            double[] hist = new double[256];
            double hueGreenSat = 0; long greenCount = 0;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int i = y * stride + x * 4;
                    double b = px[i] / 255.0, g = px[i + 1] / 255.0, r = px[i + 2] / 255.0;
                    double l = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                    luma[y * w + x] = l;
                    hist[(int)(l * 255)]++;
                    double mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b));
                    double s = mx <= 0 ? 0 : (mx - mn) / mx;
                    satSum += s;
                    if (g > r && g > b) { greenShare++; hueGreenSat += s; greenCount++; }
                    else if (r > g && r > b) redShare++;
                    else blueShare++;
                }
            }
            double total = w * h;
            double grad = 0; long gn = 0;
            for (int y = 1; y < h - 1; y += 2) { for (int x = 1; x < w - 1; x += 2) {
                double dx = luma[y * w + x + 1] - luma[y * w + x - 1];
                double dy = luma[(y + 1) * w + x] - luma[(y - 1) * w + x];
                grad += Math.Sqrt(dx * dx + dy * dy); gn++;
            } }
            double meanL = 0; for (int k = 0; k < 256; k++) meanL += hist[k] * (k / 255.0); meanL /= total;
            double varL = 0; for (int k = 0; k < 256; k++) varL += hist[k] * Math.Pow(k / 255.0 - meanL, 2); varL /= total;
            return string.Format("meanL={0:F3} sdL={1:F3} p1={2:F3} p10={3:F3} p50={4:F3} p90={5:F3} p99={6:F3} sat={7:F3} green={8:F2} red={9:F2} blue={10:F2} greenSat={11:F3} grad={12:F4} above85={13:P2} below10={14:P2} p999={15:F3}",
                meanL, Math.Sqrt(varL), Pct(hist, total, 0.01), Pct(hist, total, 0.10), Pct(hist, total, 0.50), Pct(hist, total, 0.90), Pct(hist, total, 0.99), satSum / total, greenShare / total, redShare / total, blueShare / total, greenCount > 0 ? hueGreenSat / greenCount : 0, grad / gn, Frac(hist, total, 0.85, 1.01), Frac(hist, total, 0.0, 0.10), Pct(hist, total, 0.999));
        }
    }
}
"@
Get-ChildItem $Dir -Filter *.png | ForEach-Object { "{0,-26} {1}" -f $_.Name, [ImageStats]::Of($_.FullName) }

param([string]$Image, [int]$X, [int]$Y, [int]$W, [int]$H)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class PatchStats {
    public static string Of(string path, int x0, int y0, int pw, int ph) {
        using (var bmp = new Bitmap(path)) {
            int w = bmp.Width, h = bmp.Height;
            var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int stride = Math.Abs(data.Stride);
            byte[] px = new byte[stride * h];
            Marshal.Copy(data.Scan0, px, 0, px.Length);
            bmp.UnlockBits(data);
            double[] l = new double[pw * ph];
            double r0 = 0, g0 = 0, b0 = 0;
            for (int y = 0; y < ph; y++) for (int x = 0; x < pw; x++) {
                int i = (y0 + y) * stride + (x0 + x) * 4;
                double b = px[i] / 255.0, g = px[i + 1] / 255.0, r = px[i + 2] / 255.0;
                l[y * pw + x] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                r0 += r; g0 += g; b0 += b;
            }
            int n = pw * ph;
            string res = string.Format("rgb=({0:F2},{1:F2},{2:F2}) ", r0 / n, g0 / n, b0 / n);
            int[] cells = new int[] { 1, 3, 9, 27, 81 };
            foreach (int c in cells) {
                int cw = pw / c, ch = ph / c;
                if (cw < 2 || ch < 2) continue;
                double[] means = new double[cw * ch];
                for (int cy = 0; cy < ch; cy++) for (int cx = 0; cx < cw; cx++) {
                    double s = 0;
                    for (int y = 0; y < c; y++) for (int x = 0; x < c; x++) s += l[(cy * c + y) * pw + cx * c + x];
                    means[cy * cw + cx] = s / (c * c);
                }
                double m = 0; foreach (double v in means) m += v; m /= means.Length;
                double var = 0; foreach (double v in means) var += (v - m) * (v - m); var /= means.Length;
                res += string.Format("sd@{0}px={1:F4} ", c, Math.Sqrt(var));
            }
            return res;
        }
    }
}
"@
[PatchStats]::Of($Image, $X, $Y, $W, $H)

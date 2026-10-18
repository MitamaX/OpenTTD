param([string]$Image, [string]$Box)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class ScaleStd {
    public static string Of(string path, int x0, int y0, int w, int h) {
        using (var bmp = new Bitmap(path)) {
            var d = bmp.LockBits(new Rectangle(0, 0, bmp.Width, bmp.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int stride = Math.Abs(d.Stride); var px = new byte[stride * bmp.Height]; Marshal.Copy(d.Scan0, px, 0, px.Length); bmp.UnlockBits(d);
            double[,] luma = new double[h, w]; double[,] hue = new double[h, w];
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                int i = (y0 + y) * stride + (x0 + x) * 4;
                double b = px[i] / 255.0, g = px[i + 1] / 255.0, r = px[i + 2] / 255.0;
                luma[y, x] = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                hue[y, x] = (g - r) / Math.Max(g + r, 1e-3);
            }
            string s = "";
            foreach (int cell in new int[] { 1, 3, 9, 27, 81 }) {
                double sum = 0, sum2 = 0, hs = 0, hs2 = 0; int n = 0;
                for (int by = 0; by + cell <= h; by += cell) for (int bx = 0; bx + cell <= w; bx += cell) {
                    double m = 0, hm = 0;
                    for (int y = 0; y < cell; y++) for (int x = 0; x < cell; x++) { m += luma[by + y, bx + x]; hm += hue[by + y, bx + x]; }
                    m /= cell * cell; hm /= cell * cell; sum += m; sum2 += m * m; hs += hm; hs2 += hm * hm; n++;
                }
                double mean = sum / n; double sd = Math.Sqrt(Math.Max(sum2 / n - mean * mean, 0));
                double hmean = hs / n; double hsd = Math.Sqrt(Math.Max(hs2 / n - hmean * hmean, 0));
                s += string.Format(" L{0}={1:F3}/h{2:F3}", cell, sd, hsd);
            }
            return s;
        }
    }
}
"@
foreach ($b in $Box.Split(';')) {
    $n, $x, $y, $w, $h = $b.Split(',')
    "{0,-10}{1}" -f $n, [ScaleStd]::Of($Image, [int]$x, [int]$y, [int]$w, [int]$h)
}

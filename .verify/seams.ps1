param([string]$Dir, [int]$Top = 3)
Add-Type -ReferencedAssemblies System.Drawing @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class Seams {
    public static string Of(string path, int top) {
        using (var bmp = new Bitmap(path)) {
            int w = bmp.Width, h = bmp.Height;
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            int stride = Math.Abs(d.Stride); var px = new byte[stride * h]; Marshal.Copy(d.Scan0, px, 0, px.Length); bmp.UnlockBits(d);
            var luma = new double[w * h];
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { int i = y * stride + x * 4; luma[y * w + x] = 0.2126 * px[i + 2] + 0.7152 * px[i + 1] + 0.0722 * px[i]; }
            var score = new double[h];
            var signed = new double[h];
            for (int y = 2; y < h - 2; y++) {
                int agree = 0, count = 0; double sum = 0;
                for (int x = 0; x < w; x++) {
                    double above = (luma[(y - 2) * w + x] + luma[(y - 1) * w + x]) * 0.5;
                    double below = (luma[(y + 1) * w + x] + luma[(y + 2) * w + x]) * 0.5;
                    double step = below - above;
                    double inner = Math.Abs(luma[y * w + x] - luma[(y - 1) * w + x]) + Math.Abs(luma[(y + 1) * w + x] - luma[(y + 2) * w + x]);
                    if (Math.Abs(step) > 6 && Math.Abs(luma[y * w + x] - luma[(y - 1) * w + x]) + Math.Abs(luma[(y+1) * w + x] - luma[y * w + x]) < Math.Abs(step) * 1.6) { count++; if (step < 0) agree--; else agree++; sum += step; }
                }
                score[y] = count; signed[y] = count > 0 ? Math.Abs(agree) / (double)count : 0;
            }
            var rows = new List<int>();
            for (int y = 2; y < h - 2; y++) rows.Add(y);
            rows.Sort((a, b) => (score[b] * signed[b]).CompareTo(score[a] * signed[a]));
            string s = "";
            var picked = new List<int>();
            foreach (int y in rows) {
                bool near = false; foreach (int p in picked) if (Math.Abs(p - y) < 6) near = true;
                if (near) continue;
                picked.Add(y); s += string.Format(" y={0}:cols={1:F0}/{2:F2}", y, score[y], signed[y]);
                if (picked.Count >= top) break;
            }
            return s;
        }
    }
}
"@
Get-ChildItem $Dir -Filter *.png | ForEach-Object { "{0,-24}{1}" -f $_.Name, [Seams]::Of($_.FullName, $Top) }

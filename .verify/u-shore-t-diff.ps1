param([string]$A = "u-shore-t-base2", [string]$B = "u-shore-t-q")
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class DiffMap {
    static byte[] Px(Bitmap b) { var d = b.LockBits(new Rectangle(0,0,b.Width,b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb); var p = new byte[d.Stride*b.Height]; Marshal.Copy(d.Scan0,p,0,p.Length); b.UnlockBits(d); return p; }
    public static string Run(string a, string b, string outPath) {
        using (var x = new Bitmap(a)) using (var y = new Bitmap(b)) {
            int w = x.Width, h = x.Height; var pa = Px(x); var pb = Px(y);
            long changed = 0; var cells = new int[(w/50+1)*(h/50+1)];
            using (var o = new Bitmap(w, h, PixelFormat.Format32bppArgb)) {
                for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
                    int k = (j*w+i)*4; int d = Math.Max(Math.Abs(pa[k]-pb[k]), Math.Max(Math.Abs(pa[k+1]-pb[k+1]), Math.Abs(pa[k+2]-pb[k+2])));
                    if (d > 14) { changed++; cells[(j/50)*(w/50+1)+i/50]++; o.SetPixel(i,j,Color.Red); } else { int g = (pb[k]+pb[k+1]+pb[k+2])/6; o.SetPixel(i,j,Color.FromArgb(g,g,g)); }
                }
                o.Save(outPath, ImageFormat.Png);
            }
            string hot = ""; for (int c = 0; c < cells.Length; c++) if (cells[c] > 150) hot += string.Format("({0},{1}:{2}) ", (c%(w/50+1))*50, (c/(w/50+1))*50, cells[c]);
            return changed + " " + hot;
        }
    }
}
"@
$root = "C:\Users\Hajun\Desktop\Claude\OpenTTD\OpenTTD\.verify\shots"
New-Item -ItemType Directory -Force "$root\u-shore-t-diff" | Out-Null
foreach ($f in @(Get-ChildItem "$root\$B\*.png")) { try {
  $left = "$root\$A\$($f.Name)"
  if (-not (Test-Path $left)) { continue }
  "$($f.BaseName): " + [DiffMap]::Run($left, $f.FullName, "$root\u-shore-t-diff\$($f.Name)") } catch { "ERR " + $_ }
}



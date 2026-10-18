import sys, importlib.util
spec = importlib.util.spec_from_file_location("px", __file__.replace("diag-S8-box.py", "pixstat.py"))
src = open(spec.origin).read().split("w, h, bpp, a = load")[0]
ns = {}
exec(src, ns)
w, h, bpp, img = ns["load"](sys.argv[1])
for box in sys.argv[2:]:
    name, x0, y0, x1, y1 = box.split(",")
    x0, y0, x1, y1 = map(int, (x0, y0, x1, y1))
    lum = {}
    for y in range(y0, y1):
        for x in range(x0, x1):
            i = (y * w + x) * bpp
            lum[(x, y)] = (0.2126 * img[i] + 0.7152 * img[i + 1] + 0.0722 * img[i + 2], img[i], img[i + 1], img[i + 2])
    vals = [v[0] for v in lum.values()]
    n = len(vals); mean = sum(vals) / n
    sd = (sum((v - mean) ** 2 for v in vals) / n) ** 0.5
    grad = [abs(lum[(x, y)][0] - lum[(x + 1, y)][0]) for (x, y) in lum if (x + 1, y) in lum]
    rgb = [sum(v[k] for v in lum.values()) / n for k in (1, 2, 3)]
    print(f"{name}: rgb=({rgb[0]:.0f},{rgb[1]:.0f},{rgb[2]:.0f}) lum={mean:.1f} sd={sd:.2f} grad={sum(grad)/len(grad):.2f}")

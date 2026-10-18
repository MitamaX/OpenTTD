import sys, glob, math, random
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from scan import load, Map, bridges, features

def seen_points(verify):
    pts = []
    for f in glob.glob(str(verify / "shots-*.txt")):
        for line in open(f, encoding="utf-8", errors="ignore"):
            p = line.split()
            if len(p) >= 3 and p[0] not in ("wait", "glide"):
                try: pts.append((float(p[1]), float(p[2])))
                except ValueError: pass
    return pts

def novelty(pts, x, y): return min((math.hypot(x - a, y - b) for a, b in pts), default=99)

m = Map(load(sys.argv[1]))
pts = seen_points(Path(__file__).parent.parent)
groups = {}
for b in bridges(m):
    (ax, ay), (bx, by) = b["a"], b["b"]
    key = f"{b['kind']}-{b['tr']}-t{b['btype']}-{'+'.join(b['under'])}"
    cx, cy = (ax + bx) / 2 + .5, (ay + by) / 2 + .5
    groups.setdefault(key, []).append((round(novelty(pts, cx, cy), 1), b["a"], b["b"], b["len"], b["slopeA"], b["slopeB"]))
for k, x, y in features(m):
    if k in ("crossing", "rapids", "station") or k.startswith("slope"):
        groups.setdefault(k, []).append((round(novelty(pts, x + .5, y + .5), 1), (x, y)))
for k in sorted(groups):
    items = sorted(groups[k], key=lambda t: -t[0])
    print(k, len(items))
    for it in items[:8]: print("   ", it)

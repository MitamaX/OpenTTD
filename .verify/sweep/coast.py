import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from scan import load, Map
from pick import seen_points, novelty
m = Map(load(sys.argv[1]))
pts = seen_points(Path(__file__).parent.parent)
found = []
for y in range(2, m.h - 2):
    for x in range(2, m.w - 2):
        if m.under(x, y) != "coast": continue
        near = {m.under(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
        if near & {"rail", "road", "tunnelbridge", "crossing"}:
            found.append((round(novelty(pts, x + .5, y + .5), 1), x, y, sorted(near)))
for f in sorted(found, reverse=True)[:25]: print(f)
for x, y in [(6,54),(65,212),(148,237),(26,108)]:
    print("rapid", x, y, [m.under(x+dx,y+dy) for dx,dy in ((1,0),(-1,0),(0,1),(0,-1))])

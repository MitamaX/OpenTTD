import sys, struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent))
from savinfo import Reader, unpack, chunks

KINDS = "clear rail road house trees station water void industry tunnelbridge object".split()
TRANSPORT = ["rail", "road", "water", "?"]

def load(path):
    reader = Reader(unpack(Path(path).read_bytes()))
    raw = {}
    for name, records in chunks(reader):
        if name == "MAPS" and records:
            raw["size"] = struct.unpack(">II", records[0][:8])
        if name in ("MAPT", "MAPH", "MAPO", "MAP5", "MAPE", "MAP2") and records:
            raw[name] = records[0]
    return raw

class Map:
    def __init__(self, raw):
        self.raw = raw
        self.w, self.h = raw["size"]
    def i(self, x, y): return y * self.w + x
    def kind(self, x, y): return KINDS[self.raw["MAPT"][self.i(x, y)] >> 4]
    def bridge_above(self, x, y): return (self.raw["MAPT"][self.i(x, y)] >> 2) & 3
    def m5(self, x, y): return self.raw["MAP5"][self.i(x, y)]
    def m1(self, x, y): return self.raw["MAPO"][self.i(x, y)]
    def m6(self, x, y): return self.raw["MAPE"][self.i(x, y)]
    def height(self, x, y): return self.raw["MAPH"][self.i(x, y)]
    def corners(self, x, y): return [self.height(x, y), self.height(x + 1, y), self.height(x, y + 1), self.height(x + 1, y + 1)]
    def sloped(self, x, y): return len(set(self.corners(x, y))) > 1
    def under(self, x, y):
        k = self.kind(x, y)
        if k == "road" and (self.m5(x, y) >> 6) == 1: return "crossing"
        if k == "water": return ["sea", "canal", "river"][(self.m1(x, y) >> 5) & 3] if (self.m5(x, y) >> 4) == 0 else "coast"
        return k

def bridges(m):
    out = []
    for y in range(m.h - 1):
        for x in range(m.w - 1):
            if m.kind(x, y) != "tunnelbridge": continue
            v = m.m5(x, y)
            d = v & 3
            if d not in (0, 3) and False: pass
            dx, dy = [(-1, 0), (0, 1), (1, 0), (0, -1)][d]
            if dx > 0 or dy < 0 or (dx < 0 and False): pass
            if d not in (2, 1): continue
            tx, ty = x + dx, y + dy
            span = []
            while 0 < tx < m.w - 1 and 0 < ty < m.h - 1 and m.kind(tx, ty) != "tunnelbridge":
                span.append((tx, ty)); tx += dx; ty += dy
            if not 0 < tx < m.w - 1 or not 0 < ty < m.h - 1: continue
            bridge = bool(v & 0x80)
            under = sorted({m.under(*t) for t in span})
            out.append(dict(kind="bridge" if bridge else "tunnel", tr=TRANSPORT[(v >> 2) & 3], a=(x, y), b=(tx, ty), len=len(span) + 1,
                            btype=(m.m6(x, y) >> 2) & 0xF, under=under, slopeA=m.sloped(x, y), slopeB=m.sloped(tx, ty)))
    return out

def features(m):
    feats = []
    for y in range(1, m.h - 1):
        for x in range(1, m.w - 1):
            k = m.under(x, y)
            if k == "crossing": feats.append(("crossing", x, y))
            if k == "river" and m.sloped(x, y): feats.append(("rapids", x, y))
            if k in ("rail", "road") and m.sloped(x, y) and m.bridge_above(x, y) == 0: feats.append(("slope_" + k, x, y))
            if k == "station": feats.append(("station", x, y))
    return feats

if __name__ == "__main__":
    m = Map(load(sys.argv[1]))
    print("size", m.w, m.h)
    for b in bridges(m): print(b)
    if len(sys.argv) > 2:
        from collections import Counter
        f = features(m)
        print(Counter(k for k, *_ in f))
        for kind in ("crossing", "rapids", "station"):
            print(kind, [(x, y) for k, x, y in f if k == kind])
        for kind in ("slope_rail", "slope_road"):
            print(kind, [(x, y) for k, x, y in f if k == kind])

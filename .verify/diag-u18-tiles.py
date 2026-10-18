import sys, struct
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from savinfo import Reader, unpack, chunks
path, x0, y0, x1, y1 = sys.argv[1], *map(int, sys.argv[2:6])
reader = Reader(unpack(Path(path).read_bytes()))
raw = {}
for name, records in chunks(reader):
    if name == "MAPS" and records:
        w, h = struct.unpack(">II", records[0][:8])
    if name in ("MAPT", "MAPH", "MAP5") and records:
        raw[name] = records[0]
T = "CRrHTSWVItOx"
H = lambda x, y: raw["MAPH"][y * w + x]
for y in range(y0, y1 + 1):
    for x in range(x0, x1 + 1):
        i = y * w + x
        t = raw["MAPT"][i]
        n, ww, e, s = H(x, y), H(x + 1, y), H(x, y + 1), H(x + 1, y + 1)
        print(f"{x},{y} {T[t >> 4]} br{(t >> 2) & 3} m5={raw['MAP5'][i]:02x} N{n} W{ww} E{e} S{s}")

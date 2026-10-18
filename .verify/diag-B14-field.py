import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from savinfo import Reader, unpack, chunks

reader = Reader(unpack(Path(sys.argv[1]).read_bytes()))
raw = {}
for name, records in chunks(reader):
    if name == "MAPS" and records:
        w, h = struct.unpack(">II", records[0][:8])
    if name in ("MAPT", "MAPH") and records:
        raw[name] = records[0]

WATER = 6


def is_water(x, y):
    return raw["MAPT"][y * w + x] >> 4 == WATER


def height(x, y):
    return raw["MAPH"][y * w + x]


def hash2(x, y):
    m = 0xFFFFFFFF
    h = ((x & m) * 0x8DA6B343 ^ (y & m) * 0xD8163841) & m
    h = ((h ^ (h >> 15)) * 0x2C1B3C6D) & m
    h ^= h >> 12
    return h / 4294967296.0


def noise(u, v):
    cu, cv = math.floor(u), math.floor(v)
    fu, fv = u - cu, v - cv
    su = fu * fu * (3 - 2 * fu)
    sv = fv * fv * (3 - 2 * fv)
    north = hash2(cu, cv) + (hash2(cu + 1, cv) - hash2(cu, cv)) * su
    south = hash2(cu, cv + 1) + (hash2(cu + 1, cv + 1) - hash2(cu, cv + 1)) * su
    return north + (south - north) * sv


def warp(x, y, seed):
    return (noise(x * 1.7 + seed, y * 1.7 + seed) - 0.5) * 2 * 0.22


def weights(f):
    g = 1 - f
    w0 = g ** 3 / 6
    w1 = (4 - 6 * f * f + 3 * f ** 3) / 6
    w3 = f ** 3 / 6
    return [w0, w1, 1 - w0 - w1 - w3, w3]


def field(x, y):
    tx = x + warp(x, y, 7.1) - 0.5
    ty = y + warp(x, y, 93.4) - 0.5
    cx, cy = math.floor(tx), math.floor(ty)
    wx, wy = weights(tx - cx), weights(ty - cy)
    total = 0.0
    for j in range(4):
        for i in range(4):
            total += wx[i] * wy[j] * (1.0 if is_water(cx + i - 1, cy + j - 1) else 0.0)
    return total


print("corner heights 166..169 x 59..61:", [[height(x, y) for x in range(166, 170)] for y in range(59, 62)])
for y in (59.8, 59.9, 60.0, 60.05, 60.1, 60.2):
    print(f"y={y:5.2f} " + " ".join(f"{field(x / 10, y):.2f}" for x in range(1670, 1681)))

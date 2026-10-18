import lzma
import struct
import sys
import zlib
from pathlib import Path

ARRAY_TYPES = {1, 2, 3, 4}
TABLE_TYPES = {3, 4}
RIFF = 0
TYPE_MASK = 0x0F
COUNTED = {"CITY": "towns", "VEHS": "vehicle parts", "STNN": "stations", "INDY": "industries"}
HOUSE_TILE = 3
TILE_TYPE_SHIFT = 4
LARGEST_TOWNS = 5


def unpack(data):
    tag = data[:4]
    body = data[8:]
    if tag == b"OTTX":
        return lzma.decompress(body)
    if tag == b"OTTZ":
        return zlib.decompress(body)
    if tag == b"OTTN":
        return body
    raise SystemExit(f"unsupported save format {tag!r}")


class Reader:
    def __init__(self, data):
        self.data = data
        self.at = 0

    def byte(self):
        value = self.data[self.at]
        self.at += 1
        return value

    def gamma(self):
        value = self.byte()
        if not value & 0x80:
            return value
        if not value & 0x40:
            return ((value & 0x3F) << 8) | self.byte()
        if not value & 0x20:
            return ((value & 0x1F) << 16) | (self.byte() << 8) | self.byte()
        if not value & 0x10:
            return ((value & 0x0F) << 24) | (self.byte() << 16) | (self.byte() << 8) | self.byte()
        return (self.byte() << 24) | (self.byte() << 16) | (self.byte() << 8) | self.byte()

    def take(self, length):
        block = self.data[self.at:self.at + length]
        self.at += length
        return block


def chunks(reader):
    while reader.at + 4 <= len(reader.data):
        name = reader.take(4).decode("latin-1")
        if name == "\0\0\0\0":
            return
        mode = reader.byte()
        kind = mode & TYPE_MASK
        if kind == RIFF:
            length = (reader.byte() << 16) | ((mode >> 4) << 24)
            length += struct.unpack(">H", reader.take(2))[0]
            yield name, [reader.take(length)]
            continue
        if kind not in ARRAY_TYPES:
            raise SystemExit(f"chunk {name} has unknown type {kind}")
        records = []
        while True:
            length = reader.gamma()
            if length == 0:
                break
            records.append(reader.take(length - 1))
        if kind in TABLE_TYPES:
            records = records[1:]
        yield name, [record for record in records if record]


def largest_towns(width, kinds, owners):
    houses = {}
    for tile, kind in enumerate(kinds):
        if kind >> TILE_TYPE_SHIFT != HOUSE_TILE:
            continue
        town = struct.unpack_from(">H", owners, tile * 2)[0]
        houses.setdefault(town, []).append((tile % width, tile // width))
    ranked = sorted(houses.items(), key=lambda item: -len(item[1]))[:LARGEST_TOWNS]
    lines = []
    for town, tiles in ranked:
        x = sum(tx for tx, _ in tiles) // len(tiles)
        y = sum(ty for _, ty in tiles) // len(tiles)
        lines.append(f"  town {town}: {len(tiles)} house tiles around {x} {y}")
    return lines


def describe(path):
    reader = Reader(unpack(Path(path).read_bytes()))
    found = {}
    width = height = None
    raw = {}
    for name, records in chunks(reader):
        if name == "MAPS" and records:
            width, height = struct.unpack(">II", records[0][:8])
        if name in COUNTED:
            found[COUNTED[name]] = len(records)
        if name in ("MAPT", "MAP2"):
            raw[name] = records[0]
    parts = [f"map {width}x{height}"] + [f"{label} {found.get(label, 0)}" for label in COUNTED.values()]
    return "\n".join([f"{Path(path).name}: " + ", ".join(parts)] + largest_towns(width, raw["MAPT"], raw["MAP2"]))


if __name__ == "__main__":
    for save in sys.argv[1:]:
        print(describe(save))

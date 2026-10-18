#!/usr/bin/env python3
import argparse
import sys
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

VERIFY = Path(__file__).resolve().parent
BOX_REFERENCE_SIZE = (1600, 900)
CHANGE_THRESHOLD = 8
CROP_MAX_SIZE = (760, 430)
MAX_SCALE = 3.0
GAP = 6
LABEL_HEIGHT = 18
FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONT_SIZE = 12
SKY_MIN_BLUE = 165
SKY_MIN_BLUE_OVER_RED = 45
SKY_MIN_BLUE_OVER_GREEN = 18
SKY_MIN_RED = 95


@dataclass
class Box:
    group: str
    shot: str
    x0: int
    y0: int
    x1: int
    y1: int

    def scaled(self, size):
        sx, sy = size[0] / BOX_REFERENCE_SIZE[0], size[1] / BOX_REFERENCE_SIZE[1]
        return (round(self.x0 * sx), round(self.y0 * sy), round(self.x1 * sx), round(self.y1 * sy))


def shot_folder(name):
    path = Path(name)
    return path if path.is_dir() else VERIFY / "shots" / name


def read_boxes(path, groups):
    boxes = []
    for line in Path(path).read_text().splitlines():
        fields = line.split()
        if len(fields) < 6 or fields[0].startswith("#"):
            continue
        if groups and fields[0] not in groups:
            continue
        boxes.append(Box(fields[0], fields[1], *map(int, fields[2:6])))
    return boxes


def sky_count(pixels):
    r, g, b = (pixels[..., channel].astype(np.int16) for channel in range(3))
    return int(np.count_nonzero((b >= SKY_MIN_BLUE) & (b - r >= SKY_MIN_BLUE_OVER_RED) & (b - g >= SKY_MIN_BLUE_OVER_GREEN) & (r >= SKY_MIN_RED)))


def measure(before, after):
    difference = np.abs(before.astype(np.int16) - after.astype(np.int16)).max(axis=2)
    changed = int(np.count_nonzero(difference > CHANGE_THRESHOLD))
    return {
        "changed": changed,
        "changed_pct": changed * 100.0 / difference.size,
        "mean_diff": float(difference.mean()),
        "sky_before": sky_count(before),
        "sky_after": sky_count(after),
    }


def load_rgb(path):
    return Image.open(path).convert("RGB")


def side_by_side(left, right):
    canvas = Image.new("RGB", (left.width + GAP + right.width, max(left.height, right.height)))
    canvas.paste(left, (0, 0))
    canvas.paste(right, (left.width + GAP, 0))
    return canvas


def compose_group(panels, font):
    half = max(panel["size"][0] for panel in panels)
    canvas = Image.new("RGB", (half * 2 + GAP, sum(panel["size"][1] + LABEL_HEIGHT for panel in panels)))
    draw = ImageDraw.Draw(canvas)
    top = 0
    for panel in panels:
        draw.text((2, top + 1), panel["label"], fill="white", font=font)
        canvas.paste(panel["before"].resize(panel["size"], Image.NEAREST), (0, top + LABEL_HEIGHT))
        canvas.paste(panel["after"].resize(panel["size"], Image.NEAREST), (half + GAP, top + LABEL_HEIGHT))
        top += panel["size"][1] + LABEL_HEIGHT
    return canvas


def panel_size(rect):
    width, height = rect[2] - rect[0], rect[3] - rect[1]
    scale = min(MAX_SCALE, CROP_MAX_SIZE[0] / width, CROP_MAX_SIZE[1] / height)
    return (max(1, int(width * scale)), max(1, int(height * scale)))


def compare_boxes(base, after, out, boxes, font):
    groups = OrderedDict()
    for box in boxes:
        groups.setdefault(box.group, []).append(box)
    print(f"{'id':<5} {'shot':<28} {'box':<21} {'changed%':>8} {'changed':>8} {'mean':>6} {'sky':>15}")
    for group, members in groups.items():
        panels = []
        for box in members:
            before_path, after_path = base / f"{box.shot}.png", after / f"{box.shot}.png"
            if not before_path.exists() or not after_path.exists():
                print(f"{group:<5} {box.shot:<28} missing shot")
                continue
            before, later = load_rgb(before_path), load_rgb(after_path)
            rect = box.scaled(before.size)
            before_crop, after_crop = before.crop(rect), later.crop(rect)
            stat = measure(np.asarray(before_crop), np.asarray(after_crop))
            sky = f"{stat['sky_before']}->{stat['sky_after']}"
            area = f"{rect[0]}-{rect[2]},{rect[1]}-{rect[3]}"
            print(f"{group:<5} {box.shot:<28} {area:<21} {stat['changed_pct']:>8.1f} {stat['changed']:>8} {stat['mean_diff']:>6.2f} {sky:>15}")
            label = f"{box.shot} {area}  sky {sky} changed {stat['changed_pct']:.1f}% mean {stat['mean_diff']:.2f}"
            panels.append({"label": label, "before": before_crop, "after": after_crop, "size": panel_size(rect)})
        if panels:
            compose_group(panels, font).save(out / f"{group}.png")


def compare_pairs(base, after, out):
    pairs = out / "pairs"
    pairs.mkdir(exist_ok=True)
    print(f"{'shot':<28} {'changed%':>8} {'mean':>6}")
    for before_path in sorted(base.glob("*.png")):
        after_path = after / before_path.name
        if not after_path.exists():
            continue
        before, later = load_rgb(before_path), load_rgb(after_path)
        stat = measure(np.asarray(before), np.asarray(later))
        print(f"{before_path.stem:<28} {stat['changed_pct']:>8.1f} {stat['mean_diff']:>6.2f}")
        side_by_side(before, later).save(pairs / before_path.name)


def main():
    parser = argparse.ArgumentParser(description="Side-by-side before/after shots with per-box change and sky counts.")
    parser.add_argument("-Base", "--base", default="user-base", help="shot label or folder")
    parser.add_argument("-After", "--after", default="user-after", help="shot label or folder")
    parser.add_argument("-Out", "--out", default="user-compare", help="output label or folder")
    parser.add_argument("-Boxes", "--boxes", default=str(VERIFY / "user-compare-boxes.txt"))
    parser.add_argument("-Ids", "--ids", nargs="*", default=[], help="only these box ids")
    parser.add_argument("-Pairs", "--pairs", action="store_true", help="also pair every shot the two folders share")
    args = parser.parse_args()

    base, after = shot_folder(args.base), shot_folder(args.after)
    out = Path(args.out) if "/" in args.out else VERIFY / "shots" / args.out
    for folder in (base, after):
        if not folder.is_dir():
            sys.exit(f"no shot folder {folder}")
    out.mkdir(parents=True, exist_ok=True)
    compare_boxes(base, after, out, read_boxes(args.boxes, set(args.ids)), ImageFont.truetype(FONT_PATH, FONT_SIZE))
    if args.pairs:
        compare_pairs(base, after, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

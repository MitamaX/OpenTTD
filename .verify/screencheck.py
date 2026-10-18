#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

VERIFY = Path(__file__).resolve().parent
LUMA_WEIGHTS = np.array([0.299, 0.587, 0.114], dtype=np.float32)

SKY_SEED_ROWS = 4
SKY_DARK = np.array([60, 96, 136], dtype=np.float32)
SKY_LIGHT = np.array([140, 172, 208], dtype=np.float32)
SKY_MIN_BLUE_OVER_RED = 30
SKY_MIN_BLUE_OVER_GREEN = 12
SKY_COLOUR_TOLERANCE = 18.0
SKY_PALETTE_SIZE = 32
SKY_PALETTE_MIN_ROW_SHARE = 20
SKY_HOLE_MIN_AREA = 12
SKY_HOLE_MIN_DEPTH = 6
SKY_HOLE_MAX_ROUGHNESS = 3.0
SKY_HOLE_MAX_MEAN_DISTANCE = 12.0
SKY_HOLE_RING = 3
SKY_HOLE_MIN_TERRAIN_SHARE = 0.6
WATER_MIN_BLUE_OVER_RED = 15

BLACK_LUMA = 26.0
BLACK_MIN_AREA = 16

STEP_OFFSET = 2
STEP_THRESHOLD = 14.0
AXIS_MIN_RUN = 60
AXIS_SIGN_TOLERANCE_COLUMNS = 1

HOUGH_EDGE_THRESHOLD = 28.0
HOUGH_ANGLE_STEPS = 180
HOUGH_PEAKS = 60
HOUGH_PEAK_SUPPRESS_RHO = 6
HOUGH_PEAK_SUPPRESS_ANGLE = 3
HOUGH_ORIENTATION_COS = 0.85
LINE_MIN_LENGTH = 90
LINE_MAX_GAP = 4
LINE_SIDE_OFFSET = 3
LINE_MIN_STEP = 16.0
LINE_MIN_SIGN_SHARE = 0.8
AXIS_ANGLE_TOLERANCE_DEG = 2

RIDGE_SIDE_OFFSET = 3
RIDGE_THRESHOLD = 6.0
THIN_LINE_MIN_LENGTH = 140
THIN_LINE_MIN_COVER = 0.75

BOX_MERGE_MARGIN = 4
LINE_DUPLICATE_SLACK = 6
LINE_DUPLICATE_SHARE = 0.7
CROP_SCALE = 2


def label_components(mask):
    parent = []

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    runs_by_row = []
    for y in range(mask.shape[0]):
        row = np.concatenate(([0], mask[y].view(np.int8), [0]))
        edges = np.flatnonzero(np.diff(row))
        runs = []
        for start, end in zip(edges[::2], edges[1::2]):
            label = len(parent)
            parent.append(label)
            runs.append((start, end, label))
        if y:
            previous = runs_by_row[-1]
            j = 0
            for start, end, label in runs:
                while j < len(previous) and previous[j][1] <= start:
                    j += 1
                k = j
                while k < len(previous) and previous[k][0] < end:
                    a, b = find(label), find(previous[k][2])
                    if a != b:
                        parent[max(a, b)] = min(a, b)
                    k += 1
        runs_by_row.append(runs)

    components = {}
    for y, runs in enumerate(runs_by_row):
        for start, end, label in runs:
            root = find(label)
            entry = components.setdefault(root, [0, start, end - 1, y, y, []])
            entry[0] += end - start
            entry[1] = min(entry[1], start)
            entry[2] = max(entry[2], end - 1)
            entry[4] = y
            entry[5].append((y, start, end))
    return list(components.values())


def component_mask(component, shape):
    mask = np.zeros(shape, dtype=bool)
    for y, start, end in component[5]:
        mask[y, start:end] = True
    return mask


def sky_palette(pixels, sky):
    counts = sky.sum(axis=1)
    known = counts > pixels.shape[1] // SKY_PALETTE_MIN_ROW_SHARE
    if not known.any():
        return None
    sums = (pixels * sky[..., None]).sum(axis=1)
    colours = sums[known] / counts[known, None]
    return colours[::max(1, len(colours) // SKY_PALETTE_SIZE)]


def palette_distance(pixels, palette):
    nearest = np.full(pixels.shape[:2], np.inf, dtype=np.float32)
    for colour in palette:
        nearest = np.minimum(nearest, np.linalg.norm(pixels - colour, axis=2))
    return nearest


def blue_like(pixels):
    r, g, b = pixels[..., 0], pixels[..., 1], pixels[..., 2]
    return (b - r >= SKY_MIN_BLUE_OVER_RED) & (b - g >= SKY_MIN_BLUE_OVER_GREEN)


def component_roughness(luma, mask, x0, y0):
    patch = luma[y0:y0 + mask.shape[0], x0:x0 + mask.shape[1]]
    across = mask[:, 1:] & mask[:, :-1]
    down = mask[1:, :] & mask[:-1, :]
    steps = np.concatenate((np.abs(np.diff(patch, axis=1))[across], np.abs(np.diff(patch, axis=0))[down]))
    return float(steps.mean()) if len(steps) else 0.0


def segment_distance(pixels, low, high):
    axis = high - low
    t = np.clip(((pixels - low) @ axis) / float(axis @ axis), 0, 1)
    return np.linalg.norm(pixels - (low + t[..., None] * axis), axis=2)


def eroded(mask):
    inner = mask.copy()
    inner[1:, :] &= mask[:-1, :]
    inner[:-1, :] &= mask[1:, :]
    inner[:, 1:] &= mask[:, :-1]
    inner[:, :-1] &= mask[:, 1:]
    return inner


def dilated(mask, steps):
    grown = mask.copy()
    for _ in range(steps):
        step = grown.copy()
        step[1:, :] |= grown[:-1, :]
        step[:-1, :] |= grown[1:, :]
        step[:, 1:] |= grown[:, :-1]
        step[:, :-1] |= grown[:, 1:]
        grown = step
    return grown


def terrain_share_around(pixels, mask, x0, y0):
    height, width = pixels.shape[:2]
    m = SKY_HOLE_RING
    left, top = max(0, x0 - m), max(0, y0 - m)
    right, bottom = min(width, x0 + mask.shape[1] + m), min(height, y0 + mask.shape[0] + m)
    local = np.zeros((bottom - top, right - left), dtype=bool)
    local[y0 - top:y0 - top + mask.shape[0], x0 - left:x0 - left + mask.shape[1]] = mask
    ring = dilated(local, m) & ~local
    patch = pixels[top:bottom, left:right][ring]
    if len(patch) == 0:
        return 0.0
    bluish = patch[:, 2] - patch[:, 0] >= WATER_MIN_BLUE_OVER_RED
    return float(1 - bluish.mean())


def find_sky_holes(pixels, luma):
    candidate = blue_like(pixels)
    shape = candidate.shape
    main = np.zeros(shape, dtype=bool)
    seed_columns = candidate[:SKY_SEED_ROWS].all(axis=0)
    if seed_columns.any():
        for component in label_components(candidate):
            if component[3] == 0 and any(seed_columns[s:e].any() for y, s, e in component[5] if y == 0):
                main |= component_mask(component, shape)
    distance = segment_distance(pixels, SKY_DARK, SKY_LIGHT)
    palette = sky_palette(pixels, main)
    if palette is not None:
        distance = np.minimum(distance, palette_distance(pixels, palette))
    close = distance < SKY_COLOUR_TOLERANCE
    horizon = np.where(main.any(axis=0), shape[0] - 1 - np.argmax(main[::-1], axis=0), -1)
    holes = []
    for component in label_components(close & candidate & ~main):
        area, x0, x1, y0, y1, runs = component
        if area < SKY_HOLE_MIN_AREA:
            continue
        column_horizon = horizon[x0:x1 + 1].max()
        depth = y0 - column_horizon
        if depth < SKY_HOLE_MIN_DEPTH:
            continue
        mask = component_mask(component, shape)[y0:y1 + 1, x0:x1 + 1]
        inner = eroded(mask)
        roughness = component_roughness(luma, inner if inner.sum() >= SKY_HOLE_MIN_AREA else mask, x0, y0)
        mean_distance = float(distance[y0:y1 + 1, x0:x1 + 1][mask].mean())
        if roughness > SKY_HOLE_MAX_ROUGHNESS or mean_distance > SKY_HOLE_MAX_MEAN_DISTANCE:
            continue
        terrain = terrain_share_around(pixels, mask, x0, y0)
        if terrain < SKY_HOLE_MIN_TERRAIN_SHARE:
            continue
        score = area * terrain * (1 - mean_distance / SKY_COLOUR_TOLERANCE)
        holes.append(("sky_hole", x0, x1, y0, y1, score, f"area={area} below_horizon={depth} rough={roughness:.2f} sky_dist={mean_distance:.1f} terrain_ring={terrain:.2f}"))
    return holes


def find_black(luma):
    found = []
    for area, x0, x1, y0, y1, runs in label_components(luma < BLACK_LUMA):
        if area >= BLACK_MIN_AREA:
            found.append(("black", x0, x1, y0, y1, float(area), f"area={area}"))
    return found


def axis_runs(step, axis):
    signed = np.sign(step) * (np.abs(step) > STEP_THRESHOLD)
    if axis == 1:
        signed = signed.T
    found = []
    width = AXIS_SIGN_TOLERANCE_COLUMNS
    for sign in (1, -1):
        hit = signed == sign
        widened = hit.copy()
        for shift in range(1, width + 1):
            widened[:, shift:] |= hit[:, :-shift]
            widened[:, :-shift] |= hit[:, shift:]
        for column in range(0, hit.shape[1], width * 2 + 1):
            line = np.concatenate(([0], widened[:, column].view(np.int8), [0]))
            edges = np.flatnonzero(np.diff(line))
            for start, end in zip(edges[::2], edges[1::2]):
                if end - start >= AXIS_MIN_RUN:
                    strength = float(np.abs(step if axis == 0 else step.T)[start:end, max(0, column - width):column + width + 1].max(axis=1).mean())
                    found.append((column, start, end - 1, strength))
    boxes = []
    for column, start, end, strength in found:
        length = end - start + 1
        detail = f"len={length} step={strength:.0f}"
        if axis == 0:
            boxes.append(("axis_vertical", column - width, column + width, start, end, length * strength / 100, detail))
        else:
            boxes.append(("axis_horizontal", start, end, column - width, column + width, length * strength / 100, detail))
    return boxes


def gradients(luma):
    gx = np.zeros_like(luma)
    gy = np.zeros_like(luma)
    o = STEP_OFFSET
    gx[:, o:-o] = luma[:, 2 * o:] - luma[:, :-2 * o]
    gy[o:-o, :] = luma[2 * o:, :] - luma[:-2 * o, :]
    return gx, gy


def hough_lines(luma, gx, gy):
    magnitude = np.hypot(gx, gy)
    edge_mask = magnitude > HOUGH_EDGE_THRESHOLD
    ys, xs = np.nonzero(edge_mask)
    if len(xs) == 0:
        return []
    edge_cos = gx[ys, xs] / magnitude[ys, xs]
    edge_sin = gy[ys, xs] / magnitude[ys, xs]
    accumulator, angles, diagonal = accumulate_hough(xs, ys, luma.shape, edge_cos, edge_sin)
    found = []
    for index, rho_index in hough_peaks(accumulator, LINE_MIN_LENGTH // 2):
        found += step_segments(luma, edge_mask, angles[index], rho_index - diagonal, index * 180.0 / HOUGH_ANGLE_STEPS)
    return found


def step_segments(luma, edge_mask, angle, rho, angle_deg):
    xi, yi, normal = line_points(luma.shape, angle, rho)
    if len(xi) == 0:
        return []
    height, width = luma.shape
    nx, ny = int(np.round(normal[0])), int(np.round(normal[1]))
    on_edge = edge_mask[yi, xi] | edge_mask[np.clip(yi + ny, 0, height - 1), np.clip(xi + nx, 0, width - 1)]
    o = LINE_SIDE_OFFSET
    ax = np.clip(np.round(xi + normal[0] * o).astype(int), 0, width - 1)
    ay = np.clip(np.round(yi + normal[1] * o).astype(int), 0, height - 1)
    bx = np.clip(np.round(xi - normal[0] * o).astype(int), 0, width - 1)
    by = np.clip(np.round(yi - normal[1] * o).astype(int), 0, height - 1)
    side_step = luma[ay, ax] - luma[by, bx]
    axis_aligned = min(angle_deg % 90, 90 - angle_deg % 90) <= AXIS_ANGLE_TOLERANCE_DEG
    kind = "axis_line" if axis_aligned else "straight_line"
    boxes = []
    for a, b in runs_with_gaps(on_edge):
        length = b - a + 1
        if length < LINE_MIN_LENGTH:
            continue
        steps = side_step[a:b + 1]
        mean_step = float(np.abs(steps.mean()))
        sign_share = float(max((steps > 0).mean(), (steps < 0).mean()))
        if mean_step < LINE_MIN_STEP or sign_share < LINE_MIN_SIGN_SHARE:
            continue
        x0, x1 = int(min(xi[a], xi[b])), int(max(xi[a], xi[b]))
        y0, y1 = int(min(yi[a], yi[b])), int(max(yi[a], yi[b]))
        detail = f"len={length} step={mean_step:.0f} sign={sign_share:.2f} angle={angle_deg:.0f}"
        boxes.append((kind, x0, x1, y0, y1, length * mean_step * sign_share / 100, detail))
    return boxes


def dark_ridge(luma):
    o = RIDGE_SIDE_OFFSET
    padded = np.pad(luma, o, mode="edge")
    height, width = luma.shape
    shifted = lambda dy, dx: padded[o + dy:o + dy + height, o + dx:o + dx + width]
    ridge = np.zeros_like(luma)
    for dy, dx in ((0, o), (o, 0), (o, o), (o, -o)):
        ridge = np.maximum(ridge, np.minimum(shifted(dy, dx), shifted(-dy, -dx)) - luma)
    return ridge


def accumulate_hough(xs, ys, shape, edge_cos=None, edge_sin=None):
    height, width = shape
    diagonal = int(np.ceil(np.hypot(width, height)))
    angles = np.deg2rad(np.arange(HOUGH_ANGLE_STEPS) * 180.0 / HOUGH_ANGLE_STEPS)
    accumulator = np.zeros((HOUGH_ANGLE_STEPS, 2 * diagonal + 1), dtype=np.int32)
    for index, angle in enumerate(angles):
        c, s = np.cos(angle), np.sin(angle)
        keep = slice(None) if edge_cos is None else np.abs(edge_cos * c + edge_sin * s) > HOUGH_ORIENTATION_COS
        rho = np.round(xs[keep] * c + ys[keep] * s).astype(np.int64) + diagonal
        accumulator[index] = np.bincount(rho, minlength=accumulator.shape[1])
    return accumulator, angles, diagonal


def hough_peaks(accumulator, minimum):
    work = accumulator.copy()
    for _ in range(HOUGH_PEAKS):
        index, rho_index = np.unravel_index(np.argmax(work), work.shape)
        if work[index, rho_index] < minimum:
            return
        for d in range(-HOUGH_PEAK_SUPPRESS_ANGLE, HOUGH_PEAK_SUPPRESS_ANGLE + 1):
            work[(index + d) % HOUGH_ANGLE_STEPS, max(0, rho_index - HOUGH_PEAK_SUPPRESS_RHO):rho_index + HOUGH_PEAK_SUPPRESS_RHO + 1] = 0
        yield index, rho_index


def thin_lines(luma):
    ridge = dark_ridge(luma)
    mask = ridge > RIDGE_THRESHOLD
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return []
    accumulator, angles, diagonal = accumulate_hough(xs, ys, luma.shape)
    found = []
    for index, rho_index in hough_peaks(accumulator, THIN_LINE_MIN_LENGTH // 2):
        found += ridge_segments(ridge, mask, angles[index], rho_index - diagonal, index * 180.0 / HOUGH_ANGLE_STEPS)
    return found


def line_points(shape, angle, rho):
    height, width = shape
    normal = np.array([np.cos(angle), np.sin(angle)])
    direction = np.array([-normal[1], normal[0]])
    span = int(np.hypot(width, height))
    t = np.arange(-span, span + 1)
    px = normal[0] * rho + t * direction[0]
    py = normal[1] * rho + t * direction[1]
    margin = LINE_SIDE_OFFSET + 1
    inside = (px >= margin) & (px < width - margin) & (py >= margin) & (py < height - margin)
    return np.round(px[inside]).astype(int), np.round(py[inside]).astype(int), normal


def runs_with_gaps(hits):
    segments = []
    start = None
    gap = 0
    for i, hit in enumerate(hits):
        if hit:
            if start is None:
                start = i
            last = i
            gap = 0
        elif start is not None:
            gap += 1
            if gap > LINE_MAX_GAP:
                segments.append((start, last))
                start = None
    if start is not None:
        segments.append((start, last))
    return segments


def ridge_segments(ridge, mask, angle, rho, angle_deg):
    xi, yi, normal = line_points(ridge.shape, angle, rho)
    if len(xi) == 0:
        return []
    nx, ny = int(np.round(normal[0])), int(np.round(normal[1]))
    height, width = ridge.shape
    near = mask[yi, xi] | mask[np.clip(yi + ny, 0, height - 1), np.clip(xi + nx, 0, width - 1)] | mask[np.clip(yi - ny, 0, height - 1), np.clip(xi - nx, 0, width - 1)]
    strength = np.maximum.reduce([ridge[np.clip(yi + k * ny, 0, height - 1), np.clip(xi + k * nx, 0, width - 1)] for k in (-1, 0, 1)])
    boxes = []
    for a, b in runs_with_gaps(near):
        length = b - a + 1
        if length < THIN_LINE_MIN_LENGTH:
            continue
        cover = float(near[a:b + 1].mean())
        depth = float(strength[a:b + 1].mean())
        if cover < THIN_LINE_MIN_COVER:
            continue
        x0, x1 = int(min(xi[a], xi[b])), int(max(xi[a], xi[b]))
        y0, y1 = int(min(yi[a], yi[b])), int(max(yi[a], yi[b]))
        detail = f"len={length} depth={depth:.0f} cover={cover:.2f} angle={angle_deg:.0f}"
        boxes.append(("thin_dark_line", x0, x1, y0, y1, length * depth * cover / 100, detail))
    return boxes


def merge_boxes(boxes):
    merged = []
    for box in sorted(boxes, key=lambda b: -b[5]):
        kind, x0, x1, y0, y1, score, detail = box
        for i, other in enumerate(merged):
            if other[0] == kind and x0 <= other[2] + BOX_MERGE_MARGIN and other[1] <= x1 + BOX_MERGE_MARGIN and y0 <= other[4] + BOX_MERGE_MARGIN and other[3] <= y1 + BOX_MERGE_MARGIN:
                merged[i] = (kind, min(x0, other[1]), max(x1, other[2]), min(y0, other[3]), max(y1, other[4]), other[5] + score, other[6])
                break
        else:
            merged.append(box)
    return sorted(merged, key=lambda b: -b[5])


def box_overlap_share(a, b):
    width = min(a[2], b[2]) - max(a[1], b[1]) + 1 + LINE_DUPLICATE_SLACK
    height = min(a[4], b[4]) - max(a[3], b[3]) + 1 + LINE_DUPLICATE_SLACK
    if width <= 0 or height <= 0:
        return 0.0
    smaller = min((a[2] - a[1] + 1 + LINE_DUPLICATE_SLACK) * (a[4] - a[3] + 1 + LINE_DUPLICATE_SLACK), (b[2] - b[1] + 1 + LINE_DUPLICATE_SLACK) * (b[4] - b[3] + 1 + LINE_DUPLICATE_SLACK))
    return width * height / smaller


def drop_duplicate_lines(boxes):
    kept = []
    for box in sorted(boxes, key=lambda b: -b[5]):
        if all(box_overlap_share(box, other) < LINE_DUPLICATE_SHARE for other in kept):
            kept.append(box)
    return kept


def scan_shot(path):
    pixels = np.asarray(Image.open(path).convert("RGB")).astype(np.float32)
    luma = pixels @ LUMA_WEIGHTS
    gx, gy = gradients(luma)
    boxes = find_sky_holes(pixels, luma) + find_black(luma)
    boxes += merge_boxes(axis_runs(gx, 0) + axis_runs(gy, 1))
    boxes += drop_duplicate_lines(hough_lines(luma, gx, gy))
    boxes += drop_duplicate_lines(thin_lines(luma))
    return boxes


def shot_paths(target):
    path = Path(target)
    if not path.exists():
        path = VERIFY / "shots" / target
    return sorted(path.glob("*.png")) if path.is_dir() else [path]


def overlaps(box, region):
    _, x0, x1, y0, y1 = box[:5]
    rx0, rx1, ry0, ry1 = region
    return x0 <= rx1 and rx0 <= x1 and y0 <= ry1 and ry0 <= y1


def parse_region(text):
    xs, ys = text.split(",")
    x0, x1 = (int(v) for v in xs.split("-"))
    y0, y1 = (int(v) for v in ys.split("-"))
    return x0, x1, y0, y1


def command_scan(args):
    region = parse_region(args.region) if args.region else None
    for target in args.targets:
        for path in shot_paths(target):
            boxes = scan_shot(path)
            if region:
                boxes = [b for b in boxes if overlaps(b, region)]
            if args.kind:
                boxes = [b for b in boxes if b[0] in args.kind]
            boxes = sorted(boxes, key=lambda b: -b[5])[:args.top]
            for kind, x0, x1, y0, y1, score, detail in boxes:
                print(f"{path.stem}\t{kind}\t{x0}-{x1},{y0}-{y1}\t{score:.1f}\t{detail}")
            sys.stdout.flush()


def command_crop(args):
    x0, x1, y0, y1 = parse_region(args.region)
    image = Image.open(args.shot).convert("RGB").crop((x0, y0, x1 + 1, y1 + 1))
    image = image.resize((image.width * args.scale, image.height * args.scale), Image.NEAREST)
    image.save(args.out)


def main():
    parser = argparse.ArgumentParser(description="Suspect boxes in shots: sky holes inside the ground, black blobs, long straight colour steps.")
    commands = parser.add_subparsers(dest="command", required=True)
    scan = commands.add_parser("scan", help="tab separated lines: shot kind x0-x1,y0-y1 score detail")
    scan.add_argument("targets", nargs="+", help="shot label, folder or png")
    scan.add_argument("--region", help="only boxes overlapping x0-x1,y0-y1")
    scan.add_argument("--kind", nargs="*", help="sky_hole black axis_vertical axis_horizontal axis_line straight_line thin_dark_line")
    scan.add_argument("--top", type=int, default=40)
    scan.set_defaults(run=command_scan)
    crop = commands.add_parser("crop", help="cut x0-x1,y0-y1 out of a shot, enlarged")
    crop.add_argument("shot")
    crop.add_argument("region")
    crop.add_argument("out")
    crop.add_argument("--scale", type=int, default=CROP_SCALE)
    crop.set_defaults(run=command_crop)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()

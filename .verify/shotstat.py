#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image

VERIFY = Path(__file__).resolve().parent
DARK_LEVEL = 8
BLANK_MAX_STDDEV = 2.0
BLANK_MIN_DARK_SHARE = 0.98
COLOUR_SAMPLE_STRIDE = 97
LUMA_WEIGHTS = np.array([0.299, 0.587, 0.114], dtype=np.float32)


def shot_paths(target):
    path = Path(target)
    if not path.exists():
        path = VERIFY / "shots" / target
    return sorted(path.glob("*.png")) if path.is_dir() else [path]


def main():
    parser = argparse.ArgumentParser(description="Brightness and blankness of shots, one line each; fails if any shot is blank.")
    parser.add_argument("targets", nargs="+", help="shot label, folder or png")
    args = parser.parse_args()

    blank = 0
    print(f"{'shot':<28} {'size':>9} {'mean':>6} {'stddev':>6} {'dark%':>6} {'colours':>7}")
    for target in args.targets:
        for path in shot_paths(target):
            pixels = np.asarray(Image.open(path).convert("RGB"))
            luma = pixels.astype(np.float32) @ LUMA_WEIGHTS
            dark_share = float(np.mean(luma < DARK_LEVEL))
            colours = len(np.unique(pixels.reshape(-1, 3)[::COLOUR_SAMPLE_STRIDE], axis=0))
            is_blank = luma.std() < BLANK_MAX_STDDEV or dark_share > BLANK_MIN_DARK_SHARE
            blank += is_blank
            size = f"{pixels.shape[1]}x{pixels.shape[0]}"
            print(f"{path.stem:<28} {size:>9} {luma.mean():>6.1f} {luma.std():>6.1f} {dark_share * 100:>6.1f} {colours:>7}{'  BLANK' if is_blank else ''}")
    return 1 if blank else 0


if __name__ == "__main__":
    sys.exit(main())

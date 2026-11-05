#!/usr/bin/env python3
"""Validate Supercell Wx Vulkan smoke screenshots.

Log lines prove command submission. This script proves user-visible output
made it into the captured frame.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from PIL import Image


def is_radar_like(r: int, g: int, b: int) -> bool:
    """Match saturated radar palette colors while avoiding muted basemap tones."""
    spread = max(r, g, b) - min(r, g, b)
    if spread < 55 or max(r, g, b) < 105:
        return False

    green = g >= 120 and g - r >= 25 and g - b >= 25
    yellow = r >= 135 and g >= 120 and b <= 115
    red = r >= 140 and g <= 135 and b <= 135
    cyan_blue = b >= 140 and (g >= 100 or r <= 110) and spread >= 65
    return green or yellow or red or cyan_blue


def is_saturated(r: int, g: int, b: int) -> bool:
    return max(r, g, b) >= 95 and max(r, g, b) - min(r, g, b) >= 45


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--min-radar-pixels", type=int, default=500)
    parser.add_argument("--min-colorbar-pixels", type=int, default=100)
    parser.add_argument("--min-colorbar-colors", type=int, default=8)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if not args.image.exists():
        print(f"missing image: {args.image}", file=sys.stderr)
        return 2

    image = Image.open(args.image).convert("RGBA")
    width, height = image.size
    if width < 200 or height < 150:
        print(f"image too small: {width}x{height}", file=sys.stderr)
        return 2

    x0 = int(width * 0.08)
    x1 = int(width * 0.92)
    y0 = int(height * 0.12)
    y1 = int(height * 0.82)

    radar_pixels = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b, a = image.getpixel((x, y))
            if a > 0 and is_radar_like(r, g, b):
                radar_pixels += 1

    colorbar_pixels = 0
    colorbar_colors: set[tuple[int, int, int]] = set()
    best_colorbar_row = 0
    colorbar_bands = list(range(0, int(height * 0.12))) + list(
        range(int(height * 0.88), height)
    )
    for y in colorbar_bands:
        row_pixels = 0
        for x in range(x0, x1):
            r, g, b, a = image.getpixel((x, y))
            if a > 0 and is_saturated(r, g, b):
                colorbar_pixels += 1
                row_pixels += 1
                colorbar_colors.add((r // 32, g // 32, b // 32))
        best_colorbar_row = max(best_colorbar_row, row_pixels)

    metrics = {
        "image": str(args.image),
        "width": width,
        "height": height,
        "radar_pixels": radar_pixels,
        "colorbar_pixels": colorbar_pixels,
        "colorbar_colors": len(colorbar_colors),
        "best_colorbar_row": best_colorbar_row,
    }

    failures: list[str] = []
    if radar_pixels < args.min_radar_pixels:
        failures.append(f"radar_pixels {radar_pixels} < {args.min_radar_pixels}")
    if colorbar_pixels < args.min_colorbar_pixels:
        failures.append(
            f"colorbar_pixels {colorbar_pixels} < {args.min_colorbar_pixels}"
        )
    if len(colorbar_colors) < args.min_colorbar_colors:
        failures.append(
            f"colorbar_colors {len(colorbar_colors)} < {args.min_colorbar_colors}"
        )
    min_colorbar_row = int((x1 - x0) * 0.20)
    if best_colorbar_row < min_colorbar_row:
        failures.append(f"best_colorbar_row {best_colorbar_row} < {min_colorbar_row}")

    if args.json:
        print(json.dumps({**metrics, "failures": failures}, indent=2))
    else:
        print(
            "capture metrics: "
            f"{width}x{height}, "
            f"radar_pixels={radar_pixels}, "
            f"colorbar_pixels={colorbar_pixels}, "
            f"colorbar_colors={len(colorbar_colors)}, "
            f"best_colorbar_row={best_colorbar_row}"
        )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print("OK: Vulkan capture contains expected radar/color-table pixels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

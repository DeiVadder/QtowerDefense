"""Draws the icon of the game: a stone tower in an iron octagon, in the colors of src/client/Theme.qml.

    python scripts/make_icon.py

Writes src/client/assets/icon.png (256 pixels, the window and the splash screen) and
src/app/qtowerdefense.ico (16 to 256 pixels, the program on Windows). Every size is drawn on its
own, with smooth edges: each pixel row is sampled SAMPLES times, spans cover pixels in part.
"""

import math
import struct
import sys
import zlib

from qtd_common import REPO

SAMPLES = 4
SIZES = (16, 24, 32, 48, 64, 128, 256)
IRON_DARK = (0x1E, 0x21, 0x24)
IRON = (0x4A, 0x4F, 0x54)
IRON_LIGHT = (0x86, 0x8D, 0x92)
FIELD = (0x6D, 0x2A, 0x20)  # heroShield
STONE = (0x9A, 0xA1, 0xA6)  # helmetLight
STONE_SHADE = (0x6B, 0x72, 0x77)  # helmetMiddle
WOOD_DARK = (0x2F, 0x1C, 0x0F)
GOLD = (0xF2, 0xC2, 0x3E)
ACCENT = (0x2C, 0xDE, 0x85)


def octagon(inset):
    """A regular octagon in the unit square, its edges pulled in by inset."""
    low, high = inset, 1 - inset
    cut = (high - low) / (2 + 2**0.5)
    return [(low + cut, low), (high - cut, low), (high, low + cut), (high, high - cut),
            (high - cut, high), (low + cut, high), (low, high - cut), (low, low + cut)]  # fmt: skip


def box(left, top, right, bottom):
    return [(left, top), (right, top), (right, bottom), (left, bottom)]


def arch(left, right, top, bottom, steps=12):
    """A door: a box with a half circle on top."""
    middle, radius = (left + right) / 2, (right - left) / 2
    curve = [(middle - radius * math.cos(math.pi * k / steps),
              top + radius - radius * math.sin(math.pi * k / steps)) for k in range(steps + 1)]  # fmt: skip
    return [(left, bottom), *curve, (right, bottom)]


# From the back to the front.
SHAPES = [
    (octagon(0.0), IRON_DARK),
    (octagon(0.035), IRON_LIGHT),
    (octagon(0.06), IRON),
    (octagon(0.11), FIELD),
    (box(0.488, 0.09, 0.512, 0.27), IRON_DARK),  # the pole
    ([(0.512, 0.10), (0.68, 0.145), (0.512, 0.19)], ACCENT),  # the flag
    (box(0.28, 0.27, 0.38, 0.35), STONE),  # three merlons
    (box(0.45, 0.27, 0.55, 0.35), STONE),
    (box(0.62, 0.27, 0.72, 0.35), STONE),
    (box(0.28, 0.345, 0.72, 0.42), STONE),  # the parapet
    (box(0.33, 0.42, 0.67, 0.86), STONE),  # the tower
    (box(0.5, 0.42, 0.67, 0.86), STONE_SHADE),  # its side in shade
    (box(0.28, 0.405, 0.72, 0.43), STONE_SHADE),  # the ledge under the parapet
    (box(0.465, 0.5, 0.535, 0.6), GOLD),  # a lit window
    (arch(0.43, 0.57, 0.69, 0.86), WOOD_DARK),  # the gate
]


def coverage(polygon, size):
    """How much of each pixel the polygon covers, 0 to 1, as rows of floats."""
    rows = [[0.0] * size for _ in range(size)]
    points = [(x * size, y * size) for x, y in polygon]
    edges = list(zip(points, points[1:] + points[:1]))
    for row in range(size):
        for sample in range(SAMPLES):
            y = row + (sample + 0.5) / SAMPLES
            crossings = sorted(
                x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                for (x0, y0), (x1, y1) in edges
                if (y0 <= y < y1) or (y1 <= y < y0)
            )
            for start, end in zip(crossings[::2], crossings[1::2]):
                add_span(rows[row], max(0.0, start), min(float(size), end))
    return rows


def add_span(row, start, end):
    """Adds a span of one sample row, pixels at its ends only in part."""
    first, last = int(start), min(int(end), len(row) - 1)
    for pixel in range(first, last + 1):
        covered = min(end, pixel + 1) - max(start, pixel)
        if covered > 0:
            row[pixel] += covered / SAMPLES


def over(below, color, alpha):
    """A color with the given cover over a pixel, in straight alpha as PNG keeps it."""
    *base, cover = below
    total = alpha + cover * (1 - alpha)
    mixed = [(c * alpha + b * cover * (1 - alpha)) / total for c, b in zip(color, base)]
    return (*mixed, total)


def picture(size):
    """RGBA rows, each shape painted over the ones before."""
    pixels = [[(0.0, 0.0, 0.0, 0.0)] * size for _ in range(size)]
    for polygon, color in SHAPES:
        cover = coverage(polygon, size)
        for y in range(size):
            for x in range(size):
                alpha = min(1.0, cover[y][x])
                if alpha > 0:
                    pixels[y][x] = over(pixels[y][x], color, alpha)
    return pixels


def png(pixels):
    size = len(pixels)
    raw = b"".join(
        b"\0" + bytes(round(v) for r, g, b, a in line for v in (r, g, b, a * 255)) for line in pixels
    )

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))  # fmt: skip


def ico(images):
    """An icon file of PNG pictures, as Windows reads it since Vista."""
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = len(head) + 16 * len(images)
    entries, data = b"", b""
    for size, image in images:
        side = 0 if size >= 256 else size  # 0 means 256
        entries += struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(image), offset + len(data))
        data += image
    return head + entries + data


def main():
    images = [(size, png(picture(size))) for size in SIZES]
    (REPO / "src" / "client" / "assets" / "icon.png").write_bytes(images[-1][1])
    (REPO / "src" / "app" / "qtowerdefense.ico").write_bytes(ico(images))
    return 0


if __name__ == "__main__":
    sys.exit(main())

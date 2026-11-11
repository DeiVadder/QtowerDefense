"""Draws the icon of the game: a stone tower in an iron octagon, in the colors of src/client/Theme.qml.

    python scripts/make_icon.py

Writes src/client/assets/icon.png (256 pixels, the window and the splash screen),
src/app/qtowerdefense.ico (16 to 256 pixels, the program on Windows), and for the other platforms
(E-46) src/app/macos/qtowerdefense.icns, the launcher icons of Android under
src/app/android/res/mipmap-*/ and the App Store icon of iOS under src/app/ios/Assets.xcassets/.
Every size is drawn on its own, with smooth edges: each pixel row is sampled SAMPLES times, spans
cover pixels in part.
"""

import json
import math
import struct
import sys
import zlib

from qtd_common import REPO

SAMPLES = 4
SIZES = (16, 24, 32, 48, 64, 128, 256)
# The pictures of an icns file by their type, see the Apple icon format.
ICNS_TYPES = ((b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128), (b"ic08", 256),
              (b"ic09", 512), (b"ic10", 1024))  # fmt: skip
# The launcher icon of Android in each density bucket, 48 dp each.
ANDROID_SIZES = (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192))
IOS_SIZE = 1024
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


def opaque(pixels, color):
    """The picture over a solid color: the App Store takes no icon with transparent pixels."""
    return [[over((*color, 1.0), pixel[:3], pixel[3])[:3] + (1.0,) for pixel in line]
            for line in pixels]  # fmt: skip


def png(pixels, alpha=True):
    """RGBA, or RGB without the alpha channel, which the App Store refuses even when it is opaque."""
    size = len(pixels)
    channels = (lambda r, g, b, a: (r, g, b, a * 255)) if alpha else (lambda r, g, b, a: (r, g, b))
    raw = b"".join(
        b"\0" + bytes(round(v) for pixel in line for v in channels(*pixel)) for line in pixels
    )

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", size, size, 8, 6 if alpha else 2, 0, 0, 0)
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


def icns(images):
    """An Apple icon file: a header, then each picture as PNG with its type and length."""
    entries = b"".join(kind + struct.pack(">I", 8 + len(data)) + data for kind, data in images)
    return b"icns" + struct.pack(">I", 8 + len(entries)) + entries


def ios_catalog(folder, image):
    """An asset catalog with one icon of 1024 pixels, Xcode makes the other sizes from it."""
    icon_set = folder / "AppIcon.appiconset"
    icon_set.mkdir(parents=True, exist_ok=True)
    (icon_set / "icon.png").write_bytes(image)
    info = {"author": "xcode", "version": 1}
    contents = {"images": [{"filename": "icon.png", "idiom": "universal", "platform": "ios",
                            "size": f"{IOS_SIZE}x{IOS_SIZE}"}], "info": info}  # fmt: skip
    (icon_set / "Contents.json").write_text(json.dumps(contents, indent=2) + "\n",
                                              encoding="utf-8", newline="\n")
    (folder / "Contents.json").write_text(json.dumps({"info": info}, indent=2) + "\n",
                                          encoding="utf-8", newline="\n")


def android_icons(res, pictures):
    """The launcher icon in each density bucket, as res/mipmap-<bucket>/ic_launcher.png."""
    for bucket, size in ANDROID_SIZES:
        folder = res / f"mipmap-{bucket}"
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "ic_launcher.png").write_bytes(png(pictures[size]))


def main():
    wanted = set(SIZES) | {size for _, size in ICNS_TYPES} | {size for _, size in ANDROID_SIZES}
    pictures = {size: picture(size) for size in sorted(wanted)}
    images = [(size, png(pictures[size])) for size in SIZES]
    app = REPO / "src" / "app"
    (REPO / "src" / "client" / "assets" / "icon.png").write_bytes(images[-1][1])
    (app / "qtowerdefense.ico").write_bytes(ico(images))
    (app / "macos").mkdir(exist_ok=True)
    (app / "macos" / "qtowerdefense.icns").write_bytes(
        icns([(kind, png(pictures[size])) for kind, size in ICNS_TYPES]))
    android_icons(app / "android" / "res", pictures)
    ios_catalog(app / "ios" / "Assets.xcassets", png(opaque(pictures[IOS_SIZE], IRON_DARK), alpha=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())

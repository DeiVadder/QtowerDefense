"""The icon: smooth edges, and an icon file Windows can read."""

import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import make_icon


class CoverageTest(unittest.TestCase):
    def test_a_box_covers_its_pixels_whole_and_its_edge_by_half(self):
        rows = make_icon.coverage(make_icon.box(0, 0, 0.625, 1), 4)  # 2.5 of 4 pixels
        for row in rows:
            self.assertEqual(row, [1, 1, 0.5, 0])

    def test_a_slope_covers_in_part(self):
        rows = make_icon.coverage([(0, 0), (1, 1), (0, 1)], 2)  # the lower left half
        self.assertEqual(rows[0][1], 0)
        self.assertEqual(rows[1][0], 1)
        self.assertTrue(0 < rows[0][0] < 1)


class OverTest(unittest.TestCase):
    def test_a_half_covered_empty_pixel_keeps_the_color(self):
        self.assertEqual(make_icon.over((0, 0, 0, 0), (200, 100, 50), 0.5), (200, 100, 50, 0.5))

    def test_half_over_a_full_pixel_mixes(self):
        self.assertEqual(make_icon.over((0, 0, 0, 1), (200, 100, 50), 0.5), (100, 50, 25, 1))


class FileTest(unittest.TestCase):
    def test_the_icon_file_names_each_picture(self):
        data = make_icon.ico([(16, b"a" * 5), (256, b"b" * 7)])
        self.assertEqual(struct.unpack_from("<HHH", data), (0, 1, 2))
        first = struct.unpack_from("<BBBBHHII", data, 6)
        second = struct.unpack_from("<BBBBHHII", data, 22)
        self.assertEqual((first[0], first[6], first[7]), (16, 5, 38))
        self.assertEqual((second[0], second[6], second[7]), (0, 7, 43))  # 0 is 256
        self.assertEqual(data[38:], b"a" * 5 + b"b" * 7)

    def test_the_apple_icon_file_names_each_picture_with_its_length(self):
        data = make_icon.icns([(b"ic07", b"a" * 5), (b"ic08", b"b" * 7)])
        self.assertEqual(data[:4], b"icns")
        self.assertEqual(struct.unpack_from(">I", data, 4)[0], len(data))
        self.assertEqual(data[8:12], b"ic07")
        self.assertEqual(struct.unpack_from(">I", data, 12)[0], 13)  # with its own 8 bytes
        self.assertEqual(data[16:21], b"a" * 5)
        self.assertEqual(data[21:25], b"ic08")

    def test_the_app_store_icon_has_no_transparent_pixel(self):
        pixels = make_icon.opaque(make_icon.picture(16), make_icon.IRON_DARK)
        self.assertTrue(all(pixel[3] == 1.0 for line in pixels for pixel in line))
        self.assertEqual(pixels[0][0][:3], make_icon.IRON_DARK)  # a corner outside the octagon
        picture = make_icon.png(pixels, alpha=False)
        self.assertEqual(picture[25], 2, "colour type RGB, no alpha channel")
        self.assertEqual(len(zlib.decompress(picture[41:-12])), 16 * (1 + 16 * 3))

    def test_the_ios_catalog_names_its_one_icon(self):
        with tempfile.TemporaryDirectory() as folder:
            catalog = Path(folder) / "Assets.xcassets"
            make_icon.ios_catalog(catalog, b"picture")
            contents = json.loads((catalog / "AppIcon.appiconset" / "Contents.json").read_text())
            self.assertEqual(contents["images"], [{"filename": "icon.png", "idiom": "universal",
                                                   "platform": "ios", "size": "1024x1024"}])
            self.assertEqual((catalog / "AppIcon.appiconset" / "icon.png").read_bytes(), b"picture")
            self.assertTrue((catalog / "Contents.json").exists())

    def test_android_gets_a_launcher_icon_of_48_dp_in_each_density(self):
        sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
        pictures = {size: [[(0, 0, 0, 1.0)] * size] * size for size in sizes.values()}
        with tempfile.TemporaryDirectory() as folder:
            make_icon.android_icons(Path(folder), pictures)
            for bucket, size in sizes.items():
                data = (Path(folder) / f"mipmap-{bucket}" / "ic_launcher.png").read_bytes()
                self.assertEqual(struct.unpack_from(">II", data, 16), (size, size), bucket)

    def test_a_picture_is_a_png_of_its_size(self):
        picture = make_icon.png(make_icon.picture(16))
        self.assertTrue(picture.startswith(b"\x89PNG"))
        self.assertEqual(struct.unpack_from(">II", picture, 16), (16, 16))


if __name__ == "__main__":
    unittest.main()

"""The icon: smooth edges, and an icon file Windows can read."""

import struct
import sys
import unittest
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

    def test_a_picture_is_a_png_of_its_size(self):
        picture = make_icon.png(make_icon.picture(16))
        self.assertTrue(picture.startswith(b"\x89PNG"))
        self.assertEqual(struct.unpack_from(">II", picture, 16), (16, 16))


if __name__ == "__main__":
    unittest.main()

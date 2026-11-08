"""Posing skinned models: keys between frames, joints moving vertices, frames on one ground."""

import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import gltf_mesh
import import_assets

HALF_TURN_Y = [0.0, 1.0, 0.0, 0.0]  # a rotation by 180 degrees around y
QUARTER = math.sqrt(0.5)


def moved_by(x, y, z):
    return [[1, 0, 0, x], [0, 1, 0, y], [0, 0, 1, z], [0, 0, 0, 1]]


class SampleTest(unittest.TestCase):
    def test_a_value_is_held_before_and_after_the_keys(self):
        self.assertEqual(gltf_mesh.sample([1, 2], [(1, 0, 0), (3, 0, 0)], 0, False), [1, 0, 0])
        self.assertEqual(gltf_mesh.sample([1, 2], [(1, 0, 0), (3, 0, 0)], 5, False), [3, 0, 0])

    def test_between_keys_it_blends(self):
        self.assertEqual(gltf_mesh.sample([0, 2], [(0, 0, 0), (4, 2, 0)], 0.5, False), [1, 0.5, 0])

    def test_a_rotation_stays_a_unit_and_takes_the_shorter_way(self):
        start = (0.0, 0.0, 0.0, 1.0)
        # The same rotation as a quarter turn, written with the opposite sign.
        end = (0.0, -QUARTER, 0.0, -QUARTER)
        halfway = gltf_mesh.sample([0, 1], [start, end], 0.5, True)
        self.assertAlmostEqual(sum(value * value for value in halfway), 1)
        self.assertGreater(halfway[3], 0.9)  # an eighth turn, not most of the way round
        self.assertGreater(halfway[1], 0)


class SkinTest(unittest.TestCase):
    def test_a_vertex_follows_its_joints_by_weight(self):
        matrices = [moved_by(0, 0, 0), moved_by(2, 0, 0)]
        place, normal = gltf_mesh.skinned((1, 0, 0), (0, 1, 0), (0, 1, 0, 0), (0.75, 0.25, 0, 0),
                                          matrices)  # fmt: skip
        self.assertEqual(place, [1.5, 0, 0])
        self.assertEqual(normal, [0, 1, 0])

    def test_a_normal_turns_but_does_not_move(self):
        turned = gltf_mesh.matrix_of({"rotation": HALF_TURN_Y, "translation": [5, 5, 5]})
        place, normal = gltf_mesh.skinned((1, 0, 0), (1, 0, 0), (0,), (1.0,), [turned])
        self.assertEqual([round(value, 6) for value in place], [4, 5, 5])
        self.assertEqual([round(value, 6) for value in normal], [-1, 0, 0])


class FramesTest(unittest.TestCase):
    def test_frames_share_the_scale_of_the_first_and_none_sinks(self):
        first = {"positions": [[0, 0, 0], [2, 4, 0]]}
        lower = {"positions": [[0, -1, 0], [2, 3, 0]]}
        import_assets.normalized(first, "height", [lower])
        self.assertEqual(first["positions"], [[-0.25, 0.25, 0], [0.25, 1.25, 0]])
        self.assertEqual(lower["positions"], [[-0.25, 0, 0], [0.25, 1, 0]])

    def test_the_walk_frames_match_the_client(self):
        header = Path(import_assets.REPO / "src" / "client" / "game_session.h").read_text()
        self.assertIn(f"constexpr int kWalkFrames = {import_assets.WALK_FRAMES};", header)


if __name__ == "__main__":
    unittest.main()

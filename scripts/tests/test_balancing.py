"""The corridors of the balancing runs: read from the table, measured from the games."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import balancing
from balancing import Bot, Game, Metric, Scope, World

TABLE = """Text before.
<!-- corridors begin -->
| metric      | bot   | scope      | min  | max |
|-------------|-------|------------|------|-----|
| win_rate    | good  | each_map   | 0.9  | 1   |
| win_rate    | naive | campaign   | 0    | 0.5 |
| win_rate    | naive | tutorial   | 1    | 1   |
| spend_share | good  | each_tower | 0.1  | 0.6 |
| maps_used   | good  | each_tower | 2    | 2   |
| mean_gold   | good  | each_map   | 10   | 100 |
<!-- corridors end -->
"""

WORLD = World(maps=["tutorial", "first", "second"], campaign=["first", "second"],
              towers=["arrow", "frost"])  # fmt: skip


def won(spent=None, gold=50.0):
    return Game(True, spent or {}, gold)


def lost(spent=None, gold=50.0):
    return Game(False, spent or {}, gold)


class ParseTest(unittest.TestCase):
    def test_every_row_becomes_a_corridor(self):
        corridors = balancing.parse_corridors(TABLE)
        self.assertEqual(len(corridors), 6)
        self.assertEqual(corridors[1].metric, Metric.WIN_RATE)
        self.assertEqual(corridors[1].bot, Bot.NAIVE)
        self.assertEqual(corridors[1].scope, Scope.CAMPAIGN)
        self.assertEqual(corridors[1].high, 0.5)

    def test_a_map_name_is_a_scope_of_its_own(self):
        corridor = balancing.parse_corridors(TABLE)[2]
        self.assertEqual(corridor.scope, Scope.MAP)
        self.assertEqual(corridor.map, "tutorial")

    def test_wrong_rows_are_named(self):
        rows = {
            "unknown metric": "| speed | good | each_map | 0 | 1 |",
            "unknown bot": "| win_rate | clever | each_map | 0 | 1 |",
            "missing cell": "| win_rate | good | 0 | 1 |",
            "no number": "| win_rate | good | each_map | low | 1 |",
            "min above max": "| win_rate | good | each_map | 1 | 0 |",
        }
        for name, row in rows.items():
            with self.subTest(name), self.assertRaisesRegex(ValueError, row.replace("|", r"\|")):
                balancing.parse_corridors(f"{balancing.BEGIN}\n| h |\n|-|\n{row}\n{balancing.END}")

    def test_an_empty_row_is_named(self):
        with self.assertRaisesRegex(ValueError, "a row needs 5 cells"):
            balancing.parse_corridors(f"{balancing.BEGIN}\n| h |\n|-|\n|  |\n{balancing.END}")

    def test_a_file_without_markers_has_no_table(self):
        with self.assertRaisesRegex(ValueError, "no corridor table"):
            balancing.parse_corridors("| win_rate | good | each_map | 0 | 1 |")


class CheckTest(unittest.TestCase):
    def results(self):
        return {
            Bot.GOOD: {
                "tutorial": [won(gold=20.0), won(gold=40.0)],
                "first": [won({"arrow": 60}), won({"arrow": 60, "frost": 20})],
                "second": [won({"frost": 60}), lost({"arrow": 20, "frost": 60})],
            },
            Bot.NAIVE: {
                "tutorial": [won(), won()],
                "first": [won(), lost()],
                "second": [lost(), lost()],
            },
        }

    def report(self, results):
        return balancing.check(balancing.parse_corridors(TABLE), WORLD, results)

    def test_a_value_outside_its_corridor_fails(self):
        lines, passed = self.report(self.results())
        self.assertFalse(passed, "\n".join(lines))  # the good bot loses half on the second map
        outside = [line for line in lines if line.startswith("OUT")]
        self.assertEqual(outside, ["OUT win_rate good second: 0.50 in [0.9, 1]"])

    def test_the_campaign_counts_all_its_games_together(self):
        lines, _ = self.report(self.results())
        self.assertIn("ok  win_rate naive campaign: 0.25 in [0, 0.5]", lines)

    def test_spend_shares_are_parts_of_the_gold_over_the_campaign(self):
        lines, _ = self.report(self.results())
        self.assertIn("ok  spend_share good arrow: 0.50 in [0.1, 0.6]", lines)
        self.assertIn("ok  spend_share good frost: 0.50 in [0.1, 0.6]", lines)

    def test_a_tower_counts_on_every_map_where_it_stands(self):
        results = self.results()
        results[Bot.GOOD]["second"] = [won({"frost": 80}), won({"frost": 80})]
        lines, passed = self.report(results)
        self.assertIn("ok  maps_used good frost: 2.00 in [2, 2]", lines)
        self.assertIn("OUT maps_used good arrow: 1.00 in [2, 2]", lines)
        self.assertFalse(passed)

    def test_mean_gold_is_the_mean_over_the_games_of_a_map(self):
        lines, _ = self.report(self.results())
        self.assertIn("ok  mean_gold good tutorial: 30.00 in [10, 100]", lines)

    def test_a_map_that_is_not_on_the_world_map_is_an_error(self):
        table = TABLE.replace("| tutorial   |", "| nowhere    |")
        with self.assertRaisesRegex(ValueError, "no map nowhere"):
            balancing.check(balancing.parse_corridors(table), WORLD, self.results())

    def test_win_rates_have_no_value_per_tower(self):
        table = TABLE.replace("| win_rate    | good  | each_map   |", "| win_rate    | good  | each_tower |")
        with self.assertRaisesRegex(ValueError, "win_rate has no value per tower"):
            balancing.check(balancing.parse_corridors(table), WORLD, self.results())

    def test_shares_need_towers_as_their_scope(self):
        table = TABLE.replace("| spend_share | good  | each_tower |", "| spend_share | good  | each_map   |")
        with self.assertRaisesRegex(ValueError, "needs the scope each_tower"):
            balancing.check(balancing.parse_corridors(table), WORLD, self.results())


if __name__ == "__main__":
    unittest.main()

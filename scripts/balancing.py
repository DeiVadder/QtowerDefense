"""Balancing runs (E-39): the bots play every map of the world with many seeds, and the results
must stay inside the corridors of docs/Balancing.md.

The corridors are the table between the corridor markers of that file. The script fails when a
value falls outside its corridor, when a row cannot be read, or when a game cannot be played.
"""

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

from qtd_common import REPO

CORRIDORS = REPO / "docs" / "Balancing.md"
CONTENT = REPO / "content"
BEGIN = "<!-- corridors begin -->"
END = "<!-- corridors end -->"
COLUMNS = 5
JOBS = 6
TIMEOUT_SECONDS = 600


class Metric(Enum):
    WIN_RATE = "win_rate"
    SPEND_SHARE = "spend_share"
    MAPS_USED = "maps_used"
    MEAN_GOLD = "mean_gold"


class Bot(Enum):
    NAIVE = "naive"
    GOOD = "good"


class Scope(Enum):
    EACH_MAP = "each_map"
    CAMPAIGN = "campaign"
    EACH_TOWER = "each_tower"
    MAP = "map"  # one map, named in the row


@dataclass(frozen=True)
class Corridor:
    metric: Metric
    bot: Bot
    scope: Scope
    map: str | None
    low: float
    high: float


@dataclass(frozen=True)
class Game:
    won: bool
    spent: dict
    mean_gold: float


@dataclass(frozen=True)
class World:
    maps: list  # the ids of the world map, in order
    campaign: list  # the maps that use the shared catalog
    towers: list  # the towers of the catalog


def parse_corridors(text):
    """The rows of the corridor table. Raises ValueError with the row that is wrong."""
    if BEGIN not in text or END not in text:
        raise ValueError("no corridor table between the markers")
    lines = text.split(BEGIN, 1)[1].split(END, 1)[0].strip().splitlines()
    corridors = []
    for line in lines[2:]:  # the header and its rule
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != COLUMNS:
            raise ValueError(f"a row needs {COLUMNS} cells: {line}")
        metric, bot, scope, low, high = cells
        named = scope in {item.value for item in Scope} and scope != Scope.MAP.value
        try:
            corridor = Corridor(
                Metric(metric), Bot(bot), Scope(scope) if named else Scope.MAP,
                None if named else scope, float(low), float(high),
            )  # fmt: skip
        except ValueError as error:
            raise ValueError(f"{error}: {line}") from None
        if corridor.low > corridor.high:
            raise ValueError(f"min is above max: {line}")
        corridors.append(corridor)
    return corridors


def read_world(content=CONTENT):
    world = json.loads((content / "world.json").read_text(encoding="utf-8"))
    maps = [Path(section["map"]).stem for section in world["sections"]]
    campaign = [
        name for name in maps
        if "catalog" in json.loads((content / f"{name}.json").read_text(encoding="utf-8"))
    ]  # fmt: skip
    catalog = json.loads((content / "catalog.json").read_text(encoding="utf-8"))
    return World(maps, campaign, [tower["id"] for tower in catalog["towers"]])


def play(program, name, bot, runs):
    """The games of one bot on one map, from the statistics of the headless runner."""
    command = [
        str(program), "--map", str(CONTENT / f"{name}.json"), "--bot", bot.value,
        "--runs", str(runs), "--statistics",
    ]  # fmt: skip
    result = subprocess.run(command, capture_output=True, text=True, timeout=TIMEOUT_SECONDS)
    if result.returncode != 0:
        problem = (result.stdout + result.stderr).strip()
        raise RuntimeError(f"{name} with {bot.value}: exit {result.returncode} {problem}")
    games = []
    for game in json.loads(result.stdout)["games"]:
        statistics = game["statistics"]
        if "error" in statistics:
            raise RuntimeError(f"{name} with {bot.value}: {statistics['error']}")
        curve = statistics["goldCurve"]
        if not curve:
            raise RuntimeError(f"{name} with {bot.value}: a game without a gold curve")
        games.append(Game(game["status"] == "won", statistics["spent"], sum(curve) / len(curve)))
    return games


def shares(world, games):
    """By tower, its part of the gold in standing towers over the campaign."""
    spent = defaultdict(float)
    for name in world.campaign:
        for game in games[name]:
            for tower, gold in game.spent.items():
                spent[tower] += gold
    total = sum(spent.values()) or 1
    return {tower: spent[tower] / total for tower in world.towers}


def maps_used(world, games):
    return {
        tower: sum(any(game.spent.get(tower, 0) > 0 for game in games[name])
                   for name in world.campaign)
        for tower in world.towers
    }  # fmt: skip


def win_rate(games):
    return sum(game.won for game in games) / len(games)


def values(corridor, world, results):
    """The measured values a corridor speaks about, as (what, value) pairs."""
    games = results[corridor.bot]
    if corridor.metric in (Metric.SPEND_SHARE, Metric.MAPS_USED):
        if corridor.scope != Scope.EACH_TOWER:
            raise ValueError(f"{corridor.metric.value} needs the scope each_tower")
        measure = shares if corridor.metric == Metric.SPEND_SHARE else maps_used
        return list(measure(world, games).items())
    if corridor.scope == Scope.EACH_TOWER:
        raise ValueError(f"{corridor.metric.value} has no value per tower")
    if corridor.scope == Scope.EACH_MAP:
        names = world.maps
    elif corridor.scope == Scope.CAMPAIGN:
        names = world.campaign
    elif corridor.map in world.maps:
        names = [corridor.map]
    else:
        raise ValueError(f"no map {corridor.map} on the world map")
    if corridor.metric == Metric.WIN_RATE and corridor.scope == Scope.CAMPAIGN:
        return [("campaign", win_rate([game for name in names for game in games[name]]))]
    if corridor.metric == Metric.WIN_RATE:
        return [(name, win_rate(games[name])) for name in names]
    return [(name, sum(game.mean_gold for game in games[name]) / len(games[name]))
            for name in names]  # fmt: skip


def check(corridors, world, results):
    """The lines of the report and whether every value stays inside its corridor."""
    lines = []
    passed = True
    for corridor in corridors:
        for what, value in values(corridor, world, results):
            inside = corridor.low <= value <= corridor.high
            passed &= inside
            lines.append(
                f"{'ok ' if inside else 'OUT'} {corridor.metric.value} {corridor.bot.value} "
                f"{what}: {value:.2f} in [{corridor.low:g}, {corridor.high:g}]"
            )
    return lines, passed


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", default="build/release", help="build folder with qtd_headless")
    parser.add_argument("--runs", type=int, default=50, help="games per bot and map")
    options = parser.parse_args()
    if options.runs < 1:
        parser.error("--runs must be at least 1")
    program = REPO / options.build / "bin" / "qtd_headless.exe"
    try:
        corridors = parse_corridors(CORRIDORS.read_text(encoding="utf-8"))
    except ValueError as error:
        print(f"docs/Balancing.md: {error}")
        return 1
    world = read_world()
    bots = sorted({corridor.bot for corridor in corridors}, key=lambda bot: bot.value)
    jobs = [(bot, name) for bot in bots for name in world.maps]
    try:
        with ThreadPoolExecutor(JOBS) as pool:
            played = list(pool.map(lambda job: play(program, job[1], job[0], options.runs), jobs))
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"balancing: a game could not be played: {error}")
        return 1
    results = defaultdict(dict)
    for (bot, name), games in zip(jobs, played):
        results[bot][name] = games
    try:
        lines, passed = check(corridors, world, results)
    except ValueError as error:
        print(f"docs/Balancing.md: {error}")
        return 1
    print("\n".join(lines))
    print("balancing: " + ("inside the corridors" if passed else "OUTSIDE a corridor"))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

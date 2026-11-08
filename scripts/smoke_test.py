"""Plays the real game through its control interface, with GPU rendering, and checks what comes out.

The game runs in ticks. Real time only serves the frame rate of the standard scene and the check
that the interface is off by default. The screenshots land in build/smoke for a look at the end of a
stage, no pixel is compared. Run from an environment with Qt on the PATH, see scripts/env.cmd.
"""

import json
import os
import subprocess
import sys
import threading
import time

from qtd_common import REPO

BIN = REPO / "build" / "release" / "bin"
OUT = REPO / "build" / "smoke"
CONTENT = REPO / "content"
GOLDEN = REPO / "tests" / "golden"
WINDOW = BIN / "QtowerDefense.exe"
HEADLESS = BIN / "qtd_headless.exe"
TIMEOUT_SECONDS = 60
FRAME_SECONDS = 5
SILENCE_SECONDS = 2
# An empty scene with the HUD shows 6 colours, the map at least 18.
MIN_COLORS = 12
SHOT_WAIT_SECONDS = 0.2
SHOT_TRIES = int(10 / SHOT_WAIT_SECONDS)
# Section 4 of the assignment: 60 frames per second with 200 enemies.
MIN_FRAMES_PER_SECOND = 60
# What a frame may cost, on the CPU and on the GPU: room for 400 frames per second. The frame rate
# stops at a pace of its own and says nothing above it.
MAX_FRAME_MICROSECONDS = 2_500
BENCHMARK_ENEMIES = 200
TRAINING_GROUND = CONTENT / "training_ground.json"

# The same game for the window and for qtd_headless, with the screenshot after each step.
GAME = [
    ("start.png", {"action": "startMap", "map": str(TRAINING_GROUND), "seed": "7"}),
    (None, {"action": "buildTower", "x": 4, "y": 3, "towerType": 1}),
    ("wave.png", {"action": "advance", "ticks": 200}),
    ("end.png", {"action": "advance", "ticks": 2000}),
]


class Game:
    """A program under control, one request and one answer at a time."""

    def __init__(self, program, log_name):
        self.log_path = OUT / log_name
        self.log = self.log_path.open("w", encoding="utf-8")
        # Without this, Qt sends the messages of a window program to the debugger. Without vsync,
        # the frame rate says something about the game and not about the display.
        # QSG_RHI_PROFILE gives the timestamps for the GPU time of a frame.
        environment = dict(
            os.environ, QT_FORCE_STDERR_LOGGING="1", QSG_NO_VSYNC="1", QSG_RHI_PROFILE="1"
        )
        self.process = subprocess.Popen(
            [program, "--control"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self.log,
            env=environment,
            text=True,
        )
        self.watchdog = threading.Timer(TIMEOUT_SECONDS, self.process.kill)
        self.watchdog.daemon = True
        self.watchdog.start()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.watchdog.cancel()
        self.process.kill()
        self.process.wait()
        self.log.close()

    def ask(self, action, **fields):
        answer = self.try_ask(action, **fields)
        if not answer["ok"]:
            raise SystemExit(f"{action}: {answer['error']}")
        return answer

    def try_ask(self, action, **fields):
        self.process.stdin.write(json.dumps({"action": action, **fields}) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise SystemExit(f"{action}: no answer, see {self.log_path}")
        return json.loads(line)

    def build(self, x, y, tower_type):
        answer = self.ask("buildTower", x=x, y=y, towerType=tower_type)
        if answer["result"] != "accepted":
            raise SystemExit(f"tower at {x}, {y}: {answer['result']}")

    def wait_for_end(self):
        code = self.process.wait()
        if code != 0:
            raise SystemExit(f"{self.log_path.stem} ended with {code}")


def screenshot(game, name):
    path = OUT / name
    # The window refuses pictures until it is on the screen (E-43).
    for _ in range(SHOT_TRIES):
        answer = game.try_ask("screenshot", file=str(path))
        if answer["ok"] or "not on the screen" not in answer["error"]:
            break
        time.sleep(SHOT_WAIT_SECONDS)
    if not answer["ok"]:
        raise SystemExit(f"screenshot: {answer['error']}")
    picture = answer["screenshot"]
    if picture["colors"] < MIN_COLORS or path.stat().st_size == 0:
        raise SystemExit(f"{name}: empty or of few colours: {picture}")
    print(f"{name}: {picture}")
    return path.read_bytes()


def play_in_window(game):
    """Plays GAME with screenshots. Returns the hash after each step."""
    hashes = []
    pictures = []
    for name, request in GAME:
        fields = {key: value for key, value in request.items() if key != "action"}
        answer = game.ask(request["action"], **fields)
        hashes.append(answer["hash"])
        if name:
            pictures.append(screenshot(game, name))
    if answer["status"] == "running" or len(answer["towers"]) != 1:
        raise SystemExit(f"not the game it should be: {answer}")
    if len(set(pictures)) != len(pictures):
        raise SystemExit("the picture stays the same while the game goes on")
    replay = OUT / "game.replay.json"
    game.ask("saveReplay", file=str(replay))
    return hashes, replay


def check_same_game_without_window(hashes, replay):
    """Same requests, same game, from the outside: the window must not bring anything of its own."""
    with Game(HEADLESS, "headless.log") as game:
        for (_, request), expected in zip(GAME, hashes):
            fields = {key: value for key, value in request.items() if key != "action"}
            answer = game.ask(request["action"], **fields)
            if answer["hash"] != expected:
                raise SystemExit(f"{request}: {answer['hash']} without window, {expected} with it")
    report = subprocess.run(
        [HEADLESS, "--map", TRAINING_GROUND, "--replay", replay],
        stdout=subprocess.PIPE,
        text=True,
    ).stdout
    if f'"hash":"{hashes[-1]}"' not in report or '"matches":true' not in report:
        raise SystemExit(f"the replay differs: {report}, expected {hashes[-1]}")


def frame_cost(before, after, key):
    """The mean of one cost per frame, in microseconds, between two answers."""
    frames = after["frameCosts"]["frames"] - before["frameCosts"]["frames"]
    return round((after["frameCosts"][key] - before["frameCosts"][key]) / frames, 1)


def measure_frames(game):
    """Real time on purpose: the frame rate of the standard scene with all its enemies."""
    golden = json.loads((GOLDEN / "benchmark_arena.replay.json").read_text(encoding="utf-8"))
    answer = game.ask("startMap", map=str(CONTENT / "benchmark_arena.json"), seed="7")
    while len(answer["enemies"]) < BENCHMARK_ENEMIES:
        answer = game.ask("advance", ticks=30)
        if answer["status"] != "running":
            raise SystemExit("the wave ended before all enemies were out")
    for command in golden["commands"]:
        game.build(command["x"], command["y"], command["towerType"])
    before = game.ask("resume")
    started = time.monotonic()
    time.sleep(FRAME_SECONDS)
    after = game.ask("pause")
    seconds = time.monotonic() - started
    screenshot(game, "benchmark.png")
    frames_per_second = round((after["frames"] - before["frames"]) / seconds, 1)
    if frames_per_second < MIN_FRAMES_PER_SECOND:
        raise SystemExit(f"{frames_per_second} frames per second")
    if after["tick"] <= before["tick"]:
        raise SystemExit("the game stood still during the measurement")
    cpu = frame_cost(before, after, "cpuMicroseconds")
    gpu = frame_cost(before, after, "gpuMicroseconds")
    if gpu <= 0:
        raise SystemExit("no GPU time: the backend gave no timestamps")
    if max(cpu, gpu) > MAX_FRAME_MICROSECONDS:
        raise SystemExit(f"a frame costs {cpu} on the CPU and {gpu} on the GPU, in microseconds")
    return {
        "framesPerSecond": frames_per_second,
        "cpuMicroseconds": cpu,
        "gpuMicroseconds": gpu,
        "ticks": after["tick"] - before["tick"],
        "enemiesAtStart": len(before["enemies"]),
        "enemiesAtEnd": len(after["enemies"]),
        "towers": len(after["towers"]),
    }


def check_end_of_input():
    """The end of the input ends the program, without a word."""
    with Game(WINDOW, "end_of_input.log") as game:
        game.ask("startMap", map=str(TRAINING_GROUND), seed="1")
        game.process.stdin.close()
        game.wait_for_end()
    check_log(game)


def check_off_by_default():
    """Real time on purpose: nothing may answer, and silence takes a moment to show."""
    process = subprocess.Popen(
        [WINDOW], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
    )
    process.stdin.write('{"action":"observe"}\n')
    process.stdin.flush()
    time.sleep(SILENCE_SECONDS)
    ran_on = process.poll() is None
    process.kill()
    if not ran_on or process.stdout.read():
        raise SystemExit("without --control, the game must run on and say nothing")


def check_log(game):
    log = game.log_path.read_text(encoding="utf-8").strip()
    if log:
        raise SystemExit(f"{game.log_path.name} is not empty:\n{log}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with Game(WINDOW, "game.log") as game:
        hashes, replay = play_in_window(game)
        baseline = measure_frames(game)
        game.ask("quit")  # with the input still open, as when a window closes
        game.wait_for_end()
    check_log(game)
    check_same_game_without_window(hashes, replay)
    check_end_of_input()
    check_off_by_default()
    (OUT / "report.json").write_text(json.dumps(baseline) + "\n", encoding="utf-8")
    print(f"smoke test passed: {baseline}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

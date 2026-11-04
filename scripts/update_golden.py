"""Rewrites the checkpoints of all golden replays in tests/golden from the current rules.

Run this only for an intended rule change, in a commit of its own that gives the reason. A golden
file needs no more than content, seed and commands to start with. Run from an environment with Qt on
the PATH, see scripts/env.cmd.
"""

import json
import subprocess
import sys

from qtd_common import REPO

GOLDEN = REPO / "tests" / "golden"
HEADLESS = REPO / "build" / "debug" / "bin" / "qtd_headless.exe"


def rewrite(path):
    golden = json.loads(path.read_text(encoding="utf-8"))
    command = [
        HEADLESS,
        "--map", REPO / "content" / golden["content"],
        "--seed", golden["seed"],
        "--script", path,
        "--record", path,
    ]  # fmt: skip
    report = subprocess.run(
        list(map(str, command)), stdout=subprocess.PIPE, text=True, check=True
    ).stdout
    print(f"{path.name}: {report.strip()}")


def main():
    files = sorted(GOLDEN.glob("*.replay.json"))
    if not files:
        print(f"no golden replays in {GOLDEN}")
        return 1
    for path in files:
        rewrite(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())

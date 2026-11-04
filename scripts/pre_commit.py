"""The fast part of verify for the git hook: format of staged files and the source rules."""

import subprocess
import sys

from qtd_common import REPO, GitError, git, is_cpp

SCRIPTS = ["check_length.py", "check_layers.py", "check_determinism.py"]


def main():
    try:
        staged = git("diff", "--cached", "--name-only", "--diff-filter=ACMR")
    except GitError as error:
        print(f"pre-commit: {error}, nothing committed.")
        return 1
    formatted = [str(REPO / n) for n in staged if is_cpp(n) or n.endswith(".qml")]
    commands = [[f"{REPO}/scripts/{script}"] for script in SCRIPTS]
    commands.append([f"{REPO}/scripts/check_types.py", "--staged"])
    if formatted:
        commands.append([f"{REPO}/scripts/check_format.py", *formatted])
    failed = False
    for command in commands:
        failed = subprocess.run([sys.executable, *command]).returncode != 0 or failed
    if failed:
        print("pre-commit: checks failed, nothing committed.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

"""Format check: clang-format and qmlformat. Without arguments it checks the whole project."""

import subprocess
import sys
from pathlib import Path

from qtd_common import CPP_SUFFIXES, LLVM_DIR, QT_DIR, REPO, is_cpp, project_files, relative


def check_cpp(paths):
    if not paths:
        return 0
    command = [str(LLVM_DIR / "clang-format.exe"), "--dry-run", "-Werror", *map(str, paths)]
    return subprocess.run(command, cwd=REPO).returncode


def check_qml(paths):
    failures = 0
    for path in paths:
        command = [str(QT_DIR / "bin" / "qmlformat.exe"), str(path)]
        result = subprocess.run(command, cwd=REPO, capture_output=True, text=True, encoding="utf-8")
        if result.returncode != 0 or result.stdout != path.read_text(encoding="utf-8"):
            print(f"{relative(path)}: not formatted (qmlformat -i){result.stderr}")
            failures += 1
    return failures


def main(arguments):
    paths = [Path(a).resolve() for a in arguments] or project_files(*CPP_SUFFIXES, ".qml")
    cpp = [p for p in paths if is_cpp(p)]
    qml = [p for p in paths if p.suffix == ".qml"]
    failed = check_cpp(cpp) != 0
    failed = check_qml(qml) != 0 or failed
    print(f"check_format: {len(cpp)} C++ and {len(qml)} QML files, {'FAILED' if failed else 'ok'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

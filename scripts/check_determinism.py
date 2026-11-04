"""Forbids everything in src/core that would break bit-identical simulation runs.

The brief names float, double, QHash, QRandomGenerator, QDateTime and std::chrono; the rest of the
list follows from its rules on randomness, iteration order, wall clock and threads.
"""

import re
import sys

from qtd_common import (
    CPP_SUFFIXES,
    PP_NUMBER,
    REPO,
    project_files,
    relative,
    strip_comments_and_strings,
)

FORBIDDEN = {
    r"\b(float|double|qreal|qfloat16)\b": "floating point",
    r"\b(QHash|QMultiHash|QSet)\b": "unstable iteration order",
    r"\bunordered_\w+": "unstable iteration order",
    r"\bQRandomGenerator\w*\b": "outside randomness",
    r"\b(s?rand|random_device|mt19937\w*|\w+_distribution)\b": "outside randomness",
    r"\brandom\b(?=>)": "outside randomness",
    r"\b(QDateTime|QDate|QTime|QTimer|QElapsedTimer|QDeadlineTimer)\b": "wall clock",
    r"\bchrono\b": "wall clock",
    r"\b(time|clock)\s*\(": "wall clock",
    r"\b(QThread\w*|QtConcurrent|QFuture|thread|jthread|async)\b(?!_)": "threads",
}
PATTERNS = [(re.compile(pattern), reason) for pattern, reason in FORBIDDEN.items()]
NUMBER = re.compile(PP_NUMBER)
INCLUDE_LINE = re.compile(r"\s*#\s*include\b")
DECIMAL_EXPONENT = re.compile(r"\d+e[+-]?\d")


def is_float_literal(number):
    """Tells a floating point literal from an integer, for a token matched by NUMBER."""
    digits = number.replace("'", "").split("_")[0].lower()
    if digits.startswith("0x"):
        return "." in digits or "p" in digits
    if digits.startswith("0b"):
        return False
    return "." in digits or DECIMAL_EXPONENT.match(digits) is not None


def findings(text):
    """Yields (line number, matched text, reason) in C++ source text."""
    stripped = strip_comments_and_strings(text)
    for number, line in enumerate(stripped.split("\n"), start=1):
        for pattern, reason in PATTERNS:
            for match in pattern.finditer(line):
                yield number, match.group(0), reason
        if not INCLUDE_LINE.match(line):  # paths such as <QtCore/6.12.0/...> hold no numbers
            for match in NUMBER.finditer(line):
                if is_float_literal(match.group(0)):
                    yield number, match.group(0), "floating point"


def main():
    count = 0
    core = REPO / "src" / "core"
    for path in project_files(*CPP_SUFFIXES):
        if core in path.parents:
            for number, token, reason in findings(path.read_text(encoding="utf-8")):
                print(f"{relative(path)}:{number}: {token} is forbidden in core ({reason})")
                count += 1
    print(f"check_determinism: {count} problems")
    return 1 if count else 0


if __name__ == "__main__":
    sys.exit(main())

"""Limits: 60 code lines per function, 600 lines per file. Exceptions live in the baseline.

A function counts its non-empty lines from the opening to the closing brace, without comment-only
lines. Baseline lines read `path;function or <file>;lines;reason`. The baseline only shrinks: an
entry fails when it is stale, higher than the real length, or new compared to the branch dev.
"""

import re
import sys

from qtd_common import (
    CPP_SUFFIXES,
    REPO,
    git,
    is_cpp,
    project_files,
    relative,
    strip_comments_and_strings,
)

MAX_FUNCTION_LINES = 60
MAX_FILE_LINES = 600
BASELINE_NAME = "scripts/length_baseline.txt"
BASELINE = REPO / BASELINE_NAME
FILE_ENTRY = "<file>"
SCOPE_HEADER = re.compile(r"\b(namespace|class|struct|enum|union|extern)\b")


def matching_brace(text, start):
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    return len(text) - 1


def header_before(text, position):
    start = max(text.rfind(c, 0, position) for c in ";{}") + 1
    return text[start:position]


def function_name(header):
    match = re.search(r"([~\w:]+(?:\s*(?:operator\s*\S+))?)\s*\(", header)
    return match.group(1) if match else "<anonymous>"


def body_end(text, opening):
    """End of a function body, stepping over brace initializers of a constructor."""
    closing = matching_brace(text, opening)
    while True:
        rest = text[closing + 1 :].lstrip()
        if not rest.startswith((",", "{")):
            return closing
        following = text.find("{", closing + 1)
        if following < 0:
            return closing
        closing = matching_brace(text, following)


def code_lines(text, first, last):
    return sum(1 for line in text[first : last + 1].split("\n") if line.strip())


def cpp_functions(text):
    """Yields (name, code lines) of every function body in stripped C++ text."""
    index = 0
    while (index := text.find("{", index)) >= 0:
        header = header_before(text, index)
        if SCOPE_HEADER.search(header) and "(" not in header.split("{")[0]:
            index += 1  # step into the scope
            continue
        if "(" in header:
            end = body_end(text, index)
            yield function_name(header), code_lines(text, index, end)
            index = end + 1
        else:
            index = matching_brace(text, index) + 1


def qml_functions(text):
    for match in re.finditer(r"\bfunction\s+(\w+)\s*\([^{]*\{", text):
        opening = match.end() - 1
        yield match.group(1), code_lines(text, opening, matching_brace(text, opening))


def measure(path):
    """Yields (entry name, lines, limit) for everything in one file."""
    raw = path.read_text(encoding="utf-8")
    yield FILE_ENTRY, raw.count("\n") + 1, MAX_FILE_LINES
    if is_cpp(path) or path.suffix == ".qml":
        stripped = strip_comments_and_strings(raw)
        finder = qml_functions if path.suffix == ".qml" else cpp_functions
        for name, lines in finder(stripped):
            yield name, lines, MAX_FUNCTION_LINES


def parse_baseline(lines):
    entries = {}
    for line in lines:
        line = line.strip()
        if line and not line.startswith("#"):
            path, name, length, reason = (line.split(";", 3) + ["", "", ""])[:4]
            entries[(path, name)] = (int(length), reason.strip())
    return entries


def compare(measured, baseline, previous):
    """measured: {(path, name): (lines, limit)}. Returns the list of problems."""
    problems = []
    for key, (lines, limit) in measured.items():
        if lines > limit and key not in baseline:
            problems.append(f"{key[0]}: {key[1]} has {lines} lines, limit {limit}")
        elif lines > limit and lines > baseline[key][0]:
            problems.append(f"{key[0]}: {key[1]} grew to {lines}, baseline {baseline[key][0]}")
    for key, (length, reason) in baseline.items():
        lines, limit = measured.get(key, (0, 0))
        if not reason:
            problems.append(f"baseline {key}: reason missing")
        if lines <= limit:
            problems.append(f"baseline {key}: stale, remove the entry")
        elif length > lines:
            problems.append(f"baseline {key}: lower the entry to {lines}")
        if previous is not None and (key not in previous or length > previous[key][0]):
            problems.append(f"baseline {key}: new or higher than on dev; it may only shrink")
    return problems


def baseline_on_dev():
    """The baseline of dev, None while dev has none. A missing dev branch raises GitError."""
    name = "scripts/length_baseline.txt"
    if not git("ls-tree", "--name-only", "dev", "--", name):
        return None
    return parse_baseline(git("show", f"dev:{name}"))


def main():
    measured = {}
    for path in project_files(*CPP_SUFFIXES, ".qml", ".py"):
        for name, lines, limit in measure(path):
            key = (relative(path), name)
            if key not in measured or lines > measured[key][0]:
                measured[key] = (lines, limit)
    baseline = parse_baseline(BASELINE.read_text(encoding="utf-8").splitlines())
    problems = compare(measured, baseline, baseline_on_dev())
    for problem in problems:
        print(problem)
    print(f"check_length: {len(measured)} items, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

"""Keeps docs/Typen.md, the index of all types in src, up to date and finds doubled type names.

Sessions read the index before they add a type, so that nobody defines the same thing twice. It
only sees names: the same idea under another name is a question for the reviewer. Without
arguments this checks, with --write it writes the index anew. It reads line by line and follows
braces, so #ifdef branches with unequal braces would confuse it; the project has none.
"""

import argparse
import collections
import re
import subprocess
import sys
from dataclasses import dataclass

from qtd_common import (
    CPP_SUFFIXES,
    REPO,
    git,
    is_cpp,
    project_files,
    relative,
    strip_comments_and_strings,
)

INDEX = REPO / "docs" / "Typen.md"
# A definition only: forward declarations end in a semicolon and fall through.
DECLARATION = re.compile(
    r"^\s*(?:template\s*<.*>\s*)?"
    r"(enum\s+class|enum\s+struct|enum|class|struct|union)\s+"
    r"(?:(?:\[\[[^\]]*\]\]|alignas\([^)]*\))\s*)*"  # attributes
    r"(?:[A-Z][A-Z0-9_]*\s+)*"  # export macros
    r"(?!final\b)((?:\w+::)*\w+)(?:\s+final)?\s*(?::[^;{]*)?(?:\{.*)?$"
)
ALIAS = re.compile(r"^\s*(?:template\s*<.*>\s*)?using\s+(\w+)\s*=|^\s*typedef\s.*\b(\w+)\s*;")
NAMESPACE = re.compile(r"^\s*(?:inline\s+)?namespace\b")
DOC_LINE = re.compile(r"^\s*///\s?(.*)$")
TEMPLATE_LINE = re.compile(r"^\s*template\s*<")
FIRST_SENTENCE = re.compile(r"^(.*?\.)(?:\s|$)")
HEADER = """# Typen

Erzeugt von `scripts/check_types.py --write`, nicht von Hand ändern. Vor jedem neuen Typ hier und mit
der LSP-Symbolsuche nachsehen, ob es ihn schon gibt.
"""


@dataclass(frozen=True)
class Type:
    name: str
    kind: str
    path: str
    purpose: str


def purpose_above(lines, index):
    """The first sentence of the /// comment above a line, past a template line."""
    index -= 1
    if index >= 0 and TEMPLATE_LINE.match(lines[index]):
        index -= 1
    comment = []
    while index >= 0 and (match := DOC_LINE.match(lines[index])):
        comment.insert(0, match.group(1))
        index -= 1
    text = " ".join(comment).strip()
    sentence = FIRST_SENTENCE.match(text)
    return (sentence.group(1) if sentence else text).replace("|", "\\|")


def scope_names(scopes):
    """The enclosing types, or None inside a function or anything else that is no type."""
    if any(kind == "other" for kind, _ in scopes):
        return None
    return [name for kind, name in scopes if kind == "type"]


def declared(line):
    """(name, kind) if the line declares a type or an alias."""
    if match := DECLARATION.match(line):
        return match.group(2), " ".join(match.group(1).split())
    if match := ALIAS.match(line):
        return match.group(1) or match.group(2), "using"
    return None


def find_types(text, path):
    """All types and aliases outside of functions. Nested types carry their parents in the name."""
    lines = text.splitlines()
    scopes = []  # (kind, name) of every open brace
    opening = ("other", "")  # what the next brace opens
    types = []
    for index, line in enumerate(strip_comments_and_strings(text).splitlines()):
        found = declared(line)
        parents = scope_names(scopes)
        if found and parents is not None:
            name = "::".join([*parents, found[0]])
            types.append(Type(name, found[1], path, purpose_above(lines, index)))
            if found[1] != "using":
                opening = ("type", found[0])
        elif NAMESPACE.match(line):
            opening = ("namespace", "")
        for character in line:
            if character == "{":
                scopes.append(opening)
                opening = ("other", "")
            elif character == "}" and scopes:
                scopes.pop()
    return types


def doubled_names(types):
    """The problems for names that stand for more than one type."""
    places = collections.defaultdict(list)
    for found in types:
        places[found.name].append(found.path)
    return [
        f"{name}: declared more than once, in {', '.join(paths)}"
        for name, paths in sorted(places.items())
        if len(paths) > 1
    ]


def render(types):
    layers = collections.defaultdict(list)
    for found in types:
        layers[found.path.split("/")[1]].append(found)
    parts = [HEADER]
    for layer in sorted(layers):
        parts.append(f"\n## {layer}\n\n| Typ | Art | Datei | Zweck |\n|---|---|---|---|\n")
        for found in sorted(layers[layer], key=lambda t: (t.path, t.name)):
            file = found.path.removeprefix(f"src/{layer}/")
            parts.append(f"| `{found.name}` | {found.kind} | `{file}` | {found.purpose} |\n")
    return "".join(parts)


def read_index():
    return INDEX.read_text(encoding="utf-8") if INDEX.exists() else ""


def project_types():
    """The types of the working tree, untracked files included."""
    types = []
    for path in project_files(*CPP_SUFFIXES):
        name = relative(path).replace("\\", "/")
        if name.startswith("src/"):
            types += find_types(path.read_text(encoding="utf-8"), name)
    return types


def staged_text(name):
    return subprocess.run(
        ["git", "-C", str(REPO), "show", f":{name}"],
        capture_output=True, encoding="utf-8", check=False,
    ).stdout  # fmt: skip


def staged_types():
    """The types of what the commit is going to hold."""
    types = []
    for name in git("ls-files", "--cached", "src"):
        if is_cpp(name):
            types += find_types(staged_text(name), name)
    return types


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="write the index anew")
    parser.add_argument("--staged", action="store_true", help="check the git index, for the hook")
    options = parser.parse_args()

    types = staged_types() if options.staged else project_types()
    problems = doubled_names(types)
    expected = render(types)
    index_name = relative(INDEX).replace("\\", "/")
    if options.write:
        INDEX.write_text(expected, encoding="utf-8", newline="\n")
    else:
        current = staged_text(index_name) if options.staged else read_index()
        if current != expected:
            problems.append(f"{index_name} is out of date, run check_types.py --write and add it")
    for problem in problems:
        print(problem)
    print(f"check_types: {len(types)} types, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

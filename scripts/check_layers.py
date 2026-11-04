"""Checks the allowed dependencies between the layers: includes and CMake link lines.

Order: core <- automation <- client <- programs. qtd_headless stays free of Gui.
"""

import posixpath
import re
import sys

from qtd_common import CPP_SUFFIXES, QT_DIR, REPO, project_files, relative

ANY = None
GUI_FREE_QT = {"QtCore", "QtNetwork"}
GUI_FREE_LINKS = {"Qt6::Core", "Qt6::Network"}

# layer: (allowed Qt modules, allowed project include roots, allowed link names)
LAYERS = {
    "core": ({"QtCore"}, {"core"}, {"Qt6::Core"}),
    "automation": (GUI_FREE_QT, {"core", "automation"}, GUI_FREE_LINKS | {"qtd_core"}),
    "client": (ANY, {"core", "automation", "client"}, ANY),
    "app": (ANY, {"core", "automation", "client", "app"}, ANY),
    "headless": (
        GUI_FREE_QT,
        {"core", "automation", "headless"},
        GUI_FREE_LINKS | {"qtd_core", "qtd_automation"},
    ),
}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.MULTILINE)
LINK_BLOCK = re.compile(r"target_link_libraries\s*\(([^)]*)\)", re.IGNORECASE)  # as in CMake
LINK_NAME = re.compile(r"\bQt6::\w+|\bqtd_\w+")


def qt_header_index():
    """Maps every public Qt header name to its module, for includes such as <QString>."""
    index = {}
    for module in (QT_DIR / "include").iterdir():
        if module.is_dir():
            for header in module.iterdir():
                index.setdefault(header.name, module.name)
    return index


def qt_module(include, index):
    first = include.split("/")[0]
    if "/" in include and first.startswith("Qt"):
        return first
    return index.get(include)


def reached_layers(path, directory):
    """The first folders below src that a normalized include reaches, in lower case.

    A quoted include resolves against the including file, then against the include root src.
    """
    beside = posixpath.normpath(f"{directory}/{path}").split("/")
    roots = [path.split("/")[0]] if "/" in path else []
    if beside[0] == "src" and len(beside) > 2:
        roots.append(beside[1])
    return {root.lower() for root in roots}


def include_problems(layer, include, index, directory=None):
    """directory: the folder of the including file, relative to the repository."""
    qt_allowed, project_allowed, _ = LAYERS[layer]
    path = posixpath.normpath(include.replace("\\", "/"))
    module = qt_module(path, index)
    if module is not None:
        if qt_allowed is not ANY and module not in qt_allowed:
            return f"layer {layer} must not use {module} (<{include}>)"
        return None
    reached = reached_layers(path, directory or f"src/{layer}")
    if any(root in LAYERS and root not in project_allowed for root in reached):
        return f"layer {layer} must not include {include}"
    return None


def link_problems(layer, cmake_text):
    allowed = LAYERS[layer][2]
    if allowed is ANY:
        return []
    names = set()
    for block in LINK_BLOCK.findall(cmake_text):
        names.update(LINK_NAME.findall(block.split(maxsplit=1)[-1]))  # first word: the target
    return [f"layer {layer} must not link {name}" for name in sorted(names - allowed)]


def main():
    index = qt_header_index()
    problems = []
    for path in project_files(*CPP_SUFFIXES):
        parts = relative(path).split("/")
        if parts[0] != "src" or parts[1] not in LAYERS:
            continue
        for include in INCLUDE.findall(path.read_text(encoding="utf-8")):
            problem = include_problems(parts[1], include, index, "/".join(parts[:-1]))
            if problem:
                problems.append(f"{relative(path)}: {problem}")
    for layer in LAYERS:
        cmake = REPO / "src" / layer / "CMakeLists.txt"
        text = cmake.read_text(encoding="utf-8")
        problems += [f"{relative(cmake)}: {p}" for p in link_problems(layer, text)]
    for problem in problems:
        print(problem)
    print(f"check_layers: {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

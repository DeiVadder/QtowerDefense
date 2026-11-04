"""Paths and helpers shared by the scripts. Works without scripts/env.cmd (the git hook has none)."""

import os
import re
import subprocess
import sys
from pathlib import Path

# A redirected stream uses the ANSI code page here. An unprintable character in a finding must not
# abort a check and hide the finding.
for _stream in (sys.stdout, sys.stderr):
    if hasattr(_stream, "reconfigure"):
        _stream.reconfigure(errors="backslashreplace")

REPO = Path(__file__).resolve().parent.parent
QT_DIR = Path(os.environ.get("QTD_QT_DIR", r"C:\Dev\Qt\6.12.0\msvc2022_64"))
LLVM_DIR = Path(os.environ.get("QTD_LLVM_DIR", r"C:\Dev\VS2022BuildTools\VC\Tools\Llvm\x64\bin"))
CPPCHECK = Path(os.environ.get("QTD_CPPCHECK", r"C:\Dev\Strawberry\c\bin\cppcheck.exe"))
OPENCPPCOVERAGE = Path(
    os.environ.get("QTD_OPENCPPCOVERAGE", r"C:\dev\OpenCppCoverage\OpenCppCoverage.exe")
)

CPP_SOURCE_SUFFIXES = {".cpp", ".cc", ".cxx"}
CPP_HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inl"}
CPP_SUFFIXES = CPP_SOURCE_SUFFIXES | CPP_HEADER_SUFFIXES

# A preprocessing number of the C++ grammar, with its digit separators and exponent signs.
PP_NUMBER = r"(?<![\w.])\.?\d(?:[eEpP][+-]|'?[\w.])*"


class GitError(RuntimeError):
    """git failed: the file lists are unknown, so no check may pass."""


def is_cpp(path):
    return Path(path).suffix.lower() in CPP_SUFFIXES


def git(*args):
    """Runs git in the repository and returns its stdout lines. Raises GitError if it fails."""
    result = subprocess.run(
        ["git", "-c", "core.quotePath=off", "-C", str(REPO), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0:
        raise GitError(f"git {' '.join(args)}: {result.stderr.strip()}")
    return result.stdout.splitlines()


def project_files(*suffixes):
    """All tracked and untracked (not ignored) files with one of the suffixes, as absolute paths."""
    names = git("ls-files", "--cached", "--others", "--exclude-standard")
    paths = (REPO / name for name in names)
    return sorted(p for p in paths if p.suffix.lower() in suffixes and p.exists())


def relative(path):
    return Path(path).resolve().relative_to(REPO).as_posix()


_TOKEN = re.compile(
    rf"""
    //(?:\\\n|[^\n])*                         # line comment, a final backslash continues it
    | /\*.*?\*/                               # block comment
    | R"(?P<delim>[^(\s]*)\(.*?\)(?P=delim)"  # raw string
    | "(?:\\.|[^"\\\n])*"                     # string
    | '(?:\\.|[^'\\\n])*'                     # character
    | (?P<number>{PP_NUMBER})                 # kept, its digit separators open no character
    """,
    re.DOTALL | re.VERBOSE,
)


def strip_comments_and_strings(text):
    """Blanks comments and literals but keeps every newline, so line numbers stay valid."""

    def blank(match):
        if match.group("number"):
            return match.group(0)
        return re.sub(r"[^\n]", " ", match.group(0))

    return _TOKEN.sub(blank, text)

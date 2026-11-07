"""The project gate. --quick is the everyday run, without arguments everything runs.

Every stage ends as PASS, FAIL or MISSING. MISSING names checks of the brief whose subject does not
exist yet: it never fails the run but it is reported, so that nothing passes silently.
"""

import argparse
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from qtd_common import (
    CPP_SOURCE_SUFFIXES,
    CPPCHECK,
    LLVM_DIR,
    QT_DIR,
    REPO,
    git,
    is_cpp,
    project_files,
    relative,
)

LOGS = REPO / "build" / "verify-logs"
SCRIPTS = REPO / "scripts"
PYTHON = sys.executable
TIDY_JOBS = 6
FUZZ_MAPS = ("training_ground.json", "mire_siege.json", "benchmark_arena.json")
FUZZ_RUNS = 40
FUZZ_TIMEOUT_SECONDS = 300
BALANCING_RUNS = 50
FIRST_WINDOWS_EXCEPTION = 0xC0000000
TIMED_OUT = -1
TIDY_TIMEOUT_SECONDS = 300


def execute(log, *command, cwd=REPO, timeout=None):
    """Runs a command, appends its output to the log and returns its exit code or TIMED_OUT."""
    log.write(f"\n$ {' '.join(map(str, command))}\n")
    log.flush()
    arguments = list(map(str, command))
    try:
        result = subprocess.run(
            arguments, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        log.write(f"timed out after {timeout} s: {command[-1]}\n")
        log.flush()
        return TIMED_OUT
    if result.returncode != 0:
        log.write(f"exit code {result.returncode}: {command[-1]}\n")
        log.flush()
    return result.returncode


def run(log, *command, cwd=REPO, timeout=None):
    return execute(log, *command, cwd=cwd, timeout=timeout) == 0


def build(log, preset):
    return run(log, "cmake", "--preset", preset) and run(log, "cmake", "--build", "--preset", preset)


def stage_build_and_test(preset):
    def stage(log):
        return build(log, preset) and run(log, "ctest", "--preset", preset)

    return stage


def stage_script(*arguments):
    def stage(log):
        return run(log, PYTHON, *arguments)

    return stage


def stage_source_rules(log):
    results = [
        run(log, PYTHON, SCRIPTS / name)
        for name in ("check_length.py", "check_layers.py", "check_determinism.py", "check_types.py")
    ]
    results.append(run(log, PYTHON, "-m", "unittest", "discover", "-s", SCRIPTS / "tests"))
    return all(results)


def stage_qmllint(log):
    files = project_files(".qml")
    # Without the resource files, a source file finds its neighbours by folder and misses singletons.
    resources = (REPO / "build" / "debug" / "src" / "client" / ".qt" / "rcc").glob("*.qrc")
    return run(
        log, QT_DIR / "bin" / "qmllint.exe", "--max-warnings", "0",
        "-I", REPO / "build" / "debug" / "qml",
        *[arg for qrc in sorted(resources) for arg in ("--resource", qrc)], *files,
    )  # fmt: skip


def tool_failed(code):
    """True if the tool hung or died from a Windows exception, instead of reporting a result."""
    return code == TIMED_OUT or code >= FIRST_WINDOWS_EXCEPTION


def tidy_files(log, files):
    command = [LLVM_DIR / "clang-tidy.exe", "-p", REPO / "build" / "debug", "--quiet"]
    limit = TIDY_TIMEOUT_SECONDS
    with ThreadPoolExecutor(TIDY_JOBS) as pool:
        codes = list(pool.map(lambda f: execute(log, *command, f, timeout=limit), files))
    # clang-tidy 19.1.5 sometimes dies (access violation, illegal instruction) or spins forever when
    # several run side by side, without any output. That says nothing about the file, so it gets a
    # second run on its own. A second failure of any kind fails the stage.
    crashed = [f for f, code in zip(files, codes) if tool_failed(code)]
    log.write(f"{len(crashed)} clang-tidy crashes or hangs, repeating those files one by one\n")
    retried = [run(log, *command, f, timeout=limit) for f in crashed]
    return all(retried) and all(c == 0 or tool_failed(c) for c in codes)


def changed_sources():
    """Source files changed against dev, and the source file beside a changed header."""
    names = set(git("diff", "--name-only", "--diff-filter=ACMR", "dev"))
    names |= set(git("ls-files", "--others", "--exclude-standard"))
    paths = {(REPO / n).with_suffix(s) for n in names if is_cpp(n) for s in CPP_SOURCE_SUFFIXES}
    return sorted(p for p in paths if p.exists())


def stage_tidy_changed(log):
    files = changed_sources()
    log.write(f"{len(files)} changed files\n")
    return tidy_files(log, files)


def stage_tidy_all(log):
    return tidy_files(log, project_files(*CPP_SOURCE_SUFFIXES))


def stage_cppcheck(log):
    # This cppcheck searches its std.cfg in a build path of its packager; the working directory
    # is the one location it also tries, and absolute source paths keep working from there.
    configs = CPPCHECK.parent.parent / "share" / "Cppcheck" / "cfg"
    return run(
        log, CPPCHECK, "--enable=warning,style,performance,portability", "--library=qt",
        f"--library={SCRIPTS / 'cppcheck-qtd.cfg'}",
        "--language=c++", "--std=c++20", "--check-level=exhaustive", "--inline-suppr",
        "--error-exitcode=1",
        f"--suppressions-list={SCRIPTS / 'cppcheck-suppressions.txt'}",
        "-I", REPO / "src", REPO / "src", REPO / "tests",
        cwd=configs,
    )  # fmt: skip


def stage_golden_replays(log):
    return run(log, "ctest", "--preset", "debug", "-L", "golden", "--no-tests=error")


def stage_coverage(log):
    return build(log, "coverage") and run(log, PYTHON, SCRIPTS / "coverage.py")


def stage_fuzz(log):
    """Runs after the sanitizer stages, which build the programs. A broken game leaves its replay
    in the folder of the logs."""
    LOGS.mkdir(parents=True, exist_ok=True)
    for preset in ("asan", "ubsan"):
        program = REPO / "build" / preset / "bin" / "qtd_headless.exe"
        for name in FUZZ_MAPS:
            arguments = ("--map", REPO / "content" / name, "--bot", "fuzz", "--runs", FUZZ_RUNS)
            if not run(log, program, *arguments, cwd=LOGS, timeout=FUZZ_TIMEOUT_SECONDS):
                return False
    return True


def missing(reason):
    return reason


QUICK_STAGES = [
    ("build and tests (debug)", stage_build_and_test("debug")),
    ("format (clang-format, qmlformat)", stage_script(SCRIPTS / "check_format.py")),
    ("source rules (length, layers, determinism, types)", stage_source_rules),
    ("qmllint", stage_qmllint),
    ("clang-tidy, changed files", stage_tidy_changed),
    ("golden replays", stage_golden_replays),
]
FULL_STAGES = [
    ("clang-tidy, all files", stage_tidy_all),
    ("cppcheck", stage_cppcheck),
    ("build and tests (release)", stage_build_and_test("release")),
    ("AddressSanitizer", stage_build_and_test("asan")),
    ("UBSan", stage_build_and_test("ubsan")),
    ("coverage", stage_coverage),
    ("fuzz bot (ASan, UBSan)", stage_fuzz),
    ("balancing runs", stage_script(SCRIPTS / "balancing.py", "--runs", BALANCING_RUNS)),
    ("smoke test with screenshot", stage_script(SCRIPTS / "smoke_test.py")),
]


def run_stage(index, name, action):
    """Returns (name, state, seconds, detail)."""
    if isinstance(action, str):
        print(f"[ MISSING ] {name}: not yet available, {action}", flush=True)
        return name, "MISSING", 0.0, action
    path = LOGS / f"{index:02d}-{name.split(' (')[0].split(',')[0].replace(' ', '_')}.log"
    started = time.monotonic()
    with open(path, "w", encoding="utf-8", errors="replace") as log:
        passed = action(log)
    seconds = time.monotonic() - started
    state = "PASS" if passed else "FAIL"
    print(f"[ {state:<7} ] {name} ({seconds:.1f} s)", flush=True)
    if not passed:
        lines = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
        print("\n".join(lines[-40:]))
        print(f"Full log: {relative(path)}")
    return name, state, seconds, relative(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quick", action="store_true", help="fast stages only")
    options = parser.parse_args()

    LOGS.mkdir(parents=True, exist_ok=True)
    stages = QUICK_STAGES if options.quick else QUICK_STAGES + FULL_STAGES
    started = time.monotonic()
    results = [run_stage(i, name, action) for i, (name, action) in enumerate(stages, start=1)]
    states = [result[1] for result in results]
    verdict = "FAILED" if "FAIL" in states else "GREEN"
    print(
        f"verify {'--quick' if options.quick else 'full'}: {verdict} in "
        f"{time.monotonic() - started:.0f} s: {states.count('PASS')} passed, "
        f"{states.count('FAIL')} failed, {states.count('MISSING')} not yet available"
    )
    return 1 if "FAIL" in states else 0


if __name__ == "__main__":
    sys.exit(main())

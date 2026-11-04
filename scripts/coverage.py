"""Line coverage with OpenCppCoverage. Fails below the thresholds of the brief, section 4.

Every source file of a measured layer must appear in the reports. A static library only brings the
object files a test references into the test program, so an untested file would otherwise be absent
from the reports instead of counting as uncovered.
"""

import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from qtd_common import CPP_SOURCE_SUFFIXES, OPENCPPCOVERAGE, REPO, project_files, relative

THRESHOLDS = {"core": 85.0, "automation": 70.0}
BUILD = REPO / "build" / "coverage"
OUTPUT = BUILD / "coverage-results"


def test_programs():
    """The commands of the CTest tests labeled core or automation."""
    command = ["ctest", "--test-dir", str(BUILD), "-L", "core|automation", "--show-only=json-v1"]
    listing = json.loads(subprocess.run(command, capture_output=True, text=True, check=True).stdout)
    return [test["command"] for test in listing["tests"] if "command" in test]


def run_program(program):
    target = OUTPUT / (Path(program[0]).stem + ".xml")
    command = [
        str(OPENCPPCOVERAGE),
        "--quiet",
        "--sources",
        str(REPO / "src"),
        "--excluded_sources",
        str(REPO / "build"),
        "--export_type",
        f"cobertura:{target}",
        "--",
        *program,
    ]
    subprocess.run(command, check=True, cwd=BUILD, stdout=subprocess.DEVNULL)
    return target


def layer_of(filename):
    normalized = filename.replace("\\", "/").lower()
    for layer in THRESHOLDS:
        if f"/src/{layer}/" in f"/{normalized}":
            return layer
    return None


def collect(report, hits):
    """Merges one Cobertura report into hits: {(layer, file, line): covered}."""
    for class_node in ET.parse(report).getroot().iter("class"):
        filename = class_node.get("filename")
        layer = layer_of(filename)
        if layer:
            for line in class_node.iter("line"):
                key = (layer, filename, line.get("number"))
                hits[key] = hits.get(key, False) or int(line.get("hits")) > 0


def expected_sources():
    """The source files of the measured layers, relative to the repository."""
    names = map(relative, project_files(*CPP_SOURCE_SUFFIXES))
    return [name for name in names if layer_of(name)]


def unmeasured(hits, sources):
    """The sources without a measured line. Reports name files in their own case and root."""
    measured = {"/" + key[1].replace("\\", "/").lower() for key in hits}
    return [s for s in sources if not any(m.endswith(f"/{s.lower()}") for m in measured)]


def evaluate(hits, sources):
    """Prints one line per layer. A layer without lines or with an unmeasured file fails."""
    ok = True
    missing = unmeasured(hits, sources)
    for name in missing:
        print(f"{name}: no coverage data, no test reaches this file")
    for layer, threshold in THRESHOLDS.items():
        lines = [covered for key, covered in hits.items() if key[0] == layer]
        rate = 100.0 * sum(lines) / len(lines) if lines else 0.0
        without_data = [name for name in missing if layer_of(name) == layer]
        passed = bool(lines) and rate >= threshold and not without_data
        ok = ok and passed
        state = "ok" if passed else "BELOW THRESHOLD"
        state = f"FAILED, {len(without_data)} files without data" if without_data else state
        print(f"{layer}: {sum(lines)}/{len(lines)} lines = {rate:.1f} % (min {threshold}) {state}")
    return ok


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    hits = {}
    for program in test_programs():
        collect(run_program(program), hits)
    return 0 if evaluate(hits, expected_sources()) else 1


if __name__ == "__main__":
    sys.exit(main())

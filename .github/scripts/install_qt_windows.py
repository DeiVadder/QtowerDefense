"""Installs Qt for Windows from download.qt.io, for the workflow build.yml (E-50).

aqtinstall 3.3.0 cannot read the repository of Qt 6.12 for Windows: since 6.12 each compiler has its
own folder (qt6_6120/qt6_6120_msvc2022_64), aqt looks for qt6_6120/qt6_6120. This script reads that
folder directly. Updates.xml is checked against its SHA-256 and every archive against its SHA-1, both
taken from download.qt.io itself; the archives may come from a mirror. 7-Zip unpacks them.

    python install_qt_windows.py 6.12.0 msvc2022_64 <out> qtquick3d qtshadertools ...

gives <out>/6.12.0/msvc2022_64 with the essentials of Qt and the named add-ons.
"""

import hashlib
import subprocess
import sys
import tempfile
import time
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop"


def fetch(url: str) -> bytes:
    for attempt in range(1, 4):
        try:
            with urllib.request.urlopen(url, timeout=120) as response:
                return response.read()
        except OSError as error:
            print(f"Try {attempt} failed: {url}: {error}", flush=True)
            time.sleep(10 * attempt)
    raise SystemExit(f"Cannot download {url}")


def checked(data: bytes, algorithm: str, expected: str, what: str) -> bytes:
    actual = hashlib.new(algorithm, data).hexdigest()
    if actual != expected.strip().lower():
        raise SystemExit(f"{algorithm} of {what} is {actual}, expected {expected}")
    return data


def main(version: str, compiler: str, out: str, modules: list[str]) -> None:
    digits = version.replace(".", "")
    arch = f"win64_{compiler}"
    folder = f"{ROOT}/qt6_{digits}/qt6_{digits}_{compiler}"

    xml_data = fetch(f"{folder}/Updates.xml")
    expected = fetch(f"{folder}/Updates.xml.sha256").decode().split()[0]
    root = ET.fromstring(checked(xml_data, "sha256", expected, "Updates.xml"))

    wanted = {f"qt.qt6.{digits}.{arch}"} | {f"qt.qt6.{digits}.addons.{m}.{arch}" for m in modules}
    packages = {p.findtext("Name"): p for p in root.iter("PackageUpdate")}
    missing = sorted(wanted - packages.keys())
    if missing:
        raise SystemExit(f"Not in the repository: {', '.join(missing)}")

    Path(out).mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        for name in sorted(wanted):
            package = packages[name]
            package_version = package.findtext("Version")
            archives = [a.strip() for a in (package.findtext("DownloadableArchives") or "").split(",")]
            for archive in filter(None, archives):
                url = f"{folder}/{name}/{package_version}{archive}"
                sha1 = fetch(f"{url}.sha1").decode().split()[0]
                path = Path(temp) / archive
                path.write_bytes(checked(fetch(url), "sha1", sha1, archive))
                print(f"{name}: {archive}", flush=True)
                subprocess.run(["7z", "x", "-y", "-bso0", f"-o{out}", str(path)], check=True)
                path.unlink()

    if not (Path(out) / version / compiler / "bin" / "Qt6Core.dll").is_file():
        raise SystemExit(f"Qt6Core.dll is missing under {out}/{version}/{compiler}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:])

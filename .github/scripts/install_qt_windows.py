"""Installs Qt for Windows from download.qt.io, for the workflow build.yml (E-50).

aqtinstall 3.3.0 cannot read the repository of Qt 6.12 for Windows: since 6.12 each compiler has its
own folder (qt6_6120/qt6_6120_msvc2022_64), aqt looks for qt6_6120/qt6_6120. This script reads that
folder directly. Updates.xml and every archive are checked against their SHA-256, which must come
from download.qt.io itself; the archives may come from a mirror. Each archive goes where the
"Extract" operation of its package says, as the Qt installer does. 7-Zip unpacks them.

    python install_qt_windows.py 6.12.0 msvc2022_64 <out> qtquick3d qtshadertools ...

gives <out>/6.12.0/msvc2022_64 with the essentials of Qt and the named add-ons.
"""

import hashlib
import http.client
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

HOST = "download.qt.io"
ROOT = f"https://{HOST}/online/qtsdkrepository/windows_x86/desktop"
TRIES = 3


def fetch(url: str, trusted: bool = False) -> bytes:
    """Downloads url. A trusted file (a hash or Updates.xml) must not come from a mirror."""
    for attempt in range(1, TRIES + 1):
        try:
            with urllib.request.urlopen(url, timeout=120) as response:
                if trusted and urllib.parse.urlparse(response.geturl()).hostname != HOST:
                    raise SystemExit(f"{url} was redirected to {response.geturl()}")
                return response.read()
        except (OSError, http.client.HTTPException) as error:
            print(f"Try {attempt} failed: {url}: {error}", flush=True)
            time.sleep(10 * attempt)
    raise SystemExit(f"Cannot download {url}")


def sha256_of(url: str) -> str:
    return fetch(f"{url}.sha256", trusted=True).decode().split()[0].lower()


def checked_fetch(url: str, expected: str, trusted: bool = False) -> bytes:
    """Downloads url until its SHA-256 matches; a flaky mirror gets the same tries as the network."""
    for attempt in range(1, TRIES + 1):
        data = fetch(url, trusted)
        actual = hashlib.sha256(data).hexdigest()
        if actual == expected:
            return data
        print(f"Try {attempt}: SHA-256 of {url} is {actual}, expected {expected}", flush=True)
    raise SystemExit(f"Wrong SHA-256 for {url}")


def targets(package: ET.Element, out: str, default: str) -> dict[str, str]:
    """Maps each archive of the package to the folder its Extract operation names."""
    result = {}
    for operation in package.iter("Operation"):
        arguments = [a.text or "" for a in operation.findall("Argument")]
        if operation.get("name") == "Extract" and len(arguments) == 2:
            result[arguments[1].strip()] = arguments[0].strip().replace("@TargetDir@", out)
    archives = [a.strip() for a in (package.findtext("DownloadableArchives") or "").split(",")]
    return {archive: result.get(archive, default) for archive in archives if archive}


def main(version: str, compiler: str, out: str, modules: list[str]) -> None:
    digits = version.replace(".", "")
    arch = f"win64_{compiler}"
    folder = f"{ROOT}/qt6_{digits}/qt6_{digits}_{compiler}"
    out = str(Path(out).resolve())
    default = str(Path(out) / version / compiler)

    xml_data = checked_fetch(f"{folder}/Updates.xml", sha256_of(f"{folder}/Updates.xml"), True)
    root = ET.fromstring(xml_data)

    wanted = {f"qt.qt6.{digits}.{arch}"} | {f"qt.qt6.{digits}.addons.{m}.{arch}" for m in modules}
    packages = {p.findtext("Name"): p for p in root.iter("PackageUpdate")}
    missing = sorted(wanted - packages.keys())
    if missing:
        raise SystemExit(f"Not in the repository: {', '.join(missing)}")

    with tempfile.TemporaryDirectory() as temp:
        for name in sorted(wanted):
            package = packages[name]
            package_version = package.findtext("Version")
            for archive, target in targets(package, out, default).items():
                url = f"{folder}/{name}/{package_version}{archive}"
                path = Path(temp) / archive
                path.write_bytes(checked_fetch(url, sha256_of(url)))
                print(f"{name}: {archive} -> {target}", flush=True)
                Path(target).mkdir(parents=True, exist_ok=True)
                subprocess.run(["7z", "x", "-y", "-bso0", f"-o{target}", str(path)], check=True)
                path.unlink()

    if not (Path(default) / "bin" / "Qt6Core.dll").is_file():
        raise SystemExit(f"Qt6Core.dll is missing under {default}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:])

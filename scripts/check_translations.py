"""Translations (E-40): every text of the client has its German translation, and nothing else.

Visible properties of QML must not be set to plain texts, they would escape lupdate. The
translations keep the placeholders of their texts.

lupdate reads the sources into a file of its own, with the project file of the debug build. The
texts it finds must be the texts of the German file, each translated, plurals with all their
forms. The English file holds the plurals only. Fix a failure with `cmake --build --preset debug --target update_translations`
and translate what it marks as unfinished.
"""

import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from qtd_common import QT_DIR, REPO

CLIENT = REPO / "src" / "client"
GERMAN = CLIENT / "i18n" / "qtowerdefense_de.ts"
ENGLISH = CLIENT / "i18n" / "qtowerdefense_en.ts"
PLACEHOLDER = re.compile(r"%(?:\d+|n)")
# A visible property set to a plain text in QML, which no translation can reach.
BARE_TEXT = re.compile(
    r"^\s*(?:text|title|caption|placeholderText|Accessible\.name|Accessible\.description)"
    r"\s*:\s*\"[^\"]*[A-Za-z][^\"]*\""
)
PROJECT = REPO / "build" / "debug" / ".lupdate" / "QtowerDefense_lupdate_project.json"


def messages(path):
    """By (context, source): whether it is a plural, and its translated forms (empty if open)."""
    found = {}
    for context in ET.parse(path).getroot().iter("context"):
        name = context.findtext("name")
        for message in context.iter("message"):
            translation = message.find("translation")
            plural = message.get("numerus") == "yes"
            open_ = translation is None or translation.get("type") in ("unfinished", "vanished",
                                                                         "obsolete")
            forms = [] if open_ else (
                [form.text or "" for form in translation.iter("numerusform")] if plural
                else [translation.text or ""]
            )  # fmt: skip
            found[(name, message.findtext("source"))] = (plural, forms)
    return found


def problems(sources, german, english):
    """What is wrong, one line each."""
    lines = []
    for key in sorted(sources.keys() - german.keys()):
        lines.append(f"not in the German file: {key[0]}: {key[1]}")
    for key in sorted(german.keys() - sources.keys()):
        lines.append(f"no longer in the sources: {key[0]}: {key[1]}")
    for key, (plural, forms) in sorted(german.items()):
        if not forms or not all(forms) or (plural and len(forms) < 2):
            lines.append(f"not translated: {key[0]}: {key[1]}")
        elif any(placeholders(form) != placeholders(key[1]) for form in forms):
            lines.append(f"other placeholders in the translation: {key[0]}: {key[1]}")
    plurals = {key for key, (plural, _) in sources.items() if plural}
    for key in sorted(plurals - english.keys()):
        lines.append(f"no English plural: {key[0]}: {key[1]}")
    for key, (_, forms) in sorted(english.items()):
        if len(forms) < 2 or not all(forms):
            lines.append(f"English plural not written: {key[0]}: {key[1]}")
        elif any(placeholders(form) != placeholders(key[1]) for form in forms):
            lines.append(f"other placeholders in the English plural: {key[0]}: {key[1]}")
    return lines


def placeholders(text):
    return sorted(PLACEHOLDER.findall(text))


def bare_texts(files):
    """Lines of QML that show a text without qsTr, as `file:line: text`."""
    lines = []
    for path in files:
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if BARE_TEXT.match(line):
                lines.append(f"{path.name}:{number}: text without qsTr: {line.strip()}")
    return lines


def scan_sources():
    """The texts of the sources, read as the build reads them: with the include paths of the
    project file that CMake writes, so that classes keep their namespace."""
    with tempfile.TemporaryDirectory() as folder:
        scanned = Path(folder) / "scan.ts"
        command = [str(QT_DIR / "bin" / "lupdate.exe"), "-silent", "-locations", "none",
                   "-project", str(PROJECT), "-ts", str(scanned)]  # fmt: skip
        subprocess.run(command, check=True, capture_output=True)
        return messages(scanned)


def main():
    if not PROJECT.exists():
        print(f"check_translations: configure the debug build first, {PROJECT} is missing")
        return 1
    lines = problems(scan_sources(), messages(GERMAN), messages(ENGLISH))
    lines += bare_texts(sorted(CLIENT.glob("*.qml")))
    for line in lines:
        print(line)
    print(f"check_translations: {len(lines)} problems")
    return 1 if lines else 0


if __name__ == "__main__":
    sys.exit(main())

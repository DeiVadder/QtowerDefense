"""The translation check: what lupdate finds against what the files hold."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_translations

PLAIN = ("Hud", "Pause")
PLURAL = ("WorldMap", "Best: %n round(s)")


class ProblemsTest(unittest.TestCase):
    def sources(self):
        return {PLAIN: (False, []), PLURAL: (True, [])}

    def german(self):
        return {PLAIN: (False, ["Pause"]), PLURAL: (True, ["Bestwert: %n Runde", "%n Runden"])}

    def english(self):
        return {PLURAL: (True, ["Best: %n round", "Best: %n rounds"])}

    def test_complete_files_have_no_problems(self):
        self.assertEqual(check_translations.problems(self.sources(), self.german(), self.english()),
                         [])  # fmt: skip

    def test_a_new_text_is_named(self):
        sources = self.sources() | {("Hud", "Save"): (False, [])}
        self.assertEqual(check_translations.problems(sources, self.german(), self.english()),
                         ["not in the German file: Hud: Save"])  # fmt: skip

    def test_a_text_gone_from_the_sources_is_named(self):
        sources = {PLURAL: (True, [])}
        self.assertEqual(check_translations.problems(sources, self.german(), self.english()),
                         ["no longer in the sources: Hud: Pause"])  # fmt: skip

    def test_open_translations_and_missing_forms_are_named(self):
        german = {PLAIN: (False, []), PLURAL: (True, ["Bestwert: %n Runde"])}
        self.assertEqual(check_translations.problems(self.sources(), german, self.english()),
                         ["not translated: Hud: Pause",
                          "not translated: WorldMap: Best: %n round(s)"])  # fmt: skip

    def test_english_needs_every_plural(self):
        self.assertEqual(check_translations.problems(self.sources(), self.german(), {}),
                         ["no English plural: WorldMap: Best: %n round(s)"])  # fmt: skip
        english = {PLURAL: (True, ["Best: %n round", ""])}
        self.assertEqual(check_translations.problems(self.sources(), self.german(), english),
                         ["English plural not written: WorldMap: Best: %n round(s)"])  # fmt: skip

    def test_a_lost_placeholder_is_named(self):
        german = self.german() | {PLAIN: (False, ["Pause %1"])}
        self.assertEqual(check_translations.problems(self.sources(), german, self.english()),
                         ["other placeholders in the translation: Hud: Pause"])  # fmt: skip
        english = {PLURAL: (True, ["Best: one round", "Best: %n rounds"])}
        self.assertEqual(check_translations.problems(self.sources(), self.german(), english),
                         ["other placeholders in the English plural: WorldMap: Best: %n round(s)"])

    def test_plain_texts_in_qml_are_named(self):
        with tempfile.TemporaryDirectory() as folder:
            qml = Path(folder) / "Panel.qml"
            qml.write_text(
                'Text {\n    text: "Hello"\n    title: qsTr("Fine")\n    Accessible.name: "Gold"\n'
                '    objectName: "plain"\n    text: "%1 %"\n    text: ""\n}\n',
                encoding="utf-8",
            )
            self.assertEqual(check_translations.bare_texts([qml]),
                             ['Panel.qml:2: text without qsTr: text: "Hello"',
                              'Panel.qml:4: text without qsTr: Accessible.name: "Gold"'])

    def test_the_files_of_the_client_are_read(self):
        german = check_translations.messages(check_translations.GERMAN)
        self.assertEqual(german[PLAIN], (False, ["Pause"]))
        self.assertEqual(german[PLURAL], (True, ["Bestwert: %n Runde", "Bestwert: %n Runden"]))


if __name__ == "__main__":
    unittest.main()

"""The type index must list what is there, and only that."""

import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_types
from test_checks import TempRepo


class TypeIndexTest(unittest.TestCase):
    SOURCE = """/// A thing of the game. It has more to say.
/// On a second line.
struct Thing
{
    enum class Kind : quint8 {
        One,
    };
};

class Forward;

/// One word.
template <typename Type>
class Holder : public Base
{
};

using Number = qint64;
// class InAComment
const char *text = "struct InAString {";
"""

    def names(self, text=SOURCE):
        return [found.name for found in check_types.find_types(text, "src/core/a.h")]

    def test_finds_types_and_aliases(self):
        self.assertEqual(self.names(), ["Thing", "Thing::Kind", "Holder", "Number"])

    def test_kinds(self):
        kinds = [t.kind for t in check_types.find_types(self.SOURCE, "src/core/a.h")]
        self.assertEqual(kinds, ["struct", "enum class", "class", "using"])

    def test_purpose_is_the_first_sentence(self):
        found = check_types.find_types(self.SOURCE, "src/core/a.h")
        self.assertEqual(found[0].purpose, "A thing of the game.")
        self.assertEqual(found[1].purpose, "")
        self.assertEqual(found[2].purpose, "One word.")

    def test_a_parent_ends_with_its_indent(self):
        text = "struct A\n{\n    struct B\n    {\n    };\n    struct C\n    {\n    };\n};\nstruct D\n{\n};\n"
        self.assertEqual(self.names(text), ["A", "A::B", "A::C", "D"])

    def test_every_form_of_a_definition_counts(self):
        text = (
            "class A final : public B\n{\n};\n"
            "struct [[nodiscard]] C\n{\n};\n"
            "class QTD_EXPORT D\n{\n};\n"
            "template <typename T> class E\n{\n};\n"
            "union F\n{\n};\n"
            "typedef int G;\n"
            "struct H { int x; };\n"
            "namespace n {\nstruct I\n{\n};\n}\n"
            "enum class J : quint8 { One, Two };\n"
            "struct K : B { int x; };\n"
            "struct alignas(8) L\n{\n};\n"
            "template <typename T> using M = int;\n"
        )
        expected = ["A", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M"]
        self.assertEqual(self.names(text), expected)

    def test_a_qualified_definition_keeps_its_whole_name(self):
        text = "struct Foo::Private\n{\n};\nstruct Bar::Private\n{\n};\n"
        self.assertEqual(self.names(text), ["Foo::Private", "Bar::Private"])

    def test_forward_declarations_do_not_count(self):
        text = "class A;\nstruct B;\nenum class C : int;\nenum D : int;\n"
        self.assertEqual(self.names(text), [])

    def test_types_inside_functions_do_not_count(self):
        text = (
            "struct A\n{\n    void f()\n    {\n        struct Local\n        {\n"
            "        };\n    }\n    struct B\n    {\n    };\n};\n"
            "void g()\n{\n    enum class Other {\n    };\n}\n"
        )
        self.assertEqual(self.names(text), ["A", "A::B"])

    def test_the_same_name_in_two_files_is_a_problem(self):
        types = check_types.find_types("struct A\n{\n};\n", "src/core/a.h")
        types += check_types.find_types("class A\n{\n};\n", "src/client/b.h")
        problems = check_types.doubled_names(types)
        self.assertEqual(len(problems), 1)
        self.assertIn("src/core/a.h, src/client/b.h", problems[0])

    def test_the_same_nested_name_under_two_parents_is_fine(self):
        text = "struct A\n{\n    enum class K {\n    };\n};\nstruct B\n{\n    enum class K {\n    };\n};\n"
        types = check_types.find_types(text, "src/core/a.h")
        self.assertEqual(check_types.doubled_names(types), [])

    def test_the_index_groups_by_layer(self):
        types = check_types.find_types("/// Core.\nstruct A\n{\n};\n", "src/core/a.h")
        types += check_types.find_types("struct B\n{\n};\n", "src/client/b.h")
        index = check_types.render(types)
        self.assertLess(index.index("## client"), index.index("## core"))
        self.assertIn("| `A` | struct | `a.h` | Core. |", index)

    def test_the_project_has_no_problems(self):
        types = check_types.project_types()
        self.assertGreater(len(types), 20)
        self.assertEqual(check_types.doubled_names(types), [])


class StagedTypeIndexTest(TempRepo):
    def setUp(self):
        super().setUp()
        patcher = mock.patch.object(check_types, "REPO", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        (self.root / "docs").mkdir()
        patcher = mock.patch.object(check_types, "INDEX", self.root / "docs" / "Typen.md")
        patcher.start()
        self.addCleanup(patcher.stop)

    def check(self, *arguments):
        with mock.patch.object(sys, "argv", ["check_types.py", *arguments]):
            with contextlib.redirect_stdout(io.StringIO()) as output:
                return check_types.main(), output.getvalue()

    def stage_index(self):
        self.check("--write")
        self.run_git("add", "-A")

    def test_a_staged_type_needs_a_staged_index(self):
        self.write("src/core/a.h", "struct A\n{\n};\n")
        self.stage_index()
        self.assertEqual(self.check("--staged")[0], 0)
        self.write("src/core/b.h", "struct B\n{\n};\n")
        self.run_git("add", "src/core/b.h")
        self.check("--write")  # written, but not added
        code, output = self.check("--staged")
        self.assertEqual(code, 1)
        self.assertIn("out of date", output)

    def test_untracked_files_do_not_block_a_commit(self):
        self.write("src/core/a.h", "struct A\n{\n};\n")
        self.stage_index()
        self.write("src/core/other.h", "struct A\n{\n};\n")
        self.assertEqual(self.check("--staged")[0], 0)
        self.assertEqual(self.check()[0], 1)  # verify sees it


if __name__ == "__main__":
    unittest.main()

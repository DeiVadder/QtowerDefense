"""The rule scripts must find what they are for, and only that."""

import io
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_determinism
import check_format
import check_layers
import check_length
import coverage
import pre_commit
import qtd_common
import verify
from qtd_common import REPO, strip_comments_and_strings

BYPASS_SUFFIXES = (".hpp", ".inl", ".cc", ".cxx", ".hxx")


class TempRepo(unittest.TestCase):
    """A real, empty git repository in place of the project."""

    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.root = Path(folder.name).resolve()
        self.run_git("init", "-q")
        for module in (qtd_common, check_determinism, check_layers):
            patcher = mock.patch.object(module, "REPO", self.root)
            patcher.start()
            self.addCleanup(patcher.stop)

    def run_git(self, *arguments):
        subprocess.run(["git", "-C", str(self.root), *arguments], check=True)

    def write(self, name, text=""):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path


class StripTest(unittest.TestCase):
    def test_keeps_lines_and_removes_content(self):
        text = 'int a; // float\n/* double\n*/ auto s = "QHash"; auto r = R"x(rand())x";\n'
        stripped = strip_comments_and_strings(text)
        self.assertEqual(stripped.count("\n"), 3)
        for word in ("float", "double", "QHash", "rand"):
            self.assertNotIn(word, stripped)
        self.assertIn("auto s", stripped)

    def test_digit_separators_are_no_character_literals(self):
        text = "int a = 1'000; double b = 2'000; int c = 0xFF'FF;\n"
        self.assertEqual(strip_comments_and_strings(text), text)

    def test_character_literals_with_prefix_stay_blanked(self):
        stripped = strip_comments_and_strings("auto a = 'x'; auto b = u8'y'; auto c = L'z';\n")
        for letter in "xyz":
            self.assertNotIn(letter, stripped)


class DeterminismTest(unittest.TestCase):
    def tokens(self, text):
        return [token for _, token, _ in check_determinism.findings(text)]

    def test_finds_every_named_token(self):
        for token in ("float", "double", "QHash", "QRandomGenerator", "QDateTime", "chrono"):
            self.assertIn(token, self.tokens(f"{token} x;\n"))

    def test_finds_std_chrono_and_includes(self):
        self.assertEqual(self.tokens("auto t = std::chrono::seconds(1);\n"), ["chrono"])
        self.assertEqual(self.tokens("#include <QHash>\n"), ["QHash"])
        self.assertEqual(self.tokens("#include <random>\n"), ["random"])

    def test_ignores_comments_strings_and_longer_names(self):
        text = '// float\nconst char *s = "double";\nint floats = 0; int doubled = 1;\n'
        self.assertEqual(self.tokens(text), [])

    def test_reports_the_line(self):
        self.assertEqual(next(check_determinism.findings("int a;\n\nfloat b;\n"))[0], 3)

    def test_finds_floating_point_literals(self):
        self.assertEqual(self.tokens("auto a = 1.5; auto b = 2.0f / 3;\n"), ["1.5", "2.0f"])
        literals = ("1.", ".5", "1.f", "1e5", "1E-3", "1e5f", "1.5e+3L", "1'000.5", "2'0e1")
        for literal in literals + ("0x1.8p3", "0x1p-2", "0X.8P+1", "0x1p4f"):
            self.assertEqual(self.tokens(f"auto a = {literal};\n"), [literal])
        self.assertEqual(self.tokens("#define PI 3.14\n"), ["3.14"])

    def test_ignores_integers_and_names_that_look_like_literals(self):
        for text in ("0x1e5", "0xE", "123", "1'000", "0b101", "10u", "1uLL", "a1.b", "v.e5", "x1e5"):
            self.assertEqual(self.tokens(f"auto a = {text};\n"), [])
        self.assertEqual(self.tokens("#include <QtCore/6.12.0/QtCore/private/x_p.h>\n"), [])

    def test_digit_separators_do_not_hide_code(self):
        self.assertEqual(self.tokens("int a = 1'000; double b = 2'000;\n"), ["double"])


class LayerTest(unittest.TestCase):
    INDEX = {"QString": "QtCore", "QImage": "QtGui", "QTcpSocket": "QtNetwork"}

    def problem(self, layer, include):
        return check_layers.include_problems(layer, include, self.INDEX)

    def test_core_is_limited_to_qtcore(self):
        self.assertIsNone(self.problem("core", "QString"))
        self.assertIsNotNone(self.problem("core", "QImage"))
        self.assertIsNotNone(self.problem("core", "QTcpSocket"))
        self.assertIsNotNone(self.problem("core", "QtQuick/qquickitem.h"))

    def test_project_includes_follow_the_order(self):
        self.assertIsNotNone(self.problem("core", "automation/build_info.h"))
        self.assertIsNone(self.problem("automation", "core/version.h"))
        self.assertIsNotNone(self.problem("automation", "client/graphics.h"))
        self.assertIsNotNone(self.problem("headless", "client/graphics.h"))

    def test_headless_stays_gui_free(self):
        self.assertIsNotNone(self.problem("headless", "QImage"))
        self.assertIsNone(self.problem("client", "QImage"))

    def test_link_lines(self):
        text = "target_link_libraries(x PUBLIC Qt6::Core Qt6::Gui qtd_client)"
        self.assertEqual(
            check_layers.link_problems("core", text),
            ["layer core must not link Qt6::Gui", "layer core must not link qtd_client"],
        )
        self.assertEqual(check_layers.link_problems("client", text), [])

    def test_relative_includes_are_resolved(self):
        for include in (
            "../client/app_info.h",
            "./../client/app_info.h",
            "..\\client\\app_info.h",
            "core/../client/app_info.h",
            "../Client/app_info.h",
            "../../src/client/app_info.h",
        ):
            self.assertIsNotNone(self.problem("core", include), include)
        deep = check_layers.include_problems("core", "../../client/a.h", self.INDEX, "src/core/sub")
        self.assertIsNotNone(deep)

    def test_allowed_relative_includes_stay_allowed(self):
        for layer, include in (("core", "version.h"), ("core", "sub/x.h"), ("core", "../core/a.h")):
            self.assertIsNone(self.problem(layer, include), include)
        self.assertIsNone(self.problem("automation", "../core/version.h"))

    def test_cmake_commands_are_case_insensitive(self):
        self.assertEqual(
            check_layers.link_problems("core", "TARGET_LINK_LIBRARIES(qtd_core PUBLIC Qt6::Gui)"),
            ["layer core must not link Qt6::Gui"],
        )


class LengthTest(unittest.TestCase):
    def functions(self, text):
        return dict(check_length.cpp_functions(strip_comments_and_strings(text)))

    def test_counts_code_lines_only(self):
        text = "void f()\n{\n    // comment\n\n    g();\n}\n"
        self.assertEqual(self.functions(text), {"f": 3})

    def test_finds_functions_in_namespace_and_class(self):
        text = "namespace n {\nclass C\n{\n    int m() const\n    {\n        return 1;\n    }\n};\n}\n"
        self.assertEqual(self.functions(text), {"m": 3})

    def test_constructor_with_brace_initializers_is_one_function(self):
        text = "C::C()\n    : a{1},\n      b{2}\n{\n    run();\n}\n"
        self.assertEqual(list(self.functions(text)), ["C::C"])
        self.assertGreaterEqual(self.functions(text)["C::C"], 3)

    def test_inner_blocks_belong_to_the_function(self):
        text = "void f()\n{\n    if (a) {\n        b();\n    }\n}\nvoid g()\n{\n}\n"
        self.assertEqual(self.functions(text), {"f": 5, "g": 2})

    def test_data_initializers_are_no_functions(self):
        self.assertEqual(self.functions("const int table[] = {1, 2};\nstruct S { int a{0}; };\n"), {})

    def test_baseline_rules(self):
        key = ("a.cpp", "f")
        compare = check_length.compare
        self.assertEqual(len(compare({key: (61, 60)}, {}, None)), 1)
        self.assertEqual(compare({key: (61, 60)}, {key: (61, "reason")}, None), [])
        self.assertEqual(len(compare({key: (62, 60)}, {key: (61, "reason")}, None)), 1)
        self.assertEqual(len(compare({key: (61, 60)}, {key: (70, "reason")}, None)), 1)
        self.assertEqual(len(compare({key: (50, 60)}, {key: (61, "reason")}, None)), 1)
        self.assertEqual(len(compare({key: (61, 60)}, {key: (61, "")}, None)), 1)
        self.assertEqual(len(compare({key: (61, 60)}, {key: (61, "reason")}, {})), 1)


class CoverageTest(unittest.TestCase):
    SOURCES = ["src/core/b.cpp", "src/automation/a.cpp"]

    def test_layer_of_path(self):
        self.assertEqual(coverage.layer_of(r"dev\Q\src\core\version.cpp"), "core")
        self.assertEqual(coverage.layer_of(r"dev\Q\src\client\x.cpp"), None)

    def test_unmeasurable_layer_fails(self):
        hits = {("core", "a", "1"): True}
        self.assertFalse(coverage.evaluate(hits, []))

    def test_threshold(self):
        hits = {("automation", r"dev\src\automation\a.cpp", str(i)): i < 7 for i in range(10)}
        core = r"dev\src\core\b.cpp"
        hits |= {("core", core, str(i)): i < 85 for i in range(100)}
        self.assertTrue(coverage.evaluate(hits, self.SOURCES))
        hits[("core", core, "84")] = False
        self.assertFalse(coverage.evaluate(hits, self.SOURCES))


class CoverageGateTest(TempRepo):
    """The whole gate: reports of the test programs against the files in src."""

    REPORT = '<coverage><class filename="{}"><line number="1" hits="1"/></class></coverage>'

    def gate(self):
        measured = ("dev\\Q\\src\\core\\version.cpp", "dev\\Q\\src\\automation\\build_info.cpp")
        reports = [self.write(f"report{i}.xml", self.REPORT.format(n)) for i, n in enumerate(measured)]
        with (
            mock.patch.object(coverage, "OUTPUT", self.root / "out"),
            mock.patch.object(coverage, "test_programs", return_value=[0, 1]),
            mock.patch.object(coverage, "run_program", side_effect=lambda i: reports[i]),
        ):
            return coverage.main()

    def setUp(self):
        super().setUp()
        self.write("src/core/version.cpp")
        self.write("src/automation/build_info.cpp")
        self.write("src/client/not_measured.cpp")

    def test_all_files_measured(self):
        self.assertEqual(self.gate(), 0)

    def test_file_without_data_fails_in_every_layer(self):
        for name in ("src/core/untested.cpp", "src/automation/untested.cpp", "src/core/sub/x.cc"):
            path = self.write(name)
            self.assertEqual(self.gate(), 1, name)
            path.unlink()


class SuffixTest(TempRepo):
    """No C++ file escapes the checks because of its file name."""

    def test_determinism_reads_every_suffix(self):
        for suffix in BYPASS_SUFFIXES:
            path = self.write(f"src/core/a{suffix}", "double a;\n")
            self.assertEqual(check_determinism.main(), 1, suffix)
            path.unlink()

    def test_non_ascii_file_name_is_checked(self):
        self.write("src/core/größe.cpp", "double a;\n")
        self.assertEqual(check_determinism.main(), 1)

    def test_layers_reads_every_suffix(self):
        for layer in check_layers.LAYERS:
            self.write(f"src/{layer}/CMakeLists.txt")
        for suffix in BYPASS_SUFFIXES:
            path = self.write(f"src/core/a{suffix}", '#include "../client/app_info.h"\n')
            with mock.patch.object(check_layers, "qt_header_index", return_value={}):
                self.assertEqual(check_layers.main(), 1, suffix)
            path.unlink()

    def test_format_takes_every_suffix(self):
        paths = [self.write(f"a{suffix}") for suffix in BYPASS_SUFFIXES]
        with mock.patch.object(check_format, "check_cpp", return_value=0) as check_cpp:
            check_format.main([])
        self.assertEqual(sorted(check_cpp.call_args.args[0]), sorted(paths))
        with mock.patch.object(check_format, "check_cpp", return_value=0) as check_cpp:
            check_format.main([str(paths[0])])
        self.assertEqual(check_cpp.call_args.args[0], paths[:1])

    def test_length_measures_every_suffix(self):
        body = "    g();\n" * (check_length.MAX_FUNCTION_LINES + 1)
        for suffix in BYPASS_SUFFIXES:
            path = self.write(f"a{suffix}", f"void f()\n{{\n{body}}}\n")
            self.assertIn("f", [entry[0] for entry in check_length.measure(path)], suffix)

    def test_length_main_reads_every_suffix(self):
        baseline = self.write("baseline.txt")
        body = "    g();\n" * (check_length.MAX_FUNCTION_LINES + 1)
        for suffix in BYPASS_SUFFIXES:
            path = self.write(f"a{suffix}", f"void f()\n{{\n{body}}}\n")
            with (
                mock.patch.object(check_length, "BASELINE", baseline),
                mock.patch.object(check_length, "baseline_on_dev", return_value=None),
            ):
                self.assertEqual(check_length.main(), 1, suffix)
            path.unlink()

    def test_tidy_takes_every_source_suffix(self):
        for name in ("a.cc", "b.cxx", "c.cpp", "d.hpp", "e.inl", "f.h"):
            self.write(name)
        with mock.patch.object(verify, "tidy_files", return_value=True) as tidy_files:
            verify.stage_tidy_all(None)
        self.assertEqual([p.name for p in tidy_files.call_args.args[1]], ["a.cc", "b.cxx", "c.cpp"])

    def test_changed_sources_follow_headers_and_sources(self):
        self.write("src/a.cc")
        self.write("src/b.cxx")
        self.write("src/b.hpp")
        self.write("src/c.cpp")
        names = ["src/a.cc", "src/b.hpp", "docs/c.md"]
        with (
            mock.patch.object(verify, "REPO", self.root),
            mock.patch.object(verify, "git", return_value=names),
        ):
            self.assertEqual([p.name for p in verify.changed_sources()], ["a.cc", "b.cxx"])

    def test_hook_formats_every_suffix(self):
        names = [f"a{suffix}" for suffix in BYPASS_SUFFIXES]
        result = mock.Mock(returncode=0)
        with (
            mock.patch.object(pre_commit, "git", return_value=names),
            mock.patch.object(pre_commit.subprocess, "run", return_value=result) as run,
        ):
            pre_commit.main()
        command = run.call_args.args[0]
        self.assertTrue(command[1].endswith("check_format.py"))
        self.assertEqual(len(command), 2 + len(names))


class TidyHangTest(unittest.TestCase):
    """A hanging clang-tidy ends the run instead of blocking it forever."""

    def test_timeout_stops_the_process(self):
        with tempfile.TemporaryFile("w+", encoding="utf-8") as log:
            sleeper = "import time; time.sleep(60)"
            code = verify.execute(log, sys.executable, "-c", sleeper, timeout=0.5)
        self.assertEqual(code, verify.TIMED_OUT)

    def tidy(self, *codes):
        with mock.patch.object(verify, "execute", side_effect=codes) as execute:
            result = verify.tidy_files(io.StringIO(), [Path("a.cpp")])
        self.assertEqual(execute.call_count, len(codes))
        return result

    def test_one_hang_is_repeated(self):
        self.assertTrue(self.tidy(verify.TIMED_OUT, 0))

    def test_any_windows_exception_is_repeated(self):
        for code in (0xC0000005, 0xC000001D):
            self.assertTrue(self.tidy(code, 0), hex(code))

    def test_second_hang_fails(self):
        self.assertFalse(self.tidy(verify.TIMED_OUT, verify.TIMED_OUT))

    def test_finding_is_not_repeated(self):
        self.assertFalse(self.tidy(1))


class ConsoleEncodingTest(unittest.TestCase):
    """A character the console cannot show must not abort a check."""

    def test_unprintable_output_survives_redirection(self):
        program = "import qtd_common; print('\\u65e5\\u672c \\ufffd')"
        result = subprocess.run(
            [sys.executable, "-c", program],
            cwd=REPO / "scripts",
            env={**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0"},
            capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"\\u65e5", result.stdout)


class GitFailureTest(unittest.TestCase):
    """Without a repository every check fails instead of passing over zero files."""

    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        patcher = mock.patch.object(qtd_common, "REPO", Path(folder.name) / "missing")
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_git_raises(self):
        with self.assertRaises(qtd_common.GitError):
            qtd_common.git("ls-files")

    def test_file_checks_raise(self):
        with self.assertRaises(qtd_common.GitError):
            check_determinism.main()

    def test_hook_refuses_the_commit(self):
        self.assertEqual(pre_commit.main(), 1)


class LengthBaselineOnDevTest(TempRepo):
    def test_missing_branch_fails(self):
        with self.assertRaises(qtd_common.GitError):
            check_length.baseline_on_dev()

    def test_branch_without_baseline_is_no_failure(self):
        identity = ("-c", "user.name=test", "-c", "user.email=test@example.invalid")
        self.run_git(*identity, "commit", "-q", "--allow-empty", "-m", "empty")
        self.run_git("branch", "dev")
        self.assertIsNone(check_length.baseline_on_dev())


class ConfigurationTest(unittest.TestCase):
    TIDY = (REPO / ".clang-tidy").read_text(encoding="utf-8")

    def test_narrowing_conversions_are_checked(self):
        self.assertNotIn("-bugprone-narrowing-conversions", self.TIDY)

    def test_every_disabled_check_has_a_reason(self):
        disabled = re.findall(r"^\s*-([a-z]+-[\w-]+),?$", self.TIDY, re.MULTILINE)
        self.assertGreater(len(disabled), 0)
        decisions = (REPO / "docs" / "Entscheidungen.md").read_text(encoding="utf-8")
        for check in disabled:
            self.assertIn(f"`{check}`", decisions)

    def test_clang_tidy_sees_every_header_suffix(self):
        header_filter = re.search(r"HeaderFilterRegex: '(.+)'", self.TIDY).group(1)
        for suffix in (".h", ".hpp", ".hxx", ".inl"):
            self.assertRegex(f"C:/dev/src/core/a{suffix}", header_filter)

    def test_documented_leftovers(self):
        building = (REPO / "docs" / "Building.md").read_text(encoding="utf-8")
        self.assertIn("Qt6TaskTree", building)
        performance = (REPO / "docs" / "Performance-Verlauf.md").read_text(encoding="utf-8")
        self.assertIn("Etappe 5", performance)


if __name__ == "__main__":
    unittest.main()

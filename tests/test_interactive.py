"""Run from the project root after compiling all executables."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "search_trees"
if EXE.with_suffix(".exe").exists():
    EXE = EXE.with_suffix(".exe")
spec = importlib.util.spec_from_file_location("comments", ROOT / "tools/check_comments.py")
comments = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comments)


class InteractiveTests(unittest.TestCase):
    def run_console(self, text):
        run = subprocess.run([str(EXE)], input=text, text=True, capture_output=True, timeout=15)
        self.assertEqual(run.returncode, 0, run.stderr)
        return run.stdout

    def test_operations_for_every_kind(self):
        for kind in range(1, 6):
            with self.subTest(kind=kind):
                output = self.run_console(f"{kind}\n1\n10\n1\n10\n3\n10\n4\n5\n2\n10\n2\n10\n4\n0\n")
                for expected in ("Inserted.", "Key already exists.", "Found.", "Size: 1",
                                 "Structure valid.", "Deleted.", "Key not found.", "Size: 0", "Goodbye."):
                    self.assertIn(expected, output)

    def test_invalid_input_and_extreme_keys(self):
        output = self.run_console("abc\n9\n1\n99\n1\n\n12junk\n2147483648\n" + "9" * 300 +
                                  "\n-2147483648\n1\n2147483647\n3\n-2147483648\n3\n2147483647\n5\n0\n")
        self.assertIn("Invalid integer", output)
        self.assertIn("Input too long", output)
        self.assertEqual(output.count("Inserted."), 2)
        self.assertEqual(output.count("Found."), 2)
        self.assertIn("Structure valid.", output)

    def test_switch_preserves_independent_sets(self):
        output = self.run_console("1\n1\n7\n6\n2\n3\n7\n6\n1\n3\n7\n0\n")
        self.assertEqual(output.count("Key not found."), 1)
        self.assertEqual(output.count("Found."), 1)

    def test_eof_at_each_prompt(self):
        for text in ("", "1\n", "1\n1\n", "1\n6\n", "1\n1\n42"):
            with self.subTest(text=text):
                self.assertIn("Goodbye.", self.run_console(text))

    def test_comment_count_includes_inline_not_strings(self):
        source = ('int a; // trailing\n'
                  'char *s = "/* not a comment */ // neither";\n'
                  '/*\n * explanation\n */\n'
                  'int b; /* one */ int c; /* two */\n'
                  '\n')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.c"
            path.write_text(source)
            result = comments.measure(path)
        self.assertEqual(result["total_lines"], 7)
        self.assertEqual(result["comment_lines"], 5)
        self.assertAlmostEqual(result["ratio_all_lines"], 5 / 7)


if __name__ == "__main__":
    unittest.main()

"""Check comment-line semantics independently of any application entry point."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('comments', ROOT / 'tools/check_comments.py')
comments = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comments)


class CommentCounterTests(unittest.TestCase):
    def measure(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'sample.c'
            path.write_text(text)
            return comments.measure(path)

    def test_inline_and_block_comments(self):
        result = self.measure('int a; // explanation\n/*\n * block\n */\n')
        self.assertEqual(result['comment_lines'], 4)
        self.assertEqual(result['total_lines'], 4)

    def test_literal_markers_are_not_comments(self):
        result = self.measure('char *s = "/* not comment */ // text";\nchar c = \'/\';\n')
        self.assertEqual(result['comment_lines'], 0)

    def test_multiple_comments_count_once(self):
        result = self.measure('int a; /* first */ int b; /* second */\n\n')
        self.assertEqual(result['comment_lines'], 1)
        self.assertEqual(result['ratio_all_lines'], 0.5)

    def test_empty_file(self):
        self.assertEqual(self.measure('')['ratio_all_lines'], 0)


if __name__ == '__main__':
    unittest.main()

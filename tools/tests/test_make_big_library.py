import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "make-big-library.py"
SPEC = importlib.util.spec_from_file_location("make_big_library", SCRIPT)
big = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(big)


class BigLibrary(unittest.TestCase):
    def run_main(self, *args):
        out = io.StringIO()
        with redirect_stdout(out), redirect_stderr(out):
            code = big.main(["make-big-library.py", *args])
        return code, out.getvalue()

    def test_writes_both_files_into_an_empty_folder(self):
        with tempfile.TemporaryDirectory() as d:
            code, _ = self.run_main(d)
            root = Path(d) / "cable-twii"
            self.assertEqual(0, code)
            self.assertEqual(["config.toml", "library.toml"], sorted(p.name for p in root.iterdir()))

    def test_refuses_to_overwrite_an_existing_library(self):
        for name in ("library.toml", "config.toml"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as d:
                root = Path(d) / "cable-twii"
                root.mkdir()
                (root / name).write_text("keep", encoding="utf-8")
                code, out = self.run_main(d)
                self.assertEqual(1, code)
                self.assertIn("--force", out)
                self.assertEqual("keep", (root / name).read_text(encoding="utf-8"))
                self.assertEqual([name], [p.name for p in root.iterdir()])

    def test_force_overwrites(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "cable-twii"
            root.mkdir()
            (root / "library.toml").write_text("old", encoding="utf-8")
            code, _ = self.run_main("--force", d)
            self.assertEqual(0, code)
            self.assertTrue((root / "library.toml").read_text(encoding="utf-8").startswith("version = 1\n"))
            self.assertTrue((root / "config.toml").is_file())

    def test_usage_without_a_folder(self):
        code, out = self.run_main()
        self.assertEqual(2, code)
        self.assertIn("usage: make-big-library.py [--force] <dir>", out)


if __name__ == "__main__":
    unittest.main()

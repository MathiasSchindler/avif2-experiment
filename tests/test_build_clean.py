import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BuildCleanTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name) / "project"
        (self.root / "tools").mkdir(parents=True)
        shutil.copyfile(ROOT / "Makefile", self.root / "Makefile")
        shutil.copyfile(
            ROOT / "tools/build_checks.py", self.root / "tools/build_checks.py")

    def clean(self) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["make", "--no-print-directory", "-j1", "clean",
             f"PYTHON={sys.executable}"],
            cwd=self.root, text=True, capture_output=True)

    def test_removes_all_outputs_but_preserves_oracles(self) -> None:
        build = self.root / "build"
        for name in ("native", "refactor-m8-diag-on", "wasm-wp14", "custom build"):
            directory = build / name / "obj"
            directory.mkdir(parents=True)
            (directory / "decoder.o").write_bytes(b"object")
        (build / "compile_commands.json").write_text("[]")
        (build / ".cache").write_bytes(b"cache")
        for name in ("avm", "avm-current"):
            directory = build / "reference" / name
            directory.mkdir(parents=True)
            (directory / "avmdec").write_bytes(b"oracle")
        source = self.root / "retained.obu"
        source.write_bytes(b"input")

        result = self.clean()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual({entry.name for entry in build.iterdir()}, {"reference"})
        for name in ("avm", "avm-current"):
            self.assertEqual(
                (build / "reference" / name / "avmdec").read_bytes(), b"oracle")
        self.assertEqual(source.read_bytes(), b"input")
        self.assertEqual(self.clean().returncode, 0)

    def test_missing_build_directory_is_a_successful_noop(self) -> None:
        result = self.clean()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / "build").exists())

    def test_without_oracles_leaves_an_empty_build_directory(self) -> None:
        output = self.root / "build" / "custom" / "object"
        output.parent.mkdir(parents=True)
        output.write_bytes(b"object")
        result = self.clean()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(list((self.root / "build").iterdir()), [])

    def test_build_directory_symlink_is_rejected(self) -> None:
        external = self.root.parent / "external"
        marker = external / "native" / "keep"
        marker.parent.mkdir(parents=True)
        marker.write_bytes(b"outside")
        (self.root / "build").symlink_to(external, target_is_directory=True)

        result = self.clean()

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("symbolic-link", result.stderr)
        self.assertEqual(marker.read_bytes(), b"outside")
        self.assertTrue((self.root / "build").is_symlink())

    def test_child_symlinks_are_unlinked_without_following_them(self) -> None:
        external = self.root.parent / "external"
        external.mkdir()
        marker = external / "keep"
        marker.write_bytes(b"outside")
        build = self.root / "build"
        build.mkdir()
        (build / "linked").symlink_to(external, target_is_directory=True)
        (build / "dangling").symlink_to(external / "missing")
        (build / "reference").symlink_to(external, target_is_directory=True)

        result = self.clean()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual({entry.name for entry in build.iterdir()}, {"reference"})
        self.assertTrue((build / "reference").is_symlink())
        self.assertEqual(marker.read_bytes(), b"outside")

    def test_build_file_is_rejected_without_removing_it(self) -> None:
        build = self.root / "build"
        build.write_bytes(b"not a directory")
        result = self.clean()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not a directory", result.stderr)
        self.assertEqual(build.read_bytes(), b"not a directory")


if __name__ == "__main__":
    unittest.main()

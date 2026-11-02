import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from check_prediction import (  # noqa: E402
    PRODUCTION_SOURCES,
    check_smooth_blend,
    initializer,
    integers,
    read_production,
)


class PredictionEvidenceTests(unittest.TestCase):
    def test_default_split_owners(self) -> None:
        production = read_production(None)
        self.assertEqual(
            production, "\n".join(path.read_text() for path in PRODUCTION_SOURCES))
        self.assertEqual(len(integers(initializer(
            production, "dr_interp_filter"))), 128)
        check_smooth_blend(production)

    def test_explicit_sources_replace_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            first = Path(name) / "first.c"
            second = Path(name) / "second.c"
            first.write_text("first owner\n")
            second.write_text("second owner\n")
            self.assertEqual(read_production([first]), "first owner\n")
            self.assertEqual(
                read_production([first, second]), "first owner\n\nsecond owner\n")

    def test_single_monolithic_source(self) -> None:
        production = read_production(None)
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "prediction.c"
            path.write_text(production)
            self.assertEqual(read_production([path]), production)
            check_smooth_blend(read_production([path]))

    def test_missing_source_is_not_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            with self.assertRaises(FileNotFoundError):
                read_production([Path(name) / "missing.c"])

    def test_rejects_changed_smooth_table(self) -> None:
        production = read_production(None)
        changed = production.replace(
            "32U, 16U, 8U, 4U, 2U, 1U, 0U, 0U",
            "31U, 16U, 8U, 4U, 2U, 1U, 0U, 0U", 1)
        self.assertNotEqual(production, changed)
        with self.assertRaisesRegex(ValueError, "8-wide smooth blend differs"):
            check_smooth_blend(changed)

    def test_rejects_changed_geometric_process(self) -> None:
        production = read_production(None)
        for before, after in (
            ("position << 1U", "position << 2U"),
            ("distance >= 6U", "distance >= 5U"),
            ("64U >> distance", "32U >> distance"),
        ):
            with self.subTest(expression=before):
                changed = production.replace(before, after)
                self.assertNotEqual(production, changed)
                with self.assertRaisesRegex(ValueError, "geometric smooth blend"):
                    check_smooth_blend(changed)


if __name__ == "__main__":
    unittest.main()

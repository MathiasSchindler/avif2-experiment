import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from obu_carriage import parse_obus  # noqa: E402
from run_spatial_still_frontiers import (  # noqa: E402
    frame_payload_truncation,
    validate_lock,
)


class SpatialStillFrontierTests(unittest.TestCase):
    def test_lock_is_complete_and_valid(self) -> None:
        lock = json.loads(
            (ROOT / "tests/spatial-still-frontiers.json").read_text(
                encoding="ascii"
            )
        )
        self.assertEqual(len(validate_lock(lock)["cases"]), 4)

    def test_frame_truncation_preserves_valid_carriage(self) -> None:
        data = bytes((1, 8, 8, 16, 1, 2, 3, 4, 5, 6, 7))
        shortened = frame_payload_truncation(data)
        obus = parse_obus(shortened)
        self.assertEqual([obu.obu_type for obu in obus], [2, 4])
        self.assertEqual(len(shortened), len(data) - 1)

    def test_rejects_duplicate_case_ids(self) -> None:
        lock = json.loads(
            (ROOT / "tests/spatial-still-frontiers.json").read_text(
                encoding="ascii"
            )
        )
        lock["cases"][1]["case_id"] = lock["cases"][0]["case_id"]
        with self.assertRaisesRegex(ValueError, "duplicate"):
            validate_lock(lock)


if __name__ == "__main__":
    unittest.main()
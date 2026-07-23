import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from run_m9_filter_evidence import (  # noqa: E402
    compare_manifests,
    record_manifest,
)


def i420_size(width: int, height: int) -> int:
    return (
        width * height
        + 2 * ((width + 1) // 2) * ((height + 1) // 2)
    )


class FilterEvidenceTests(unittest.TestCase):
    def test_records_and_compares_stage_local_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            deblocked = directory / "deblocked.i420"
            cdef = directory / "cdef.i420"
            deblocked.write_bytes(bytes(range(i420_size(4, 4))))
            cdef.write_bytes(bytes(range(1, i420_size(4, 4) + 1)))
            expected = record_manifest(
                "synthetic",
                [
                    ("deblocked", deblocked, 4, 4),
                    ("cdef", cdef, 4, 4),
                ],
            )
            actual = record_manifest(
                "synthetic",
                [
                    ("deblocked", deblocked, 4, 4),
                    ("cdef", cdef, 4, 4),
                ],
            )
            comparison = compare_manifests(expected, actual)
            self.assertTrue(comparison["match"])
            self.assertIsNone(comparison["first_mismatch_stage"])

            changed = bytearray(cdef.read_bytes())
            changed[0] ^= 1
            cdef.write_bytes(changed)
            actual = record_manifest(
                "synthetic",
                [
                    ("deblocked", deblocked, 4, 4),
                    ("cdef", cdef, 4, 4),
                ],
            )
            comparison = compare_manifests(expected, actual)
            self.assertFalse(comparison["match"])
            self.assertEqual(comparison["first_mismatch_stage"], "cdef")
            self.assertFalse(comparison["stages"]["cdef"]["plane_matches"]["y"])
            self.assertTrue(comparison["stages"]["cdef"]["plane_matches"]["u"])
            self.assertTrue(comparison["stages"]["cdef"]["plane_matches"]["v"])

    def test_rejects_unknown_or_duplicate_stages(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            path = Path(directory_name) / "stage.i420"
            path.write_bytes(bytes(i420_size(3, 5)))
            with self.assertRaisesRegex(ValueError, "unknown"):
                record_manifest("odd", [("unknown", path, 3, 5)])
            with self.assertRaisesRegex(ValueError, "duplicate"):
                record_manifest(
                    "odd",
                    [
                        ("cdef", path, 3, 5),
                        ("cdef", path, 3, 5),
                    ],
                )

    def test_rejects_different_source_obus(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            stage = directory / "stage.i420"
            first_obu = directory / "first.obu"
            second_obu = directory / "second.obu"
            stage.write_bytes(bytes(i420_size(4, 4)))
            first_obu.write_bytes(b"first")
            second_obu.write_bytes(b"second")
            expected = record_manifest(
                "synthetic",
                [("filtered", stage, 4, 4)],
                first_obu,
            )
            actual = record_manifest(
                "synthetic",
                [("filtered", stage, 4, 4)],
                second_obu,
            )
            with self.assertRaisesRegex(ValueError, "source OBU"):
                compare_manifests(expected, actual)

    def test_rejects_incomplete_stage_records(self) -> None:
        malformed = {
            "schema": "av2h-m9-filter-evidence-v1",
            "fixture": "synthetic",
            "source_obu": None,
            "stage_order": ["cdef"],
            "stages": {"cdef": {}},
        }
        with self.assertRaisesRegex(ValueError, "not I420"):
            compare_manifests(malformed, malformed)


if __name__ == "__main__":
    unittest.main()

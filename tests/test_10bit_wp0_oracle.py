import copy
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from check_10bit_wp0_oracle import (  # noqa: E402
    DEFAULT_ORACLE,
    FIXTURE_ROOT,
    first_mismatch,
    read_oracle,
    validate_oracle,
)
from generate_10bit_wp0_oracle import (  # noqa: E402
    DISABLED_ARGUMENTS,
    encode_command,
)


class TenBitWp0OracleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.oracle = read_oracle(DEFAULT_ORACLE)
        self.expected = (
            FIXTURE_ROOT / self.oracle["decoded"]["file"]
        ).read_bytes()

    def test_locked_oracle_is_valid(self) -> None:
        validate_oracle(self.oracle, FIXTURE_ROOT)

    def test_identical_output_has_no_mismatch(self) -> None:
        self.assertIsNone(first_mismatch(self.oracle, self.expected, self.expected))

    def test_first_sample_mismatch_reports_plane_and_coordinates(self) -> None:
        actual = bytearray(self.expected)
        u_offset = self.oracle["decoded"]["planes"][1]["offset_bytes"]
        actual[u_offset + 2 * (2 * 8 + 3)] ^= 1
        self.assertEqual(
            first_mismatch(self.oracle, self.expected, bytes(actual)),
            {
                "kind": "sample",
                "plane": "U",
                "sample_index": 19,
                "x": 3,
                "y": 2,
                "expected": int.from_bytes(
                    self.expected[u_offset + 38 : u_offset + 40], "little"
                ),
                "actual": int.from_bytes(actual[u_offset + 38 : u_offset + 40], "little"),
            },
        )

    def test_size_mismatch_is_reported_before_samples(self) -> None:
        self.assertEqual(
            first_mismatch(self.oracle, self.expected, self.expected[:-2]),
            {"kind": "size", "expected_size": 768, "actual_size": 766},
        )

    def test_changed_contract_is_rejected(self) -> None:
        changed = copy.deepcopy(self.oracle)
        changed["census"]["base_qindex"] = 255
        with self.assertRaisesRegex(ValueError, "base Q-index"):
            validate_oracle(changed, FIXTURE_ROOT)

    def test_changed_obu_ledger_is_rejected(self) -> None:
        changed = copy.deepcopy(self.oracle)
        changed["obu"]["units"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "temporal_delimiter OBU ledger"):
            validate_oracle(changed, FIXTURE_ROOT)

    def test_changed_source_hash_is_rejected(self) -> None:
        changed = copy.deepcopy(self.oracle)
        changed["generation"]["source_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "source SHA-256"):
            validate_oracle(changed, FIXTURE_ROOT)

    def test_generation_explicitly_constrains_unrelated_tools(self) -> None:
        command = encode_command(Path("avmenc"), Path("source.yuv"), Path("out.obu"))
        self.assertTrue(set(DISABLED_ARGUMENTS).issubset(command))
        self.assertIn("--use-intra-dct-only=1", command)
        self.assertIn("--qp=208", command)
        self.assertNotIn("--use-intra-default-tx-only=1", command)

    def test_changed_disabled_tool_evidence_is_rejected(self) -> None:
        changed = copy.deepcopy(self.oracle)
        changed["trace_evidence"]["disabled_sample_tools"]["palette"] = False
        with self.assertRaisesRegex(ValueError, "disabled sample-tool"):
            validate_oracle(changed, FIXTURE_ROOT)


if __name__ == "__main__":
    unittest.main()
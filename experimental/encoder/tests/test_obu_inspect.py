#!/usr/bin/env python3

import hashlib
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))

from obu_inspect import ObuParseError, decode_uleb128, parse_obus


class ObuInspectTests(unittest.TestCase):
    def test_uleb128(self) -> None:
        self.assertEqual(decode_uleb128(b"\xac\x02", 0), (300, 2))
        with self.assertRaisesRegex(ObuParseError, "non-canonical"):
            decode_uleb128(b"\x81\x00", 0)

    def test_boundaries_and_payload_bits(self) -> None:
        obus = parse_obus(b"\x01\x08\x03\x04\xaa\x55")
        self.assertEqual(len(obus), 2)
        self.assertEqual(obus[0].obu_type, 2)
        self.assertEqual(obus[1].obu_type, 1)
        self.assertEqual(obus[1].payload, b"\xaa\x55")
        self.assertEqual(
            obus[1].record(True)["payload_bits"],
            "10101010 01010101",
        )

    def test_rejects_truncated_obu(self) -> None:
        with self.assertRaisesRegex(ObuParseError, "extends past input"):
            parse_obus(b"\x02\x08")

    def test_fixture_manifest_and_obu_boundaries(self) -> None:
        fixture_root = (
            ROOT / "experimental/encoder/fixtures/avm-v1.0.0-monochrome"
        )
        manifest = json.loads((fixture_root / "manifest.json").read_text())
        self.assertEqual(
            manifest["acceptance"]["accepted_dimensions"],
            [[8, 8], [16, 16], [64, 64]],
        )
        for case in manifest["cases"]:
            obu_path = ROOT / case["obu"]["path"]
            data = obu_path.read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), case["obu"]["sha256"])
            self.assertEqual([obu.obu_type for obu in parse_obus(data)], [2, 1, 4])


if __name__ == "__main__":
    unittest.main()

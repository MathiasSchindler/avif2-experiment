#!/usr/bin/env python3

import json
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENCODER / "tools"))

from ac8x8_color_gate import FORMATS, SPECS, fixture_cases


class Ac8x8ColorGateTests(unittest.TestCase):
    def test_fixture_matrix_is_deterministic_and_focused(self) -> None:
        first, corpus = fixture_cases()
        second, _ = fixture_cases()
        self.assertEqual(
            [item["source_sha256"] for item in first],
            [item["source_sha256"] for item in second])
        self.assertEqual(len(first), len(FORMATS) * len(SPECS))
        self.assertEqual({item["pixel_format"] for item in first}, set(FORMATS))
        self.assertEqual(
            {(item["width"], item["height"]) for item in first},
            {(9, 15), (16, 16), (64, 64), (72, 64)})
        self.assertEqual({item["qindex"] for item in first}, {60, 100, 180})
        self.assertEqual(
            {item["chroma"] for item in first if item["pixel_format"] != "i400"},
            {"neutral", "non-neutral"})
        self.assertEqual(len(corpus["prepared_lock_sha256"]), 64)
        self.assertEqual(len(corpus["prepared_plane_sha256"]), 64)

    def test_frozen_production_gate_passes(self) -> None:
        report = json.loads(
            (ENCODER / "metrics" / "ac8x8-color-gate.json").read_text())
        self.assertEqual(report["schema"], "av2-encoder-ac8x8-color-gate-v1")
        self.assertEqual(report["configuration"]["case_count"], 12)
        self.assertEqual(report["status"], "pass")
        self.assertTrue(report["gates"]["legacy_hashes"])
        self.assertTrue(report["gates"]["error_capacity_atomic"])
        self.assertTrue(report["gates"]["cli_reachability_all_formats"])
        self.assertTrue(report["gates"]["api_reachability_all_formats"])
        self.assertTrue(report["gates"]["local_avm_project_exact"])
        self.assertTrue(report["gates"]["uv_unchanged"])
        self.assertEqual(report["summary"]["bounded_successes"], 12)
        self.assertEqual(report["summary"]["bounded_failures"], 0)
        self.assertEqual(
            report["api_surface"],
            {"i400": True, "i420": True, "i444": True})
        for item in report["cases"]:
            self.assertTrue(item["default_off_legacy_exact"])
            self.assertEqual(len(item["source_sha256"]), 64)


if __name__ == "__main__":
    unittest.main()

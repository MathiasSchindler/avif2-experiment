#!/usr/bin/env python3

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
BASELINE = (
    ROOT / "experimental" / "encoder" / "metrics"
    / "encoder-quality-baseline.json"
)


class EncoderQualityBaselineTests(unittest.TestCase):
    def test_frozen_baseline_is_complete_and_validator_exact(self) -> None:
        baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
        self.assertEqual(baseline["schema"], "av2-encoder-quality-baseline-v1")
        self.assertEqual(baseline["configuration"]["qindices"], [60, 100, 180])
        self.assertEqual(
            baseline["counts"],
            {
                "prepared_sources": 4,
                "full_format_images": 12,
                "targeted_format_images": 18,
                "full_cases": 36,
                "targeted_cases": 54,
                "total_cases": 90,
            },
        )
        self.assertTrue(baseline["summary"]["all_deterministic"])
        self.assertTrue(baseline["summary"]["all_validator_exact"])
        self.assertEqual(len(baseline["cases"]), 90)
        self.assertEqual(len({case["id"] for case in baseline["cases"]}), 90)
        self.assertEqual(
            {case["pixel_format"] for case in baseline["cases"]},
            {"i400", "i420", "i444"},
        )
        for case in baseline["cases"]:
            self.assertTrue(case["deterministic_rerun"])
            self.assertTrue(case["avm_project_exact"])
            self.assertGreater(case["obu_bytes"], 0)
            self.assertEqual(len(case["obu_sha256"]), 64)
            self.assertEqual(len(case["decoded_sha256"]), 64)
            self.assertGreater(case["workspace_bytes"], 0)
            self.assertEqual(
                [plane["plane"] for plane in case["planes"]],
                ["y"] if case["pixel_format"] == "i400" else ["y", "u", "v"],
            )


if __name__ == "__main__":
    unittest.main()

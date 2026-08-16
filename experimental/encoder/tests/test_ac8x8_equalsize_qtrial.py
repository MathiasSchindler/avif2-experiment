#!/usr/bin/env python3

import json
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
METRIC = ENCODER / "metrics" / "ac8x8-equalsize-qtrial.json"


class Ac8x8EqualSizeQtrialTests(unittest.TestCase):
    def test_frozen_policy_is_bounded_exact_and_honest(self) -> None:
        report = json.loads(METRIC.read_text(encoding="utf-8"))
        self.assertEqual(report["schema"], "av2-ac8x8-equalsize-qtrial-v1")
        policy = report["policy"]
        self.assertEqual(policy["maximum_ac_trials"], 16)
        self.assertIn("lower quality", policy["quality_semantics"])
        self.assertEqual(policy["fallback"], "locked DC-only stream at the requested qindex")
        self.assertEqual(report["summary"]["case_count"], 30)
        self.assertEqual(report["summary"]["ac_subset_cases"], 9)
        self.assertTrue(report["summary"]["all_cases_at_or_below_budget"])
        self.assertTrue(report["summary"]["all_selected_validator_exact"])
        self.assertTrue(report["summary"]["all_selected_deterministic"])
        self.assertEqual(report["summary"]["prepared_sse_regressions"], 0)
        self.assertGreater(report["summary"]["validator_rejected_trial_count"], 0)
        for case in report["cases"]:
            qindices = [trial["qindex"] for trial in case["trials"]]
            self.assertEqual(qindices, sorted(set(qindices)))
            self.assertGreaterEqual(qindices[0], case["requested_qindex"])
            self.assertEqual(qindices[-1], 255)
            self.assertLessEqual(len(qindices), policy["maximum_ac_trials"])
            self.assertLessEqual(
                case["selection"]["obu_bytes"], case["byte_budget"])
            self.assertTrue(case["fallback"]["locked_baseline_exact"])

    def test_failure_is_the_locked_ac_subset_frontier(self) -> None:
        report = json.loads(METRIC.read_text(encoding="utf-8"))
        summary = report["summary"]
        self.assertTrue(summary["byte_gate_pass"])
        self.assertTrue(summary["prepared_gate_pass"])
        self.assertFalse(summary["gain_gate_pass"])
        self.assertFalse(summary["minimum_wins_pass"])
        self.assertFalse(summary["success"])


if __name__ == "__main__":
    unittest.main()

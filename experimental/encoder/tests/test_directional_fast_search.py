#!/usr/bin/env python3

import importlib.util
import json
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "experimental/encoder/tools/directional_fast_search.py"
MODEL_TOOL = ROOT / "experimental/encoder/tools/directional_intra_model.py"
REPORT = ROOT / "experimental/encoder/metrics/directional-fast-search.json"
SPEC = importlib.util.spec_from_file_location("directional_fast_search", TOOL)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
MODEL_SPEC = importlib.util.spec_from_file_location(
    "directional_intra_model_for_fast_search", MODEL_TOOL
)
assert MODEL_SPEC and MODEL_SPEC.loader
MODEL = importlib.util.module_from_spec(MODEL_SPEC)
sys.modules[MODEL_SPEC.name] = MODEL
MODEL_SPEC.loader.exec_module(MODEL)


class DirectionalFastSearchTests(unittest.TestCase):
    def test_prefilter_is_stable_and_direction_sensitive(self) -> None:
        vertical_lines = bytes(
            64 if (x // 2) & 1 else 192
            for y in range(64) for x in range(64)
        )
        result = MODULE.prefilter(vertical_lines, 64, 64)
        self.assertTrue(result["evaluate_exact"])
        self.assertLess(
            result["scores"]["vertical"], result["scores"]["horizontal"]
        )
        large_checker = bytes(
            48 if ((x // 4) ^ (y // 4)) & 1 else 208
            for y in range(128) for x in range(128)
        )
        self.assertTrue(MODULE.prefilter(
            large_checker, 128, 128)["early_pruned"])

    def test_frozen_report_passes_quality_and_runtime_gate(self) -> None:
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        MODULE.check_report(report)
        summary = report["summary"]
        self.assertEqual(summary["decision_agreement_total"], 90)
        self.assertEqual(summary["safe_decisions"], 90)
        self.assertTrue(summary["search_off_locked"])
        self.assertGreaterEqual(
            summary["directional_subset_byte_gain_percent"], 10
        )
        self.assertGreaterEqual(summary["directional_subset_wins"], 3)
        self.assertEqual(
            summary["model_140_case_agreement"]["agreement"], 140
        )
        self.assertTrue(summary["all_bytes_nonregressing"])
        self.assertTrue(summary["all_plane_sse_nonregressing"])
        self.assertTrue(report["determinism"]["all_shortlisted_deterministic"])

    def test_policy_only_commits_admissible_exact_results(self) -> None:
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        for case in report["cases"]:
            if case["policy_commit"]:
                self.assertTrue(case["prefilter"]["evaluate_exact"])
                self.assertTrue(case["admissible"])
            self.assertEqual(
                case["decision_agrees"],
                case["policy_commit"] == case["oracle_commit"],
            )

    def test_writer_and_cdf_trials_are_transactional(self) -> None:
        state = MODEL.SyntaxState()
        source = MODEL.generated_pattern("horizontal-gradient", 8, 8)
        before_tell = state.writer.tell()
        before_rows = json.dumps(state.rows, sort_keys=True)
        candidates = MODEL.candidate_cost(
            source, 8, 8, source[:8], source[::8], state
        )
        self.assertEqual(state.writer.tell(), before_tell)
        self.assertEqual(json.dumps(state.rows, sort_keys=True), before_rows)
        winner = candidates[0]
        commit_before = state.writer.tell()
        MODEL.write_luma_mode(state, winner["mode"], 8, 8)
        self.assertEqual(
            state.writer.tell() - commit_before, winner["rate_tell_bits"]
        )


if __name__ == "__main__":
    unittest.main()

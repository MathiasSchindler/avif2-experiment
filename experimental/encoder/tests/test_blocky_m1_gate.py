#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
TOOLS = ENCODER / "tools"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ENCODER / "tests"))

from blocky_m1_gate import (  # noqa: E402
    DEFAULT_ENCODER,
    DEFAULT_OUTPUT,
    DEFAULT_WORK,
    RD_SEARCH,
    frontier,
    parse_subsets,
    probe_rd_support,
    rd_emitted_candidate,
    rd_winner_categories,
    search_category,
    selected_prepared_records,
    select_rate_ceiling_baseline,
    supporting_prepared_crop_evidence,
    supports_rd_in_help,
    targeted_cases,
)


class BlockyM1GateUnitTests(unittest.TestCase):
    def test_selected_prepared_records_choose_smallest_variant_per_source(self) -> None:
        lock = json.loads((ROOT / "images/recipes/prepared-lock.json").read_text())
        selected = selected_prepared_records(lock, "selected")
        self.assertEqual(len(selected), 14)
        self.assertEqual({item["source_id"] for item in selected},
                         {item["source_id"] for item in lock["preparations"]})
        self.assertEqual({item["preparation_id"] for item in selected}, {"le896"})
        self.assertEqual(len(selected_prepared_records(lock, "le1440")), 14)

    def test_frontier_drops_dominated_results_and_preserves_tie_order(self) -> None:
        records = [
            {"search": "off", "status": "pass", "obu_bytes": 40, "reported_sse": [10]},
            {"search": "bounded", "status": "pass", "obu_bytes": 50, "reported_sse": [5]},
            {"search": "directional", "status": "pass", "obu_bytes": 40, "reported_sse": [10]},
            {"search": "intra", "status": "pass", "obu_bytes": 39, "reported_sse": [12]},
            {"search": "directional-full", "status": "pass", "obu_bytes": 55, "reported_sse": [11]},
        ]
        kept = frontier(records)
        self.assertEqual([item["search"] for item in kept], ["intra", "off", "bounded"])

    def test_rate_ceiling_baseline_prefers_best_quality_candidate(self) -> None:
        legacy = [
            {"search": "off", "status": "pass", "obu_bytes": 40, "reported_sse": [20]},
            {"search": "bounded", "status": "pass", "obu_bytes": 60, "reported_sse": [5]},
            {"search": "intra", "status": "pass", "obu_bytes": 55, "reported_sse": [8]},
            {"search": "directional", "status": "pass", "obu_bytes": 52, "reported_sse": [12]},
        ]
        rd = {"search": RD_SEARCH, "status": "pass", "obu_bytes": 52, "reported_sse": [7]}
        baseline, candidates = select_rate_ceiling_baseline(legacy, rd)
        self.assertIsNotNone(baseline)
        assert baseline is not None
        self.assertEqual(baseline["search"], "bounded")
        self.assertEqual({item["search"] for item in candidates}, {"bounded", "intra", "directional"})

    def test_rate_ceiling_can_require_validator_exact_candidates(self) -> None:
        legacy = [
            {
                "search": "bounded", "status": "pass", "obu_bytes": 60,
                "reported_sse": [5], "validated": False,
            },
            {
                "search": "intra", "status": "pass", "obu_bytes": 55,
                "reported_sse": [8], "validated": True,
                "avm_project_exact": True, "local_sse_matches_decode": True,
            },
        ]
        rd = {"search": RD_SEARCH, "status": "pass", "obu_bytes": 52, "reported_sse": [7]}
        baseline, candidates = select_rate_ceiling_baseline(
            legacy, rd, require_validation=True,
        )
        self.assertIsNotNone(baseline)
        assert baseline is not None
        self.assertEqual(baseline["search"], "intra")
        self.assertEqual([item["search"] for item in candidates], ["intra"])

    def test_locked_crop_is_supporting_not_exit_evidence(self) -> None:
        evidence = supporting_prepared_crop_evidence()
        self.assertTrue(evidence["strict_pareto_win"])
        self.assertFalse(evidence["satisfies_m1_exit"])
        self.assertIn("full prepared-image", evidence["reason"])

    def test_search_category_maps_cli_surface_and_mode_tokens(self) -> None:
        self.assertEqual(search_category("off", {}), "skip")
        self.assertEqual(search_category("bounded", {}), "ac")
        self.assertEqual(search_category("plane-qdelta", {}), "plane-qdelta")
        self.assertEqual(search_category("directional-full", {"intra_mode": "53"}), "directional")
        self.assertEqual(search_category("intra", {"luma_mode": "5"}), "smooth")
        self.assertEqual(search_category("intra", {"luma_mode": "8"}), "paeth")
        self.assertEqual(search_category("intra", {"luma_mode": "0"}), "dc")

    def test_rd_winner_categories_use_emitted_counts(self) -> None:
        self.assertEqual(
            rd_winner_categories({
                "rd_mode_dc": "2",
                "rd_mode_vertical": "1",
                "rd_mode_smooth_h": "3",
                "rd_mode_paeth": "0",
                "rd_residual_skip": "1",
                "rd_residual_ac": "4",
            }),
            ["ac", "dc", "directional", "skip", "smooth"],
        )

    def test_emitted_candidate_is_distinct_from_internal_incumbent(self) -> None:
        searches = {
            "bounded": {
                "status": "pass", "obu_sha256": "a", "obu_bytes": 20,
                "reported_sse": [10],
            },
            "off": {
                "status": "pass", "obu_sha256": "b", "obu_bytes": 10,
                "reported_sse": [20],
            },
            "rd": {
                "status": "pass", "obu_sha256": "a", "obu_bytes": 20,
                "reported_sse": [10],
            },
        }
        self.assertFalse(rd_emitted_candidate("i400", searches))
        self.assertTrue(rd_emitted_candidate("i420", searches))

    def test_targeted_cases_cover_formats_and_expected_focus(self) -> None:
        cases, corpus = targeted_cases()
        self.assertEqual(len(cases), 9)
        self.assertEqual(corpus["case_count"], 9)
        self.assertEqual({case["pixel_format"] for case in cases}, {"i400", "i420", "i444"})
        self.assertEqual(sum("qdelta" in case["tags"] for case in cases), 2)
        self.assertTrue(any("odd-dimension" in case["tags"] for case in cases))
        expected = {category for case in cases for category in case["expected_categories"]}
        self.assertTrue({"skip", "dc", "directional", "smooth", "paeth", "ac"}.issubset(expected))

    def test_help_parser_requires_rd_as_a_search_choice(self) -> None:
        self.assertTrue(supports_rd_in_help("encoder --search off|bounded|rd|intra"))
        self.assertFalse(supports_rd_in_help("encoder RAW --width W --height H"))
        self.assertFalse(supports_rd_in_help("output-bit-depth=8"))


class BlockyM1GateExecutionTests(unittest.TestCase):
    def test_tool_blocks_cleanly_when_rd_is_unavailable(self) -> None:
        output = ENCODER / "build/blocky-m1-gate-blocked.json"
        work = ENCODER / "build/blocky-m1-gate-blocked-work"
        artifacts = ENCODER / "build/blocky-m1-gate-blocked-artifacts"
        probe_work = ENCODER / "build/blocky-m1-gate-blocked-probe"
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(artifacts, ignore_errors=True)
        shutil.rmtree(probe_work, ignore_errors=True)
        probe_work.mkdir(parents=True)
        probe = probe_rd_support(DEFAULT_ENCODER, probe_work)
        shutil.rmtree(probe_work, ignore_errors=True)
        if probe["available"]:
            self.skipTest("experimental encoder already supports --search rd")
        completed = subprocess.run([
            "python3", str(TOOLS / "blocky_m1_gate.py"),
            "--output", str(output),
            "--work-dir", str(work),
            "--artifacts-dir", str(artifacts),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 1, completed.stderr.decode())
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(report["schema"], "av2-encoder-blocky-m1-gate-v1")
        self.assertEqual(report["status"], "blocked")
        self.assertFalse(report["gates"]["rd_available"])
        self.assertIn("rd unavailable", report["reason"])
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(artifacts, ignore_errors=True)
        output.unlink(missing_ok=True)

    def test_probe_reports_current_encoder_support_state(self) -> None:
        if not DEFAULT_ENCODER.is_file():
            self.skipTest("experimental encoder is unavailable")
        work = ENCODER / "build/blocky-m1-gate-probe"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        probe = probe_rd_support(DEFAULT_ENCODER, work)
        self.assertIn("help_mentions_rd", probe)
        self.assertIn("probe_returncode", probe)
        if probe["available"]:
            self.assertTrue(probe["help_mentions_rd"])
            self.assertEqual(probe["probe_returncode"], 0)
            self.assertIsNone(probe["reason"])
        else:
            self.assertIn("search", probe["reason"].lower())
        shutil.rmtree(work, ignore_errors=True)

    def test_targeted_subset_runs_only_when_rd_is_supported(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("external validator integration disabled by unit-only run")
        if not DEFAULT_ENCODER.is_file():
            self.skipTest("experimental encoder is unavailable")
        work = ENCODER / "build/blocky-m1-gate-targeted-work"
        probe_work = ENCODER / "build/blocky-m1-gate-targeted-probe"
        output = ENCODER / "build/blocky-m1-gate-targeted.json"
        artifacts = ENCODER / "build/blocky-m1-gate-targeted-artifacts"
        for path in (work, probe_work, artifacts):
            shutil.rmtree(path, ignore_errors=True)
        probe_work.mkdir(parents=True)
        probe = probe_rd_support(DEFAULT_ENCODER, probe_work)
        shutil.rmtree(probe_work, ignore_errors=True)
        if not probe["available"]:
            self.skipTest("experimental encoder does not yet support --search rd")
        for path in (
            ROOT / "build/reference/avm/avmdec",
            ROOT / "build/host-cmake/av2heif-decoder",
        ):
            if not path.is_file():
                self.skipTest(f"validator is unavailable: {path}")
        completed = subprocess.run([
            "python3", str(TOOLS / "blocky_m1_gate.py"),
            "--subsets", "targeted",
            "--output", str(output),
            "--work-dir", str(work),
            "--artifacts-dir", str(artifacts),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertIn(completed.returncode, {0, 1}, completed.stderr.decode())
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertTrue(report["support_probe"]["available"])
        self.assertEqual(report["configuration"]["selected_subsets"], parse_subsets("targeted"))
        self.assertEqual(report["summary"]["case_count"], 9)
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(artifacts, ignore_errors=True)
        output.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3

import json
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
TOOLS = ENCODER / "tools"
sys.path.insert(0, str(TOOLS))

from blocky_m1_diagnostic import (  # noqa: E402
    CROPS_FIXTURE,
    DEFAULT_ENCODER,
    crop_planar_source,
    equivalence_classes,
    parse_corpora,
    probe_rd_support,
    selected_targeted_cases,
)


class BlockyM1DiagnosticUnitTests(unittest.TestCase):
    def test_parse_corpora_accepts_targeted_and_baya(self) -> None:
        self.assertEqual(parse_corpora("targeted,baya"), ["targeted", "baya"])
        with self.assertRaises(Exception):
            parse_corpora("prepared")

    def test_selected_targeted_cases_are_pinned(self) -> None:
        cases, meta = selected_targeted_cases()
        self.assertEqual(len(cases), 7)
        self.assertEqual(meta["targeted_case_count"], 7)
        self.assertIn("smooth-strong-center-i400-q60", {case["id"] for case in cases})

    def test_crop_fixture_stays_inside_baya_bounds(self) -> None:
        fixture = json.loads(CROPS_FIXTURE.read_text(encoding="utf-8"))
        for crop in fixture["crops"]:
            self.assertEqual(crop["preparation_id"], "le1440")
            self.assertEqual(crop["source_id"], "baya-weaver")
            self.assertLessEqual(crop["crop"]["x"] + crop["crop"]["width"], 1440)
            self.assertLessEqual(crop["crop"]["y"] + crop["crop"]["height"], 960)
            self.assertIn(crop["expected_rd_relation"], {"rd-unique", "rd-equals-bounded"})

    def test_crop_planar_source_handles_i420_and_i444(self) -> None:
        i420 = bytes(range(16)) + bytes(range(100, 104)) + bytes(range(200, 204))
        cropped_i420 = crop_planar_source(i420, "i420", 4, 4, 2, 2, 2, 2)
        self.assertEqual(cropped_i420, bytes([10, 11, 14, 15, 103, 203]))

        plane = bytes(range(16))
        i444 = plane + bytes(range(50, 66)) + bytes(range(100, 116))
        cropped_i444 = crop_planar_source(i444, "i444", 4, 4, 1, 1, 2, 2)
        self.assertEqual(
            cropped_i444,
            bytes([5, 6, 9, 10, 55, 56, 59, 60, 105, 106, 109, 110]),
        )

    def test_equivalence_classes_group_identical_hashes(self) -> None:
        searches = {
            "off": {"status": "pass", "obu_sha256": "a", "obu_bytes": 20, "reported_sse": [0]},
            "bounded": {"status": "pass", "obu_sha256": "b", "obu_bytes": 30, "reported_sse": [1]},
            "rd": {"status": "pass", "obu_sha256": "b", "obu_bytes": 30, "reported_sse": [1]},
        }
        groups = equivalence_classes(searches)
        self.assertEqual(len(groups), 2)
        self.assertEqual(groups[1]["searches"], ["bounded", "rd"])


class BlockyM1DiagnosticExecutionTests(unittest.TestCase):
    def test_single_skip_case_runs_when_rd_is_supported(self) -> None:
        if not DEFAULT_ENCODER.is_file():
            self.skipTest("experimental encoder is unavailable")
        probe_work = ENCODER / "build/blocky-m1-diagnostic-probe"
        shutil.rmtree(probe_work, ignore_errors=True)
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
        work = ENCODER / "build/blocky-m1-diagnostic-test-work"
        artifacts = ENCODER / "build/blocky-m1-diagnostic-test-artifacts"
        output = ENCODER / "build/blocky-m1-diagnostic-test.json"
        for path in (work, artifacts):
            shutil.rmtree(path, ignore_errors=True)
        completed = subprocess.run(
            [
                "python3",
                str(TOOLS / "blocky_m1_diagnostic.py"),
                "--corpora",
                "targeted",
                "--case-filter",
                "^skip-flat-odd-i400-q100$",
                "--output",
                str(output),
                "--work-dir",
                str(work),
                "--artifacts-dir",
                str(artifacts),
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        report = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(report["summary"]["case_count"], 1)
        case = report["cases"][0]
        self.assertEqual(case["rd_relation"], "rd-collapsed")
        self.assertIn("off", case["rd_matches"])
        self.assertIn("bounded", case["rd_matches"])
        rd_tokens = case["searches"]["rd"]["tokens"]
        self.assertGreater(int(rd_tokens["rd_trials"]), 0)
        self.assertGreater(
            sum(int(rd_tokens[name]) for name in (
                "rd_mode_dc", "rd_mode_vertical", "rd_mode_horizontal",
                "rd_mode_smooth", "rd_mode_smooth_v", "rd_mode_smooth_h",
                "rd_mode_paeth",
            )),
            0,
        )
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(artifacts, ignore_errors=True)
        output.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()

import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from unittest import TestCase


ENCODER = Path(__file__).parents[1]
TOOLS = ENCODER / "tools"
SPEC = importlib.util.spec_from_file_location(
    "m3_boundary_partition_transcript",
    TOOLS / "m3_boundary_partition_transcript.py",
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
VECTORS = Path(__file__).parent / "m3_boundary_partition_vectors.json"
PROBES = Path(__file__).parent / "m3_boundary_partition_probes.json"
LEGACY = (
    Path(__file__).parent / "m3_boundary_partition_legacy_probes.json"
)


class M3BoundaryPartitionTranscriptTests(TestCase):
    def test_locked_vectors_match_independent_model(self) -> None:
        self.assertEqual(json.loads(VECTORS.read_text()), MODULE.vectors())

    def test_c_writer_matches_independent_range_model(self) -> None:
        self.assertTrue(MODULE.DRIVER.is_file())
        work = ENCODER / f"build/m3-boundary-model-test-{os.getpid()}"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            for size in MODULE.CLASSES:
                tile = work / f"{size}.tile"
                subprocess.run(
                    [
                        str(MODULE.DRIVER), str(size), str(size), "flat",
                        "tile", str(tile),
                    ],
                    check=True,
                )
                self.assertEqual(
                    hashlib.sha256(tile.read_bytes()).hexdigest(),
                    MODULE.transcript(size, size)["tile_sha256"],
                )
        finally:
            shutil.rmtree(work, ignore_errors=True)

    def test_exact_8x8_context_and_range_divergence(self) -> None:
        cases = {
            case["size"]: case for case in json.loads(VECTORS.read_text())[
                "cases"
            ]
        }
        for size in (24, 40, 56):
            self.assertEqual(cases[size]["correct_event"]["row"], 0)
            self.assertEqual(cases[size]["buggy_event"]["row"], 4)
            self.assertEqual(
                cases[size]["correct_event"]["cdf_before"][:3],
                [4684, 0, 0],
            )
        self.assertNotEqual(
            cases[24]["correct_event"]["state_after"]["range"],
            cases[24]["buggy_event"]["state_after"]["range"],
        )
        self.assertEqual(
            cases[40]["correct_event"]["state_after"],
            cases[40]["buggy_event"]["state_after"],
        )
        self.assertNotEqual(
            cases[56]["correct_event"]["state_after"]["range"],
            cases[56]["buggy_event"]["state_after"]["range"],
        )
        for size in (32, 48, 64):
            self.assertIsNone(cases[size]["first_difference"])

    def test_legacy_rejections_and_working_classes_are_locked(self) -> None:
        report = json.loads(LEGACY.read_text())
        cases = {case["name"]: case for case in report["cases"]}
        for size in (24, 56):
            for profile in ("flat", "nonflat"):
                self.assertEqual(
                    cases[f"{size}x{size}-{profile}"]["avmdec_returncode"], 1
                )
        for size in (32, 40, 48, 64):
            for profile in ("flat", "nonflat"):
                case = cases[f"{size}x{size}-{profile}"]
                self.assertEqual(case["avmdec_returncode"], 0)
                self.assertEqual(case["project_decoder_returncode"], 0)
        self.assertEqual(
            cases["24x24-nonflat"]["rejection_offset"]["tell_bits"], 204
        )
        self.assertEqual(
            cases["56x56-nonflat"]["rejection_offset"]["tell_bits"], 830
        )

    def test_fixed_probe_report_has_exact_decoder_parity(self) -> None:
        report = json.loads(PROBES.read_text())
        self.assertTrue(report["all_pass"])
        self.assertEqual(len(report["cases"]), 12)
        self.assertTrue(all(
            case["avmdec_returncode"] == 0
            and case["project_decoder_returncode"] == 0
            and case["validators_exact_equal"]
            for case in report["cases"]
        ))

    def test_live_decoder_parity(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY"):
            self.skipTest("external validators disabled for unit-only run")
        work = ENCODER / f"build/m3-boundary-live-test-{os.getpid()}"
        shutil.rmtree(work, ignore_errors=True)
        try:
            current = MODULE.probe(work)
            locked = json.loads(PROBES.read_text())
            self.assertTrue(current["all_pass"])
            self.assertEqual(
                [
                    (case["name"], case["obu_sha256"], case["output_sha256"])
                    for case in current["cases"]
                ],
                [
                    (case["name"], case["obu_sha256"], case["output_sha256"])
                    for case in locked["cases"]
                ],
            )
        finally:
            shutil.rmtree(work, ignore_errors=True)

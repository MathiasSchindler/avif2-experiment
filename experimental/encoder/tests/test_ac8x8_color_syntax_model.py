import importlib.util
import json
import os
import shutil
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ac8x8_color_syntax_model",
    ENCODER / "tools/ac8x8_color_syntax_model.py",
)
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)
VECTORS = ENCODER / "tests/ac8x8_color_syntax_vectors.json"
PROBES = ENCODER / "tests/ac8x8_color_syntax_probes.json"


class Ac8x8ColorSyntaxModelTests(unittest.TestCase):
    def test_frozen_vectors_match_model(self):
        frozen = json.loads(VECTORS.read_text())
        self.assertEqual(frozen, MODEL.vectors())
        self.assertEqual(frozen["case_count"], 10)
        self.assertEqual(
            sum(case["luma_transform_count"] for case in frozen["cases"]),
            206,
        )
        self.assertEqual(
            sum(case["chroma_transform_count"] for case in frozen["cases"]),
            66,
        )
        self.assertEqual(
            sum(case["transcript_event_count"] for case in frozen["cases"]),
            338,
        )
        self.assertTrue(all(
            case["luma_nonzero_ac_transform_count"] ==
            case["luma_transform_count"]
            for case in frozen["cases"]
        ))

    def test_exact_plane_order_contexts_and_u_to_v_coupling(self):
        coupled = 0
        for definition in MODEL.CASES:
            case = MODEL.case_model(definition)
            events = case["_transcript"]
            for index, event in enumerate(events):
                if event["plane"] == "U":
                    self.assertEqual(events[index + 1]["plane"], "V")
                    self.assertEqual(
                        events[index + 1]["transform"], event["transform"]
                    )
                    self.assertEqual(event["skip"], event["base_skip"] + 6)
                if event["plane"] == "V" and event["u_eob_coupling"]:
                    coupled += 1
                    self.assertEqual(event["skip"], event["base_skip"] + 6)
            first_chroma = next(
                index for index, event in enumerate(events)
                if event["plane"] != "Y"
            )
            self.assertTrue(all(
                event["plane"] == "Y"
                for event in events[:first_chroma]
            ))
        self.assertGreater(coupled, 0)

    def test_frozen_complete_stream_parity(self):
        report = json.loads(PROBES.read_text())
        self.assertEqual(report["case_count"], 10)
        self.assertEqual(report["triple_exact_count"], 10)
        self.assertTrue(report["all_pass"])
        self.assertTrue(report["avmdec_hash_matches_pin"])
        self.assertTrue(report["project_hash_matches_pin"])
        self.assertTrue(all(
            case["local_avm_project_exact"] for case in report["cases"]
        ))

    def test_live_complete_stream_parity(self):
        if os.environ.get("ENCODER_TEST_UNIT_ONLY"):
            self.skipTest("external validators disabled for unit-only run")
        work = ENCODER / f"build/ac8x8-color-syntax-test-{os.getpid()}"
        shutil.rmtree(work, ignore_errors=True)
        try:
            current = MODEL.probe(work)
            frozen = json.loads(PROBES.read_text())
            self.assertTrue(current["all_pass"])
            self.assertEqual(
                [
                    (
                        case["name"], case["obu_sha256"],
                        case["local_reconstruction_sha256"],
                    )
                    for case in current["cases"]
                ],
                [
                    (
                        case["name"], case["obu_sha256"],
                        case["local_reconstruction_sha256"],
                    )
                    for case in frozen["cases"]
                ],
            )
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()

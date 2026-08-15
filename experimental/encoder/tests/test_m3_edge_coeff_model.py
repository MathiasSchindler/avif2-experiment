import importlib.util
import json
import os
import shutil
import sys
from pathlib import Path
from unittest import TestCase


TOOLS = Path(__file__).parents[1] / "tools"
ENCODER = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    "m3_edge_coeff_model", TOOLS / "m3_edge_coeff_model.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
VECTORS = Path(__file__).parent / "m3_edge_coeff_context_vectors.json"
PROBES = Path(__file__).parent / "m3_edge_coeff_avm_probes.json"


class M3EdgeCoeffModelTests(TestCase):
    def test_locked_vectors_match_model(self) -> None:
        self.assertEqual(json.loads(VECTORS.read_text()), MODULE.vectors())

    def test_all_qindices_and_edge_shapes_are_frozen(self) -> None:
        vectors = MODULE.vectors()
        self.assertEqual(vectors["case_count"], 14)
        self.assertEqual(vectors["qindex_cases"], [1, 90, 91, 100, 141, 191, 255])
        for case in vectors["cases"]:
            self.assertEqual(
                [f"{leaf['width']}x{leaf['height']}" for leaf in case["leaves"]],
                ["8x16", "16x8", "8x8"],
            )
            self.assertEqual(case["edge_transform_count"], 5)
            self.assertEqual(
                [(tx["x"], tx["y"]) for tx in case["transforms"]],
                [(16, 0), (16, 8), (0, 16), (8, 16), (16, 16)],
            )

    def test_nonzero_transcripts_have_exact_coefficient_order(self) -> None:
        for case in MODULE.vectors()["cases"]:
            by_transform = {}
            for event in case["events"]:
                key = (event["leaf_index"], event["tx_index"])
                by_transform.setdefault(key, []).append(event["name"])
            for transform in case["transforms"]:
                key = (transform["leaf_index"], transform["tx_index"])
                names = by_transform[key]
                if transform["level"] == 0:
                    self.assertEqual(names, ["txb_skip", "primary_tx_type"])
                else:
                    self.assertEqual(names[:4], [
                        "txb_skip", "eob_pt_64", "primary_tx_type",
                        "coeff_base_lf_eob",
                    ])
                    self.assertIn("dc_sign", names)

    def test_private_probe_matrix_keeps_production_isolated(self) -> None:
        report = json.loads(PROBES.read_text())
        self.assertEqual(report["schema"], "av2-m3-edge-coeff-probes-v2")
        self.assertEqual(report["case_count"], 14)
        self.assertEqual(report["triple_exact_count"], 14)
        self.assertTrue(report["all_pass"])
        self.assertTrue(report["avmdec_hash_matches_pin"])
        self.assertEqual(report["prior_partition_induced_mismatch_count"], 11)
        self.assertEqual(
            report["remaining_partition_induced_range_divergence"], 0
        )
        self.assertEqual(report["genuine_edge_coefficient_context_errors"], 0)
        self.assertTrue(report["production_enabled"])
        exact = [
            (case["profile"], case["qindex"])
            for case in report["cases"] if case["local_avm_project_exact"]
        ]
        self.assertEqual(len(exact), 14)

    def test_live_private_probe_matrix(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY"):
            self.skipTest("external validators disabled for unit-only run")
        work = ENCODER / f"build/m3-edge-coeff-test-{os.getpid()}"
        shutil.rmtree(work, ignore_errors=True)
        try:
            current = MODULE.probe(work)
            locked = json.loads(PROBES.read_text())
            self.assertTrue(current["all_pass"])
            self.assertTrue(current["avmdec_hash_matches_pin"])
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
                    for case in locked["cases"]
                ],
            )
        finally:
            shutil.rmtree(work, ignore_errors=True)

import importlib.util
import json
import sys
from pathlib import Path
from unittest import TestCase


TOOLS = Path(__file__).parents[1] / "tools"
SPEC = importlib.util.spec_from_file_location(
    "m3_multileaf_cdf_model", TOOLS / "m3_multileaf_cdf_model.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
VECTORS = Path(__file__).parent / "m3_multileaf_cdf_vectors.json"
PROBES = Path(__file__).parent / "m3_multileaf_cdf_avm_probes.json"


class M3MultileafCdfModelTests(TestCase):
    def test_locked_vectors_match_model(self) -> None:
        self.assertEqual(json.loads(VECTORS.read_text()), MODULE.vectors())

    def test_pinned_probe_report_is_locked_and_successful(self) -> None:
        report = json.loads(PROBES.read_text())
        self.assertEqual(report["schema"], "av2-m3-multileaf-avm-probes-v1")
        self.assertTrue(report["all_avm_pass"])
        self.assertFalse(report["project_decoder_all_pass"])
        self.assertEqual(
            [case["name"] for case in report["cases"]],
            [f"{width}x{height}" for width, height in MODULE.CASES],
        )

    def test_every_adaptive_event_chains_exactly_by_row(self) -> None:
        for width, height in MODULE.CASES:
            result = MODULE.model(width, height)
            last = {}
            for event in result["events"]:
                if "row_key" not in event:
                    continue
                key = event["row_key"]
                if key in last:
                    self.assertEqual(event["cdf_before"], last[key])
                last[key] = event["cdf_after"]
            self.assertEqual(dict(sorted(last.items())),
                             result["final_cdf_rows"])

    def test_second_transform_divergence_is_spatial_and_adaptive(self) -> None:
        result = MODULE.model(32, 32)
        first_leaf = next(
            leaf for leaf in result["leaves"]
            if leaf["width"] == leaf["height"] == 16
        )
        transforms = [
            tx for tx in result["transforms"]
            if tx["x"] >= first_leaf["x"]
            and tx["x"] < first_leaf["x"] + 16
            and tx["y"] >= first_leaf["y"]
            and tx["y"] < first_leaf["y"] + 16
        ]
        self.assertEqual(len(transforms), 4)
        self.assertEqual(transforms[0]["txb_skip_context"], 1)
        self.assertGreater(transforms[1]["txb_skip_context"], 1)
        first_eob = next(
            event for event in result["events"]
            if event["name"] == "eob_pt_64"
            and event["leaf_index"] == transforms[0]["leaf_index"]
            and event["tx_index"] == 0
        )
        second_eob = next(
            event for event in result["events"]
            if event["name"] == "eob_pt_64"
            and event["leaf_index"] == transforms[1]["leaf_index"]
            and event["tx_index"] == 1
        )
        self.assertEqual(second_eob["cdf_before"], first_eob["cdf_after"])
        self.assertNotEqual(second_eob["cdf_before"],
                            first_eob["cdf_before"])

    def test_q_context_is_selected_from_frame_base_qindex(self) -> None:
        self.assertEqual(
            [MODULE.model(32, 32, q)["q_context"]
             for q in MODULE.Q_CONTEXT_CASES],
            [0, 1, 2, 3],
        )
        row_keys = []
        for q in MODULE.Q_CONTEXT_CASES:
            event = next(
                item for item in MODULE.model(32, 32, q)["events"]
                if item["name"] == "txb_skip"
            )
            row_keys.append(event["row_key"])
        self.assertEqual(len(set(row_keys)), 4)

    def test_flat_case_only_updates_skip_rows(self) -> None:
        result = MODULE.model(32, 32, flat=True)
        coefficient_names = {
            event["name"] for event in result["events"]
            if event.get("tx_index", -1) >= 0
        }
        self.assertEqual(
            coefficient_names, {"txb_skip", "primary_tx_type"}
        )
        self.assertTrue(all(tx["level"] == 0 for tx in result["transforms"]))

    def test_normative_materialization_and_update_examples(self) -> None:
        tables = MODULE.parse_normative_tables()
        cdf = MODULE.materialize(tables["tx_do_partition"][1], 2)
        self.assertEqual(cdf, [3148, 0, 0, 2, 3, 4])
        MODULE.update_cdf(cdf, 2, 0)
        self.assertEqual(cdf, [2952, 0, 1, 2, 3, 4])

import importlib.util
import json
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ac8x8_chroma_context_model",
    ENCODER / "tools/ac8x8_chroma_context_model.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class ChromaContextModelTest(unittest.TestCase):
    def test_scans_match_pinned_avm(self):
        MODEL.verify_inputs()
        for size in MODEL.SIZES:
            self.assertEqual(MODEL.scan(*size), MODEL.pinned_scan(*size))
            self.assertEqual(sorted(MODEL.scan(*size)),
                             list(range(size[0] * size[1])))

    def test_chroma_lf_and_plane_contexts(self):
        for width, height in MODEL.SIZES:
            levels = [0] * (width * height)
            levels[1] = 5
            self.assertTrue(MODEL.is_lf(0, width))
            self.assertFalse(MODEL.is_lf(1, width))
            u = MODEL.base_context(levels, width, height, 0, "U", True)
            v = MODEL.base_context(levels, width, height, 0, "V", True)
            self.assertEqual(v, u + 4)
            self.assertLessEqual(MODEL.br_context(
                levels, width, height, 0), 3)

    def test_u_to_v_neighbor_and_reset_state(self):
        state = MODEL.NeighborState.empty(2, 2)
        self.assertEqual(state.contexts("U"), (6, 0))
        self.assertEqual(MODEL.v_skip_context(2, False), 2)
        self.assertEqual(MODEL.v_skip_context(2, True), 8)
        state.update([-3, 2, 0, 0])
        self.assertNotEqual(state.contexts("U"), (6, 0))
        state.reset_superblock()
        self.assertEqual(state.contexts("U"), (6, 0))

    def test_eob_base_br_remainder_sign_and_adaptation(self):
        shared = MODEL.State()
        for width, height in MODEL.SIZES:
            levels = [0] * (width * height)
            order = MODEL.scan(width, height)
            levels[0], levels[order[1]], levels[order[-1]] = -31, 8, -32767
            first = MODEL.encode_coefficients(
                levels, width, height, "V", 150, shared)
            second = MODEL.encode_coefficients(
                levels, width, height, "V", 150, shared)
            names = {event["name"] for event in first["events"]}
            self.assertTrue(any(name.startswith("eob_pt_") for name in names))
            self.assertIn("base_lf_uv", names)
            self.assertIn("br_uv", names)
            self.assertIn("remainder", names)
            self.assertIn("sign_literal", names)
            self.assertGreater(first["tell_bits"], 0)
            self.assertNotEqual(first["events"], second["events"])

    def test_frozen_vectors_and_report(self):
        vectors = json.loads(
            (ENCODER / "tests/ac8x8_chroma_context_vectors.json").read_text())
        report = json.loads(
            (ENCODER / "metrics/ac8x8-chroma-context-rd.json").read_text())
        self.assertEqual(vectors, MODEL.build_vectors())
        self.assertEqual(report, MODEL.build_report())
        self.assertEqual(len(vectors["vectors"]), 14)
        self.assertEqual(len(report["rd_cases"]), 14)
        self.assertGreater(report["mean_scalar_ac_sse_gain_percent"], 0)


if __name__ == "__main__":
    unittest.main()

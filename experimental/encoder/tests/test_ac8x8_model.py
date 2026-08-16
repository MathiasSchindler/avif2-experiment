import importlib.util
import json
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ac8x8_model", ENCODER / "tools/ac8x8_model.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class Ac8x8ModelTest(unittest.TestCase):
    def test_normative_inputs_and_complete_scan(self):
        MODEL.verify_attachments()
        self.assertEqual(MODEL.derive_scan(), MODEL.SCAN)
        self.assertEqual(sorted(MODEL.SCAN), list(range(64)))

    def test_forward_inverse_basis_and_clipping(self):
        constant = ((129,) * 8,) * 8
        analysis = MODEL.forward(constant)
        self.assertEqual(analysis[0], 64)
        self.assertTrue(all(value == 0 for value in analysis[1:]))
        for position in range(64):
            coeff = [0] * 64
            coeff[position] = 32767 if position & 1 else -32768
            residual = MODEL.inverse(coeff)
            self.assertTrue(all(-256 <= value <= 255
                                for row in residual for value in row))

    def test_full_position_and_magnitude_surface_encodes(self):
        for position in range(64):
            for magnitude in (1, 4, 5, 6, 8, 31, 32767):
                levels = [0] * 64
                levels[position] = -magnitude if position & 1 else magnitude
                coding = MODEL.encode_coefficients(levels, 100)
                self.assertEqual(coding.eob, MODEL.SCAN.index(position) + 1)
                self.assertGreater(coding.tell_bits, 0)
        levels = [0] * 64
        levels[0], levels[1], levels[55] = -31, 8, -32767
        names = [event["name"] for event in
                 MODEL.encode_coefficients(levels, 210).events]
        self.assertIn("base", names)
        self.assertIn("br", names)
        self.assertIn("remainder", names)
        self.assertIn("dc_sign", names)
        self.assertIn("sign_bit", names)

    def test_context_examples_cover_lf_and_high_frequency(self):
        levels = [0] * 64
        levels[1], levels[2], levels[8], levels[9], levels[16] = 3, 4, 5, 2, 6
        self.assertEqual(MODEL.lf_context(levels, 0), 8)
        self.assertGreaterEqual(MODEL.base_context(levels, 63), 10)
        self.assertLess(MODEL.br_context(levels, 63, False), 7)

    def test_frozen_vectors_and_rd_report_are_current(self):
        vectors = json.loads((ENCODER / "tests/ac8x8_vectors.json").read_text())
        report = json.loads((ENCODER / "metrics/ac8x8-rd.json").read_text())
        self.assertEqual(vectors, MODEL.build_vectors())
        self.assertEqual(report, MODEL.build_report())
        self.assertEqual(len(vectors["vectors"]), 20)
        self.assertEqual(len(report["cases"]), 24)

    def test_search_is_deterministic_and_general_surface_wins_rd(self):
        wins = 0
        for samples in MODEL.patterns().values():
            first = MODEL.search(samples, 100)
            second = MODEL.search(samples, 100)
            self.assertEqual(first, second)
            dc = MODEL.search(samples, 100, max_nonzero=1, max_positions=1,
                              allowed_positions=(0,))
            m4 = MODEL.search(samples, 100, max_nonzero=2, max_positions=2,
                              allowed_positions=(0, 8))
            self.assertLessEqual(first.rd_cost, dc.rd_cost)
            self.assertLessEqual(first.rd_cost, m4.rd_cost)
            wins += first.sse < min(dc.sse, m4.sse)
        self.assertGreaterEqual(wins, 5)


if __name__ == "__main__":
    unittest.main()

import importlib.util
import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1] / "tools"
SPEC = importlib.util.spec_from_file_location(
    "m3_dc_model", TOOLS / "m3_dc_model.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class M3DcModelTest(unittest.TestCase):
    def test_qlookup_boundaries(self):
        self.assertEqual(MODEL.qlookup(0), 64)
        self.assertEqual(MODEL.qlookup(1), 40)
        self.assertEqual(MODEL.qlookup(24), 78)
        self.assertEqual(MODEL.qlookup(25), 80)
        self.assertEqual(MODEL.qlookup(255), 61440)
        self.assertEqual([MODEL.coeff_q_context(q)
                          for q in (90, 91, 140, 141, 190, 191)],
                         [0, 1, 1, 2, 2, 3])

    def test_inverse_dc_scaling(self):
        for residual in range(-32, 33):
            self.assertEqual(MODEL.inverse_dct_dc(32 * residual, 4),
                             residual)
            self.assertEqual(MODEL.inverse_dct_dc(64 * residual, 8),
                             residual)
        self.assertEqual(
            MODEL.forward_dc([7] * 16, 3, 4), 2 * 16 * 4)
        self.assertEqual(
            MODEL.forward_dc([7] * 64, 3, 8), 64 * 4)

    def test_dc_level_search_and_clipping(self):
        exact = MODEL.choose_dc_level([129] * 64, 128, 8, 1)
        self.assertEqual((exact.quantized_dc, exact.reconstruction,
                          exact.squared_error), (7, 129, 0))
        positive = MODEL.choose_dc_level([255] * 64, 250, 8, 191)
        negative = MODEL.choose_dc_level([0] * 64, 5, 8, 191)
        self.assertGreaterEqual(positive.reconstruction, 250)
        self.assertLessEqual(negative.reconstruction, 5)
        self.assertTrue(0 <= positive.reconstruction <= 255)
        self.assertTrue(0 <= negative.reconstruction <= 255)

    def test_ramp_uses_block_mean(self):
        ramp = list(range(64))
        vector = MODEL.choose_dc_level(ramp, 128, 8, 141)
        for level in (vector.quantized_dc - 1, vector.quantized_dc + 1):
            _, _, reconstruction = MODEL.reconstruct_dc(level, 141, 128, 8)
            error = sum((sample - reconstruction) ** 2 for sample in ramp)
            self.assertGreaterEqual(error, vector.squared_error)

    def test_prediction_boundaries(self):
        self.assertEqual(MODEL.dc_prediction(None, None, 8), 128)
        self.assertEqual(MODEL.dc_prediction([10] * 8, None, 8), 10)
        self.assertEqual(MODEL.dc_prediction(None, [20] * 8, 8), 20)
        self.assertEqual(
            MODEL.dc_prediction([10] * 8, [20] * 8, 8), 15)
        self.assertEqual(MODEL.dc_prediction(
            list(range(8)), list(range(8, 16)), 8), 8)

    def test_symbol_transcript(self):
        zero = MODEL.symbol_transcript(0, 8, 255)
        self.assertEqual([(item["name"], item["symbol"]) for item in zero],
                         [("all_zero", 1)])
        one = MODEL.symbol_transcript(-1, 8, 255)
        self.assertEqual([(item["name"], item["symbol"]) for item in one],
                         [("all_zero", 0), ("eob_pt_64", 0),
                          ("coeff_base_lf_eob", 0), ("dc_sign", 1)])
        large = MODEL.symbol_transcript(21, 4, 141)
        self.assertEqual([(item["name"], item.get("symbol"))
                          for item in large],
                         [("all_zero", 0), ("eob_pt_16", 0),
                          ("coeff_base_lf_eob", 4),
                          ("coeff_br_lf", 3), ("dc_sign", 0),
                          ("coeff_remainder", None)])
        self.assertEqual(large[-1]["value"], 13)
        self.assertEqual(large[-1]["q_length_bits"], "00000")
        self.assertEqual(large[-1]["golomb_length_bits"], "1")
        self.assertEqual(large[-1]["coeff_rem_width"], 2)
        self.assertEqual(large[-1]["coeff_rem"], 3)

    def test_contexts(self):
        self.assertEqual(MODEL.all_zero_context(True, [4], [4]), 0)
        self.assertEqual(MODEL.all_zero_context(False, [1], [2]), 3)
        self.assertEqual(MODEL.all_zero_context(False, [1, 2], []), 3)
        self.assertEqual(MODEL.dc_sign_context([1], []), 1)
        self.assertEqual(MODEL.dc_sign_context([2], []), 2)
        self.assertEqual(MODEL.dc_sign_context([1], [2]), 0)

    def test_lossless_paths(self):
        for residual in (-63, -1, 0, 1, 63):
            block = MODEL.lossless_wht_dc(4 * residual)
            self.assertTrue(all(value == residual
                                for row in block for value in row))
            self.assertEqual(MODEL.lossless_idtx_residual(residual),
                             residual)

    def test_vector_surface(self):
        vectors = MODEL.vectors()
        self.assertEqual(len(vectors), 76)
        self.assertEqual({vector.size for vector in vectors}, {4, 8})
        self.assertTrue(any(vector.quantized_dc < 0 for vector in vectors))
        self.assertTrue(any(vector.quantized_dc > 0 for vector in vectors))
        self.assertTrue(any(vector.source_mean_num %
                            vector.source_mean_den for vector in vectors))


if __name__ == "__main__":
    unittest.main()

import importlib.util
import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1] / "tools"
SPEC = importlib.util.spec_from_file_location(
    "m4_ac_model", TOOLS / "m4_ac_model.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class M4AcModelTest(unittest.TestCase):
    def test_normative_attachments_and_scan(self):
        MODEL.verify_attachments()
        self.assertEqual(MODEL.scan_8x8_2d(), MODEL.SCAN_8X8_2D)
        self.assertEqual(MODEL.SCAN_8X8_2D[:6], (0, 8, 1, 16, 9, 2))

    def test_forward_dc_and_first_ac(self):
        constant = ((129,) * 8,) * 8
        analysis = MODEL.forward_analysis(constant, 128)
        self.assertEqual(analysis[0][0], 64)
        self.assertTrue(all(
            analysis[y][x] == 0
            for y in range(8) for x in range(8) if (y, x) != (0, 0)))
        gradient = MODEL.forward_analysis(MODEL.vertical_gradient(), 128)
        self.assertNotEqual(gradient[0][0], 0)
        self.assertNotEqual(gradient[1][0], 0)

    def test_inverse_basis_orientation(self):
        positive = MODEL.inverse_dct(0, 256)
        self.assertGreater(positive[0][0], positive[-1][0])
        self.assertTrue(all(len(set(row)) == 1 for row in positive))

    def test_canonical_gradient_has_dc_plus_first_ac(self):
        vector = MODEL.choose_levels(MODEL.vertical_gradient(), 50)
        self.assertNotEqual(vector.quantized_dc, 0)
        self.assertNotEqual(vector.quantized_ac10, 0)
        self.assertEqual(len(vector.reconstruction), 8)
        self.assertTrue(all(0 <= value <= 255
                            for row in vector.reconstruction for value in row))

    def test_all_constant_bytes_stay_in_encodable_level_range(self):
        for value in range(256):
            samples = ((value,) * 8,) * 8
            vector = MODEL.choose_levels(samples, 50)
            self.assertIn(
                abs(vector.quantized_dc),
                range(MODEL.MIN_LEVEL_MAGNITUDE,
                      MODEL.MAX_LEVEL_MAGNITUDE + 1),
            )
            self.assertIn(
                abs(vector.quantized_ac10),
                range(MODEL.MIN_LEVEL_MAGNITUDE,
                      MODEL.MAX_LEVEL_MAGNITUDE + 1),
            )

    def test_eob_base_br_and_sign_transcript(self):
        vector = MODEL.choose_levels(MODEL.vertical_gradient(), 50)
        transcript = MODEL.symbol_transcript(vector)
        named = [(item["name"], item.get("position"), item.get("symbol"))
                 for item in transcript]
        self.assertEqual(named[0], ("all_zero", None, 0))
        self.assertEqual(transcript[0]["cdf"], (24799, 56, 0))
        self.assertEqual(named[1], ("eob_pt_64", None, 1))
        self.assertIn(("coeff_base_lf_eob", 1,
                       min(abs(vector.quantized_ac10) - 1, 5)), named)
        self.assertIn(("coeff_base_lf", 0,
                       min(abs(vector.quantized_dc), 6)), named)
        summaries = [item for item in transcript
                     if item["name"] == "br_summary"]
        self.assertEqual([(item["position"], item["context"])
                          for item in summaries], [(1, 7), (0, 1)])
        self.assertIn(("sign_bit", 1, int(vector.quantized_ac10 < 0)), named)
        self.assertIn(("dc_sign", 0, int(vector.quantized_dc < 0)), named)


if __name__ == "__main__":
    unittest.main()

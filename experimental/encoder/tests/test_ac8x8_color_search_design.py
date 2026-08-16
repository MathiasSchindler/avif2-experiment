import importlib.util
import re
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ac8x8_color_search_design",
    ENCODER / "tools/ac8x8_color_search_design.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class Ac8x8ColorSearchDesignTests(unittest.TestCase):
    def test_896_layout_projections(self):
        report = MODEL.build_report()
        layouts = report["layouts"]
        self.assertEqual(layouts["I400"]["size"], 6_493_736)
        self.assertEqual(layouts["I420"]["size"], 7_309_096)
        self.assertEqual(layouts["I444"]["size"], 9_717_544)
        for layout in layouts.values():
            self.assertEqual(layout["stream_capacity"], 803_072)
            self.assertEqual(layout["transform_count"][0], 12_544)
            self.assertEqual(layout["searched_blocks"], 256)
            self.assertEqual(layout["candidate_trials"], 98_304)

    def test_uv_remains_one_dc_level_per_transform(self):
        i420 = MODEL.query_layout(MODEL.PixelFormat.I420, 896, 896)
        i444 = MODEL.query_layout(MODEL.PixelFormat.I444, 896, 896)
        self.assertEqual(i420.transform_count[1:], (3_136, 3_136))
        self.assertEqual(i444.transform_count[1:], (3_136, 3_136))
        expected = 12_544 * 64 * 2 + 2 * 3_136 * 2
        self.assertEqual(i420.coefficient_bytes, expected)
        self.assertEqual(i444.coefficient_bytes, expected)

    def test_odd_i420_plane_geometry(self):
        layout = MODEL.query_layout(MODEL.PixelFormat.I420, 9, 15)
        self.assertEqual(layout.plane_width, (9, 5, 5))
        self.assertEqual(layout.plane_height, (15, 8, 8))
        self.assertEqual(layout.plane_size, (135, 40, 40))

    def test_existing_numeric_and_cli_contract_is_preserved(self):
        header = (ENCODER / "include/encoder/ac8x8.h").read_text()
        main = (ENCODER / "src/main.c").read_text()
        values = dict(re.findall(
            r"(ENC_AC8X8_SEARCH_[A-Z_]+)\s*=\s*(\d+)", header))
        self.assertEqual(values["ENC_AC8X8_SEARCH_OFF"], "0")
        self.assertEqual(values["ENC_AC8X8_SEARCH_BOUNDED"], "1")
        for name in ("off", "bounded", "directional",
                     "directional-exact", "intra"):
            self.assertIn(f'"{name}"', main)

    def test_invalid_geometry_is_closed(self):
        for width, height in ((0, 1), (1, 0), (1441, 1), (1, 1441)):
            with self.subTest(width=width, height=height):
                with self.assertRaises(ValueError):
                    MODEL.query_layout(MODEL.PixelFormat.I400, width, height)


if __name__ == "__main__":
    unittest.main()

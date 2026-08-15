#!/usr/bin/env python3

import importlib.util
import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1] / "tools"
SPEC = importlib.util.spec_from_file_location(
    "m3_multisuperblock_model", TOOLS / "m3_multisuperblock_model.py"
)
assert SPEC and SPEC.loader
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class M3MultiSuperblockModelTests(unittest.TestCase):
    CASES = (
        (65, 1), (1, 65), (65, 65), (127, 64), (64, 127),
        (127, 127), (128, 128), (129, 129), (896, 424),
        (598, 896), (896, 896),
    )

    def test_row_major_roots_context_epochs_and_exact_cover(self) -> None:
        for width, height in self.CASES:
            with self.subTest(width=width, height=height):
                result = MODEL.model(width, height)
                roots = result["superblock_order"]
                self.assertEqual(
                    roots,
                    sorted(roots, key=lambda root: (root["row"], root["column"])),
                )
                self.assertTrue(result["transform_exact_cover"])
                self.assertEqual(
                    result["transform_count"],
                    ((width + 7) // 8) * ((height + 7) // 8),
                )
                self.assertEqual(result["left_context_span"], 16)
                self.assertEqual(result["cdf_epoch_count"], 1)
                for root in roots:
                    self.assertEqual(
                        root["left_context_epoch"], root["row"] // 16
                    )

    def test_boundary_rounded_root_counts(self) -> None:
        for width, height in self.CASES:
            result = MODEL.model(width, height)
            expected = ((width + 63) // 64) * ((height + 63) // 64)
            self.assertEqual(len(result["superblock_order"]), expected)


if __name__ == "__main__":
    unittest.main()

import json
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]


class ChromaAcProductionTests(unittest.TestCase):
    def test_frozen_gate_passes(self):
        report = json.loads(
            (ENCODER / "metrics/chroma-ac-gate.json").read_text())
        self.assertEqual(report["schema"], "av2-encoder-chroma-ac-gate-v1")
        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["summary"]["bounded_successes"], 12)
        self.assertGreaterEqual(report["summary"]["chroma_wins"], 4)
        self.assertGreater(report["summary"]["chroma_sse_gain"], 0)
        self.assertTrue(all(report["gates"].values()))

    def test_all_production_transform_sizes_are_covered(self):
        report = json.loads(
            (ENCODER / "metrics/chroma-ac-gate.json").read_text())
        self.assertEqual(
            report["configuration"]["transform_sizes"],
            ["4x4", "4x8", "8x4", "8x8",
             "8x16", "16x8", "16x16"])


if __name__ == "__main__":
    unittest.main()

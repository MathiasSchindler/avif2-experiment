import json
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]


class FullDirectionalGateTests(unittest.TestCase):
    def test_frozen_gate_passes(self):
        report = json.loads(
            (ENCODER / "metrics/full-directional-gate.json").read_text())
        self.assertEqual(
            report["schema"], "av2-encoder-full-directional-gate-v1")
        self.assertEqual(report["status"], "pass")
        self.assertEqual(report["summary"]["successful_cases"], 28)
        self.assertGreaterEqual(report["summary"]["strict_wins"], 8)
        self.assertGreaterEqual(report["summary"]["prepared_wins"], 4)
        self.assertGreater(report["summary"]["total_sse_gain"], 0)
        self.assertTrue(all(report["gates"].values()))


if __name__ == "__main__":
    unittest.main()

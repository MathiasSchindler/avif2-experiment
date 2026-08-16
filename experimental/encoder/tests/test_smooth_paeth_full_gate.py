import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"


class SmoothPaethFullGateTests(unittest.TestCase):
    def test_locked_full_dimension_report(self) -> None:
        output = ENCODER / "build/smooth-paeth-full-gate-test.json"
        completed = subprocess.run([
            "python3", str(ENCODER / "tools/smooth_paeth_full_gate.py"),
            "--output", str(output),
            "--work-dir", str(ENCODER / "build/smooth-paeth-full-gate-test"),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        data = json.loads(output.read_text())
        self.assertEqual(data["configuration"]["dimensions"], [1440, 1440])
        self.assertEqual(data["summary"]["successful_cases"], 4)
        self.assertTrue(all(data["gates"].values()))
        self.assertGreater(data["summary"]["strict_wins"], 0)


if __name__ == "__main__":
    unittest.main()

import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"


class SmoothPaethProductionTests(unittest.TestCase):
    def test_locked_production_report(self) -> None:
        output = ENCODER / "build/smooth-paeth-production-test.json"
        completed = subprocess.run([
            "python3", str(ENCODER / "tools/smooth_paeth_production.py"),
            "--output", str(output),
            "--work-dir", str(ENCODER / "build/smooth-paeth-production-test"),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        data = json.loads(output.read_text())
        self.assertEqual(data["cases"], 224)
        self.assertTrue(data["gate"]["passed"])
        self.assertGreater(data["incremental_wins"], 0)


if __name__ == "__main__":
    unittest.main()

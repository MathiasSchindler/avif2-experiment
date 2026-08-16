import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"


class PlaneQDeltaGateTests(unittest.TestCase):
    def test_locked_production_gate(self) -> None:
        output = ENCODER / "build/plane-qdelta-gate-test.json"
        completed = subprocess.run([
            "python3", str(ENCODER / "tools/plane_qdelta_gate.py"),
            "--output", str(output),
            "--work-dir", str(ENCODER / "build/plane-qdelta-gate-test"),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        data = json.loads(output.read_text())
        self.assertEqual(data["summary"]["successful_cases"], 5)
        self.assertTrue(all(data["gates"].values()))
        self.assertEqual(data["summary"]["strict_wins"], 4)


if __name__ == "__main__":
    unittest.main()

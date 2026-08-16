import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "ac8x8_hardbudget_model",
    ENCODER / "tools/ac8x8_hardbudget_model.py")
MODEL = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class Ac8x8HardbudgetModelTest(unittest.TestCase):
    def test_frozen_frontier_records_exact_blocker(self):
        report = json.loads(
            (ENCODER / "metrics/ac8x8-hardbudget.json").read_text())
        summary = report["summary"]
        self.assertEqual(report["schema"], "av2-ac8x8-hardbudget-model-v1")
        self.assertEqual(summary["baseline_bytes"], 786)
        self.assertEqual(summary["candidate_bytes"], 786)
        self.assertEqual(summary["maximum_reachable_wins_at_8_percent_gain"], 7)
        self.assertEqual(summary["minimum_aggregate_bytes_for_reachable_frontier"],
                         1048)
        self.assertEqual(
            summary["cases_without_any_winning_candidate"],
            ["target-horizontal-lines-2-i400-q180",
             "target-vertical-lines-2-i400-q180"])
        self.assertEqual(summary["status"], "blocked")

    def test_hosted_driver_is_deterministic(self):
        driver = MODEL.build_driver()
        work = ENCODER / "build/ac8x8-hardbudget-unit"
        work.mkdir(parents=True, exist_ok=True)
        raw = work / "checker.i400"
        raw.write_bytes(bytes(
            32 if (x + y) & 1 else 224
            for y in range(8) for x in range(8)))
        command = [str(driver), str(raw), "8", "8", "100"]
        first = subprocess.run(
            command, check=True, text=True, capture_output=True).stdout
        second = subprocess.run(
            command, check=True, text=True, capture_output=True).stdout
        self.assertEqual(first, second)
        states = [json.loads(line) for line in first.splitlines()]
        self.assertGreaterEqual(len(states), 2)
        self.assertLess(states[-1]["sse"], states[0]["sse"])


if __name__ == "__main__":
    unittest.main()

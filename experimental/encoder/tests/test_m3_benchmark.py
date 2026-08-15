#!/usr/bin/env python3

import json
import shutil
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))
sys.path.insert(0, str(ROOT / "experimental/encoder/tests"))

from m3_benchmark import distortion, generated_pixels, main
from validator_support import configured_path, require_executable, require_validator


class M3BenchmarkTests(unittest.TestCase):
    def test_generated_gray_fixtures_are_stable(self) -> None:
        ramp = generated_pixels(
            {"generator": "ramp-x", "width": 4, "height": 2}
        )
        impulse = generated_pixels(
            {"generator": "impulse", "width": 3, "height": 3}
        )
        odd = generated_pixels(
            {"generator": "odd-edge", "width": 3, "height": 2}
        )
        self.assertEqual(ramp, bytes([0, 85, 170, 255]) * 2)
        self.assertEqual(impulse, bytes([0, 0, 0, 0, 255, 0, 0, 0, 0]))
        self.assertEqual(odd, bytes([32, 32, 224, 224, 224, 224]))

    def test_integer_distortion_representation(self) -> None:
        metrics = distortion(bytes([0, 10]), bytes([0, 12]))
        self.assertEqual(metrics["sse"], 4)
        self.assertEqual(metrics["mse_numerator"], 2)
        self.assertEqual(metrics["mse_denominator"], 1)
        self.assertEqual(metrics["psnr_millidb"], 45121)
        self.assertFalse(metrics["psnr_infinite"])
        exact = distortion(b"abc", b"abc")
        self.assertEqual(exact["sse"], 0)
        self.assertTrue(exact["psnr_infinite"])
        self.assertIsNone(exact["psnr_millidb"])

    def test_rectangular_raw_encoder_passes_both_decoders(self) -> None:
        encoder = require_executable(
            self,
            ROOT / "experimental/encoder/build/av2-encoder-experiment",
            "experimental encoder",
        )
        avmdec = require_validator(
            self,
            configured_path("AVMDEC", ROOT / "build/reference/avm/avmdec"),
            "pinned avmdec",
        )
        project = require_validator(
            self,
            configured_path(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder",
            ),
            "project decoder",
        )
        work = ROOT / "experimental/encoder/build/m3-benchmark-test"
        shutil.rmtree(work, ignore_errors=True)
        output = work / "metrics.jsonl"
        result = main([
            "--fixture=ramp-x-16x8",
            f"--encoder={encoder}",
            f"--avmdec={avmdec}",
            f"--project-decoder={project}",
            f"--work-dir={work / 'work'}",
            f"--output={output}",
        ])
        self.assertEqual(result, 0)
        record = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(record["status"], "pass")
        self.assertTrue(record["decoder_exact"])
        self.assertGreater(record["obu_bytes"], 0)
        self.assertTrue((work / "work/ramp-x-16x8/encoded.obu").is_file())
        shutil.rmtree(work)

    def test_suite_locks_square_portrait_and_landscape_inputs(self) -> None:
        suite = json.loads(
            (ROOT / "experimental/encoder/metrics/fixtures.json").read_text()
        )
        prepared = {
            fixture["orientation"]: fixture
            for fixture in suite["fixtures"] if fixture["kind"] == "prepared"
        }
        self.assertEqual(set(prepared), {"square", "portrait", "landscape"})
        self.assertEqual(prepared["square"]["id"], "pitti-ceiling-896x896")
        for fixture in prepared.values():
            self.assertEqual(len(fixture["sha256"]), 64)
            self.assertTrue(fixture["metadata"].endswith("/manifest.json"))


if __name__ == "__main__":
    unittest.main()

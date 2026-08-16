import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
REPORT = ENCODER / "metrics/blockiness-m0.json"
DRIVER = ENCODER / "build/m3-i400-intra-driver"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT = ROOT / "build/host-cmake/av2heif-decoder"

import sys
sys.path.insert(0, str(ENCODER / "tools"))
from blockiness_m0 import boundary_diagnostic, ssim_q1e9, write_pgm


class BlockinessM0MetricTests(unittest.TestCase):
    def test_identical_plane_has_perfect_ssim_and_no_boundary_error(self) -> None:
        plane = bytes((x * 7 + y * 11) & 255 for y in range(16)
                      for x in range(16))
        self.assertEqual(ssim_q1e9(plane, plane, 16, 16), 1_000_000_000)
        metric = boundary_diagnostic(plane, plane, 16, 16, 8)
        self.assertEqual(metric["boundary_gradient_error_squared"], 0)
        self.assertEqual(metric["interior_gradient_error_squared"], 0)

    def test_boundary_metric_subtracts_real_source_gradient(self) -> None:
        source = bytes(
            32 if x < 8 else 224 for y in range(16) for x in range(16)
        )
        exact = boundary_diagnostic(source, source, 16, 16, 8)
        self.assertEqual(exact["boundary_gradient_error_squared"], 0)
        decoded = bytearray(source)
        for y in range(16):
            decoded[y * 16 + 7] = 0
            decoded[y * 16 + 8] = 255
        artifact = boundary_diagnostic(source, bytes(decoded), 16, 16, 8)
        self.assertGreater(
            artifact["boundary_gradient_error_squared"], 0
        )
        self.assertGreater(artifact["boundary_excess_discontinuity"], 0)

    def test_pgm_crop_is_exact(self) -> None:
        plane = bytes(range(64))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "crop.pgm"
            write_pgm(
                output, plane, 8, 8,
                {"x": 2, "y": 3, "width": 3, "height": 2},
            )
            self.assertEqual(
                output.read_bytes(),
                b"P5\n3 2\n255\n" + bytes((26, 27, 28, 34, 35, 36)),
            )


class LargeI400IntraParityTests(unittest.TestCase):
    def test_split_leaf_smooth_extensions_match_both_decoders(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("validators disabled by explicit unit-only run")
        for path in (DRIVER, AVMDEC, PROJECT):
            self.assertTrue(path.is_file(), f"missing executable: {path}")
        width = height = 32
        source = bytes(
            (50 + 3 * x + 2 * y) & 255
            for y in range(height) for x in range(width)
        )
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            input_path = root / "gradient.i400"
            obu = root / "gradient.obu"
            local = root / "local.i400"
            avm = root / "avm.i400"
            project = root / "project.i400"
            input_path.write_bytes(source)
            encoded = subprocess.run([
                str(DRIVER), str(input_path), str(width), str(height),
                "100", "3", str(obu), str(local),
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(encoded.returncode, 0, encoded.stderr.decode())
            self.assertTrue(encoded.stdout.decode().strip().endswith(" 5"))
            decoded = subprocess.run([
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm), str(obu),
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertEqual(decoded.returncode, 0, decoded.stderr.decode())
            decoded = subprocess.run(
                [str(PROJECT), str(obu), "-o", str(project)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )
            self.assertEqual(decoded.returncode, 0, decoded.stderr.decode())
            self.assertEqual(local.read_bytes(), avm.read_bytes())
            self.assertEqual(local.read_bytes(), project.read_bytes())


class BlockinessM0ReportTests(unittest.TestCase):
    def test_frozen_report_is_complete(self) -> None:
        if not REPORT.is_file():
            self.skipTest("M0 report has not been generated")
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        self.assertEqual(report["schema"], "av2-encoder-blockiness-m0-v1")
        self.assertEqual(report["status"], "pass")
        self.assertTrue(all(report["gates"].values()))
        self.assertEqual(report["summary"]["case_count"], 33)
        self.assertEqual(
            report["configuration"]["bounded_qindices"],
            [60, 80, 100, 120, 150, 165, 180, 220],
        )
        self.assertEqual(len(report["equal_byte_comparisons"]), 9)
        self.assertIn(
            report["diagnosis"]["classification"],
            {
                "residual-capacity",
                "block-boundaries",
                "both-residual-capacity-and-block-boundaries",
                "inconclusive",
            },
        )


if __name__ == "__main__":
    unittest.main()

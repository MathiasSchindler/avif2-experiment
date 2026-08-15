#!/usr/bin/env python3

import json
import shutil
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))
sys.path.insert(0, str(ROOT / "experimental/encoder/tests"))

from m3_benchmark import distortion, generated_pixels, main, prepared_crop
from m3_q_sweep import main as q_sweep_main
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

    def test_supported_fixture_fails_when_a_decoder_rejects_it(self) -> None:
        encoder = require_executable(
            self,
            ROOT / "experimental/encoder/build/av2-encoder-experiment",
            "experimental encoder",
        )
        project = require_validator(
            self,
            configured_path(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder",
            ),
            "project decoder",
        )
        rejecting_decoder = require_executable(
            self, Path("/bin/false"), "rejecting test decoder"
        )
        work = ROOT / "experimental/encoder/build/m3-rejection-test"
        shutil.rmtree(work, ignore_errors=True)
        output = work / "metrics.jsonl"
        result = main([
            "--fixture=ramp-x-16x8",
            f"--encoder={encoder}",
            f"--avmdec={rejecting_decoder}",
            f"--project-decoder={project}",
            f"--work-dir={work / 'work'}",
            f"--output={output}",
        ])
        self.assertEqual(result, 1)
        record = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(record["status"], "error")
        self.assertEqual(record["reason"],
                         "avmdec rejected the supported encoded fixture")
        self.assertNotEqual(record["avmdec_returncode"], 0)
        shutil.rmtree(work)

    def test_supported_fixture_fails_when_encoder_reports_unsupported(self) -> None:
        work = ROOT / "experimental/encoder/build/m3-unsupported-test"
        shutil.rmtree(work, ignore_errors=True)
        output = work / "metrics.jsonl"
        unsupported = {
            "fixture": "ramp-x-16x8",
            "status": "unsupported",
            "reason": "encoder rejected a supported fixture",
        }
        with patch("m3_benchmark.run_fixture", return_value=unsupported):
            result = main([
                "--fixture=ramp-x-16x8",
                "--encoder=/bin/true",
                "--avmdec=/bin/true",
                "--project-decoder=/bin/true",
                f"--work-dir={work / 'work'}",
                f"--output={output}",
            ])
        self.assertEqual(result, 1)
        record = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(record["status"], "unsupported")
        shutil.rmtree(work)

    def test_suite_locks_bounded_prepared_crops_and_q_sweep(self) -> None:
        suite = json.loads(
            (ROOT / "experimental/encoder/metrics/fixtures.json").read_text()
        )
        prepared = [
            fixture for fixture in suite["fixtures"]
            if fixture["kind"] == "prepared"
        ]
        self.assertEqual(
            {fixture["orientation"] for fixture in prepared},
            {"square", "portrait", "landscape"},
        )
        dimensions = {
            (fixture["width"], fixture["height"]) for fixture in prepared
        }
        self.assertIn((1, 1), dimensions)
        self.assertIn((15, 15), dimensions)
        self.assertIn((16, 16), dimensions)
        self.assertTrue(any(width % 2 or height % 2 for width, height in dimensions))
        self.assertTrue(any(width % 2 == height % 2 == 0
                            for width, height in dimensions))
        for fixture in prepared:
            self.assertTrue(1 <= fixture["width"] <= 16)
            self.assertTrue(1 <= fixture["height"] <= 16)
            self.assertEqual(len(fixture["sha256"]), 64)
            self.assertEqual(len(fixture["source_sha256"]), 64)
            self.assertTrue(fixture["metadata"].endswith("/manifest.json"))
            self.assertEqual(fixture["qindices"], [50, 100, 150, 200])

    def test_locked_crop_extraction_when_prepared_source_is_available(self) -> None:
        suite = json.loads(
            (ROOT / "experimental/encoder/metrics/fixtures.json").read_text()
        )
        fixture = next(
            item for item in suite["fixtures"]
            if item["id"] == "pitti-square-16x16-at-440-440"
        )
        source = ROOT / fixture["path"]
        if not source.is_file():
            self.skipTest("prepared photographic luma is unavailable")
        work = ROOT / "experimental/encoder/build/m3-crop-test"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        path, pixels = prepared_crop(fixture, work)
        self.assertEqual(path, work / "source.i400")
        self.assertEqual(len(pixels), 256)
        self.assertEqual(path.read_bytes(), pixels)
        shutil.rmtree(work)

    def test_prepared_orientations_and_bounded_edges_pass_parity(self) -> None:
        suite = json.loads(
            (ROOT / "experimental/encoder/metrics/fixtures.json").read_text()
        )
        prepared = [
            fixture for fixture in suite["fixtures"]
            if fixture["kind"] == "prepared"
        ]
        if any(not (ROOT / fixture["path"]).is_file() for fixture in prepared):
            self.skipTest("complete prepared photographic corpus is unavailable")
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
        work = ROOT / "experimental/encoder/build/m3-prepared-gate-test"
        shutil.rmtree(work, ignore_errors=True)
        output = work / "metrics.jsonl"
        arguments = [
            f"--encoder={encoder}",
            f"--avmdec={avmdec}",
            f"--project-decoder={project}",
            f"--work-dir={work / 'work'}",
            f"--output={output}",
        ]
        arguments.extend(
            f"--fixture={fixture['id']}" for fixture in prepared
        )
        self.assertEqual(main(arguments), 0)
        records = [
            json.loads(line) for line in output.read_text().splitlines()
        ]
        self.assertEqual(len(records), len(prepared) * 4)
        for record in records:
            self.assertEqual(record["status"], "pass")
            self.assertTrue(record["decoder_exact"])
            self.assertTrue(all(record["local_parity"].values()))
            self.assertIn(record["qindex"], (50, 100, 150, 200))
        shutil.rmtree(work)

    def test_locked_q_sweep_has_exact_parity_and_monotonic_sse(self) -> None:
        suite_path = ROOT / "experimental/encoder/metrics/m3-q-sweep.json"
        suite = json.loads(suite_path.read_text(encoding="utf-8"))
        if any(
            fixture["kind"] == "prepared-crop" and
            not (ROOT / fixture["path"]).is_file()
            for fixture in suite["fixtures"]
        ):
            self.skipTest("prepared q-sweep sources are unavailable")
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
        work = ROOT / "experimental/encoder/build/m3-q-sweep-test"
        shutil.rmtree(work, ignore_errors=True)
        output = work / "metrics.json"
        self.assertEqual(q_sweep_main([
            f"--suite={suite_path}",
            f"--encoder={encoder}",
            f"--avmdec={avmdec}",
            f"--project-decoder={project}",
            f"--work-dir={work / 'work'}",
            f"--output={output}",
        ]), 0)
        metrics = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(metrics["quality_direction"],
                         "lower_qindex_is_higher_quality")
        self.assertTrue(metrics["local_avm_project_parity"])
        self.assertTrue(
            metrics["aggregate_sse_non_increasing_as_quality_increases"]
        )
        self.assertEqual(metrics["case_count"], 30)
        sses = [record["sse"] for record in metrics["aggregates"]]
        self.assertEqual(sses, sorted(sses, reverse=True))
        shutil.rmtree(work)


if __name__ == "__main__":
    unittest.main()

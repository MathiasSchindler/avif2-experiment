#!/usr/bin/env python3

import json
import shutil
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))
sys.path.insert(0, str(ROOT / "experimental/encoder/tests"))

from encoder_oracle import (
    FORMATS,
    OracleError,
    main,
    normalize_raw,
    plane_layout,
)
from validator_support import configured_path, require_validator


class EncoderOracleTests(unittest.TestCase):
    def test_odd_i420_layout_is_canonical_planar(self) -> None:
        self.assertEqual(
            plane_layout(17, 15, FORMATS["yuv420p10le"]),
            [
                {"plane": "Y", "width": 17, "height": 15, "offset_bytes": 0, "size_bytes": 510},
                {"plane": "U", "width": 9, "height": 8, "offset_bytes": 510, "size_bytes": 144},
                {"plane": "V", "width": 9, "height": 8, "offset_bytes": 654, "size_bytes": 144},
            ],
        )

    def test_normalization_rejects_wrong_size(self) -> None:
        work = ROOT / "build/encoder-oracle-tests/wrong-size"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        raw = work / "short.raw"
        raw.write_bytes(b"\0" * 10)
        with self.assertRaisesRegex(OracleError, "raw size"):
            normalize_raw(raw, 16, 16, FORMATS["yuv420p10le"])
        shutil.rmtree(work)

    def test_existing_reference_obu_smoke(self) -> None:
        avmdec = require_validator(
            self,
            configured_path("AVMDEC", ROOT / "build/reference/avm/avmdec"),
            "pinned avmdec",
        )
        decoder = require_validator(
            self,
            configured_path(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder",
            ),
            "project decoder",
        )
        work_root = ROOT / "build/encoder-oracle-tests/smoke"
        shutil.rmtree(work_root, ignore_errors=True)
        work_root.mkdir(parents=True)
        metrics = work_root / "metrics.jsonl"
        result = main(
            [
                str(ROOT / "tests/fixtures/10bit/avm-reference/tiny-precision-16x16-yuv420p10le-qp128.obu"),
                "--width=16",
                "--height=16",
                "--format=yuv420p10le",
                f"--avmdec={avmdec}",
                f"--project-decoder={decoder}",
                f"--work-dir={work_root / 'run'}",
                f"--metrics={metrics}",
                f"--log={work_root / 'commands.jsonl'}",
            ]
        )
        self.assertEqual(result, 0)
        record = json.loads(metrics.read_text(encoding="utf-8").splitlines()[-1])
        self.assertTrue(record["exact_equal"])
        self.assertEqual(record["reference_sha256"], record["project_sha256"])
        shutil.rmtree(work_root)

    def test_monochrome_fixture_reference_smoke(self) -> None:
        avmdec = require_validator(
            self,
            configured_path("AVMDEC", ROOT / "build/reference/avm/avmdec"),
            "pinned avmdec",
        )
        fixture = (
            ROOT
            / "experimental/encoder/fixtures/avm-v1.0.0-monochrome"
            / "constant-128-8x8.obu"
        )
        work_root = ROOT / "experimental/encoder/build/oracle-fixture-test"
        shutil.rmtree(work_root, ignore_errors=True)
        work_root.mkdir(parents=True)
        metrics = work_root / "metrics.jsonl"
        result = main(
            [
                str(fixture),
                "--width=8",
                "--height=8",
                "--format=gray8",
                f"--avmdec={avmdec}",
                "--reference-only",
                f"--work-dir={work_root / 'run'}",
                f"--metrics={metrics}",
                f"--log={work_root / 'commands.jsonl'}",
            ]
        )
        self.assertEqual(result, 0)
        record = json.loads(metrics.read_text(encoding="utf-8").splitlines()[-1])
        self.assertEqual(record["normalized_size_bytes"], 64)
        self.assertEqual(
            record["reference_sha256"],
            "1df1b7ce1fd8fcbe20cde61646875e54fe38d8945ea7911afd59e025cc520a68",
        )
        shutil.rmtree(work_root)


if __name__ == "__main__":
    unittest.main()

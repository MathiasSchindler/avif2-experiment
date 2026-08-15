#!/usr/bin/env python3

import hashlib
import json
import shutil
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]
sys.path.insert(0, str(ENCODER / "tools"))
sys.path.insert(0, str(ENCODER / "tests"))

from m2_full_stream_matrix import (
    FIXTURES,
    MANIFEST,
    PINNED_AVMDEC_SHA256,
    check_generated,
    validate,
)
from m3_dc_model import choose_dc_level
from validator_support import configured_path, require_executable, require_validator


class M2FullStreamMatrixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.encoder = require_executable(
            self, ENCODER / "build/av2-encoder-experiment",
            "experimental encoder")

    def test_current_encoder_reproduces_frozen_streams(self) -> None:
        check_generated(self.encoder)

    def test_dc_cases_reach_base_br_and_remainder_thresholds(self) -> None:
        self.assertEqual(
            [
                choose_dc_level([value] * 64, 128, 8, 100).quantized_dc
                for value in (129, 135, 139)
            ],
            [1, 5, 8],
        )

    def test_pinned_decoders_have_exact_matrix_parity(self) -> None:
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
        self.assertEqual(
            hashlib.sha256(avmdec.read_bytes()).hexdigest(),
            PINNED_AVMDEC_SHA256,
        )
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        work = ENCODER / "build/m2-full-stream-test"
        shutil.rmtree(work, ignore_errors=True)
        try:
            actual = validate(manifest["cases"], avmdec, project, work)
        finally:
            shutil.rmtree(work, ignore_errors=True)
        self.assertEqual(actual, manifest["decoded"])
        self.assertEqual(
            {item for case in manifest["cases"] for item in case["coverage"]},
            {
                "coefficient-free", "M1", "DC", "base", "BR", "remainder",
                "M3", "multi-block", "CDF-adaptation", "AC(1,0)", "sign",
                "M4",
            },
        )


if __name__ == "__main__":
    unittest.main()

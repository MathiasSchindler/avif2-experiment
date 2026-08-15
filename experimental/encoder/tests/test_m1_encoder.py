#!/usr/bin/env python3

import hashlib
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]
sys.path.insert(0, str(ENCODER / "tools"))
sys.path.insert(0, str(ENCODER / "tests"))

from m1_syntax import (
    decode_frame_header_8x8,
    decode_sequence_header_8x8,
    parse_annexb,
)
from validator_support import configured_path, require_executable, require_validator


class M1EncoderTests(unittest.TestCase):
    EXPECTED_HASH = "4c13e82fce9975d573b92b8ca2da25e1fed90308ac6857ec4d2d92dba0268b11"

    def setUp(self) -> None:
        self.binary = ENCODER / "build/av2-encoder-experiment"
        require_executable(self, self.binary, "experimental encoder")
        self.work = ENCODER / "build/m1-encoder-test"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.work, ignore_errors=True)

    def encode(self, name: str) -> Path:
        output = self.work / name
        subprocess.run(
            [str(self.binary), "--constant-128-8x8", str(output)],
            check=True,
        )
        return output

    def test_deterministic_golden_fields(self) -> None:
        first = self.encode("first.obu").read_bytes()
        second = self.encode("second.obu").read_bytes()
        self.assertEqual(first, second)
        self.assertEqual(len(first), 21)
        self.assertEqual(hashlib.sha256(first).hexdigest(), self.EXPECTED_HASH)
        units = parse_annexb(first)
        self.assertEqual([kind for kind, _ in units], [2, 1, 4])
        decode_sequence_header_8x8(units[1][1])
        _, tile = decode_frame_header_8x8(units[2][1])
        self.assertEqual(tile, bytes.fromhex("8d80"))

    def test_pinned_decoders_emit_constant_plane(self) -> None:
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
        obu = self.encode("decode.obu")
        expected = bytes([128]) * 64
        avm_raw = self.work / "avm.raw"
        subprocess.run(
            [
                str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.assertEqual(avm_raw.read_bytes(), expected)
        project_raw = self.work / "project.i400"
        subprocess.run(
            [str(project), str(obu), "-o", str(project_raw)],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.assertEqual(project_raw.read_bytes(), expected)

    def test_cli_errors_do_not_create_output(self) -> None:
        output = self.work / "bad.obu"
        completed = subprocess.run(
            [str(self.binary), "--unknown", str(output)],
            check=False,
        )
        self.assertEqual(completed.returncode, 2)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

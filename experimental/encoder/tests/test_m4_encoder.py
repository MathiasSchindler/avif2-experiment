#!/usr/bin/env python3

import hashlib
import re
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]
sys.path.insert(0, str(ENCODER / "tests"))

from validator_support import configured_path, require_executable, require_validator


class M4EncoderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.binary = ENCODER / "build/av2-encoder-experiment"
        require_executable(self, self.binary, "experimental encoder")
        self.avmdec = require_validator(
            self,
            configured_path("AVMDEC", ROOT / "build/reference/avm/avmdec"),
            "pinned avmdec",
        )
        self.project = require_validator(
            self,
            configured_path(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder",
            ),
            "project decoder",
        )
        self.work = ENCODER / "build/m4-encoder-test"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.work, ignore_errors=True)

    def encode(self, name: str, source_bytes: bytes) -> Path:
        source = self.work / f"{name}.i400"
        obu = self.work / f"{name}.obu"
        source.write_bytes(source_bytes)
        subprocess.run(
            [
                str(self.binary), "--experimental-m4-ac",
                str(source), str(obu),
            ],
            check=True, text=True, capture_output=True,
        )
        self.assertGreater(obu.stat().st_size, 0)
        return obu

    def assert_decoder_parity(self, name: str, source_bytes: bytes) -> None:
        obu = self.encode(name, source_bytes)
        avm = self.work / f"{name}.avm.i400"
        project = self.work / f"{name}.project.i400"
        subprocess.run(
            [
                str(self.avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm), str(obu),
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            [str(self.project), str(obu), "-o", str(project)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(len(avm.read_bytes()), 64)
        self.assertEqual(avm.read_bytes(), project.read_bytes())

    def test_all_constant_bytes_emit_streams(self) -> None:
        for value in range(256):
            with self.subTest(value=value):
                self.encode(f"constant-{value}", bytes([value]) * 64)

    def test_representative_full_range_patterns_decoder_parity(self) -> None:
        patterns = {
            "all-zero": bytes(64),
            "all-255": bytes([255]) * 64,
            "ramp": bytes(round(index * 255 / 63) for index in range(64)),
            "checker": bytes(
                0 if (x + y) % 2 == 0 else 255
                for y in range(8) for x in range(8)
            ),
            "impulse": bytes([255]) + bytes(63),
        }
        for name, source_bytes in patterns.items():
            with self.subTest(name=name):
                self.assert_decoder_parity(name, source_bytes)

    def test_vertical_gradient_decoder_parity(self) -> None:
        source_bytes = b"".join(
            bytes([value]) * 8
            for value in (128, 128, 128, 129, 129, 129, 130, 130)
        )
        source = self.work / "gradient.i400"
        obu = self.work / "gradient.obu"
        repeat = self.work / "repeat.obu"
        source.write_bytes(source_bytes)
        command = [
            str(self.binary), "--experimental-m4-ac",
            str(source), str(obu),
        ]
        first = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
        command[-1] = str(repeat)
        second = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
        self.assertEqual(obu.read_bytes(), repeat.read_bytes())
        self.assertEqual(first.stderr, second.stderr)
        avm = self.work / "avm.raw"
        project = self.work / "project.i400"
        subprocess.run(
            [
                str(self.avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm), str(obu),
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            [str(self.project), str(obu), "-o", str(project)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(avm.read_bytes(), source_bytes)
        self.assertEqual(project.read_bytes(), source_bytes)
        match = re.fullmatch(
            r"encoder: bytes=(\d+) sse_y=0 psnr_y=infdB "
            r"workspace_bytes=64\n",
            first.stderr,
        )
        self.assertIsNotNone(match)
        self.assertEqual(int(match.group(1)), len(obu.read_bytes()))
        self.assertEqual(len(obu.read_bytes()), 21)
        self.assertEqual(
            hashlib.sha256(obu.read_bytes()).hexdigest(),
            "9d84278811a478aaece5b87bf9340e47d1dfd20badc6af0765776840f8d98c15",
        )
        self.assertEqual(
            hashlib.sha256(avm.read_bytes()).hexdigest(),
            hashlib.sha256(project.read_bytes()).hexdigest(),
        )


if __name__ == "__main__":
    unittest.main()

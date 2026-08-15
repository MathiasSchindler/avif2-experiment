#!/usr/bin/env python3

import math
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


class M3EncoderTests(unittest.TestCase):
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
        self.work = ENCODER / "build/m3-encoder-test"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.work, ignore_errors=True)

    @staticmethod
    def round2(value: int, bits: int) -> int:
        return (value + (1 << (bits - 1))) >> bits

    @classmethod
    def reconstruct_level(cls, level: int, prediction: int) -> int:
        magnitude = cls.round2(abs(level) * 704, 3)
        dequant = max(-32768, min(32767, -magnitude if level < 0 else magnitude))
        row = max(-32768, min(32767, cls.round2(64 * dequant, 7)))
        residual = max(-256, min(255, cls.round2(64 * row, 11)))
        return max(0, min(255, prediction + residual))

    @classmethod
    def local_reconstruction(
        cls, samples: bytes, width: int, height: int
    ) -> tuple[bytes, int]:
        result = bytearray(width * height)
        total_error = 0
        for by in range(0, height, 8):
            for bx in range(0, width, 8):
                edges = []
                if by:
                    edges.extend(result[(by - 1) * width + bx : (by - 1) * width + bx + 8])
                if bx:
                    edges.extend(result[(by + y) * width + bx - 1] for y in range(8))
                prediction = 128 if not edges else cls.round2(sum(edges), len(edges).bit_length() - 1)
                block = [
                    samples[(by + y) * width + bx + x]
                    for y in range(8) for x in range(8)
                ]
                candidates = []
                for level in [0] + [
                    signed
                    for magnitude in range(1, 375)
                    for signed in (magnitude, -magnitude)
                ]:
                    value = cls.reconstruct_level(level, prediction)
                    error = sum((sample - value) ** 2 for sample in block)
                    candidates.append((level, value, error))
                level, value, error = min(
                    candidates,
                    key=lambda item: (item[2], abs(item[0]), item[0] < 0),
                )
                del level
                total_error += error
                for y in range(8):
                    result[(by + y) * width + bx : (by + y) * width + bx + 8] = bytes([value]) * 8
        return bytes(result), total_error

    def encode(
        self, name: str, packed: bytes, width: int, height: int,
        stride: int | None = None,
    ) -> tuple[Path, str]:
        if stride is None:
            stride = width
        source = self.work / f"{name}.i400"
        output = self.work / f"{name}.obu"
        if stride == width:
            source.write_bytes(packed)
        else:
            rows = [
                packed[y * width : y * width + width] +
                bytes([0xA5]) * (stride - width)
                for y in range(height)
            ]
            source.write_bytes(b"".join(rows))
        command = [
            str(self.binary), "--i400-8", str(source),
            "--width", str(width), "--height", str(height),
        ]
        if stride != width:
            command += ["--stride", str(stride)]
        command.append(str(output))
        completed = subprocess.run(
            command, check=True, text=True, capture_output=True
        )
        return output, completed.stderr

    def decode(self, name: str, obu: Path) -> tuple[bytes, bytes]:
        avm = self.work / f"{name}.avm.raw"
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
        return avm.read_bytes(), project.read_bytes()

    def check_case(
        self, name: str, samples: bytes, width: int = 8, height: int = 8,
        stride: int | None = None,
    ) -> None:
        expected, error = self.local_reconstruction(samples, width, height)
        first, metrics = self.encode(name, samples, width, height, stride)
        first_bytes = first.read_bytes()
        second, second_metrics = self.encode(
            f"{name}-repeat", samples, width, height, stride
        )
        self.assertEqual(first_bytes, second.read_bytes())
        self.assertEqual(metrics, second_metrics)
        avm, project = self.decode(name, first)
        self.assertEqual(avm, expected)
        self.assertEqual(project, expected)
        match = re.fullmatch(
            r"encoder: bytes=(\d+) sse_y=(\d+) psnr_y=(inf|\d+\.\d\d)dB "
            rf"workspace_bytes={width * height}\n",
            metrics,
        )
        self.assertIsNotNone(match)
        self.assertEqual(int(match.group(1)), len(first_bytes))
        self.assertEqual(int(match.group(2)), error)
        if error == 0:
            self.assertEqual(match.group(3), "inf")
        else:
            expected_psnr = 10 * math.log10(
                255 * 255 * width * height / error
            )
            self.assertAlmostEqual(float(match.group(3)), expected_psnr, places=2)

    def test_generated_flat_ramp_impulse(self) -> None:
        self.check_case("flat", bytes([128]) * 64)
        self.check_case("ramp", bytes(range(64)), stride=11)
        impulse = bytearray(64)
        impulse[27] = 255
        self.check_case("impulse", bytes(impulse))

    def test_16x16_grid_uses_reconstructed_neighbors(self) -> None:
        values = ((128, 200), (50, 150))
        samples = bytearray(256)
        for block_y in range(2):
            for block_x in range(2):
                for y in range(8):
                    start = (block_y * 8 + y) * 16 + block_x * 8
                    samples[start : start + 8] = bytes(
                        [values[block_y][block_x]]
                    ) * 8
        self.check_case("grid-16x16", bytes(samples), 16, 16, 19)

    def test_prepared_luma_crop_when_available(self) -> None:
        prepared = (
            ROOT / "images/prepared/le896/baya-weaver/"
            "source-896x598-gray8.i400"
        )
        if not prepared.is_file():
            self.skipTest("prepared photographic luma is unavailable")
        source = prepared.read_bytes()
        crop = b"".join(source[y * 896 : y * 896 + 8] for y in range(8))
        self.check_case("prepared-crop", crop)
        crop16 = b"".join(
            source[y * 896 : y * 896 + 16] for y in range(16)
        )
        self.check_case("prepared-crop-16x16", crop16, 16, 16)

    def test_rectangular_grids(self) -> None:
        for width, height in ((8, 16), (16, 8)):
            samples = bytearray(width * height)
            for by in range(0, height, 8):
                for bx in range(0, width, 8):
                    value = (31 + bx * 7 + by * 11) & 255
                    for y in range(8):
                        start = (by + y) * width + bx
                        samples[start : start + 8] = bytes([value]) * 8
            self.check_case(
                f"grid-{width}x{height}", bytes(samples),
                width, height, width + 3,
            )

    def test_non_power_of_two_grid_is_unsupported(self) -> None:
        source = self.work / "wide.i400"
        output = self.work / "wide.obu"
        source.write_bytes(bytes(32 * 32))
        completed = subprocess.run(
            [
                str(self.binary), "--i400-8", str(source),
                "--width", "32", "--height", "32", str(output),
            ],
            check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(completed.returncode, 5)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

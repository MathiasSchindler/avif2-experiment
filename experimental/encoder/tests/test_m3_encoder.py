#!/usr/bin/env python3

import math
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import unittest
import importlib.util
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]
sys.path.insert(0, str(ENCODER / "tests"))

from validator_support import configured_path, require_executable, require_validator

sys.path.insert(0, str(ENCODER / "tools"))
PARTITION_SPEC = importlib.util.spec_from_file_location(
    "m3_encoder_partitions", ENCODER / "tools/m3_large_partition_model.py"
)
assert PARTITION_SPEC and PARTITION_SPEC.loader
PARTITIONS = importlib.util.module_from_spec(PARTITION_SPEC)
sys.modules[PARTITION_SPEC.name] = PARTITIONS
PARTITION_SPEC.loader.exec_module(PARTITIONS)
MULTISB_SPEC = importlib.util.spec_from_file_location(
    "m3_encoder_multisuperblock",
    ENCODER / "tools/m3_multisuperblock_model.py",
)
assert MULTISB_SPEC and MULTISB_SPEC.loader
MULTISB = importlib.util.module_from_spec(MULTISB_SPEC)
sys.modules[MULTISB_SPEC.name] = MULTISB
MULTISB_SPEC.loader.exec_module(MULTISB)


class M3EncoderTests(unittest.TestCase):
    AC_QLOOKUP = (
        64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
        57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
    )

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
        self.work = ENCODER / f"build/m3-encoder-test-{os.getpid()}"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.work, ignore_errors=True)

    @staticmethod
    def round2(value: int, bits: int) -> int:
        return (value + (1 << (bits - 1))) >> bits

    @classmethod
    def qlookup(cls, qindex: int) -> int:
        if qindex < 25:
            return cls.AC_QLOOKUP[qindex]
        return (
            cls.AC_QLOOKUP[((qindex - 1) % 24) + 1]
            << ((qindex - 1) // 24)
        )

    @classmethod
    def reconstruct_level(
        cls, level: int, prediction: int, qindex: int
    ) -> int:
        magnitude = cls.round2(abs(level) * cls.qlookup(qindex), 3)
        dequant = max(-32768, min(32767, -magnitude if level < 0 else magnitude))
        row = max(-32768, min(32767, cls.round2(64 * dequant, 7)))
        residual = max(-256, min(255, cls.round2(64 * row, 11)))
        return max(0, min(255, prediction + residual))

    @classmethod
    def local_reconstruction(
        cls, samples: bytes, width: int, height: int, qindex: int = 100
    ) -> tuple[bytes, int]:
        coded_width = ((width + 7) // 8) * 8
        coded_height = ((height + 7) // 8) * 8
        padded = bytearray(coded_width * coded_height)
        total_error = 0
        if width <= 16 and height <= 16:
            transforms = [
                (x, y)
                for y in range(0, coded_height, 8)
                for x in range(0, coded_width, 8)
            ]
        elif width <= 64 and height <= 64 and width >= 17 and height >= 17:
            transforms = [
                (int(leaf["x"]) + x, int(leaf["y"]) + y)
                for leaf in PARTITIONS.model(width, height)["leaves"]
                for y in range(0, int(leaf["height"]), 8)
                for x in range(0, int(leaf["width"]), 8)
            ]
        else:
            traversal = MULTISB.model(width, height)
            assert traversal["transform_exact_cover"]
            transforms = traversal["transform_order"]
        for bx, by in transforms:
                edges = []
                if by:
                    edges.extend(
                        padded[
                            (by - 1) * coded_width + bx:
                            (by - 1) * coded_width + bx + 8
                        ]
                    )
                if bx:
                    edges.extend(
                        padded[(by + y) * coded_width + bx - 1]
                        for y in range(8)
                    )
                prediction = 128 if not edges else cls.round2(sum(edges), len(edges).bit_length() - 1)
                block = [
                    samples[
                        min(by + y, height - 1) * width +
                        min(bx + x, width - 1)
                    ]
                    for y in range(8) for x in range(8)
                ]
                candidates = []
                maximum = (
                    ((1 << 15) * 8 + cls.qlookup(qindex) - 1)
                    // cls.qlookup(qindex)
                    + 1
                )
                for level in [0] + [
                    signed
                    for magnitude in range(1, maximum + 1)
                    for signed in (magnitude, -magnitude)
                ]:
                    value = cls.reconstruct_level(level, prediction, qindex)
                    error = sum((sample - value) ** 2 for sample in block)
                    candidates.append((level, value, error))
                    if error == 0:
                        break
                level, value, error = min(
                    candidates,
                    key=lambda item: (item[2], abs(item[0]), item[0] < 0),
                )
                del level
                del error
                for y in range(by, min(by + 8, height)):
                    for x in range(bx, min(bx + 8, width)):
                        total_error += (samples[y * width + x] - value) ** 2
                for y in range(8):
                    start = (by + y) * coded_width + bx
                    padded[start : start + 8] = bytes([value]) * 8
        result = b"".join(
            padded[y * coded_width : y * coded_width + width]
            for y in range(height)
        )
        return result, total_error

    def encode(
        self, name: str, packed: bytes, width: int, height: int,
        stride: int | None = None, qindex: int = 100,
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
        if qindex != 100:
            command += ["--qindex", str(qindex)]
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
        stride: int | None = None, qindex: int = 100,
        repeat: bool = True,
    ) -> None:
        expected, error = self.local_reconstruction(
            samples, width, height, qindex
        )
        first, metrics = self.encode(
            name, samples, width, height, stride, qindex
        )
        first_bytes = first.read_bytes()
        if repeat:
            second, second_metrics = self.encode(
                f"{name}-repeat", samples, width, height, stride, qindex
            )
            self.assertEqual(first_bytes, second.read_bytes())
            self.assertEqual(metrics, second_metrics)
        avm, project = self.decode(name, first)
        self.assertEqual(avm, expected)
        self.assertEqual(project, expected)
        block_columns = (width + 7) // 8
        block_rows = (height + 7) // 8
        base_workspace = (
            block_columns * 8 * block_rows * 8 + width * height + 1
        ) // 2 * 2 + block_columns * block_rows * 2
        stream_capacity = (
            256 if width <= 16 and height <= 16
            else block_columns * block_rows * 64 + 256
        )
        workspace_size = base_workspace + (
            stream_capacity
            if width <= 16 and height <= 16
            else stream_capacity * 3
        )
        match = re.fullmatch(
            r"encoder: bytes=(\d+) sse_y=(\d+) psnr_y=(inf|\d+\.\d\d)dB "
            rf"workspace_bytes={workspace_size} qindex={qindex}\n",
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
        crop_odd = b"".join(
            source[y * 896 + 17 : y * 896 + 30] for y in range(11)
        )
        self.check_case("prepared-crop-13x11", crop_odd, 13, 11)

    def test_multisuperblock_representatives(self) -> None:
        for width, height in (
            (65, 1), (1, 65), (65, 65), (127, 64), (64, 127),
            (127, 127), (128, 128), (129, 129),
        ):
            samples = bytes(
                (
                    x * 17 + y * 29
                    + (((x // 5) ^ (y // 7)) & 1) * 61
                    + (x * y) % 23
                ) & 255
                for y in range(height)
                for x in range(width)
            )
            self.check_case(
                f"multisb-{width}x{height}", samples, width, height,
                width + 3, qindex=191, repeat=False,
            )

    def test_multisuperblock_q_contexts(self) -> None:
        width = height = 65
        samples = bytes(
            (x * 23 + y * 41 + x * y * 7 + (x ^ y) * 11) & 255
            for y in range(height)
            for x in range(width)
        )
        for qindex in (1, 100, 141, 191):
            self.check_case(
                f"multisb-q-{qindex}", samples, width, height,
                qindex=qindex, repeat=False,
            )

    def test_smallest_selected_prepared_image_when_available(self) -> None:
        prepared = (
            ROOT / "images/prepared/le896/hong-kong-harbour-night/"
            "source-896x424-gray8.i400"
        )
        if not prepared.is_file():
            self.skipTest("smallest selected prepared image is unavailable")
        first, _ = self.encode(
            "prepared-full-896x424", prepared.read_bytes(), 896, 424
        )
        second, _ = self.encode(
            "prepared-full-896x424-repeat", prepared.read_bytes(), 896, 424
        )
        self.assertEqual(first.read_bytes(), second.read_bytes())
        avm, project = self.decode("prepared-full-896x424", first)
        self.assertEqual(avm, project)
        self.assertEqual(len(avm), 896 * 424)

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

    def test_all_dimensions_one_through_sixteen(self) -> None:
        for height in range(1, 17):
            for width in range(1, 17):
                samples = bytes(
                    (x * 17 + y * 31 + x * y * 3) & 255
                    for y in range(height)
                    for x in range(width)
                )
                self.check_case(
                    f"matrix-{width}x{height}", samples, width, height,
                    width + 3, repeat=False,
                )

    def test_odd_generated_cases_and_q_sweep(self) -> None:
        width, height = 13, 11
        ramp = bytes(
            (x * 255 + (width - 1) // 2) // (width - 1)
            for _y in range(height)
            for x in range(width)
        )
        impulse = bytearray(width * height)
        impulse[(height // 2) * width + width // 2] = 255
        self.check_case("odd-ramp", ramp, width, height, width + 5)
        self.check_case(
            "odd-impulse", bytes(impulse), width, height, width + 3
        )
        for qindex in (191, 210, 230, 250, 255):
            self.check_case(
                f"q-{qindex}", ramp, width, height, qindex=qindex
            )

    def test_qindex_validation_and_default_stream_compatibility(self) -> None:
        samples = bytes(range(64))
        default, _ = self.encode("q-default", samples, 8, 8)
        source = self.work / "q-explicit.i400"
        explicit = self.work / "q-explicit.obu"
        source.write_bytes(samples)
        subprocess.run(
            [
                str(self.binary), "--i400-8", str(source),
                "--width", "8", "--height", "8",
                "--qindex", "100", str(explicit),
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(default.read_bytes(), explicit.read_bytes())
        for value in ("0", "256", "bad"):
            output = self.work / f"q-invalid-{value}.obu"
            completed = subprocess.run(
                [
                    str(self.binary), "--i400-8", str(source),
                    "--width", "8", "--height", "8",
                    "--qindex", value, str(output),
                ],
                check=False, text=True, capture_output=True,
            )
            self.assertEqual(completed.returncode, 3)
            self.assertIn("qindex must be in 1..255", completed.stderr)
            self.assertFalse(output.exists())

    def test_mixed_axis_dimensions_are_supported(self) -> None:
        source = self.work / "wide.i400"
        output = self.work / "wide.obu"
        source.write_bytes(bytes(17 * 16))
        completed = subprocess.run(
            [
                str(self.binary), "--i400-8", str(source),
                "--width", "17", "--height", "16", str(output),
            ],
            check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        self.assertEqual(completed.returncode, 0)
        self.assertTrue(output.exists())

    def test_large_representative_patterns_and_q_contexts(self) -> None:
        width, height = 17, 17
        patterns = {
            "flat": bytes([83]) * (width * height),
            "ramp": bytes(
                (x * 11 + y * 7) & 255
                for y in range(height) for x in range(width)
            ),
            "odd": bytes(
                (x * 17 + y * 31 + x * y * 3 + (x ^ y) * 5) & 255
                for y in range(height) for x in range(width)
            ),
        }
        impulse = bytearray(width * height)
        impulse[(height // 2) * width + width // 2] = 255
        patterns["impulse"] = bytes(impulse)
        for name, samples in patterns.items():
            self.check_case(
                f"large-{name}", samples, width, height, width + 3
            )
        for qindex in (1, 100, 141, 191):
            self.check_case(
                f"large-q-{qindex}", patterns["odd"], width, height,
                width + 1, qindex,
            )

    def test_all_dimensions_seventeen_through_sixty_four(self) -> None:
        frozen = json.loads(
            (ENCODER / "tests/m3_production_matrix.json").read_text()
        )
        digest = hashlib.sha256()
        case_count = 0
        for height in range(17, 65):
            for width in range(17, 65):
                samples = bytes(
                    (
                        x * 17 + y * 31 + x * y * 3 +
                        (x ^ y) * 5 + (x & 1) * 29 + (y & 1) * 47
                    ) & 255
                    for y in range(height)
                    for x in range(width)
                )
                expected, error = self.local_reconstruction(
                    samples, width, height
                )
                obu, _metrics = self.encode(
                    f"production-{width}x{height}", samples, width, height,
                    width + 3,
                )
                stream = obu.read_bytes()
                avm, project = self.decode(
                    f"production-{width}x{height}", obu
                )
                self.assertEqual(avm, expected, (width, height, "avm"))
                self.assertEqual(project, expected, (width, height, "project"))
                record = (
                    f"{width}x{height} {len(stream)} "
                    f"{hashlib.sha256(stream).hexdigest()} "
                    f"{hashlib.sha256(expected).hexdigest()} {error}\n"
                )
                digest.update(record.encode("ascii"))
                case_count += 1
        self.assertEqual(case_count, frozen["case_count"])
        self.assertEqual(digest.hexdigest(), frozen["aggregate_sha256"])


if __name__ == "__main__":
    unittest.main()

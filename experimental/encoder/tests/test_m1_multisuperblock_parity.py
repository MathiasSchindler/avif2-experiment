#!/usr/bin/env python3

import hashlib
import os
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
DRIVER = ENCODER / "build" / "m1-multisuperblock-parity-driver"
AVMDEC = Path(os.environ.get(
    "AVMDEC", ROOT / "build" / "reference" / "avm" / "avmdec"))
PROJECT_DECODER = Path(os.environ.get(
    "PROJECT_DECODER", ROOT / "build" / "host-cmake" / "av2heif-decoder"))
WIDTH = HEIGHT = 128
Y_SIZE = WIDTH * HEIGHT
CHROMA_SIZE = (WIDTH // 2) * (HEIGHT // 2)
EXPECTED_METRICS = (2519, 21952511, 3364051, 3935350, 8, 16)
EXPECTED_OBU_SHA256 = \
    "07aeb8fb4e60b92a0073dff3612bc4f791ead5f1b73c58e7afafe3d8120f12f3"
EXPECTED_RECONSTRUCTION_SHA256 = \
    "db2d454a28fd8c4bb56646c9c04bd45c8988397d31beb401b6e1a62a7012d166"


def source_bytes() -> bytes:
    y = bytes(
        (11 * x + 7 * row + (x * row) // 17) & 255
        for row in range(HEIGHT) for x in range(WIDTH)
    )
    u = bytes(
        (64 + 9 * x + 3 * row) & 255
        for row in range(HEIGHT // 2) for x in range(WIDTH // 2)
    )
    v = bytes(
        (192 + 5 * x + 13 * row) & 255
        for row in range(HEIGHT // 2) for x in range(WIDTH // 2)
    )
    return y + u + v


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def plane_sse(source: bytes, reconstruction: bytes) -> tuple[int, int, int]:
    offsets = (0, Y_SIZE, Y_SIZE + CHROMA_SIZE, len(source))
    return tuple(
        sum((left - right) ** 2 for left, right in zip(
            source[offsets[index]:offsets[index + 1]],
            reconstruction[offsets[index]:offsets[index + 1]],
        ))
        for index in range(3)
    )


def first_difference(left: bytes, right: bytes) -> str:
    offsets = (0, Y_SIZE, Y_SIZE + CHROMA_SIZE)
    widths = (WIDTH, WIDTH // 2, WIDTH // 2)
    for index, (left_sample, right_sample) in enumerate(zip(left, right)):
        if left_sample == right_sample:
            continue
        plane = 0 if index < offsets[1] else 1 if index < offsets[2] else 2
        position = index - offsets[plane]
        x = position % widths[plane]
        y = position // widths[plane]
        scale = 1 if plane == 0 else 2
        return (
            f"plane={'yuv'[plane]} sample=({x},{y}) "
            f"luma_block=({x * scale // 8 * 8},{y * scale // 8 * 8}) "
            f"superblock=({x * scale // 64 * 64},{y * scale // 64 * 64}) "
            f"local={left_sample} decoded={right_sample}"
        )
    return "length mismatch"


class M1MultisuperblockParityTests(unittest.TestCase):
    def setUp(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("external validator integration disabled")
        for path in (DRIVER, AVMDEC, PROJECT_DECODER):
            if not path.is_file():
                self.skipTest(f"required executable is unavailable: {path}")
        self.work = ENCODER / "build" / "m1-multisuperblock-parity-test"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        if hasattr(self, "work"):
            shutil.rmtree(self.work, ignore_errors=True)

    def run_driver(self, suffix: str) -> tuple[bytes, bytes, tuple[int, ...]]:
        obu = self.work / f"encoded-{suffix}.obu"
        local = self.work / f"local-{suffix}.i420"
        completed = subprocess.run(
            [str(DRIVER), str(obu), str(local)],
            cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        metrics = tuple(map(int, completed.stdout.split()))
        return obu.read_bytes(), local.read_bytes(), metrics

    def decode(self, obu: Path, output: Path, project: bool) -> bytes:
        command = (
            [str(PROJECT_DECODER), str(obu), "-o", str(output)]
            if project else
            [
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(output), str(obu),
            ]
        )
        completed = subprocess.run(
            command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        return output.read_bytes()

    def test_128x128_i420_rd_local_reconstruction_matches_decoders(self) -> None:
        obu, local, metrics = self.run_driver("first")
        second_obu, second_local, second_metrics = self.run_driver("second")
        self.assertEqual(metrics, EXPECTED_METRICS)
        self.assertEqual(second_metrics, metrics)
        self.assertEqual(second_obu, obu)
        self.assertEqual(second_local, local)
        self.assertEqual(sha256(obu), EXPECTED_OBU_SHA256)
        self.assertEqual(sha256(local), EXPECTED_RECONSTRUCTION_SHA256)

        obu_path = self.work / "locked.obu"
        obu_path.write_bytes(obu)
        avm = self.decode(obu_path, self.work / "decoded.avm.i420", False)
        project = self.decode(
            obu_path, self.work / "decoded.project.i420", True)
        self.assertEqual(avm, project)
        self.assertEqual(
            local, avm,
            f"first reconstruction mismatch: {first_difference(local, avm)}",
        )
        sse = plane_sse(source_bytes(), local)
        self.assertEqual(sse, EXPECTED_METRICS[1:4])


if __name__ == "__main__":
    unittest.main()

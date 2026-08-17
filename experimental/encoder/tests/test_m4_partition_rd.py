#!/usr/bin/env python3

import os
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
BUILD = ENCODER / "build"
ENCODER_BIN = BUILD / "av2-encoder-experiment"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"


def run(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, timeout=600)


def metrics(stderr: bytes) -> dict[str, int]:
    result: dict[str, int] = {}
    for token in stderr.decode("utf-8", errors="replace").split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if value.lstrip("-").isdigit():
            result[key] = int(value)
    return result


class M4PartitionRdTests(unittest.TestCase):
    def require_tools(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("external validator integration disabled")
        for path in (ENCODER_BIN, AVMDEC, PROJECT_DECODER):
            if not path.is_file():
                self.skipTest(f"required executable is unavailable: {path}")

    def encode(self, source: Path, output: Path, search: str,
               width: int, height: int) -> dict[str, int]:
        completed = run([
            str(ENCODER_BIN), str(source), "--width", str(width),
            "--height", str(height), "--qindex", "100",
            "--search", search, "--output", str(output),
        ])
        self.assertEqual(
            completed.returncode, 0,
            completed.stderr.decode("utf-8", errors="replace"))
        return metrics(completed.stderr)

    def decode(self, stream: Path, avm: Path, project: Path) -> bytes:
        commands = (
            [
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm), str(stream),
            ],
            [str(PROJECT_DECODER), str(stream), "-o", str(project)],
        )
        for command in commands:
            completed = run(command)
            self.assertEqual(
                completed.returncode, 0,
                completed.stderr.decode("utf-8", errors="replace"))
        decoded = avm.read_bytes()
        self.assertEqual(decoded, project.read_bytes())
        return decoded

    def test_i400_shapes_boundaries_and_parity(self) -> None:
        self.require_tools()
        work = BUILD / "m4-partition-rd-parity"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        cases = {
            "flat": (32, 32, bytes([128]) * (32 * 32)),
            "smooth": (
                32, 32,
                bytes(64 + x // 16 + y // 32
                      for y in range(32) for x in range(32)),
            ),
            "edge": (
                32, 32,
                bytes(32 if x < 15 + y // 8 else 224
                      for y in range(32) for x in range(32)),
            ),
            "checker": (
                32, 32,
                bytes(32 if (x + y) & 1 else 224
                      for y in range(32) for x in range(32)),
            ),
            "odd": (
                31, 17,
                bytes((x * 13 + y * 7) & 255
                      for y in range(17) for x in range(31)),
            ),
            "multisuperblock": (
                128, 64,
                bytes(64 + x // 16 + y // 32
                      for y in range(64) for x in range(128)),
            ),
        }
        large = 0
        small = 0
        try:
            for name, (width, height, source_bytes) in cases.items():
                with self.subTest(name=name):
                    source = work / f"{name}-{width}x{height}.i400"
                    first = work / f"{name}.first.obu"
                    repeat = work / f"{name}.repeat.obu"
                    source.write_bytes(source_bytes)
                    first_metrics = self.encode(
                        source, first, "partition-rd", width, height)
                    repeat_metrics = self.encode(
                        source, repeat, "partition-rd", width, height)
                    self.assertEqual(first.read_bytes(), repeat.read_bytes())
                    self.assertEqual(first_metrics, repeat_metrics)
                    decoded = self.decode(
                        first, work / f"{name}.avm.i400",
                        work / f"{name}.project.i400")
                    self.assertEqual(len(decoded), len(source_bytes))
                    self.assertEqual(
                        sum((a - b) ** 2
                            for a, b in zip(source_bytes, decoded)),
                        first_metrics["sse_y"])
                    large += first_metrics["rd_candidate_transform_16x16"]
                    small += first_metrics["rd_candidate_transform_8x8"]
                    if name == "odd":
                        rd = work / "odd.rd.obu"
                        self.encode(source, rd, "rd", width, height)
                        self.assertEqual(first.read_bytes(), rd.read_bytes())
            self.assertGreater(large, 0)
            self.assertGreater(small, 0)
        finally:
            shutil.rmtree(work, ignore_errors=True)

    def test_color_keeps_exact_fixed_tree(self) -> None:
        self.require_tools()
        work = BUILD / "m4-partition-rd-color"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        width = height = 64
        y_plane = bytes(
            64 + x // 16 + y // 32 if y < 32
            else min(255, 16 + x * 10 // 4 + (y - 32) * 10 // 8)
            for y in range(height) for x in range(width))
        try:
            for suffix, chroma_width in (("i420", 32), ("i444", 64)):
                with self.subTest(format=suffix):
                    u_plane = bytes(
                        64 + (x * 7 + y * 3 + 10) % 128
                        for y in range(chroma_width)
                        for x in range(chroma_width))
                    v_plane = bytes(
                        192 - (x * 5 + y * 2 + 10) % 128
                        for y in range(chroma_width)
                        for x in range(chroma_width))
                    source_bytes = y_plane + u_plane + v_plane
                    source = work / f"color-64x64.{suffix}"
                    rd = work / f"{suffix}.rd.obu"
                    adaptive = work / f"{suffix}.partition.obu"
                    source.write_bytes(source_bytes)
                    rd_metrics = self.encode(source, rd, "rd", width, height)
                    adaptive_metrics = self.encode(
                        source, adaptive, "partition-rd", width, height)
                    self.assertEqual(rd.read_bytes(), adaptive.read_bytes())
                    self.assertEqual(
                        adaptive_metrics["rd_candidate_transform_16x16"], 0)
                    self.assertEqual(
                        (adaptive_metrics["sse_y"],
                         adaptive_metrics["sse_u"],
                         adaptive_metrics["sse_v"]),
                        (rd_metrics["sse_y"],
                         rd_metrics["sse_u"],
                         rd_metrics["sse_v"]))
                    decoded = self.decode(
                        adaptive, work / f"{suffix}.avm.{suffix}",
                        work / f"{suffix}.project.{suffix}")
                    self.assertEqual(len(decoded), len(source_bytes))
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()

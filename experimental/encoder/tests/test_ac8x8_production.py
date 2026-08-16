#!/usr/bin/env python3

import re
import shutil
import subprocess
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]


class Ac8x8ProductionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.encoder = ENCODER / "build/av2-encoder-experiment"
        cls.avmdec = ROOT / "build/reference/avm/avmdec"
        cls.project = ROOT / "build/host-cmake/av2heif-decoder"
        if not all(path.is_file() for path in
                   (cls.encoder, cls.avmdec, cls.project)):
            raise unittest.SkipTest("production encoder validators unavailable")
        cls.work = ENCODER / "build/ac8x8-production-test"
        shutil.rmtree(cls.work, ignore_errors=True)
        cls.work.mkdir(parents=True)

    @classmethod
    def tearDownClass(cls) -> None:
        shutil.rmtree(cls.work, ignore_errors=True)

    @staticmethod
    def pixels(width: int, height: int, kind: str) -> bytes:
        values = []
        for y in range(height):
            for x in range(width):
                if kind == "checker":
                    value = 32 if (x + y) & 1 else 224
                elif kind == "diagonal":
                    value = 40 if x < y else 216
                elif kind == "lines":
                    value = 64 if (x // 2) & 1 else 192
                else:
                    value = (x * 37 + y * 73 + x * y * 3 + 19) & 255
                values.append(value)
        return bytes(values)

    def encode(self, name: str, source: bytes, width: int, height: int,
               qindex: int, search: str) -> tuple[bytes, bytes, str]:
        raw = self.work / f"{name}.i400"
        obu = self.work / f"{name}.obu"
        avm = self.work / f"{name}.avm.i400"
        project = self.work / f"{name}.project.i400"
        raw.write_bytes(source)
        command = [
            str(self.encoder), str(raw), "--width", str(width),
            "--height", str(height), "--qindex", str(qindex),
            "--search", search, "--output", str(obu),
        ]
        completed = subprocess.run(
            command, check=True, text=True, capture_output=True)
        subprocess.run([
            str(self.avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(obu),
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(
            [str(self.project), str(obu), "-o", str(project)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        decoded = avm.read_bytes()
        self.assertEqual(decoded, project.read_bytes())
        match = re.search(r"sse_y=(\d+)", completed.stderr)
        self.assertIsNotNone(match)
        self.assertEqual(
            int(match.group(1)),
            sum((a - b) ** 2 for a, b in zip(source, decoded)))
        return obu.read_bytes(), decoded, completed.stderr

    def test_complete_stream_parity_and_determinism(self) -> None:
        cases = (
            ("q0-checker", 8, 8, 50, "checker"),
            ("q1-diagonal", 8, 8, 100, "diagonal"),
            ("q2-detail", 8, 8, 150, "detail"),
            ("q3-lines", 8, 8, 210, "lines"),
            ("crop", 9, 15, 100, "detail"),
            ("multileaf", 16, 16, 100, "checker"),
            ("superblocks", 72, 64, 100, "lines"),
        )
        for name, width, height, qindex, kind in cases:
            with self.subTest(name=name):
                source = self.pixels(width, height, kind)
                first = self.encode(
                    name, source, width, height, qindex, "bounded")
                second = self.encode(
                    name + "-repeat", source, width, height, qindex,
                    "bounded")
                self.assertEqual(first, second)

    def test_default_is_bounded_and_off_is_legacy_compatible(self) -> None:
        source = self.pixels(64, 64, "checker")
        bounded = self.encode(
            "bounded-explicit", source, 64, 64, 100, "bounded")[0]
        off = self.encode(
            "off-explicit", source, 64, 64, 100, "off")[0]
        raw = self.work / "off-default.i400"
        obu = self.work / "off-default.obu"
        raw.write_bytes(source)
        subprocess.run([
            str(self.encoder), str(raw), "--width", "64", "--height", "64",
            "--qindex", "100", "--output", str(obu),
        ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.assertEqual(bounded, obu.read_bytes())
        self.assertNotEqual(off, obu.read_bytes())

    def test_multisuperblock_color_parity(self) -> None:
        width = 72
        height = 64
        y = self.pixels(width, height, "lines")
        for format_name, chroma_size in (("i420", width * height // 4),
                                         ("i444", width * height)):
            with self.subTest(format=format_name):
                raw = self.work / f"color.{format_name}"
                obu = self.work / f"color-{format_name}.obu"
                avm = self.work / f"color-{format_name}.avm.{format_name}"
                project = self.work / f"color-{format_name}.project.{format_name}"
                raw.write_bytes(y + bytes([96]) * chroma_size +
                                bytes([160]) * chroma_size)
                subprocess.run([
                    str(self.encoder), str(raw), "--width", str(width),
                    "--height", str(height), "--qindex", "100",
                    "--search", "bounded", "--output", str(obu),
                ], check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
                subprocess.run([
                    str(self.avmdec), "--codec=av2", "--rawvideo",
                    "--limit=1", "--output-bit-depth=8", "-o", str(avm),
                    str(obu),
                ], check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
                subprocess.run(
                    [str(self.project), str(obu), "-o", str(project)],
                    check=True, stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL)
                self.assertEqual(avm.read_bytes(), project.read_bytes())

    def test_chroma_bounded_search_is_production_reachable(self) -> None:
        source = self.work / "staged-8x8.i444"
        source.write_bytes(bytes([128]) * (8 * 8 * 3))
        completed = subprocess.run([
            str(self.encoder), str(source), "--width", "8", "--height", "8",
            "--search", "bounded", "--output", str(self.work / "staged.obu"),
        ], text=True, capture_output=True)
        self.assertEqual(completed.returncode, 0)
        self.assertTrue((self.work / "staged.obu").is_file())
        self.assertIn("sse_u=", completed.stderr)


if __name__ == "__main__":
    unittest.main()

import re
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder/build/av2-encoder-experiment"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT = ROOT / "build/host-cmake/av2heif-decoder"
WORK = ROOT / "experimental/encoder/build/directional-production"
FORMATS = ("i400", "i420", "i444")


def input_bytes(pixel_format: str) -> bytes:
    dimensions = [(64, 64)]
    if pixel_format == "i420":
        dimensions += [(32, 32), (32, 32)]
    elif pixel_format == "i444":
        dimensions += [(64, 64), (64, 64)]
    output = bytearray()
    for plane, (width, height) in enumerate(dimensions):
        for y in range(height):
            for x in range(width):
                output.append(
                    32 if plane == 0 and x < width // 2
                    else 224 if plane == 0 else 128
                )
    return bytes(output)


class DirectionalIntraProductionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        WORK.mkdir(parents=True, exist_ok=True)

    def encode(self, source: Path, search: str, output: Path) -> tuple[int, list[int]]:
        completed = subprocess.run(
            [str(ENCODER), str(source), "--width", "64", "--height", "64",
             "--search", search, "-o", str(output)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        values = [int(value) for value in re.findall(
            rb"(?:bytes|sse_[yuv])=(\d+)", completed.stderr)]
        self.assertGreaterEqual(len(values), 2)
        return values[0], values[1:]

    def test_search_off_compatibility_and_directional_complete_streams(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("complete-stream validator parity disabled by unit-only run")
        wins = 0
        off_bytes = 0
        directional_bytes = 0
        for pixel_format in FORMATS:
            source = WORK / f"vertical-edge-64x64.{pixel_format}"
            source.write_bytes(input_bytes(pixel_format))
            implicit = WORK / f"{pixel_format}-implicit.obu"
            off = WORK / f"{pixel_format}-off.obu"
            directional = WORK / f"{pixel_format}-directional.obu"
            exact = WORK / f"{pixel_format}-directional-exact.obu"
            implicit_completed = subprocess.run(
                [str(ENCODER), str(source), "--width", "64", "--height", "64",
                 "-o", str(implicit)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            self.assertEqual(implicit_completed.returncode, 0)
            before_bytes, before_sse = self.encode(source, "off", off)
            after_bytes, after_sse = self.encode(
                source, "directional", directional)
            exact_bytes, exact_sse = self.encode(
                source, "directional-exact", exact)
            self.assertEqual(implicit.read_bytes(), off.read_bytes())
            self.assertEqual(directional.read_bytes(), exact.read_bytes())
            self.assertEqual((after_bytes, after_sse), (exact_bytes, exact_sse))
            self.assertLessEqual(after_bytes, before_bytes)
            self.assertEqual(after_sse, before_sse)
            wins += after_bytes < before_bytes
            off_bytes += before_bytes
            directional_bytes += after_bytes
            avm = WORK / f"{pixel_format}-avm.raw"
            project = WORK / f"{pixel_format}-project.{pixel_format}"
            subprocess.run(
                [str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                 "--output-bit-depth=8", "-o", str(avm), str(directional)],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )
            subprocess.run(
                [str(PROJECT), str(directional), "-o", str(project)],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            )
            self.assertEqual(avm.read_bytes(), project.read_bytes())
        self.assertGreaterEqual(wins, 1)
        self.assertLessEqual(directional_bytes, off_bytes)


if __name__ == "__main__":
    unittest.main()

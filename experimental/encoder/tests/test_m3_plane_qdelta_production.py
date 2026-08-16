import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder/build/av2-encoder-experiment"
DRIVER = ROOT / "experimental/encoder/build/m3-plane-qdelta-production-driver"


class PlaneQDeltaProductionTests(unittest.TestCase):
    def test_fixed_and_searched_candidates_match(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            outputs = []
            for u_delta, v_delta in ((-8, -16), (999, 999)):
                completed = subprocess.run(
                    [
                        str(DRIVER), "141", str(u_delta), str(v_delta),
                        str(work / f"{u_delta}-{v_delta}.obu"),
                    ],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stderr)
                outputs.append(completed.stdout)
            self.assertEqual(outputs, [b"26 2136 2320 -8 -16\n"] * 2)

    def test_plane_qdelta_is_admitted_for_color_only(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            for pixel_format, size in (("i400", 64), ("i420", 96), ("i444", 192)):
                with self.subTest(pixel_format=pixel_format):
                    source = work / f"source-8x8.{pixel_format}"
                    output = work / f"output-{pixel_format}.obu"
                    source.write_bytes(bytes([128]) * size)
                    completed = subprocess.run(
                        [
                            str(ENCODER), str(source),
                            "--width", "8", "--height", "8",
                            "--search", "plane-qdelta",
                            "--output", str(output),
                        ],
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        check=False,
                    )
                    if pixel_format == "i400":
                        self.assertEqual(completed.returncode, 5)
                        self.assertIn(b"requires I420 or I444", completed.stderr)
                        self.assertFalse(output.exists())
                    else:
                        self.assertEqual(
                            completed.returncode, 0,
                            completed.stderr.decode(errors="replace"))
                        self.assertTrue(output.exists())


if __name__ == "__main__":
    unittest.main()

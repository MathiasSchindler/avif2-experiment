import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder/build/av2-encoder-experiment"


class PlaneQDeltaProductionTests(unittest.TestCase):
    def test_unadmitted_plane_qdelta_is_rejected_for_every_format(self) -> None:
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
                    self.assertEqual(completed.returncode, 3)
                    self.assertIn(b"search must be", completed.stderr)
                    self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()

import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
DRIVER = ENCODER / "build/full-directional-syntax-driver"
AVMDEC = Path(os.environ.get(
    "AVMDEC", ROOT / "build/reference/avm/avmdec"))
PROJECT = Path(os.environ.get(
    "PROJECT_DECODER", ROOT / "build/host-cmake/av2heif-decoder"))


class FullDirectionalProductionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(
            ["make", "-C", str(ENCODER),
             "build/full-directional-syntax-driver"],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def decode_pair(self, obu: Path, suffix: str):
        avm = obu.with_suffix(f".avm.{suffix}")
        project = obu.with_suffix(f".project.{suffix}")
        subprocess.run([
            str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(obu),
        ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        subprocess.run([
            str(PROJECT), str(obu), "-o", str(project),
        ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(avm.read_bytes(), project.read_bytes())

    def test_all_luma_joint_modes_are_decoder_accepted(self):
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("decoder probes disabled by unit-only run")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for token in range(9, 65):
                obu = root / f"luma-{token}.obu"
                subprocess.run([
                    str(DRIVER), "1", str(token), "0", str(obu),
                ], check=True)
                self.decode_pair(obu, "i400")

    def test_chroma_modes_and_inherited_deltas_are_decoder_accepted(self):
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("decoder probes disabled by unit-only run")
        bases = (12, 19, 26, 33, 40, 47, 54, 61)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for base in bases:
                for delta in (-3, 3):
                    luma = base + delta
                    obu = root / f"color-{luma}-{base}.obu"
                    subprocess.run([
                        str(DRIVER), "2", str(luma), str(base), str(obu),
                    ], check=True)
                    self.decode_pair(obu, "i444")


if __name__ == "__main__":
    unittest.main()

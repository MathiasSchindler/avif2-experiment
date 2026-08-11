import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_10bit_synthetic.py"
SENTINELS = {0, 1, 2, 3, 255, 256, 511, 512, 1020, 1021, 1022, 1023}


def snapshot(directory: Path) -> dict[str, bytes]:
    return {
        path.name: path.read_bytes()
        for path in sorted(directory.iterdir())
        if path.is_file()
    }


class Synthetic10BitTests(unittest.TestCase):
    def test_generation_is_exact_and_planes_match_contract(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_text:
            temporary = Path(temporary_text)
            first = temporary / "first"
            second = temporary / "second"
            for output in (first, second):
                subprocess.run(
                    [sys.executable, str(GENERATOR), "--output-dir", str(output)],
                    cwd=ROOT,
                    check=True,
                    stdout=subprocess.PIPE,
                    text=True,
                )
            self.assertEqual(snapshot(first), snapshot(second))
            manifest = json.loads(
                (first / "source-manifest.json").read_text(encoding="utf-8")
            )
            artifacts = manifest["artifacts"]
            self.assertEqual(len(artifacts), 8)
            self.assertEqual(
                {record["name"] for record in artifacts},
                {
                    "tiny-precision",
                    "odd-geometry",
                    "boundary-extremes",
                    "high-detail",
                },
            )
            self.assertEqual(
                {record["format"] for record in artifacts},
                {"yuv420p10le", "yuv444p10le"},
            )
            for record in artifacts:
                with self.subTest(file=record["file"]):
                    data = (first / record["file"]).read_bytes()
                    self.assertEqual(len(data), record["size_bytes"])
                    plane_hashes = set()
                    for plane in record["planes"]:
                        start = plane["offset_bytes"]
                        end = start + plane["size_bytes"]
                        values = {
                            data[index] | data[index + 1] << 8
                            for index in range(start, end, 2)
                        }
                        self.assertTrue(values <= set(range(1024)))
                        self.assertTrue(SENTINELS <= values)
                        self.assertEqual(
                            {value & 3 for value in values}, {0, 1, 2, 3}
                        )
                        plane_hashes.add(plane["sha256"])
                    self.assertEqual(len(plane_hashes), 3)


if __name__ == "__main__":
    unittest.main()
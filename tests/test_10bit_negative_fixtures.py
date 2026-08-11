import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_DIR = ROOT / "tests" / "fixtures" / "10bit" / "negative"
MANIFEST = FIXTURE_DIR / "manifest.json"
DECODER = ROOT / "build" / "host-sanitized" / "av2heif-decoder"


class Negative10BitFixtureTests(unittest.TestCase):
    def test_manifest_hashes_and_sanitized_failures(self) -> None:
        if not MANIFEST.is_file():
            self.skipTest("WP10 negative fixtures have not been generated")
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(document["schema"], 1)
        self.assertEqual(len(document["artifacts"]), 4)
        for record in document["artifacts"]:
            fixture = FIXTURE_DIR / record["file"]
            with self.subTest(fixture=fixture.name):
                self.assertEqual(
                    hashlib.sha256(fixture.read_bytes()).hexdigest(),
                    record["sha256"],
                )
                self.assertEqual(fixture.stat().st_size, record["size_bytes"])
        if not DECODER.is_file():
            self.skipTest("sanitized decoder has not been built")
        environment = os.environ.copy()
        environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1"
        with tempfile.TemporaryDirectory() as temporary_text:
            output = Path(temporary_text) / "negative.i420"
            for record in document["artifacts"]:
                expected = record["expected_project_decoder"]
                completed = subprocess.run(
                    [str(DECODER), str(FIXTURE_DIR / record["file"]), "-o", str(output)],
                    cwd=ROOT,
                    env=environment,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                )
                with self.subTest(status=record["name"]):
                    self.assertEqual(completed.returncode, expected["exit_code"])
                    self.assertIn(expected["stderr_contains"], completed.stderr)
                    self.assertNotIn("AddressSanitizer", completed.stderr)
                    self.assertNotIn("runtime error:", completed.stderr)


if __name__ == "__main__":
    unittest.main()
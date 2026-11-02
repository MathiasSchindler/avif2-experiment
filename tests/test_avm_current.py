import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import check_avm_current as oracle  # noqa: E402


class CurrentOracleTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.stream = (ROOT / "tests/fixtures/i444-tiny.obu").read_bytes()
        self.golden = bytes([128]) * 768
        self.encoded = self.root / "baseline.obu"
        self.encoded.write_bytes(self.stream)
        self.source = self.root / "input.i444"
        self.source.write_bytes(self.golden)
        self.case = oracle.Case(
            "photo", "tiny", "source", "le896", "i444", 16, 16,
            self.encoded, oracle.sha256(self.encoded),
            hashlib.sha256(self.golden).hexdigest(), len(self.golden),
            self.source, oracle.sha256(self.source),
            ("old-avmenc", "--threads=16", "--limit=1", "-o", "old.obu", "old.yuv"),
        )

    def run_case(self, mode: str = "both", allow_missing: bool = False) -> oracle.CaseResult:
        return oracle.check_case(
            self.case, self.root / "result", Path("current-encoder"),
            Path("current-decoder"), Path("project-decoder"), mode, allow_missing,
        )

    def test_replay_preserves_all_encoder_settings(self) -> None:
        command = oracle.replay_encode(
            self.case.encode, Path("new"), self.source, self.root / "new.obu",
        )
        self.assertEqual(command[0], "new")
        self.assertEqual(command[1:3], ["--threads=16", "--limit=1"])
        self.assertEqual(command[-1], str(self.source))
        self.assertEqual(command[command.index("-o") + 1], str(self.root / "new.obu"))

    def test_invalid_replay_command_is_rejected(self) -> None:
        for command in ((), ("avmenc", "in"), ("avmenc", "-o", "a", "-o", "b", "in")):
            with self.subTest(command=command), self.assertRaises(ValueError):
                oracle.replay_encode(command, Path("new"), self.source, self.encoded)

    def test_encoder_drift_is_not_decoder_failure(self) -> None:
        changed = self.stream[:-1] + bytes([self.stream[-1] ^ 1])
        fresh_pixels = bytes([127]) * len(self.golden)

        def execute(command: list[str], output: Path, size: int | None = None) -> oracle.CommandResult:
            if command[0] == "current-encoder":
                output.write_bytes(changed)
            else:
                output.write_bytes(self.golden if output.name.startswith("existing") else fresh_pixels)
            return {"status": "ok", "command": command, "output": oracle.file_record(output)}

        with patch.object(oracle, "execute", side_effect=execute):
            result = self.run_case()
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["existing_decode"]["status"], "exact")
        self.assertEqual(result["fresh_encode"]["status"], "changed")
        self.assertFalse(result["fresh_oracle_decode"]["matches_baseline"])
        self.assertEqual(result["fresh_project_decode"]["status"], "exact")
        self.assertEqual(self.encoded.read_bytes(), self.stream)
        self.assertEqual(self.source.read_bytes(), self.golden)

    def test_pixel_mismatch_is_a_failure(self) -> None:
        def execute(command: list[str], output: Path, size: int | None = None) -> oracle.CommandResult:
            output.write_bytes(bytes(len(self.golden)))
            return {"status": "ok"}

        with patch.object(oracle, "execute", side_effect=execute):
            result = self.run_case("decode")
        self.assertEqual(result["status"], "error")
        self.assertEqual(result["existing_decode"]["status"], "mismatch")

    def test_project_mismatch_is_a_failure_even_when_encode_is_identical(self) -> None:
        def execute(command: list[str], output: Path, size: int | None = None) -> oracle.CommandResult:
            output.write_bytes(
                self.stream if command[0] == "current-encoder"
                else bytes(len(self.golden)) if command[0] == "project-decoder"
                else self.golden
            )
            return {"status": "ok"}

        with patch.object(oracle, "execute", side_effect=execute):
            result = self.run_case()
        self.assertEqual(result["status"], "error")
        self.assertEqual(result["fresh_encode"]["status"], "identical")
        self.assertEqual(result["fresh_project_decode"]["status"], "mismatch")

    def test_bad_input_hash_is_not_used(self) -> None:
        self.encoded.write_bytes(b"modified")
        with patch.object(oracle, "execute") as execute:
            result = self.run_case()
        self.assertEqual(result["status"], "error")
        self.assertIn("SHA-256 mismatch", result["error"])
        execute.assert_not_called()

    def test_missing_media_requires_explicit_opt_in(self) -> None:
        self.encoded.unlink()
        self.assertEqual(self.run_case()["status"], "error")
        result = self.run_case(allow_missing=True)
        self.assertEqual(result["status"], "skipped")
        self.assertIn("missing reference", result["reason"])

    def test_missing_prepared_input_is_reported_as_partial(self) -> None:
        self.source.unlink()
        result = self.run_case("encode", allow_missing=True)
        self.assertEqual(result["status"], "partial")
        self.assertEqual(result["fresh_encode"]["status"], "skipped")

    def test_external_command_failure_is_preserved(self) -> None:
        completed = subprocess.CompletedProcess(["bad"], 7, "", "unsupported option")
        with patch.object(oracle.subprocess, "run", return_value=completed):
            result = oracle.execute(["bad"], self.root / "missing")
        self.assertEqual(result["status"], "error")
        self.assertEqual(result["returncode"], 7)
        self.assertEqual(result["stderr"], "unsupported option")

    def test_success_without_output_is_not_success(self) -> None:
        completed = subprocess.CompletedProcess(["bad"], 0, "", "")
        with patch.object(oracle.subprocess, "run", return_value=completed):
            result = oracle.execute(["bad"], self.root / "missing")
        self.assertEqual(result["status"], "error")
        self.assertIn("without creating output", result["error"])

    def test_output_size_is_checked(self) -> None:
        completed = subprocess.CompletedProcess(["bad"], 0, "", "")
        output = self.root / "short"
        output.write_bytes(b"x")
        with patch.object(oracle.subprocess, "run", return_value=completed):
            result = oracle.execute(["bad"], output, 2)
        self.assertEqual(result["status"], "error")
        self.assertIn("expected 2 bytes", result["error"])

    def test_ten_bit_output_depth_is_explicit(self) -> None:
        case = replace(self.case, pixel_format="yuv444p10le")
        command = oracle.decode_command(Path("avmdec"), case, self.encoded, self.source)
        self.assertIn("--output-bit-depth=10", command)

    def test_stale_and_modified_oracles_are_rejected(self) -> None:
        (self.root / "oracle-revision.txt").write_text("old\n")
        with patch.object(oracle, "run", side_effect=["new\n", ""]):
            with self.assertRaisesRegex(ValueError, "stale"):
                oracle.oracle_tools(self.root, self.root)
        with patch.object(oracle, "run", side_effect=["new\n", " M decoder.c"]):
            with self.assertRaisesRegex(ValueError, "local modifications"):
                oracle.oracle_tools(self.root, self.root)

    def test_photo_manifest_hash_is_checked(self) -> None:
        images = self.root / "images"
        images.joinpath("recipes").mkdir(parents=True)
        manifest_path = images / "reference/manifest.json"
        manifest_path.parent.mkdir()
        manifest_path.write_text("{}")
        lock = {
            "avm": {"revision": oracle.PINNED_REVISION},
            "references": [{
                "manifest": "reference/manifest.json", "manifest_sha256": "wrong",
                "outputs": {"encoded_obu": {}, "decoded": {}},
                "preparation_id": "le896", "source_id": "source",
            }],
        }
        images.joinpath("recipes/lock.json").write_text(json.dumps(lock))
        with patch.object(oracle, "IMAGES", images):
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                oracle.photo_cases("photo", "lock.json")

    def test_tracked_ten_bit_manifest_covers_both_formats(self) -> None:
        cases = oracle.ten_bit_cases()
        self.assertEqual({case.pixel_format for case in cases}, {"yuv420p10le", "yuv444p10le"})
        self.assertTrue(all(case.bit_depth == 10 for case in cases))
        self.assertTrue(any("--lossless=1" in case.encode for case in cases))

    def invoke_main(self, results: list[object]) -> int:
        images = self.root / "images"
        images.joinpath("recipes").mkdir(parents=True)
        images.joinpath("recipes/encodings.json").write_text(json.dumps({
            "suites": [{"id": "photo", "lock_file": "unused"}],
        }))
        cases = [replace(self.case, name=f"case-{index}") for index in range(len(results))]
        with (
            patch.object(oracle, "IMAGES", images),
            patch.object(oracle, "oracle_tools", return_value=(Path("encoder"), Path("decoder"), {})),
            patch.object(oracle, "photo_cases", return_value=cases),
            patch.object(oracle, "check_case", side_effect=results),
            patch.object(sys, "argv", [
                "check_avm_current.py", "--suite", "photo",
                "--decoder", str(self.source), "--output-dir", str(self.root / "report"),
            ]),
        ):
            return oracle.main()

    def test_driver_returns_failure_and_saves_disagreement(self) -> None:
        self.assertEqual(self.invoke_main([{"status": "error", "error": "pixel mismatch"}]), 1)
        report = json.loads(self.root.joinpath("report/report.json").read_text())
        self.assertTrue(report["complete"])
        self.assertEqual(report["summary"]["error"], 1)
        self.assertEqual(report["cases"][0]["error"], "pixel mismatch")

    def test_driver_cannot_pass_without_any_checked_cases(self) -> None:
        self.assertEqual(self.invoke_main([{"status": "skipped"}]), 1)

    def test_interrupted_batch_retains_completed_cases(self) -> None:
        with self.assertRaises(KeyboardInterrupt):
            self.invoke_main([{"status": "passed"}, KeyboardInterrupt()])
        report = json.loads(self.root.joinpath("report/report.json").read_text())
        self.assertFalse(report["complete"])
        self.assertEqual(report["requested_cases"], 2)
        self.assertEqual(report["summary"]["passed"], 1)
        self.assertEqual(len(report["cases"]), 1)


if __name__ == "__main__":
    unittest.main()

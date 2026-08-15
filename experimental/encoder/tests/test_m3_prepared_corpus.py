#!/usr/bin/env python3

import json
import shutil
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))

from m3_prepared_corpus import (
    DEFAULT_MANIFEST,
    corpus_manifest,
    run_case,
    verify_manifest,
)


class M3PreparedCorpusTests(unittest.TestCase):
    def test_manifest_is_current_and_inventories_every_preparation(self) -> None:
        expected = corpus_manifest()
        verify_manifest(DEFAULT_MANIFEST, expected)
        self.assertEqual(expected["inventory_count"], 28)
        self.assertEqual(expected["benchmark_count"], 14)
        self.assertEqual(
            {item["preparation_id"] for item in expected["inventory"]
             if item["selected"]},
            {"le896"},
        )
        for item in expected["inventory"]:
            self.assertEqual(item["stride"], item["width"])
            self.assertEqual(item["byte_size"], item["width"] * item["height"])
            self.assertEqual(set(item["formats"]),
                             {"master", "gray8", "yuv420p8", "yuv444p8"})
            self.assertEqual(len(item["sha256"]), 64)
            self.assertEqual(len(item["preparation_manifest_sha256"]), 64)
            self.assertEqual(len(item["source_manifest_sha256"]), 64)
            self.assertEqual(item["source_original"]["mime_type"], "image/jpeg")
            self.assertEqual(item["source_original"]["sha256"],
                             item["source_sha256"])

    def test_full_image_sources_match_locked_metadata_when_present(self) -> None:
        manifest = corpus_manifest()
        present = [
            item for item in manifest["inventory"]
            if (ROOT / item["path"]).is_file()
        ]
        if not present:
            self.skipTest("prepared image corpus is unavailable")
        for item in present:
            self.assertEqual((ROOT / item["path"]).stat().st_size,
                             item["byte_size"])

    def test_multisuperblock_parity_evidence_covers_selected_corpus(self) -> None:
        evidence = json.loads(
            (ROOT / "experimental/encoder/tests/"
             "m3_multisuperblock_parity.json").read_text()
        )
        selected = {
            item["id"] for item in corpus_manifest()["inventory"]
            if item["selected"]
        }
        self.assertEqual(evidence["case_count"], 14)
        self.assertEqual(evidence["deterministic_count"], 14)
        self.assertEqual(evidence["avm_project_parity_count"], 14)
        self.assertEqual(
            {item["id"] for item in evidence["cases"]}, selected
        )
        self.assertEqual(evidence["minimum_dimensions"], [896, 424])
        self.assertEqual(evidence["maximum_dimensions"], [896, 896])
        for item in evidence["cases"]:
            self.assertTrue(item["deterministic_rerun"])
            self.assertTrue(item["avm_project_exact"])
            self.assertGreater(item["obu_bytes"], 0)
            self.assertGreater(item["workspace_bytes"], 0)

    def test_above_bound_image_is_expected_blocked_and_deterministic(self) -> None:
        work = ROOT / "experimental/encoder/build/m3-corpus-unit-blocked"
        shutil.rmtree(work, ignore_errors=True)
        source = work / "source.i400"
        metadata = work / "manifest.json"
        work.mkdir(parents=True)
        source.write_bytes(bytes(897))
        import hashlib
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        metadata_data = {
            "dimensions": [897, 1],
            "source_sha256": "a" * 64,
            "outputs": {"gray8": {
                "filename": source.name, "byte_size": 897, "sha256": digest,
            }},
        }
        metadata.write_text(json.dumps(metadata_data))
        case = {
            "id": "unit-le896", "path": str(source.relative_to(ROOT)),
            "pixel_format": "i400-8", "width": 897, "height": 1,
            "stride": 897, "byte_size": 897, "sha256": digest,
            "source_id": "unit", "source_sha256": "a" * 64,
            "source_dimensions": [897, 1], "preparation_id": "le896",
            "source_manifest": "unused/source.json",
            "source_manifest_sha256": "c" * 64,
            "preparation_manifest": str(metadata.relative_to(ROOT)),
            "preparation_manifest_sha256":
                hashlib.sha256(metadata.read_bytes()).hexdigest(),
        }
        rejected = subprocess.CompletedProcess([], 5, b"", b"unsupported")
        with patch("m3_prepared_corpus.run", return_value=(rejected, 10)):
            record = run_case(
                case, 100, Path("/encoder"), Path("/avm"), Path("/project"),
                work / "run",
            )
        self.assertEqual(record["status"], "expected_blocked")
        self.assertTrue(record["deterministic_rerun"])
        self.assertEqual(len(record["attempts"]), 2)
        shutil.rmtree(work)

    def test_validator_rejection_after_encoder_success_is_error(self) -> None:
        work = ROOT / "experimental/encoder/build/m3-corpus-unit-validator"
        shutil.rmtree(work, ignore_errors=True)
        source = work / "source.i400"
        metadata = work / "manifest.json"
        work.mkdir(parents=True)
        source.write_bytes(bytes(8 * 8))
        import hashlib
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        metadata_data = {
            "dimensions": [8, 8],
            "source_sha256": "b" * 64,
            "outputs": {"gray8": {
                "filename": source.name, "byte_size": 64, "sha256": digest,
            }},
        }
        metadata.write_text(json.dumps(metadata_data))
        case = {
            "id": "unit-validator", "path": str(source.relative_to(ROOT)),
            "pixel_format": "i400-8", "width": 8, "height": 8,
            "stride": 8, "byte_size": 64, "sha256": digest,
            "source_id": "unit", "source_sha256": "b" * 64,
            "source_dimensions": [8, 8], "preparation_id": "le896",
            "source_manifest": "unused/source.json",
            "source_manifest_sha256": "d" * 64,
            "preparation_manifest": str(metadata.relative_to(ROOT)),
            "preparation_manifest_sha256":
                hashlib.sha256(metadata.read_bytes()).hexdigest(),
        }

        calls = 0
        def fake_run(command):
            nonlocal calls
            calls += 1
            if calls <= 2:
                self.assertTrue(Path(command[-1]).is_absolute())
                Path(command[-1]).write_bytes(b"same-obu")
                return subprocess.CompletedProcess(command, 0, b"", b""), 11
            return subprocess.CompletedProcess(command, 7, b"", b"reject"), 12

        with patch("m3_prepared_corpus.run", side_effect=fake_run):
            record = run_case(
                case, 100, Path("/encoder"), Path("/avm"), Path("/project"),
                work / "run",
            )
        self.assertEqual(record["status"], "error")
        self.assertEqual(record["reason"],
                         "avm validator rejected after encoder success")
        shutil.rmtree(work)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import intrabc_admission as admission  # noqa: E402


class IntraBcAdmissionTests(unittest.TestCase):
    def test_locked_recipe_has_32_cases_and_64_arms(self) -> None:
        lock = admission.load_json(
            ROOT / "images" / "recipes" / "prepared-lock.json"
        )
        cases = admission.build_cases(lock)
        arms = admission.build_arms(cases)

        self.assertEqual(len(cases), 32)
        self.assertEqual(len(arms), 64)
        pastel = [
            case.qp
            for case in cases
            if case.source_id == "pastel-chalks"
            and case.preparation_id == "le896"
        ]
        self.assertEqual(pastel, [0, 48, 208, 255])
        self.assertEqual(sum(case.preparation_id == "le1440" for case in cases), 28)
        zero_control = next(case for case in cases if case.qp == 255)
        self.assertFalse(zero_control.requires_intrabc_occurrence)
        self.assertTrue(all(
            case.requires_intrabc_occurrence
            for case in cases
            if case is not zero_control
        ))

    def test_census_occurrence_summary_and_policy(self) -> None:
        census = {
            "files": [
                {
                    "features": [
                        {"count": 7, "use_intrabc": 0},
                        {
                            "count": 3,
                            "use_intrabc": 1,
                            "intrabc_mode": 0,
                            "skip_transform": 1,
                        },
                        {
                            "count": 5,
                            "use_intrabc": 1,
                            "intrabc_mode": 1,
                            "skip_transform": 0,
                        },
                    ]
                }
            ]
        }
        evidence = admission.intrabc_occurrences(census)
        self.assertEqual(evidence["block_count"], 8)
        self.assertEqual(evidence["mode_counts"], {"0": 3, "1": 5})
        self.assertEqual(evidence["skipped_block_count"], 3)
        self.assertEqual(evidence["residual_block_count"], 5)
        admission.enforce_occurrence_policy(True, evidence)
        with self.assertRaisesRegex(RuntimeError, "off arm"):
            admission.enforce_occurrence_policy(False, evidence)
        with self.assertRaisesRegex(RuntimeError, "on arm"):
            admission.enforce_occurrence_policy(
                True,
                {
                    "block_count": 0,
                    "mode_counts": {},
                    "skipped_block_count": 0,
                    "residual_block_count": 0,
                },
            )
        admission.enforce_occurrence_policy(
            True,
            {
                "block_count": 0,
                "mode_counts": {},
                "skipped_block_count": 0,
                "residual_block_count": 0,
            },
            require_enabled_occurrence=False,
        )

    def test_resume_requires_matching_artifact_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            matrix_root = Path(temporary_directory)
            arm_dir = matrix_root / "case" / "on"
            arm_dir.mkdir(parents=True)
            output = arm_dir / "encoded.obu"
            output.write_bytes(b"stable")
            digest = hashlib.sha256(b"stable").hexdigest()
            case = admission.Case(
                case_id="case",
                source_id="source",
                preparation_id="le1440",
                width=4,
                height=4,
                qp=96,
                source=matrix_root / "source.i420",
                source_sha256="0" * 64,
                requires_intrabc_occurrence=True,
            )
            arm = admission.Arm(case, True)
            manifest = {
                "schema": admission.MANIFEST_SCHEMA,
                "status": "complete",
                "case_id": "case",
                "intrabc": True,
                "census": {
                    "block_count": 1,
                    "mode_counts": {"0": 1},
                    "skipped_block_count": 0,
                    "residual_block_count": 1,
                },
                "outputs": {
                    "obu": {
                        "path": "case/on/encoded.obu",
                        "bytes": 6,
                        "sha256": digest,
                    }
                },
            }
            manifest_path = arm_dir / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="ascii")
            self.assertIsNotNone(
                admission.manifest_complete(manifest_path, arm, matrix_root)
            )
            upgraded = json.loads(manifest_path.read_text(encoding="ascii"))
            self.assertTrue(upgraded["requires_intrabc_occurrence"])

            output.write_bytes(b"changed")
            self.assertIsNone(
                admission.manifest_complete(manifest_path, arm, matrix_root)
            )


if __name__ == "__main__":
    unittest.main()
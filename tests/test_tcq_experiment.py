#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import intrabc_admission as common  # noqa: E402
import tcq_experiment as experiment  # noqa: E402


class TcqExperimentTests(unittest.TestCase):
    def test_default_case_and_tcq_arms(self) -> None:
        lock = common.load_json(
            ROOT / "images" / "recipes" / "prepared-lock.json"
        )
        case = experiment.select_case(lock, "pastel-chalks", "le896", 152)
        self.assertEqual(case.case_id, "pastel-chalks-le896-i420-qp152")
        self.assertEqual((case.width, case.height), (896, 598))
        arms = experiment.build_arms([case], "both", 1)
        self.assertEqual([arm.name for arm in arms], ["tcq-off", "tcq-on"])

    def test_encode_command_pins_tcq_capable_profile(self) -> None:
        case = experiment.Case(
            case_id="source-le896-i420-qp152",
            source_id="source",
            preparation_id="le896",
            width=8,
            height=6,
            qp=152,
            source=Path("source.i420"),
            source_sha256="0" * 64,
        )
        command = experiment.encode_command(
            Path("avmenc"), experiment.Arm(case, True, 1), Path("out.obu"), 16
        )
        self.assertIn("--cpu-used=1", command)
        self.assertIn("--enable-tcq=1", command)
        self.assertIn("--enable-intra-dip=0", command)
        self.assertIn("--enable-tx-partition=0", command)
        self.assertIn("--enable-parity-hiding=0", command)
        self.assertIn("--enable-intrabc=0", command)
        self.assertIn("--enable-restoration=0", command)

    def test_checkpoint_requires_matching_contract_and_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory)
            arm_dir = output_root / "case" / "tcq-off"
            arm_dir.mkdir(parents=True)
            encoded = arm_dir / "encoded.obu"
            encoded.write_bytes(b"stable")
            digest = hashlib.sha256(b"stable").hexdigest()
            contract = {"configuration": {"cpu_used": 1, "tcq": False}}
            manifest = {
                "schema": experiment.MANIFEST_SCHEMA,
                "status": "complete",
                "encode_contract": contract,
                "reference_only": True,
                "provenance": {"tools": {"encoder": "pinned"}},
                "outputs": {
                    "obu": {
                        "path": "case/tcq-off/encoded.obu",
                        "bytes": 6,
                        "sha256": digest,
                    }
                },
            }
            manifest_path = arm_dir / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="ascii")
            self.assertIsNotNone(
                experiment.reusable_checkpoint(
                    manifest_path, contract, output_root
                )
            )
            self.assertIsNone(
                experiment.reusable_checkpoint(
                    manifest_path,
                    contract,
                    output_root,
                    require_complete=True,
                    required_reference_only=False,
                )
            )
            self.assertIsNotNone(
                experiment.reusable_checkpoint(
                    manifest_path,
                    contract,
                    output_root,
                    require_complete=True,
                    required_reference_only=True,
                    required_provenance={
                        "tools": {"encoder": "pinned"}
                    },
                )
            )
            self.assertIsNone(
                experiment.reusable_checkpoint(
                    manifest_path,
                    {"configuration": {"cpu_used": 1, "tcq": True}},
                    output_root,
                )
            )
            encoded.write_bytes(b"changed")
            self.assertIsNone(
                experiment.reusable_checkpoint(
                    manifest_path, contract, output_root
                )
            )

    def test_tool_selection_omits_local_tools_for_reference_only(self) -> None:
        args = argparse.Namespace(
            avm_build=Path("avm"),
            census=Path("census"),
            host_build=Path("host"),
            reference_only=True,
        )
        self.assertEqual(
            set(experiment.tool_paths(args)),
            {"encoder", "avm_decoder", "census"},
        )
        args.reference_only = False
        self.assertEqual(
            set(experiment.tool_paths(args)),
            {"encoder", "avm_decoder", "census", "local_decoder", "artifact"},
        )


if __name__ == "__main__":
    unittest.main()
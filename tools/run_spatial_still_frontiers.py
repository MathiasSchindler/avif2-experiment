#!/usr/bin/env python3
"""Verify spatial-still decode frontiers against pinned AVM evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from obu_carriage import encode_uleb128, parse_obus


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "av2h-spatial-still-frontiers-v1"
DEFAULT_SUITE = (
    ROOT / "images/reference"
    / "avm-966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
    / "spatial-still-8bit"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_sha256(value: object, label: str) -> str:
    if (not isinstance(value, str) or len(value) != 64 or
            any(character not in "0123456789abcdef" for character in value)):
        raise ValueError(f"invalid {label}")
    return value


def validate_lock(value: object) -> dict[str, Any]:
    if not isinstance(value, dict) or value.get("schema") != SCHEMA:
        raise ValueError("spatial-still frontier schema mismatch")
    revision = value.get("avm_revision")
    if (not isinstance(revision, str) or len(revision) != 40 or
            any(character not in "0123456789abcdef" for character in revision)):
        raise ValueError("invalid AVM revision")
    cases = value.get("cases")
    if not isinstance(cases, list) or len(cases) != 4:
        raise ValueError("spatial-still frontier lock must contain four cases")
    seen: set[str] = set()
    for case in cases:
        if not isinstance(case, dict):
            raise ValueError("spatial-still frontier case is not an object")
        case_id = case.get("case_id")
        if not isinstance(case_id, str) or not case_id or case_id in seen:
            raise ValueError("invalid or duplicate spatial-still case ID")
        seen.add(case_id)
        require_sha256(case.get("encoded_sha256"), f"{case_id} encoded hash")
        require_sha256(case.get("decoded_sha256"), f"{case_id} decoded hash")
        for checkpoint in ("entropy_tell", "luma_leaves", "chroma_leaves"):
            if type(case.get(checkpoint)) is not int or case[checkpoint] <= 0:
                raise ValueError(f"invalid {case_id} {checkpoint} checkpoint")
        truncation = case.get("truncation")
        if not isinstance(truncation, dict) or set(truncation) != {
            "stage", "reason", "syntax", "entropy_tell", "luma_leaves",
            "chroma_leaves", "row", "column", "plane", "parsed_value",
        }:
            raise ValueError(f"invalid {case_id} truncation frontier")
    value["avm_revision"] = revision
    return value


def frame_payload_truncation(data: bytes) -> bytes:
    obus = parse_obus(data)
    frame_indices = [index for index, obu in enumerate(obus)
                     if obu.obu_type == 4]
    if len(frame_indices) != 1:
        raise ValueError("expected one Closed Loop Key OBU")
    frame_index = frame_indices[0]
    frame = obus[frame_index]
    if frame.size <= frame.header_size + 1:
        raise ValueError("frame payload is too small to truncate")
    shortened_data = data[frame.data_offset:frame.data_offset + frame.size - 1]
    replacement = encode_uleb128(len(shortened_data)) + shortened_data
    encoded = [obu.encoded for obu in obus]
    encoded[frame_index] = replacement
    result = b"".join(encoded)
    parse_obus(result)
    return result


def parse_report(stdout: str) -> dict[str, Any]:
    lines = [line for line in stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError("decoder did not emit exactly one diagnostic record")
    report = json.loads(lines[0])
    if not isinstance(report, dict):
        raise RuntimeError("decoder diagnostic record is not an object")
    return report


def invoke(decoder: Path, source: Path, output: Path) -> tuple[int, dict[str, Any]]:
    completed = subprocess.run(
        [str(decoder), str(source), "-o", str(output), "--diagnostics"],
        check=False, capture_output=True, text=True,
    )
    return completed.returncode, parse_report(completed.stdout)


def verify_case(decoder: Path, suite: Path, case: dict[str, Any],
                temporary: Path) -> dict[str, Any]:
    case_id = case["case_id"]
    case_dir = suite / case_id
    encoded = case_dir / "encoded.obu"
    reference = case_dir / "decoded.i420"
    if sha256(encoded) != case["encoded_sha256"]:
        raise RuntimeError(f"{case_id}: encoded OBU hash mismatch")
    if sha256(reference) != case["decoded_sha256"]:
        raise RuntimeError(f"{case_id}: pinned AVM output hash mismatch")

    output = temporary / f"{case_id}.i420"
    returncode, report = invoke(decoder, encoded, output)
    expected_report = {
        "status": "ok", "stage": "complete", "reason": "none",
        "syntax": "none", "entropy_tell": case["entropy_tell"],
        "luma_leaves": case["luma_leaves"],
        "chroma_leaves": case["chroma_leaves"],
    }
    if returncode != 0 or any(report.get(key) != value
                              for key, value in expected_report.items()):
        raise RuntimeError(f"{case_id}: completion frontier changed: {report}")
    if sha256(output) != case["decoded_sha256"]:
        raise RuntimeError(f"{case_id}: production output differs from AVM")

    truncated = temporary / f"{case_id}-truncated.obu"
    rejected_output = temporary / f"{case_id}-truncated.i420"
    truncated.write_bytes(frame_payload_truncation(encoded.read_bytes()))
    returncode, truncation_report = invoke(decoder, truncated, rejected_output)
    if returncode == 0 or truncation_report.get("status") == "ok":
        raise RuntimeError(f"{case_id}: frame truncation was accepted")
    if any(truncation_report.get(key) != value
           for key, value in case["truncation"].items()):
        raise RuntimeError(
            f"{case_id}: truncation frontier changed: {truncation_report}"
        )
    if rejected_output.exists():
        raise RuntimeError(f"{case_id}: rejected output was created")
    return {
        "case_id": case_id,
        "output_checkpoint": case["decoded_sha256"],
        "completion": report,
        "truncation": truncation_report,
    }


def run(decoder: Path, lock_path: Path, suite: Path) -> dict[str, Any]:
    lock = validate_lock(json.loads(lock_path.read_text(encoding="ascii")))
    with tempfile.TemporaryDirectory(prefix="av2h-spatial-frontiers-") as name:
        temporary = Path(name)
        records = [verify_case(decoder, suite, case, temporary)
                   for case in lock["cases"]]
    return {
        "schema": "av2h-spatial-still-frontier-results-v1",
        "avm_revision": lock["avm_revision"],
        "cases": records,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument(
        "--lock", type=Path,
        default=ROOT / "tests/spatial-still-frontiers.json",
    )
    parser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    arguments = parser.parse_args()
    print(json.dumps(
        run(arguments.decoder.resolve(), arguments.lock, arguments.suite),
        indent=2, sort_keys=True,
    ))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
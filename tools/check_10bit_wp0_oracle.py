#!/usr/bin/env python3
"""Validate and compare the first 10-bit vertical-slice oracle."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ROOT = ROOT / "tests" / "fixtures" / "10bit"
DEFAULT_ORACLE = FIXTURE_ROOT / "wp0-oracle.json"
AVM_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
SCHEMA = "av2h-10bit-wp0-oracle-v1"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_oracle(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="ascii"))
    if not isinstance(value, dict) or value.get("schema") != SCHEMA:
        raise ValueError("WP0 oracle schema mismatch")
    return value


def plane_records(oracle: dict[str, Any]) -> list[dict[str, Any]]:
    decoded = oracle.get("decoded")
    if not isinstance(decoded, dict):
        raise ValueError("WP0 oracle lacks decoded output")
    planes = decoded.get("planes")
    if not isinstance(planes, list) or len(planes) != 3:
        raise ValueError("WP0 oracle must define Y, U, and V planes")
    return planes


def validate_oracle(oracle: dict[str, Any], fixture_root: Path) -> None:
    geometry = oracle.get("geometry")
    format_record = oracle.get("format")
    if not isinstance(geometry, dict) or not isinstance(format_record, dict):
        raise ValueError("WP0 oracle lacks format or geometry")
    width = geometry.get("width")
    height = geometry.get("height")
    if width != 16 or height != 16:
        raise ValueError("WP0 oracle geometry changed")
    if format_record.get("name") != "yuv420p10le" or (
        format_record.get("bit_depth") != 10
        or format_record.get("bytes_per_sample") != 2
        or format_record.get("byte_order") != "little-endian"
        or format_record.get("sample_maximum") != 1023
    ):
        raise ValueError("WP0 oracle format contract changed")

    generation = oracle.get("generation")
    if not isinstance(generation, dict):
        raise ValueError("WP0 oracle lacks generation evidence")
    if generation.get("avm_revision") != AVM_REVISION or (
        generation.get("encoder_qp") != 208
    ):
        raise ValueError("WP0 generation parameters changed")
    source = fixture_root / str(generation.get("source_file"))
    if sha256(source.read_bytes()) != generation.get("source_sha256"):
        raise ValueError("WP0 source SHA-256 mismatch")

    obu_record = oracle.get("obu")
    decoded_record = oracle.get("decoded")
    if not isinstance(obu_record, dict) or not isinstance(decoded_record, dict):
        raise ValueError("WP0 oracle lacks artifacts")
    for label, record in (("OBU", obu_record), ("decoded output", decoded_record)):
        artifact = fixture_root / str(record.get("file"))
        data = artifact.read_bytes()
        if len(data) != record.get("size_bytes"):
            raise ValueError(f"WP0 {label} size mismatch")
        if sha256(data) != record.get("sha256"):
            raise ValueError(f"WP0 {label} SHA-256 mismatch")

    obu_data = (fixture_root / str(obu_record["file"])).read_bytes()
    actual_units = parse_obus(obu_data)
    expected_units = obu_record.get("units")
    if not isinstance(expected_units, list) or len(expected_units) != len(actual_units):
        raise ValueError("WP0 OBU ledger count mismatch")
    for expected_unit, actual_unit in zip(expected_units, actual_units):
        actual = actual_unit.ledger_entry()
        locked = {
            "offset": actual["offset"],
            "payload_offset": actual["payload_offset"],
            "payload_size": actual["payload_bytes"],
            "sha256": actual["sha256"],
            "type": actual["type_name"],
        }
        if expected_unit != locked:
            raise ValueError(f"WP0 {actual_unit.name} OBU ledger mismatch")

    decoded_data = (fixture_root / str(decoded_record["file"])).read_bytes()
    expected_offset = 0
    expected_dimensions = (("Y", 16, 16), ("U", 8, 8), ("V", 8, 8))
    for plane, (name, plane_width, plane_height) in zip(
        plane_records(oracle), expected_dimensions
    ):
        size = plane_width * plane_height * 2
        if (
            plane.get("name") != name
            or plane.get("width") != plane_width
            or plane.get("height") != plane_height
            or plane.get("offset_bytes") != expected_offset
            or plane.get("stride_bytes") != plane_width * 2
            or plane.get("size_bytes") != size
        ):
            raise ValueError(f"WP0 {name} plane layout mismatch")
        data = decoded_data[expected_offset : expected_offset + size]
        if sha256(data) != plane.get("sha256"):
            raise ValueError(f"WP0 {name} plane SHA-256 mismatch")
        expected_offset += size
    if expected_offset != len(decoded_data):
        raise ValueError("WP0 decoded output has trailing bytes")

    census = oracle.get("census")
    if not isinstance(census, dict) or census.get("base_qindex") != 256:
        raise ValueError("WP0 census base Q-index mismatch")
    if (
        census.get("accounting_events") != 166
        or census.get("coding_blocks") != 2
        or census.get("unique_features") != 2
        or census.get("qindices") != [252]
        or census.get("transform_sizes") != ["8x16"]
        or census.get("transforms") != ["DCT_DCT"]
    ):
        raise ValueError("WP0 census complexity changed")
    if census.get("prediction_modes") != ["DC"] or (
        census.get("uv_prediction_modes") != ["DC"]
    ):
        raise ValueError("WP0 census prediction modes changed")
    trace = oracle.get("trace_evidence")
    if not isinstance(trace, dict):
        raise ValueError("WP0 trace evidence is absent")
    if trace.get("avm_revision") != AVM_REVISION:
        raise ValueError("WP0 trace AVM revision changed")
    if trace.get("frame_header_bits") != 25 or trace.get("tile_entropy_tell") != 192:
        raise ValueError("WP0 syntax endpoints changed")
    expected_filters = {
        "deblock_enabled": [0, 0, 0, 0],
        "cdef": {
            "enabled": 0,
            "strength_count": 0,
            "y_strength_0": 0,
            "uv_strength_0": 0,
        },
        "ccso": {"frame_enabled": 0, "plane_enabled": [0, 0, 0]},
        "gdf_mode": 0,
    }
    if trace.get("filters") != expected_filters:
        raise ValueError("WP0 filter identity evidence changed")
    expected_disabled_tools = {
        "cctx": True,
        "cfl": True,
        "fsc": True,
        "ibp": True,
        "intrabc": True,
        "ist": True,
        "mhccp": True,
        "mrl": True,
        "paeth": True,
        "palette": True,
        "smooth": True,
    }
    if trace.get("disabled_sample_tools") != expected_disabled_tools:
        raise ValueError("WP0 disabled sample-tool evidence changed")
    blocks = trace.get("block_order")
    expected_blocks = [
        {
            "chroma_block_size_id": 5,
            "chroma_mi_column": 0,
            "chroma_mi_row": 0,
            "luma_block_size_id": 4,
            "mi_column": 0,
            "mi_row": 0,
            "tree_type": 2,
        },
        {
            "chroma_block_size_id": 5,
            "chroma_mi_column": 0,
            "chroma_mi_row": 0,
            "luma_block_size_id": 4,
            "mi_column": 2,
            "mi_row": 0,
            "tree_type": 1,
        },
    ]
    if blocks != expected_blocks:
        raise ValueError("WP0 block order changed")

    current_decoder = oracle.get("current_decoder")
    if not isinstance(current_decoder, dict) or sha256(
        str(current_decoder.get("output_sentinel")).encode("ascii")
    ) != current_decoder.get("output_sentinel_sha256"):
        raise ValueError("WP0 output sentinel SHA-256 mismatch")


def first_mismatch(
    oracle: dict[str, Any], expected: bytes, actual: bytes
) -> dict[str, object] | None:
    expected_size = int(oracle["decoded"]["size_bytes"])
    if len(actual) != expected_size:
        return {
            "kind": "size",
            "expected_size": expected_size,
            "actual_size": len(actual),
        }
    if len(expected) != expected_size:
        raise ValueError("WP0 expected output size does not match oracle")
    for plane in plane_records(oracle):
        offset = int(plane["offset_bytes"])
        size = int(plane["size_bytes"])
        width = int(plane["width"])
        for byte_index in range(offset, offset + size, 2):
            expected_value = int.from_bytes(
                expected[byte_index : byte_index + 2], "little"
            )
            actual_value = int.from_bytes(
                actual[byte_index : byte_index + 2], "little"
            )
            if expected_value != actual_value:
                sample_index = (byte_index - offset) // 2
                return {
                    "kind": "sample",
                    "plane": plane["name"],
                    "sample_index": sample_index,
                    "x": sample_index % width,
                    "y": sample_index // width,
                    "expected": expected_value,
                    "actual": actual_value,
                }
    return None


def check_current_decoder(
    oracle: dict[str, Any], decoder: Path, fixture_root: Path
) -> None:
    expected = oracle["current_decoder"]
    sentinel = str(expected["output_sentinel"]).encode("ascii")
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1"
    with tempfile.TemporaryDirectory(prefix="av2h-10bit-wp0-") as temporary:
        output = Path(temporary) / f"output{expected['output_extension']}"
        output.write_bytes(sentinel)
        completed = subprocess.run(
            [
                str(decoder),
                str(fixture_root / oracle["obu"]["file"]),
                "-o",
                str(output),
            ],
            cwd=ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if completed.returncode != expected["exit_code"]:
            raise ValueError(
                f"current decoder exit code: expected {expected['exit_code']}, "
                f"got {completed.returncode}"
            )
        if expected["stderr_contains"] not in completed.stderr:
            raise ValueError("current decoder diagnostic changed")
        if output.read_bytes() != sentinel:
            raise ValueError("current decoder mutated output at the 10-bit gate")


def command_verify(arguments: argparse.Namespace) -> int:
    oracle = read_oracle(arguments.oracle)
    validate_oracle(oracle, arguments.fixture_root)
    if arguments.decoder is not None:
        check_current_decoder(oracle, arguments.decoder, arguments.fixture_root)
    print(f"WP0 oracle verified: {oracle['fixture']}")
    return 0


def command_compare(arguments: argparse.Namespace) -> int:
    oracle = read_oracle(arguments.oracle)
    validate_oracle(oracle, arguments.fixture_root)
    expected = (arguments.fixture_root / oracle["decoded"]["file"]).read_bytes()
    mismatch = first_mismatch(oracle, expected, arguments.actual.read_bytes())
    if mismatch is None:
        print(f"WP0 planar output matches: {oracle['fixture']}")
        return 0
    print(json.dumps(mismatch, sort_keys=True), file=sys.stderr)
    return 1


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    root.add_argument("--oracle", type=Path, default=DEFAULT_ORACLE)
    root.add_argument("--fixture-root", type=Path, default=FIXTURE_ROOT)
    subparsers = root.add_subparsers(dest="command", required=True)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--decoder", type=Path)
    verify.set_defaults(handler=command_verify)
    compare = subparsers.add_parser("compare")
    compare.add_argument("actual", type=Path)
    compare.set_defaults(handler=command_compare)
    return root


def main() -> int:
    try:
        arguments = parser().parse_args()
        return arguments.handler(arguments)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
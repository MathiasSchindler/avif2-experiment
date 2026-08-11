#!/usr/bin/env python3
"""Record and compare stage-local filter evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

from obu_carriage import CarriageError, hash_i420


ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "av2h-m9-filter-evidence-v1"
STAGE_ORDER = (
    "reconstructed",
    "deblocked",
    "cdef",
    "ccso",
    "gdf",
    "filtered",
)


def artifact(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    resolved = path.resolve()
    try:
        display_path = str(resolved.relative_to(ROOT))
    except ValueError:
        display_path = str(resolved)
    return {
        "path": display_path,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def stage_record(path: Path, width: int, height: int) -> dict[str, object]:
    record = hash_i420(path.read_bytes(), width, height)
    record["artifact"] = artifact(path)
    return record


def record_manifest(
    fixture: str,
    stages: list[tuple[str, Path, int, int]],
    source_obu: Path | None = None,
    producer: str | None = None,
) -> dict[str, object]:
    if not fixture:
        raise ValueError("fixture name must not be empty")
    if not stages:
        raise ValueError("at least one filter stage is required")
    seen: set[str] = set()
    stage_records: dict[str, object] = {}
    for name, path, width, height in stages:
        if name not in STAGE_ORDER:
            raise ValueError(f"unknown filter stage: {name}")
        if name in seen:
            raise ValueError(f"duplicate filter stage: {name}")
        seen.add(name)
        stage_records[name] = stage_record(path, width, height)
    ordered_names = [name for name in STAGE_ORDER if name in stage_records]
    return {
        "schema": SCHEMA,
        "fixture": fixture,
        "producer": producer,
        "source_obu": artifact(source_obu) if source_obu is not None else None,
        "stage_order": ordered_names,
        "stages": {name: stage_records[name] for name in ordered_names},
    }


def stage_comparison(
    expected: dict[str, Any], actual: dict[str, Any]
) -> dict[str, object]:
    geometry_keys = (
        "format",
        "width",
        "height",
        "chroma_width",
        "chroma_height",
        "bytes",
    )
    geometry_match = all(expected.get(key) == actual.get(key)
                         for key in geometry_keys)
    expected_planes = expected.get("planes", {})
    actual_planes = actual.get("planes", {})
    plane_matches = {
        plane: expected_planes.get(plane) == actual_planes.get(plane)
        for plane in ("y", "u", "v")
    }
    file_match = expected.get("file_sha256") == actual.get("file_sha256")
    return {
        "match": geometry_match and file_match and all(plane_matches.values()),
        "geometry_match": geometry_match,
        "file_match": file_match,
        "plane_matches": plane_matches,
    }


def require_integer(record: dict[str, Any], key: str) -> int:
    value = record.get(key)
    if type(value) is not int or value <= 0:
        raise ValueError(f"filter evidence stage has invalid {key}")
    return value


def require_sha256(value: object, label: str) -> str:
    if (not isinstance(value, str) or len(value) != 64 or
            any(character not in "0123456789abcdef" for character in value)):
        raise ValueError(f"filter evidence has invalid {label}")
    return value


def validate_stage_record(name: str, record: object) -> dict[str, Any]:
    if not isinstance(record, dict):
        raise ValueError(f"filter evidence stage {name} is not an object")
    if record.get("format") != "I420":
        raise ValueError(f"filter evidence stage {name} is not I420")
    width = require_integer(record, "width")
    height = require_integer(record, "height")
    chroma_width = require_integer(record, "chroma_width")
    chroma_height = require_integer(record, "chroma_height")
    byte_count = require_integer(record, "bytes")
    if (chroma_width != (width + 1) // 2 or
            chroma_height != (height + 1) // 2 or
            byte_count != width * height +
            2 * chroma_width * chroma_height):
        raise ValueError(f"filter evidence stage {name} geometry is inconsistent")
    require_sha256(record.get("file_sha256"), f"{name} file SHA-256")
    planes = record.get("planes")
    if not isinstance(planes, dict):
        raise ValueError(f"filter evidence stage {name} lacks plane hashes")
    for plane in ("y", "u", "v"):
        require_sha256(planes.get(plane), f"{name} {plane} SHA-256")
    return record


def validate_source_record(record: object) -> dict[str, Any] | None:
    if record is None:
        return None
    if not isinstance(record, dict):
        raise ValueError("filter evidence source OBU is not an object")
    require_sha256(record.get("sha256"), "source OBU SHA-256")
    return record


def validate_manifest(manifest: object) -> dict[str, Any]:
    if not isinstance(manifest, dict) or manifest.get("schema") != SCHEMA:
        raise ValueError("filter evidence schema mismatch")
    fixture = manifest.get("fixture")
    if not isinstance(fixture, str) or not fixture:
        raise ValueError("filter evidence fixture is invalid")
    order = manifest.get("stage_order")
    stages = manifest.get("stages")
    if not isinstance(order, list) or not order or not isinstance(stages, dict):
        raise ValueError("filter evidence stages are invalid")
    if order != [name for name in STAGE_ORDER if name in order] or (
        len(order) != len(set(order))
    ):
        raise ValueError("filter evidence stage order is invalid")
    if set(stages) != set(order):
        raise ValueError("filter evidence stage records do not match stage order")
    for name in order:
        validate_stage_record(name, stages[name])
    validate_source_record(manifest.get("source_obu"))
    return manifest


def compare_manifests(
    expected: dict[str, Any], actual: dict[str, Any]
) -> dict[str, object]:
    expected = validate_manifest(expected)
    actual = validate_manifest(actual)
    if expected.get("fixture") != actual.get("fixture"):
        raise ValueError("filter evidence fixture mismatch")
    expected_source = expected.get("source_obu")
    actual_source = actual.get("source_obu")
    if (expected_source is None) != (actual_source is None) or (
        expected_source is not None
        and expected_source.get("sha256") != actual_source.get("sha256")
    ):
        raise ValueError("filter evidence source OBU mismatch")
    expected_order = expected.get("stage_order")
    actual_order = actual.get("stage_order")
    if expected_order != actual_order:
        raise ValueError("filter evidence stage order mismatch")
    comparisons: dict[str, object] = {}
    first_mismatch = None
    for name in expected_order:
        comparison = stage_comparison(
            expected["stages"][name], actual["stages"][name]
        )
        comparisons[name] = comparison
        if not comparison["match"] and first_mismatch is None:
            first_mismatch = name
    return {
        "schema": "av2h-m9-filter-comparison-v1",
        "fixture": expected["fixture"],
        "match": first_mismatch is None,
        "first_mismatch_stage": first_mismatch,
        "stages": comparisons,
    }


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="ascii") as source:
        value = json.load(source)
    if not isinstance(value, dict):
        raise ValueError(f"expected JSON object: {path}")
    return value


def write_json(path: Path | None, value: object) -> None:
    rendered = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if path is None:
        print(rendered, end="")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(rendered, encoding="ascii")


def parse_stage(values: list[str]) -> tuple[str, Path, int, int]:
    name, path_value, width_value, height_value = values
    return name, Path(path_value), int(width_value), int(height_value)


def command_record(args: argparse.Namespace) -> int:
    stages = [parse_stage(values) for values in args.stage]
    manifest = record_manifest(
        args.fixture, stages, args.source_obu, args.producer
    )
    write_json(args.output, manifest)
    return 0


def command_compare(args: argparse.Namespace) -> int:
    comparison = compare_manifests(
        read_json(args.expected), read_json(args.actual)
    )
    write_json(args.output, comparison)
    return 0 if comparison["match"] else 1


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(
        description="Record or compare AV2 filter-stage I420 evidence."
    )
    subparsers = root.add_subparsers(dest="command", required=True)

    record = subparsers.add_parser("record")
    record.add_argument("--fixture", required=True)
    record.add_argument("--producer")
    record.add_argument("--source-obu", type=Path)
    record.add_argument(
        "--stage",
        action="append",
        nargs=4,
        metavar=("NAME", "PATH", "WIDTH", "HEIGHT"),
        required=True,
    )
    record.add_argument("--output", type=Path)
    record.set_defaults(handler=command_record)

    compare = subparsers.add_parser("compare")
    compare.add_argument("expected", type=Path)
    compare.add_argument("actual", type=Path)
    compare.add_argument("--output", type=Path)
    compare.set_defaults(handler=command_compare)
    return root


def main() -> int:
    try:
        args = parser().parse_args()
        return args.handler(args)
    except (CarriageError, OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Verify tracked image source records and locally present source media."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
SOURCES_ROOT = IMAGES_ROOT / "sources" / "commons"
ATTRIBUTION_PATH = IMAGES_ROOT / "ATTRIBUTION.md"

EXPECTED_LICENSES = {
    "CC-BY-4.0": (
        "Creative Commons Attribution 4.0 International",
        "CC-BY-4.0-International.txt",
    ),
    "CC-BY-SA-4.0": (
        "Creative Commons Attribution-ShareAlike 4.0 International",
        "CC-BY-SA-4.0-International.txt",
    ),
    "CC-BY-SA-2.0": (
        "Creative Commons Attribution-ShareAlike 2.0 Generic",
        "CC-BY-SA-2.0-Generic.html",
    ),
}

REQUIRED_TOP_LEVEL_FIELDS = {
    "schema_version",
    "id",
    "title",
    "source_class",
    "creator",
    "commons",
    "license",
    "attribution",
    "original",
    "review",
}


def digest(path: Path, algorithm: str) -> str:
    checksum = hashlib.new(algorithm)
    with path.open("rb") as input_file:
        while chunk := input_file.read(1024 * 1024):
            checksum.update(chunk)
    return checksum.hexdigest()


def verify_record(source_path: Path, require_media: bool, attribution: str) -> bool:
    record = json.loads(source_path.read_text(encoding="utf-8"))
    missing_fields = REQUIRED_TOP_LEVEL_FIELDS - record.keys()
    if missing_fields:
        raise ValueError(f"{source_path}: missing fields {sorted(missing_fields)}")
    if record["schema_version"] != 1:
        raise ValueError(f"{source_path}: unsupported schema version")
    if record["source_class"] != "commons":
        raise ValueError(f"{source_path}: unexpected source class")
    if record["id"] != source_path.parent.name:
        raise ValueError(f"{source_path}: source ID does not match directory")

    license_record = record["license"]
    expected_name, expected_filename = EXPECTED_LICENSES[license_record["spdx"]]
    if license_record["name"] != expected_name:
        raise ValueError(f"{source_path}: license name does not match SPDX ID")
    license_path = (source_path.parent / license_record["local_file"]).resolve()
    if license_path.name != expected_filename or not license_path.is_file():
        raise ValueError(f"{source_path}: invalid local license reference")

    for value in (
        record["id"],
        record["attribution"]["credit_line"],
        license_record["name"],
        record["original"]["sha256"],
    ):
        if str(value) not in attribution:
            raise ValueError(f"{source_path}: value missing from ATTRIBUTION.md: {value}")

    original = record["original"]
    media_path = source_path.parent / original["filename"]
    if not media_path.exists():
        if require_media:
            raise ValueError(f"{source_path}: downloaded original is missing")
        return False
    if media_path.stat().st_size != original["byte_size"]:
        raise ValueError(f"{source_path}: downloaded original has the wrong size")
    if digest(media_path, "sha1") != original["sha1"]:
        raise ValueError(f"{source_path}: downloaded original has the wrong SHA-1")
    if digest(media_path, "sha256") != original["sha256"]:
        raise ValueError(f"{source_path}: downloaded original has the wrong SHA-256")
    return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-media",
        action="store_true",
        help="fail if any ignored downloaded original is absent",
    )
    arguments = parser.parse_args()

    source_paths = sorted(SOURCES_ROOT.glob("*/source.json"))
    if not source_paths:
        raise SystemExit("no Commons source records found")
    attribution = ATTRIBUTION_PATH.read_text(encoding="utf-8")
    present = sum(
        verify_record(source_path, arguments.require_media, attribution)
        for source_path in source_paths
    )
    print(
        f"verified {len(source_paths)} source records; "
        f"local originals present: {present}/{len(source_paths)}"
    )


if __name__ == "__main__":
    main()
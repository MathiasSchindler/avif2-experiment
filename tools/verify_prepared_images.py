#!/usr/bin/env python3
"""Verify the prepared image lock and locally present prepared files."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
SOURCES_ROOT = IMAGES_ROOT / "sources" / "commons"
RECIPES_PATH = IMAGES_ROOT / "recipes" / "preparations.json"
LOCK_PATH = IMAGES_ROOT / "recipes" / "prepared-lock.json"


def sha256(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as input_file:
        while chunk := input_file.read(1024 * 1024):
            checksum.update(chunk)
    return checksum.hexdigest()


def png_contract(path: Path) -> tuple[int, int, int, int]:
    with path.open("rb") as input_file:
        header = input_file.read(33)
    if len(header) != 33 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: invalid PNG signature or truncated IHDR")
    length, chunk_type = struct.unpack(">I4s", header[8:16])
    if length != 13 or chunk_type != b"IHDR":
        raise ValueError(f"{path}: IHDR is not the first PNG chunk")
    width, height, bit_depth, color_type = struct.unpack(">IIBB", header[16:26])
    return width, height, bit_depth, color_type


def expected_size(pixel_format: str, width: int, height: int) -> int:
    if pixel_format == "gray8":
        return width * height
    if pixel_format == "yuv420p8":
        return width * height * 3 // 2
    if pixel_format == "yuv444p8":
        return width * height * 3
    raise ValueError(f"unsupported output format: {pixel_format}")


def verify_files(record: dict[str, object], require_files: bool) -> bool:
    manifest_path = IMAGES_ROOT / record["manifest"]
    if not manifest_path.exists():
        if require_files:
            raise ValueError(f"missing preparation manifest: {manifest_path}")
        return False
    if sha256(manifest_path) != record["manifest_sha256"]:
        raise ValueError(f"manifest SHA-256 mismatch: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for field in (
        "preparation_id",
        "profile_id",
        "source_id",
        "source_sha256",
        "source_dimensions",
        "dimensions",
        "linear_scale",
        "pixel_area_scale",
        "outputs",
    ):
        if manifest[field] != record[field]:
            raise ValueError(f"{manifest_path}: lock mismatch for {field}")

    width, height = record["dimensions"]
    for pixel_format, output in record["outputs"].items():
        output_path = manifest_path.parent / output["filename"]
        if not output_path.is_file():
            raise ValueError(f"missing prepared output: {output_path}")
        if output_path.stat().st_size != output["byte_size"]:
            raise ValueError(f"prepared output size mismatch: {output_path}")
        if sha256(output_path) != output["sha256"]:
            raise ValueError(f"prepared output SHA-256 mismatch: {output_path}")
        if pixel_format == "master":
            if png_contract(output_path) != (width, height, 8, 2):
                raise ValueError(f"unexpected PNG master contract: {output_path}")
        elif output["byte_size"] != expected_size(pixel_format, width, height):
            raise ValueError(f"raw plane size formula mismatch: {output_path}")
    return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-files",
        action="store_true",
        help="fail if the ignored prepared files are absent",
    )
    arguments = parser.parse_args()

    recipes = json.loads(RECIPES_PATH.read_text(encoding="utf-8"))
    profiles = {profile["id"]: profile for profile in recipes["profiles"]}
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    records = lock["preparations"]
    source_records = {
        path.parent.name: json.loads(path.read_text(encoding="utf-8"))
        for path in SOURCES_ROOT.glob("*/source.json")
    }
    expected_pairs = {
        (profile_id, source_id)
        for profile_id in profiles
        for source_id in source_records
    }
    actual_pairs = {(record["profile_id"], record["source_id"]) for record in records}
    if actual_pairs != expected_pairs or len(records) != len(expected_pairs):
        raise ValueError("prepared lock does not cover every profile/source pair once")

    present = 0
    linear_exceptions = []
    for record in records:
        profile = profiles[record["profile_id"]]
        source = source_records[record["source_id"]]
        width, height = record["dimensions"]
        source_width, source_height = record["source_dimensions"]
        if record["source_sha256"] != source["original"]["sha256"]:
            raise ValueError(f"source hash mismatch for {record['source_id']}")
        if record["source_dimensions"] != [
            source["original"]["width"],
            source["original"]["height"],
        ]:
            raise ValueError(f"source dimensions mismatch for {record['source_id']}")
        if max(width, height) != profile["target_long_edge"]:
            raise ValueError(f"target long edge mismatch for {record['source_id']}")
        if width % 2 or height % 2:
            raise ValueError(f"prepared dimensions must be even for {record['source_id']}")
        if width > source_width or height > source_height:
            raise ValueError(f"preparation upscales {record['source_id']}")
        if record["pixel_area_scale"] >= 0.5:
            raise ValueError(f"preparation exceeds 50% source area: {record['source_id']}")
        if record["linear_scale"] >= 0.5:
            linear_exceptions.append(
                (record["source_id"], profile["target_long_edge"])
            )
        present += verify_files(record, arguments.require_files)

    expected_exceptions = {
        ("finland-warbirds", 1440),
        ("scarlet-flycatcher", 1440),
    }
    if set(linear_exceptions) != expected_exceptions:
        raise ValueError(f"unexpected 50% linear-scale exceptions: {linear_exceptions}")
    print(
        f"verified {len(records)} preparation records; "
        f"local preparations present: {present}/{len(records)}; "
        f"linear-scale exceptions: {len(linear_exceptions)}"
    )


if __name__ == "__main__":
    main()
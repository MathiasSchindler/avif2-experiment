#!/usr/bin/env python3
"""Verify the AVM image-reference lock and locally present reference files."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from obu_carriage import parse_obus


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
ENCODINGS_PATH = IMAGES_ROOT / "recipes" / "encodings.json"
PREPARED_LOCK_PATH = IMAGES_ROOT / "recipes" / "prepared-lock.json"
REFERENCE_LOCK_PATH = IMAGES_ROOT / "recipes" / "reference-lock.json"


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
    if pixel_format == "i400":
        return width * height
    if pixel_format == "i420":
        return width * height * 3 // 2
    if pixel_format == "i444":
        return width * height * 3
    raise ValueError(f"unsupported pixel format: {pixel_format}")


def expected_keys(
    suite: dict[str, object], preparations: list[dict[str, object]]
) -> set[tuple[str, str, str, int]]:
    keys = set()
    for preparation in preparations:
        source_id = preparation["source_id"]
        preparation_id = preparation["preparation_id"]
        if preparation_id in suite["preparation_ids"]:
            for qp in suite["baseline_qps"]:
                keys.add(
                    (source_id, preparation_id, suite["baseline_pixel_format"], qp)
                )
    coverage = suite["format_coverage"]
    for preparation in preparations:
        if preparation["preparation_id"] != coverage["preparation_id"]:
            continue
        for pixel_format in coverage["pixel_formats"]:
            for qp in coverage["qps"]:
                keys.add(
                    (
                        preparation["source_id"],
                        preparation["preparation_id"],
                        pixel_format,
                        qp,
                    )
                )
    spectrum = suite["spectrum"]
    for qp in spectrum["additional_qps"]:
        keys.add(
            (
                spectrum["source_id"],
                spectrum["preparation_id"],
                spectrum["pixel_format"],
                qp,
            )
        )
    return keys


def verify_files(
    record: dict[str, object],
    suite: dict[str, object],
    prepared: dict[tuple[str, str], dict[str, object]],
    lock: dict[str, object],
    require_files: bool,
) -> bool:
    manifest_path = IMAGES_ROOT / record["manifest"]
    if not manifest_path.exists():
        if require_files:
            raise ValueError(f"missing reference manifest: {manifest_path}")
        return False
    if sha256(manifest_path) != record["manifest_sha256"]:
        raise ValueError(f"manifest SHA-256 mismatch: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    pixel_format = record["pixel_format"]
    key = (record["source_id"], record["preparation_id"])
    preparation = prepared[key]
    prepared_output = {
        "i400": preparation["outputs"]["gray8"],
        "i420": preparation["outputs"]["yuv420p8"],
        "i444": preparation["outputs"]["yuv444p8"],
    }[pixel_format]
    comparisons = {
        "case_id": record["case_id"],
        "suite_id": record["suite_id"],
        "source_id": record["source_id"],
        "preparation_id": record["preparation_id"],
        "dimensions": record["dimensions"],
        "qp": record["qp"],
        "outputs": record["outputs"],
        "avm": lock["avm"],
        "encoder_threads": lock["encoder_threads"],
    }
    for field, expected in comparisons.items():
        if manifest[field] != expected:
            raise ValueError(f"{manifest_path}: lock mismatch for {field}")
    if manifest["prepared_input"]["pixel_format"] != pixel_format:
        raise ValueError(f"{manifest_path}: prepared pixel format mismatch")
    if manifest["prepared_input"]["sha256"] != prepared_output["sha256"]:
        raise ValueError(f"{manifest_path}: prepared input SHA-256 mismatch")
    if manifest["prepared_manifest_sha256"] != preparation["manifest_sha256"]:
        raise ValueError(f"{manifest_path}: prepared manifest SHA-256 mismatch")
    if manifest["bit_depth"] != 8 or manifest["range"] != "full":
        raise ValueError(f"{manifest_path}: unexpected pixel contract")
    if f"--threads={lock['encoder_threads']}" not in manifest["commands"]["encode"]:
        raise ValueError(f"{manifest_path}: encoder thread count is not recorded")
    if f"--qp={record['qp']}" not in manifest["commands"]["encode"]:
        raise ValueError(f"{manifest_path}: encoder QP is not recorded")
    if pixel_format == "i420" and "--chroma-sample-position=unspecified" not in manifest[
        "commands"
    ]["encode"]:
        raise ValueError(f"{manifest_path}: chroma sample position is not recorded")

    width, height = record["dimensions"]
    for output_name, output in record["outputs"].items():
        output_path = manifest_path.parent / output["filename"]
        if not output_path.is_file():
            raise ValueError(f"missing reference output: {output_path}")
        if output_path.stat().st_size != output["byte_size"]:
            raise ValueError(f"reference output size mismatch: {output_path}")
        if sha256(output_path) != output["sha256"]:
            raise ValueError(f"reference output SHA-256 mismatch: {output_path}")
        if output_name == "decoded":
            if output_path.suffix != f".{pixel_format}":
                raise ValueError(f"decoded extension mismatch: {output_path}")
            if output["byte_size"] != expected_size(pixel_format, width, height):
                raise ValueError(f"raw plane size formula mismatch: {output_path}")
        elif output_name == "encoded_obu":
            ledger = [entry.ledger_entry() for entry in parse_obus(output_path.read_bytes())]
            if ledger != manifest["obu_ledger"]:
                raise ValueError(f"OBU ledger mismatch: {output_path}")
        elif output_name == "decoded_yuv_alias":
            if output_path.name != "decoded.yuv":
                raise ValueError(f"unexpected YUV alias filename: {output_path}")
            if output["sha256"] != record["outputs"]["decoded"]["sha256"]:
                raise ValueError(f"YUV alias differs from normative decode: {output_path}")
        elif output_name == "decoded_png":
            if output_path.name != "decoded.png":
                raise ValueError(f"unexpected PNG preview filename: {output_path}")
            if png_contract(output_path) != (width, height, 8, 2):
                raise ValueError(f"unexpected PNG preview contract: {output_path}")

    expected_outputs = {"encoded_obu", "decoded"}
    supplemental_outputs = suite["supplemental_outputs"]
    if supplemental_outputs["decoded_yuv_alias"]:
        expected_outputs.add("decoded_yuv_alias")
    if supplemental_outputs["decoded_png"]:
        expected_outputs.add("decoded_png")
    if set(record["outputs"]) != expected_outputs:
        raise ValueError(f"{manifest_path}: unexpected output set")
    return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--require-files",
        action="store_true",
        help="fail if the ignored reference files are absent",
    )
    arguments = parser.parse_args()

    encodings = json.loads(ENCODINGS_PATH.read_text(encoding="utf-8"))
    suite = encodings["suites"][0]
    prepared_lock = json.loads(PREPARED_LOCK_PATH.read_text(encoding="utf-8"))
    preparations = prepared_lock["preparations"]
    prepared = {
        (record["source_id"], record["preparation_id"]): record
        for record in preparations
    }
    lock = json.loads(REFERENCE_LOCK_PATH.read_text(encoding="utf-8"))
    if lock["avm"]["revision"] != encodings["avm_revision"]:
        raise ValueError("reference lock AVM revision differs from encoding recipe")
    if lock["encoder_threads"] < 1 or lock["generation_jobs"] < 1:
        raise ValueError("invalid generation concurrency in reference lock")
    records = lock["references"]
    actual_keys = {
        (
            record["source_id"],
            record["preparation_id"],
            record["pixel_format"],
            record["qp"],
        )
        for record in records
    }
    required_keys = expected_keys(suite, preparations)
    if actual_keys != required_keys or len(records) != len(required_keys):
        raise ValueError("reference lock does not cover the recipe matrix exactly once")

    present = sum(
        verify_files(record, suite, prepared, lock, arguments.require_files)
        for record in records
    )
    reference_root = (
        IMAGES_ROOT
        / "reference"
        / f"avm-{lock['avm']['revision']}"
        / suite["id"]
    )
    if reference_root.is_dir():
        local_cases = {path.name for path in reference_root.iterdir() if path.is_dir()}
        expected_cases = {record["case_id"] for record in records}
        if local_cases != expected_cases:
            raise ValueError("local reference directories differ from the locked cases")
    print(
        f"verified {len(records)} reference records; "
        f"local references present: {present}/{len(records)}; "
        f"AVM {lock['avm']['revision'][:12]}"
    )


if __name__ == "__main__":
    main()
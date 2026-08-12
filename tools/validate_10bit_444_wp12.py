#!/usr/bin/env python3
"""Validate the constrained unfiltered 10-bit 4:4:4 WP12 fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "tests/fixtures/10bit/avm-reference/manifest.json"
EXPECTED_RECORDS = {
    "tiny-precision-16x16-yuv444p10le-qp128": (16, 16),
    "odd-geometry-17x15-yuv444p10le-qp128": (17, 15),
}
PLANE_NAMES = ("Y", "U", "V")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_records(manifest_path: Path) -> list[dict[str, Any]]:
    document = json.loads(manifest_path.read_text(encoding="utf-8"))
    selected = {
        str(record.get("name")): record
        for record in document.get("artifacts", [])
        if record.get("name") in EXPECTED_RECORDS
    }
    missing = sorted(set(EXPECTED_RECORDS) - set(selected))
    if missing:
        raise RuntimeError(f"missing WP12 records: {', '.join(missing)}")
    records = []
    for name in EXPECTED_RECORDS:
        record = dict(selected[name])
        width, height = EXPECTED_RECORDS[name]
        if (
            record.get("pixel_format") != "yuv444p10le"
            or int(record.get("bit_depth", 0)) != 10
            or int(record.get("width", 0)) != width
            or int(record.get("height", 0)) != height
            or record.get("lossless") is not False
            or int(record.get("qp", -1)) != 128
            or record.get("census", {}).get("filter_accounting") != []
        ):
            raise RuntimeError(f"{name}: fixture is not the constrained unfiltered WP12 case")
        record["manifest_directory"] = manifest_path.parent
        records.append(record)
    return records


def validate_record(decoder: Path, temporary: Path, record: dict[str, Any]) -> str:
    name = str(record["name"])
    width = int(record["width"])
    height = int(record["height"])
    plane_size = width * height * 2
    output_size = plane_size * len(PLANE_NAMES)
    directory = Path(record["manifest_directory"])
    obu_path = directory / str(record["obu"]["file"])
    reference_path = directory / str(record["decoded"]["file"])
    encoded = obu_path.read_bytes()
    reference = reference_path.read_bytes()
    if (
        len(encoded) != int(record["obu"]["size_bytes"])
        or sha256_bytes(encoded) != record["obu"]["sha256"]
    ):
        raise RuntimeError(f"{name}: retained OBU differs from manifest")
    if (
        len(reference) != output_size
        or len(reference) != int(record["decoded"]["size_bytes"])
        or sha256_bytes(reference) != record["decoded"]["sha256"]
    ):
        raise RuntimeError(f"{name}: retained AVM output differs from manifest")

    output_path = temporary / f"{name}.yuv444p10le"
    completed = subprocess.run(
        [str(decoder), str(obu_path), "-o", str(output_path)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{name}: decoder failed ({completed.returncode}):\n"
            f"{completed.stderr.strip()}"
        )
    actual = output_path.read_bytes()
    if len(actual) != output_size:
        raise RuntimeError(
            f"{name}: expected {output_size} output bytes, found {len(actual)}"
        )
    for plane, plane_name in enumerate(PLANE_NAMES):
        start = plane * plane_size
        end = start + plane_size
        if actual[start:end] != reference[start:end]:
            raise RuntimeError(f"{name}: unfiltered {plane_name} differs from AVM")
    return f"{name}: {output_size} bytes exact unfiltered Y/U/V"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    arguments = parser.parse_args()
    decoder = arguments.decoder.resolve()
    records = load_records(arguments.manifest.resolve())
    with tempfile.TemporaryDirectory(prefix="av2heif-10bit-444-wp12-") as scratch:
        temporary = Path(scratch)
        for record in records:
            print(validate_record(decoder, temporary, record))
    print("10-bit 4:4:4 WP12: 2 records exact native unfiltered Y/U/V")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
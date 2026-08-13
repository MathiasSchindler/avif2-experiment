#!/usr/bin/env python3
"""Validate the retained 10-bit 4:4:4 matrix through native and public decode."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any

from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFESTS = tuple(
    path
    for path in (
        ROOT / "tests/fixtures/10bit/avm-reference/manifest.json",
        ROOT / "images/artifacts/10bit/avm-reference/manifest.json",
    )
    if path.is_file()
)
EXPECTED_SYNTHETIC_RECORDS = 21
EXPECTED_PHOTOGRAPHIC_RECORDS = 18


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run(command: list[str], name: str) -> None:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        diagnostic = completed.stderr.split("  local-part:", 1)[0].strip()
        raise RuntimeError(
            f"{name}: command failed ({completed.returncode}): "
            f"{' '.join(command)}\n{diagnostic}"
        )


def load_records(manifest_paths: list[Path]) -> list[dict[str, Any]]:
    records = []
    counts = []
    for manifest_path in manifest_paths:
        document = json.loads(manifest_path.read_text(encoding="utf-8"))
        selected = [
            record
            for record in document.get("artifacts", [])
            if record.get("pixel_format") == "yuv444p10le"
        ]
        counts.append(len(selected))
        for record in selected:
            copied = dict(record)
            copied["manifest_directory"] = manifest_path.parent
            records.append(copied)
    expected_counts = [EXPECTED_SYNTHETIC_RECORDS, EXPECTED_PHOTOGRAPHIC_RECORDS]
    if len(manifest_paths) == 2 and counts != expected_counts:
        raise RuntimeError(
            f"expected 4:4:4 manifest counts {expected_counts}, found {counts}"
        )
    names = [str(record["name"]) for record in records]
    if len(names) != len(set(names)):
        raise RuntimeError("10-bit 4:4:4 manifests contain duplicate names")
    return records


def expected_size(record: dict[str, Any]) -> int:
    return 6 * int(record["width"]) * int(record["height"])


def validate_record(
    decoder: Path,
    artifact_tool: Path | None,
    temporary: Path,
    record: dict[str, Any],
) -> str:
    name = str(record["name"])
    directory = Path(record["manifest_directory"])
    obu_path = directory / str(record["obu"]["file"])
    reference_path = directory / str(record["decoded"]["file"])
    encoded = obu_path.read_bytes()
    reference = reference_path.read_bytes()
    output_size = expected_size(record)
    if len(encoded) != int(record["obu"]["size_bytes"]) or (
        sha256_bytes(encoded) != record["obu"]["sha256"]
    ):
        raise RuntimeError(f"{name}: retained OBU differs from manifest")
    if len(reference) != output_size or len(reference) != int(
        record["decoded"]["size_bytes"]
    ) or sha256_bytes(reference) != record["decoded"]["sha256"]:
        raise RuntimeError(f"{name}: retained AVM output differs from manifest")

    native_output = temporary / f"{name}-native.yuv444p10le"
    run(
        [str(decoder), str(obu_path), "-o", str(native_output)],
        f"{name} native",
    )
    if native_output.read_bytes() != reference:
        raise RuntimeError(f"{name}: native output differs from AVM")

    if artifact_tool is not None:
        entries = parse_obus(encoded)
        sequence = next(
            entry for entry in entries if entry.name == "sequence_header"
        )
        frame = next(
            entry for entry in entries if entry.name == "closed_loop_key"
        )
        config = temporary / f"{name}-config.obu"
        payload = temporary / f"{name}-payload.obu"
        container = temporary / f"{name}.heif"
        public_output = temporary / f"{name}-public.yuv444p10le"
        config.write_bytes(sequence.encoded)
        payload.write_bytes(frame.encoded)
        run(
            [
                str(artifact_tool),
                "pack",
                str(config),
                str(payload),
                str(container),
                "1",
                "13",
                "1",
                "limited",
            ],
            f"{name} pack",
        )
        run(
            [str(artifact_tool), "decode", str(container), str(public_output)],
            f"{name} public",
        )
        if public_output.read_bytes() != reference:
            raise RuntimeError(f"{name}: public output differs from AVM")
        return f"{name}: {output_size} bytes exact native/public"
    return f"{name}: {output_size} bytes exact native"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--artifact-tool", type=Path)
    parser.add_argument("--native-only", action="store_true")
    parser.add_argument("--manifest", type=Path, action="append", default=[])
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 2))
    arguments = parser.parse_args()
    if arguments.jobs < 1:
        parser.error("--jobs must be at least 1")
    if arguments.native_only and arguments.artifact_tool is not None:
        parser.error("--native-only cannot be combined with --artifact-tool")
    if not arguments.native_only and arguments.artifact_tool is None:
        parser.error("--artifact-tool is required unless --native-only is used")
    manifest_paths = [
        path.resolve() for path in (arguments.manifest or DEFAULT_MANIFESTS)
    ]
    records = load_records(manifest_paths)
    if not records:
        raise RuntimeError("no retained 10-bit 4:4:4 records found")

    with tempfile.TemporaryDirectory(prefix="av2heif-10bit-444-") as scratch:
        temporary = Path(scratch)
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=min(arguments.jobs, len(records))
        ) as executor:
            results = executor.map(
                lambda record: validate_record(
                    arguments.decoder.resolve(),
                    arguments.artifact_tool.resolve()
                    if arguments.artifact_tool is not None else None,
                    temporary,
                    record,
                ),
                records,
            )
            for result in results:
                print(result)
    surfaces = "native" if arguments.native_only else "native/public"
    print(f"10-bit 4:4:4 corpus: {len(records)} records exact {surfaces}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

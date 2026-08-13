#!/usr/bin/env python3
"""Validate the production I444 corpus through canonical HEIF public decode."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = (
    ROOT / "images" / "artifacts-8bit-444" / "manifest.json"
)


def run(command: list[str]) -> None:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stderr.strip()}"
        )


def validate_record(tool: Path, manifest_path: Path, temporary: Path,
                    record: dict[str, object]) -> str:
    name = str(record["name"])
    source = manifest_path.parent / str(record["file"])
    entries = parse_obus(source.read_bytes())
    sequence = next(entry for entry in entries if entry.name == "sequence_header")
    frame = next(entry for entry in entries if entry.name == "closed_loop_key")
    config = temporary / f"{name}-config.obu"
    payload = temporary / f"{name}-payload.obu"
    container = temporary / f"{name}.heif"
    output = temporary / f"{name}.i444"
    config.write_bytes(sequence.encoded)
    payload.write_bytes(frame.encoded)
    run([
        str(tool),
        "pack",
        str(config),
        str(payload),
        str(container),
        "2",
        "2",
        "2",
        "limited",
    ])
    run([str(tool), "decode", str(container), str(output)])
    decoded = output.read_bytes()
    expected_size = int(record["width"]) * int(record["height"]) * 3
    if len(decoded) != expected_size:
        raise RuntimeError(
            f"{name}: decoded {len(decoded)} bytes, expected {expected_size}"
        )
    digest = hashlib.sha256(decoded).hexdigest()
    if digest != record["decoded_i444_sha256"]:
        raise RuntimeError(
            f"{name}: SHA-256 {digest}, expected {record['decoded_i444_sha256']}"
        )
    return f"{name}: {expected_size} bytes exact"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-tool", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")

    tool = args.artifact_tool.resolve()
    manifest_path = args.manifest.resolve()
    records = json.loads(manifest_path.read_text(encoding="utf-8")).get("artifacts", [])
    if len(records) != 12:
        raise RuntimeError(f"expected 12 artifacts, found {len(records)}")

    with tempfile.TemporaryDirectory(prefix="av2heif-i444-public-") as scratch:
        temporary = Path(scratch)
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=min(args.jobs, len(records))
        ) as executor:
            results = executor.map(
                lambda record: validate_record(tool, manifest_path, temporary, record),
                records,
            )
            for result in results:
                print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

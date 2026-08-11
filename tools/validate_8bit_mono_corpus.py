#!/usr/bin/env python3
"""Validate the production 8-bit monochrome artifact corpus."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

from production_parity import load_production_baseline


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "images" / "artifacts-8bit-mono" / "manifest.json"
DEFAULT_BASELINE = ROOT / "tests" / "8bit-production-parity.sha256"


def validate_record(
    decoder: Path,
    artifact_dir: Path,
    output_dir: Path,
    baseline: dict[str, tuple[int, int, str]],
    record: dict[str, object],
) -> str:
    name = str(record["name"])
    expected_width, expected_height, expected_digest = baseline[name]
    if (record["width"], record["height"], record["decoded_luma_sha256"]) != (
        expected_width,
        expected_height,
        expected_digest,
    ):
        raise RuntimeError(f"{name}: manifest differs from tracked baseline")
    source = artifact_dir / str(record["file"])
    output = output_dir / f"{name}.i400"
    completed = subprocess.run(
        [str(decoder), str(source), "-o", str(output)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{name}: decoder exit {completed.returncode}\n"
            f"{completed.stderr.strip()}"
        )
    decoded = output.read_bytes()
    expected_size = expected_width * expected_height
    if len(decoded) != expected_size:
        raise RuntimeError(
            f"{name}: decoded {len(decoded)} bytes, expected {expected_size}"
        )
    digest = hashlib.sha256(decoded).hexdigest()
    if digest != expected_digest:
        raise RuntimeError(
            f"{name}: SHA-256 {digest}, expected {expected_digest}"
        )
    return f"{name}: {expected_size} bytes exact"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")

    manifest_path = args.manifest.resolve()
    records = json.loads(manifest_path.read_text(encoding="utf-8")).get("artifacts", [])
    if len(records) != 12:
        raise RuntimeError(f"expected 12 artifacts, found {len(records)}")
    baseline = load_production_baseline(args.baseline.resolve(), "i400")
    if {record["name"] for record in records} != set(baseline):
        raise RuntimeError("monochrome manifest names do not match tracked baseline")

    with tempfile.TemporaryDirectory(prefix="av2heif-mono-validation-") as scratch:
        output_dir = Path(scratch)
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=min(args.jobs, len(records))
        ) as executor:
            results = executor.map(
                lambda record: validate_record(
                    args.decoder.resolve(),
                    manifest_path.parent,
                    output_dir,
                    baseline,
                    record,
                ),
                records,
            )
            for result in results:
                print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

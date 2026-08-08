#!/usr/bin/env python3
"""Validate the production 8-bit monochrome artifact corpus."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "images" / "artifacts-8bit-mono" / "manifest.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()

    manifest_path = args.manifest.resolve()
    records = json.loads(manifest_path.read_text(encoding="utf-8")).get("artifacts", [])
    if len(records) != 12:
        raise RuntimeError(f"expected 12 artifacts, found {len(records)}")

    with tempfile.TemporaryDirectory(prefix="av2heif-mono-validation-") as scratch:
        output_dir = Path(scratch)
        for record in records:
            source = manifest_path.parent / record["file"]
            output = output_dir / f"{record['name']}.i400"
            completed = subprocess.run(
                [str(args.decoder.resolve()), str(source), "-o", str(output)],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            if completed.returncode != 0:
                raise RuntimeError(
                    f"{record['name']}: decoder exit {completed.returncode}\n"
                    f"{completed.stderr.strip()}"
                )
            decoded = output.read_bytes()
            expected_size = record["width"] * record["height"]
            if len(decoded) != expected_size:
                raise RuntimeError(
                    f"{record['name']}: decoded {len(decoded)} bytes, "
                    f"expected {expected_size}"
                )
            digest = hashlib.sha256(decoded).hexdigest()
            if digest != record["decoded_luma_sha256"]:
                raise RuntimeError(
                    f"{record['name']}: SHA-256 {digest}, expected "
                    f"{record['decoded_luma_sha256']}"
                )
            print(f"{record['name']}: {expected_size} bytes exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

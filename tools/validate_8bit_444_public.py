#!/usr/bin/env python3
"""Validate the production I444 corpus through canonical HEIF public decode."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from m0_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "images" / "artifacts-8bit-444" / "manifest.json"


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-tool", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()

    tool = args.artifact_tool.resolve()
    manifest_path = args.manifest.resolve()
    records = json.loads(manifest_path.read_text(encoding="utf-8")).get("artifacts", [])
    if len(records) != 12:
        raise RuntimeError(f"expected 12 artifacts, found {len(records)}")

    with tempfile.TemporaryDirectory(prefix="av2heif-i444-public-") as scratch:
        temporary = Path(scratch)
        for record in records:
            source = manifest_path.parent / record["file"]
            entries = parse_obus(source.read_bytes())
            sequence = next(entry for entry in entries if entry.name == "sequence_header")
            frame = next(entry for entry in entries if entry.name == "closed_loop_key")
            config = temporary / f"{record['name']}-config.obu"
            payload = temporary / f"{record['name']}-payload.obu"
            container = temporary / f"{record['name']}.heif"
            output = temporary / f"{record['name']}.i444"
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
            expected_size = record["width"] * record["height"] * 3
            if len(decoded) != expected_size:
                raise RuntimeError(
                    f"{record['name']}: decoded {len(decoded)} bytes, "
                    f"expected {expected_size}"
                )
            digest = hashlib.sha256(decoded).hexdigest()
            if digest != record["decoded_i444_sha256"]:
                raise RuntimeError(
                    f"{record['name']}: SHA-256 {digest}, expected "
                    f"{record['decoded_i444_sha256']}"
                )
            print(f"{record['name']}: {expected_size} bytes exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

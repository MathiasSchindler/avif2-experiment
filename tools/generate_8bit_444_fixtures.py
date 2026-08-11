#!/usr/bin/env python3
"""Generate and validate deterministic 8-bit 4:4:4 AV2 fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from generate_i420_fixtures import FIXTURES, pixel


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm-arm64" / "avmenc.exe"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm-arm64" / "avmdec.exe"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes) -> int:
    value = 14695981039346656037
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & ((1 << 64) - 1)
    return value


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    parser.add_argument(
        "--output-dir", type=Path, default=ROOT / "tests" / "fixtures"
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest = []

    with tempfile.TemporaryDirectory(prefix="av2heif-m12-") as temporary:
        temporary_dir = Path(temporary)
        for fixture in FIXTURES:
            source = bytes(
                pixel(
                    fixture.pattern,
                    plane,
                    x,
                    y,
                    fixture.width,
                    fixture.height,
                )
                for plane in range(3)
                for y in range(fixture.height)
                for x in range(fixture.width)
            )
            raw = temporary_dir / f"{fixture.name}.i444"
            decoded = temporary_dir / f"{fixture.name}-decoded.i444"
            output = args.output_dir / f"i444-{fixture.name}.obu"
            raw.write_bytes(source)
            encode_command = [
                str(args.encoder),
                "--debug",
                "--codec=av2",
                "--obu",
                "--passes=1",
                "--limit=1",
                "--good",
                "--cpu-used=9",
                "--threads=1",
                "--profile=1",
                f"--width={fixture.width}",
                f"--height={fixture.height}",
                "--i444",
                "--bit-depth=8",
                "--input-bit-depth=8",
                "--input-chroma-subsampling-x=0",
                "--input-chroma-subsampling-y=0",
                "--force-video-mode=0",
                "--film-grain-test=0",
                "--enable-lcr=0",
                "--enable-operating-point-sets=0",
                "--enable-atlas=0",
                "--enable-intrabc=0",
                "--enable-restoration=0",
                "--end-usage=q",
                "--qp=128",
                "--test-decode=fatal",
                "--quiet",
                "-o",
                str(output),
                str(raw),
            ]
            run(encode_command)
            decode_command = [
                str(args.decoder),
                "--codec=av2",
                "--rawvideo",
                "--output-bit-depth=8",
                "--limit=1",
                "-o",
                str(decoded),
                str(output),
            ]
            run(decode_command)
            reconstructed = decoded.read_bytes()
            expected_size = fixture.width * fixture.height * 3
            if len(reconstructed) != expected_size:
                raise RuntimeError(f"unexpected AVM output for {fixture.name}")
            obu = output.read_bytes()
            manifest.append(
                {
                    "name": fixture.name,
                    "width": fixture.width,
                    "height": fixture.height,
                    "chroma_format": "4:4:4",
                    "sequence_profile_idc": 4,
                    "bit_depth": 8,
                    "pattern": fixture.pattern,
                    "file": output.name,
                    "source_sha256": sha256(source),
                    "obu_bytes": len(obu),
                    "obu_sha256": sha256(obu),
                    "decoded_sha256": sha256(reconstructed),
                    "decoded_fnv1a64": f"0x{fnv1a64(reconstructed):016x}",
                    "encode_arguments": encode_command[1:-3]
                    + ["-o", output.name, raw.name],
                    "decode_arguments": decode_command[1:-3]
                    + ["-o", decoded.name, output.name],
                }
            )

    (args.output_dir / "i444-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
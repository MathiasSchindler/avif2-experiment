#!/usr/bin/env python3
"""Generate and validate deterministic 8-bit monochrome AV2 fixtures."""

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

    with tempfile.TemporaryDirectory(prefix="av2heif-m11-") as temporary:
        temporary_dir = Path(temporary)
        for fixture in FIXTURES:
            y_plane = bytes(
                pixel(fixture.pattern, 0, x, y, fixture.width, fixture.height)
                for y in range(fixture.height)
                for x in range(fixture.width)
            )
            y4m = temporary_dir / f"{fixture.name}.y4m"
            decoded = temporary_dir / f"{fixture.name}.i420"
            output = args.output_dir / f"monochrome-{fixture.name}.obu"
            header = (
                f"YUV4MPEG2 W{fixture.width} H{fixture.height} "
                "F1:1 Ip A0:0 Cmono\nFRAME\n"
            ).encode("ascii")
            y4m.write_bytes(header + y_plane)
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
                "--profile=0",
                "--bit-depth=8",
                "--input-bit-depth=8",
                "--monochrome",
                "--force-video-mode=0",
                "--film-grain-test=0",
                "--enable-lcr=0",
                "--enable-operating-point-sets=0",
                "--enable-atlas=0",
                "--end-usage=q",
                "--qp=128",
                "--test-decode=fatal",
                "--quiet",
                "-o",
                str(output),
                str(y4m),
            ]
            run(encode_command)
            decode_command = [
                str(args.decoder),
                "--codec=av2",
                "--i420",
                "--limit=1",
                "-o",
                str(decoded),
                str(output),
            ]
            run(decode_command)
            decoded_y = decoded.read_bytes()[: len(y_plane)]
            if len(decoded_y) != len(y_plane):
                raise RuntimeError(f"short AVM decode for {fixture.name}")
            obu = output.read_bytes()
            manifest.append(
                {
                    "name": fixture.name,
                    "width": fixture.width,
                    "height": fixture.height,
                    "chroma_format": "4:0:0",
                    "bit_depth": 8,
                    "pattern": fixture.pattern,
                    "file": output.name,
                    "source_y_sha256": sha256(y_plane),
                    "obu_bytes": len(obu),
                    "obu_sha256": sha256(obu),
                    "decoded_y_sha256": sha256(decoded_y),
                    "decoded_y_fnv1a64": f"0x{fnv1a64(decoded_y):016x}",
                    "encode_arguments": encode_command[1:-3]
                    + ["-o", output.name, f"{fixture.name}.y4m"],
                    "decode_arguments": decode_command[1:-3]
                    + ["-o", f"{fixture.name}.i420", output.name],
                }
            )

    (args.output_dir / "monochrome-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
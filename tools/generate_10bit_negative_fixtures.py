#!/usr/bin/env python3
"""Generate WP10 unsupported-format and malformed AV2 fixtures."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

from generate_10bit_avm_references import (
    decode_command,
    encode_command,
    relative_command,
    require_tool,
    run,
    sha256_file,
    tool_banner,
)
from generate_10bit_synthetic import encode_u16le, generate_plane
from m0_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "tests" / "fixtures" / "10bit" / "negative"
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm-negative" / "avmenc"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm-negative" / "avmdec"


def specialized_encode_command(
    encoder: Path,
    source: Path,
    output: Path,
    kind: str,
) -> list[str]:
    command = encode_command(
        encoder, source, output, "yuv420p10le", 16, 16, 128
    )
    profile_index = command.index("--profile=0")
    command[profile_index] = "--profile=2"
    if kind == "12bit-420":
        command[command.index("--bit-depth=10")] = "--bit-depth=12"
        command[command.index("--input-bit-depth=10")] = "--input-bit-depth=12"
    elif kind == "10bit-422":
        command[command.index("--i420")] = "--i422"
        command[command.index("--input-chroma-subsampling-y=1")] = (
            "--input-chroma-subsampling-y=0"
        )
    else:
        raise ValueError(f"unknown specialized format: {kind}")
    return command


def source_bytes(kind: str) -> bytes:
    dimensions = (
        ((16, 16), (8, 8), (8, 8))
        if kind == "12bit-420"
        else ((16, 16), (8, 16), (8, 16))
    )
    values = []
    for plane, (width, height) in enumerate(dimensions):
        generated = generate_plane("tiny-precision", plane, width, height)
        if kind == "12bit-420":
            generated = [value * 4 + ((index + plane) & 3)
                         for index, value in enumerate(generated)]
        values.extend(generated)
    return encode_u16le(values)


def malformed_profile(data: bytes) -> bytes:
    entries = parse_obus(data)
    sequence = next(entry for entry in entries if entry.name == "sequence_header")
    result = bytearray(data)
    payload = sequence.data_offset + sequence.header_size
    result[payload] = (result[payload] & ~0x7C) | 0x10
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    arguments = parser.parse_args()
    output = arguments.output_dir.resolve()
    encoder = require_tool(arguments.encoder)
    decoder = require_tool(arguments.decoder)
    output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(
        prefix="wp10-negative-", dir=output.parent
    ) as temporary_text:
        temporary = Path(temporary_text)
        records = []
        valid_obus = {}
        for kind, file_name, decoded_size in (
            ("12bit-420", "unsupported-12bit-420.obu", 768),
            ("10bit-422", "unsupported-10bit-422.obu", 1024),
        ):
            source = temporary / f"{kind}.yuv"
            obu = temporary / file_name
            decoded = temporary / f"{kind}-decoded.yuv"
            source.write_bytes(source_bytes(kind))
            encode = specialized_encode_command(encoder, source, obu, kind)
            run(encode)
            run(decode_command(decoder, obu, decoded))
            if decoded.stat().st_size != decoded_size:
                raise RuntimeError(f"unexpected decoded size: {decoded.name}")
            valid_obus[kind] = obu.read_bytes()
            canonical_source = (
                ROOT / "build" / "reference" / "wp10-negative-sources" /
                f"{kind}.yuv"
            )
            canonical_obu = output / file_name
            records.append(
                {
                    "name": kind,
                    "file": file_name,
                    "kind": "valid unsupported format",
                    "size_bytes": obu.stat().st_size,
                    "sha256": sha256_file(obu),
                    "avmdec_valid": True,
                    "avmdec_output_sha256": sha256_file(decoded),
                    "source": {
                        "generation": "deterministic in-memory planar pattern",
                        "size_bytes": source.stat().st_size,
                        "sha256": sha256_file(source),
                    },
                    "encode_command": relative_command(
                        specialized_encode_command(
                            encoder, canonical_source, canonical_obu, kind
                        )
                    ),
                    "expected_project_decoder": {
                        "exit_code": 3,
                        "stderr_contains": "unsupported feature",
                    },
                }
            )

        base = valid_obus["12bit-420"]
        mutations = (
            (
                "malformed-profile-chroma",
                "malformed-profile-chroma.obu",
                malformed_profile(base),
                3,
                "unsupported feature",
            ),
            (
                "truncated-frame",
                "truncated-frame.obu",
                base[:-1],
                1,
                "truncated input",
            ),
        )
        for name, file_name, data, exit_code, message in mutations:
            path = temporary / file_name
            path.write_bytes(data)
            records.append(
                {
                    "name": name,
                    "file": file_name,
                    "kind": "malformed",
                    "size_bytes": len(data),
                    "sha256": sha256_file(path),
                    "derived_from": "unsupported-12bit-420.obu",
                    "avmdec_valid": False,
                    "expected_project_decoder": {
                        "exit_code": exit_code,
                        "stderr_contains": message,
                    },
                }
            )

        revision = run(
            ["git", "-C", str(ROOT / "vendor" / "avm"), "rev-parse", "HEAD"]
        ).strip()
        manifest = {
            "schema": 1,
            "description": "WP10 unsupported-format and malformed AV2 fixtures",
            "generator": {
                "file": "tools/generate_10bit_negative_fixtures.py",
                "sha256": sha256_file(Path(__file__).resolve()),
                "command": [
                    "python3",
                    "tools/generate_10bit_negative_fixtures.py",
                ],
            },
            "tools": {
                "avm_revision": revision,
                "avm_configuration": {"CONFIG_TESTONLY_12BIT_SUPPORT": 1},
                "avmenc": tool_banner(encoder, "AOMedia Project AV2 Encoder"),
                "avmenc_sha256": sha256_file(encoder),
                "avmdec": tool_banner(decoder, "AOMedia Project AV2 Decoder"),
                "avmdec_sha256": sha256_file(decoder),
            },
            "artifacts": sorted(records, key=lambda record: record["name"]),
        }
        (temporary / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        if output.exists():
            shutil.rmtree(output)
        output.mkdir()
        for record in records:
            shutil.copyfile(temporary / record["file"], output / record["file"])
        shutil.copyfile(temporary / "manifest.json", output / "manifest.json")

    print(f"generated {len(records)} negative fixtures in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
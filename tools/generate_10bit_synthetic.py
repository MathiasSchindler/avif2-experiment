#!/usr/bin/env python3
"""Generate the deterministic synthetic 10-bit planar source matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "tests" / "fixtures" / "10bit"
SENTINELS = (0, 1, 2, 3, 255, 256, 511, 512, 1020, 1021, 1022, 1023)
FIXTURES = (
    ("tiny-precision", 16, 16, ("flat fields", "horizontal ramp", "vertical ramp")),
    ("odd-geometry", 17, 15, ("horizontal ramp", "vertical ramp", "checkerboard")),
    ("boundary-extremes", 65, 63, ("hard edges", "impulses", "clipping extremes")),
    ("high-detail", 96, 80, ("checkerboards", "impulses", "high-frequency detail")),
)
PIXEL_FORMATS = ("yuv420p10le", "yuv444p10le")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def plane_dimensions(
    pixel_format: str, width: int, height: int
) -> tuple[tuple[int, int], ...]:
    if pixel_format == "yuv444p10le":
        return ((width, height),) * 3
    if pixel_format == "yuv420p10le":
        return (
            (width, height),
            ((width + 1) // 2, (height + 1) // 2),
            ((width + 1) // 2, (height + 1) // 2),
        )
    raise ValueError(f"unsupported pixel format: {pixel_format}")


def pattern_sample(
    fixture: str,
    plane: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> int:
    horizontal = x * 1023 // max(width - 1, 1)
    vertical = y * 1023 // max(height - 1, 1)
    checker = 96 if ((x // 2) ^ (y // 2)) & 1 else 928
    if fixture == "tiny-precision":
        if plane == 0:
            return (horizontal + (y & 3)) & 1023
        if plane == 1:
            return (vertical + 17) & 1023
        return checker
    if fixture == "odd-geometry":
        if plane == 0:
            return horizontal
        if plane == 1:
            return vertical
        return checker if x < (width + 1) // 2 else 1023 - checker
    if fixture == "boundary-extremes":
        if plane == 0:
            value = 0 if x < 64 and y < 32 else 1023
            if (x, y) in ((0, 0), (63, 31), (64, 32), (width - 1, height - 1)):
                value = 1023 - value
            return value
        if plane == 1:
            return 1023 if x in (0, width - 1) or y == height // 2 else 1
        return 0 if y < 32 else (1023 if (x & 1) == 0 else 2)
    if fixture == "high-detail":
        mixed = (
            x * (73 + plane * 18)
            + y * (151 + plane * 22)
            + x * y * (7 + plane * 2)
            + (((x ^ y) & 7) << (4 + plane))
        )
        if ((x // 4) ^ (y // 4) ^ plane) & 1:
            mixed ^= 1023
        return mixed & 1023
    raise ValueError(f"unknown fixture: {fixture}")


def generate_plane(
    fixture: str, plane: int, width: int, height: int
) -> list[int]:
    values = [
        pattern_sample(fixture, plane, x, y, width, height)
        for y in range(height)
        for x in range(width)
    ]
    for index, value in enumerate(SENTINELS):
        values[index] = value
    return values


def encode_u16le(values: list[int]) -> bytes:
    encoded = bytearray(len(values) * 2)
    for index, value in enumerate(values):
        encoded[index * 2] = value & 0xFF
        encoded[index * 2 + 1] = value >> 8
    return bytes(encoded)


def generate_artifact(
    fixture: str,
    width: int,
    height: int,
    patterns: tuple[str, ...],
    pixel_format: str,
) -> tuple[str, bytes, dict[str, object]]:
    file_name = f"{fixture}-{width}x{height}-{pixel_format}.yuv"
    payload = bytearray()
    planes = []
    for plane, (plane_width, plane_height) in enumerate(
        plane_dimensions(pixel_format, width, height)
    ):
        values = generate_plane(
            fixture, plane, plane_width, plane_height
        )
        encoded = encode_u16le(values)
        offset = len(payload)
        payload.extend(encoded)
        planes.append(
            {
                "name": "YUV"[plane],
                "width": plane_width,
                "height": plane_height,
                "offset_bytes": offset,
                "size_bytes": len(encoded),
                "sha256": sha256_bytes(encoded),
                "minimum": min(values),
                "maximum": max(values),
                "distinct_values": len(set(values)),
                "sentinels": list(SENTINELS),
                "low_two_bits": sorted({value & 3 for value in values}),
            }
        )
    data = bytes(payload)
    record: dict[str, object] = {
        "name": fixture,
        "file": file_name,
        "format": pixel_format,
        "width": width,
        "height": height,
        "bit_depth": 10,
        "bytes_per_sample": 2,
        "size_bytes": len(data),
        "sha256": sha256_bytes(data),
        "patterns": list(patterns),
        "planes": planes,
    }
    return file_name, data, record


def build_matrix() -> tuple[dict[str, bytes], dict[str, object]]:
    files: dict[str, bytes] = {}
    artifacts = []
    for fixture, width, height, patterns in FIXTURES:
        for pixel_format in PIXEL_FORMATS:
            file_name, data, record = generate_artifact(
                fixture, width, height, patterns, pixel_format
            )
            files[file_name] = data
            artifacts.append(record)
    manifest: dict[str, object] = {
        "schema": 1,
        "description": "Deterministic synthetic 10-bit AV2 source matrix",
        "generator": {
            "file": "tools/generate_10bit_synthetic.py",
            "sha256": sha256_file(Path(__file__).resolve()),
            "command": [
                "python3",
                "tools/generate_10bit_synthetic.py",
            ],
        },
        "source_contract": {
            "storage": "headerless planar unsigned 16-bit little-endian words",
            "significant_bits": 10,
            "sample_range": [0, 1023],
            "plane_order": ["Y", "U", "V"],
            "row_padding_bytes": 0,
            "required_sentinels": list(SENTINELS),
        },
        "artifacts": artifacts,
    }
    return files, manifest


def write_matrix(output: Path) -> None:
    files, manifest = build_matrix()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="10bit-synthetic-", dir=output.parent
    ) as temporary_text:
        temporary = Path(temporary_text)
        for file_name, data in files.items():
            (temporary / file_name).write_bytes(data)
        (temporary / "source-manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        output.mkdir(exist_ok=True)
        for old in output.glob("*.yuv"):
            old.unlink()
        old_manifest = output / "source-manifest.json"
        if old_manifest.exists():
            old_manifest.unlink()
        for generated in sorted(temporary.iterdir()):
            shutil.copyfile(generated, output / generated.name)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    write_matrix(arguments.output_dir.resolve())
    files, _ = build_matrix()
    print(f"generated {len(files)} synthetic 10-bit sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
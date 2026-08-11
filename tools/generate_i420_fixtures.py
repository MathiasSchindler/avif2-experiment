#!/usr/bin/env python3
"""Generate deterministic one-frame 8-bit I420 carriage experiment sources."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Fixture:
    name: str
    width: int
    height: int
    pattern: str


FIXTURES = (
    Fixture("tiny", 16, 16, "gradient"),
    Fixture("odd", 17, 15, "gradient"),
    Fixture("ordinary", 160, 120, "zones"),
    Fixture("high-detail", 96, 80, "detail"),
)


def pixel(pattern: str, plane: int, x: int, y: int, width: int, height: int) -> int:
    if pattern == "gradient":
        return (31 + plane * 67 + x * 13 + y * 7 + (x * y) % 29) & 255
    if pattern == "zones":
        zone = ((x * 4) // max(width, 1)) + 4 * ((y * 3) // max(height, 1))
        return (17 + plane * 53 + zone * 19 + x * 2 + y * 3) & 255
    if pattern == "detail":
        checker = 181 if ((x // 2) ^ (y // 2)) & 1 else 23
        return (checker + plane * 47 + x * 29 + y * 37 + (x * y * 11)) & 255
    raise ValueError(f"unknown pattern: {pattern}")


def generate(fixture: Fixture) -> bytes:
    chroma_width = (fixture.width + 1) // 2
    chroma_height = (fixture.height + 1) // 2
    planes = bytearray()
    for plane, width, height in (
        (0, fixture.width, fixture.height),
        (1, chroma_width, chroma_height),
        (2, chroma_width, chroma_height),
    ):
        planes.extend(
            pixel(fixture.pattern, plane, x, y, width, height)
            for y in range(height)
            for x in range(width)
        )
    return bytes(planes)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--fixture", choices=[fixture.name for fixture in FIXTURES], action="append"
    )
    args = parser.parse_args()

    selected = set(args.fixture or [fixture.name for fixture in FIXTURES])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest = []
    for fixture in FIXTURES:
        if fixture.name not in selected:
            continue
        data = generate(fixture)
        filename = f"{fixture.name}-{fixture.width}x{fixture.height}.i420"
        (args.output_dir / filename).write_bytes(data)
        manifest.append(
            {
                "name": fixture.name,
                "width": fixture.width,
                "height": fixture.height,
                "format": "I420",
                "bit_depth": 8,
                "pattern": fixture.pattern,
                "file": filename,
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )

    (args.output_dir / "sources.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
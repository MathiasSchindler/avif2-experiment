#!/usr/bin/env python3
"""Strict metadata discovery for prepared 8-bit planar encoder inputs."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


MAX_DIMENSION = 16384
PREPARED_NAME = re.compile(
    r"^source-([1-9][0-9]*)x([1-9][0-9]*)-"
    r"(gray8\.i400|yuv420p8\.i420|yuv444p8\.i444)$"
)
FORMAT_RECORDS = {
    "gray8.i400": ("gray8", "i400-8"),
    "yuv420p8.i420": ("yuv420p8", "i420-8"),
    "yuv444p8.i444": ("yuv444p8", "i444-8"),
}


class RawInputError(ValueError):
    pass


@dataclass(frozen=True)
class RawPlanarMetadata:
    path: Path
    pixel_format: str
    width: int
    height: int
    byte_size: int


def expected_size(pixel_format: str, width: int, height: int) -> int:
    luma = width * height
    if pixel_format == "i400-8":
        return luma
    if pixel_format == "i420-8":
        chroma = ((width + 1) // 2) * ((height + 1) // 2)
        return luma + 2 * chroma
    if pixel_format == "i444-8":
        return 3 * luma
    raise RawInputError(f"unsupported raw pixel format: {pixel_format}")


def dimensions_from_prepared_name(path: Path) -> RawPlanarMetadata:
    match = PREPARED_NAME.fullmatch(path.name)
    if match is None:
        raise RawInputError(
            "raw dimensions are not discoverable from this filename"
        )
    width, height = map(int, match.group(1, 2))
    if width > MAX_DIMENSION or height > MAX_DIMENSION:
        raise RawInputError("raw dimensions exceed the supported metadata bound")
    _, pixel_format = FORMAT_RECORDS[match.group(3)]
    return RawPlanarMetadata(
        path=path,
        pixel_format=pixel_format,
        width=width,
        height=height,
        byte_size=expected_size(pixel_format, width, height),
    )


def _positive_dimension(value: Any, field: str) -> int:
    if type(value) is not int or value <= 0 or value > MAX_DIMENSION:
        raise RawInputError(f"manifest has invalid {field}")
    return value


def dimensions_from_prepared_manifest(
    path: Path, manifest_path: Path | None = None
) -> RawPlanarMetadata:
    name_metadata = dimensions_from_prepared_name(path)
    manifest_path = manifest_path or path.parent / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise RawInputError(f"cannot read prepared manifest: {error}") from error
    if not isinstance(manifest, dict):
        raise RawInputError("prepared manifest root is not an object")
    dimensions = manifest.get("dimensions")
    if not isinstance(dimensions, list) or len(dimensions) != 2:
        raise RawInputError("prepared manifest dimensions are malformed")
    width = _positive_dimension(dimensions[0], "width")
    height = _positive_dimension(dimensions[1], "height")
    if (width, height) != (name_metadata.width, name_metadata.height):
        raise RawInputError("prepared filename and manifest dimensions disagree")

    output_key = {
        "i400-8": "gray8",
        "i420-8": "yuv420p8",
        "i444-8": "yuv444p8",
    }[name_metadata.pixel_format]
    outputs = manifest.get("outputs")
    record = outputs.get(output_key) if isinstance(outputs, dict) else None
    if not isinstance(record, dict):
        raise RawInputError("prepared manifest output record is missing")
    if record.get("filename") != path.name:
        raise RawInputError("prepared manifest output filename disagrees")
    byte_size = expected_size(name_metadata.pixel_format, width, height)
    if type(record.get("byte_size")) is not int or record["byte_size"] != byte_size:
        raise RawInputError("prepared manifest output byte size disagrees")
    return RawPlanarMetadata(
        path=path,
        pixel_format=name_metadata.pixel_format,
        width=width,
        height=height,
        byte_size=byte_size,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument("--manifest", type=Path)
    arguments = parser.parse_args()
    metadata = dimensions_from_prepared_manifest(
        arguments.path, arguments.manifest
    )
    print(json.dumps({
        "path": str(metadata.path),
        "pixel_format": metadata.pixel_format,
        "width": metadata.width,
        "height": metadata.height,
        "byte_size": metadata.byte_size,
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

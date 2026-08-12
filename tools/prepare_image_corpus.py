#!/usr/bin/env python3
"""Prepare deterministic RGB masters and 8-bit planar inputs from source records."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
SOURCES_ROOT = IMAGES_ROOT / "sources" / "commons"
OUTPUT_ROOT = IMAGES_ROOT / "prepared"
RECIPES_PATH = IMAGES_ROOT / "recipes" / "preparations.json"
LOCK_PATH = IMAGES_ROOT / "recipes" / "prepared-lock.json"


def require_tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise RuntimeError(f"required tool not found: {name}")
    return path


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        check=True,
        cwd=REPOSITORY_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return completed.stdout


def first_line(command: list[str]) -> str:
    output = run(command).splitlines()
    if not output:
        raise RuntimeError(f"tool produced no version output: {command[0]}")
    return output[0]


def sha256(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as input_file:
        while chunk := input_file.read(1024 * 1024):
            checksum.update(chunk)
    return checksum.hexdigest()


def nearest_even(numerator: int, denominator: int) -> int:
    rounded = (numerator + denominator // 2) // denominator
    lower = rounded if rounded % 2 == 0 else rounded - 1
    upper = lower + 2
    if lower < 2:
        return 2
    lower_error = abs(lower * denominator - numerator)
    upper_error = abs(upper * denominator - numerator)
    return lower if lower_error <= upper_error else upper


def scaled_dimensions(width: int, height: int, long_edge: int) -> tuple[int, int]:
    if max(width, height) < long_edge:
        raise ValueError(
            f"target long edge {long_edge} would upscale {width}x{height}"
        )
    if width >= height:
        return long_edge, nearest_even(height * long_edge, width)
    return nearest_even(width * long_edge, height), long_edge


def expected_plane_size(pixel_format: str, width: int, height: int) -> int:
    if pixel_format == "gray8":
        return width * height
    if pixel_format == "yuv420p8":
        return width * height + 2 * (width // 2) * (height // 2)
    if pixel_format == "yuv444p8":
        return width * height * 3
    raise ValueError(f"unsupported pixel format: {pixel_format}")


def file_record(path: Path) -> dict[str, object]:
    return {
        "filename": path.name,
        "byte_size": path.stat().st_size,
        "sha256": sha256(path),
    }


def prepare_master(
    magick: str,
    source: Path,
    output: Path,
    width: int,
    height: int,
) -> list[str]:
    command = [
        magick,
        str(source),
        "-auto-orient",
        "-colorspace",
        "sRGB",
        "-filter",
        "Lanczos",
        "-resize",
        f"{width}x{height}!",
        "-alpha",
        "off",
        "-depth",
        "8",
        "-strip",
        "-define",
        "png:color-type=2",
        "-define",
        "png:compression-level=9",
        str(output),
    ]
    run(command)
    return command


def prepare_plane(
    ffmpeg: str,
    master: Path,
    output: Path,
    pixel_format: str,
) -> list[str]:
    ffmpeg_format = {
        "gray8": "gray",
        "yuv420p8": "yuv420p",
        "yuv444p8": "yuv444p",
    }[pixel_format]
    command = [
        ffmpeg,
        "-v",
        "error",
        "-i",
        str(master),
        "-frames:v",
        "1",
        "-vf",
        (
            "scale=in_color_matrix=bt709:out_color_matrix=bt709:"
            "in_range=full:out_range=full:flags=accurate_rnd+full_chroma_int,"
            f"format={ffmpeg_format}"
        ),
        "-pix_fmt",
        ffmpeg_format,
        "-f",
        "rawvideo",
        "-y",
        str(output),
    ]
    run(command)
    return command


def relative_command(command: list[str], scratch: Path) -> list[str]:
    normalized = []
    for argument in command:
        value = argument.replace(str(REPOSITORY_ROOT) + "/", "")
        value = value.replace(str(scratch) + "/", "")
        if value == argument and Path(value).is_absolute():
            value = Path(value).name
        normalized.append(value)
    return normalized


def prepare_source(
    source_path: Path,
    profile: dict[str, object],
    magick: str,
    ffmpeg: str,
    magick_version: str,
    ffmpeg_version: str,
) -> Path:
    source_record = json.loads(source_path.read_text(encoding="utf-8"))
    source = source_path.parent / source_record["original"]["filename"]
    if not source.is_file():
        raise RuntimeError(f"downloaded source is missing: {source}")
    if sha256(source) != source_record["original"]["sha256"]:
        raise RuntimeError(f"source SHA-256 mismatch: {source}")

    source_width = source_record["original"]["width"]
    source_height = source_record["original"]["height"]
    long_edge = profile["target_long_edge"]
    width, height = scaled_dimensions(source_width, source_height, long_edge)
    preparation_id = f"le{long_edge}"
    destination = OUTPUT_ROOT / preparation_id / source_record["id"]

    with tempfile.TemporaryDirectory(
        prefix=f"prepare-{source_record['id']}-{preparation_id}-"
    ) as temporary_directory:
        scratch = Path(temporary_directory)
        master = scratch / "master.png"
        commands = {
            "master": relative_command(
                prepare_master(magick, source, master, width, height), scratch
            )
        }
        outputs: dict[str, dict[str, object]] = {"master": file_record(master)}
        suffixes = {
            "gray8": "i400",
            "yuv420p8": "i420",
            "yuv444p8": "i444",
        }
        for pixel_format in profile["planes"]:
            plane = scratch / (
                f"source-{width}x{height}-{pixel_format}.{suffixes[pixel_format]}"
            )
            commands[pixel_format] = relative_command(
                prepare_plane(ffmpeg, master, plane, pixel_format), scratch
            )
            expected_size = expected_plane_size(pixel_format, width, height)
            if plane.stat().st_size != expected_size:
                raise RuntimeError(
                    f"{pixel_format} size mismatch for {source_record['id']}: "
                    f"expected {expected_size}, got {plane.stat().st_size}"
                )
            outputs[pixel_format] = file_record(plane)

        linear_scale = long_edge / max(source_width, source_height)
        manifest = {
            "schema_version": 1,
            "preparation_id": preparation_id,
            "profile_id": profile["id"],
            "source_id": source_record["id"],
            "source_sha256": source_record["original"]["sha256"],
            "source_dimensions": [source_width, source_height],
            "dimensions": [width, height],
            "linear_scale": linear_scale,
            "pixel_area_scale": (width * height) / (source_width * source_height),
            "upscaled": False,
            "source_precision": "8-bit JPEG",
            "master": profile["master"],
            "range": profile["range"],
            "matrix": profile["matrix"],
            "chroma_siting": profile["chroma_siting"],
            "tools": {
                "imagemagick": magick_version,
                "ffmpeg": ffmpeg_version,
            },
            "commands": commands,
            "outputs": outputs,
        }
        (scratch / "manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(scratch, destination)
    return destination


def write_lock(destinations: list[Path]) -> None:
    records = []
    for destination in sorted(destinations):
        manifest_path = destination / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        records.append(
            {
                "manifest": str(manifest_path.relative_to(IMAGES_ROOT)),
                "manifest_sha256": sha256(manifest_path),
                "preparation_id": manifest["preparation_id"],
                "profile_id": manifest["profile_id"],
                "source_id": manifest["source_id"],
                "source_sha256": manifest["source_sha256"],
                "source_dimensions": manifest["source_dimensions"],
                "dimensions": manifest["dimensions"],
                "linear_scale": manifest["linear_scale"],
                "pixel_area_scale": manifest["pixel_area_scale"],
                "outputs": manifest["outputs"],
            }
        )
    lock = {
        "schema_version": 1,
        "generated_by": "tools/prepare_image_corpus.py",
        "preparations": records,
    }
    LOCK_PATH.write_text(
        json.dumps(lock, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", action="append", help="prepare only this source ID")
    parser.add_argument(
        "--long-edge",
        action="append",
        type=int,
        help="prepare only this configured long edge",
    )
    arguments = parser.parse_args()

    recipes = json.loads(RECIPES_PATH.read_text(encoding="utf-8"))
    profiles = recipes["profiles"]
    if arguments.long_edge:
        requested_edges = set(arguments.long_edge)
        profiles = [
            profile
            for profile in profiles
            if profile["target_long_edge"] in requested_edges
        ]
        found_edges = {profile["target_long_edge"] for profile in profiles}
        if found_edges != requested_edges:
            raise SystemExit(
                f"unconfigured long edges: {sorted(requested_edges - found_edges)}"
            )

    source_paths = sorted(SOURCES_ROOT.glob("*/source.json"))
    if arguments.source:
        requested_sources = set(arguments.source)
        source_paths = [
            path for path in source_paths if path.parent.name in requested_sources
        ]
        found_sources = {path.parent.name for path in source_paths}
        if found_sources != requested_sources:
            raise SystemExit(
                f"unknown source IDs: {sorted(requested_sources - found_sources)}"
            )

    magick = require_tool("magick")
    ffmpeg = require_tool("ffmpeg")
    magick_version = first_line([magick, "-version"])
    ffmpeg_version = first_line([ffmpeg, "-version"])
    generated = []
    for profile in profiles:
        for source_path in source_paths:
            destination = prepare_source(
                source_path,
                profile,
                magick,
                ffmpeg,
                magick_version,
                ffmpeg_version,
            )
            generated.append(destination)
            print(destination.relative_to(REPOSITORY_ROOT), flush=True)
    if arguments.source is None and arguments.long_edge is None:
        write_lock(generated)
        print(LOCK_PATH.relative_to(REPOSITORY_ROOT), flush=True)
    print(f"generated {len(generated)} preparations")


if __name__ == "__main__":
    main()
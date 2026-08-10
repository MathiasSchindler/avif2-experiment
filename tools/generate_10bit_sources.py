#!/usr/bin/env python3
"""Generate deterministic 16-bit RGB and 10-bit YUV sources from Pixel DNGs."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from array import array
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_DIR = ROOT / "images" / "artifacts-raw"
DEFAULT_OUTPUT = ROOT / "images" / "artifacts" / "10bit"
SOURCES = (
    {
        "name": "main-portrait",
        "file": "PXL_20260810_162619555.dng",
        "sha256": "9e0619907b60b93a1d0b7a1825cb8c6afc1612a7ed52f8320778bf7d271ea11c",
        "camera_module": "24 mm-equivalent main camera",
        "raw_dimensions": [8160, 6144],
        "crop": [8, 8, 8144, 6128],
        "crop_geometry": "8144x6128+8+8",
        "orientation": "RightTop (rotate 90 degrees clockwise)",
        "white_level": 15343,
    },
    {
        "name": "ultrawide-portrait",
        "file": "PXL_20260810_164538160.RAW-02.ORIGINAL.dng",
        "sha256": "125dbeca8b5a4ffb2e4ff83bd4548703f79d6ff0f607f0eda042ea55b2ab0e14",
        "camera_module": "12 mm-equivalent ultrawide camera",
        "raw_dimensions": [3730, 2808],
        "crop": [8, 8, 3714, 2792],
        "crop_geometry": "3714x2792+8+8",
        "orientation": "RightTop (rotate 90 degrees clockwise)",
        "white_level": 15344,
    },
    {
        "name": "ultrawide-landscape",
        "file": "PXL_20260810_164847950.RAW-02.ORIGINAL.dng",
        "sha256": "c9c3acbb2f879c9d0fb74a1bd6049d50876582c0a9857c1c6c21f71faeca22dd",
        "camera_module": "12 mm-equivalent ultrawide camera",
        "raw_dimensions": [3730, 2808],
        "crop": [8, 8, 3714, 2792],
        "crop_geometry": "3714x2792+8+8",
        "orientation": "TopLeft (no rotation)",
        "white_level": 15344,
    },
)
WIDTHS = (600, 1200)
SCALE_FILTER = (
    "scale=in_color_matrix=bt709:out_color_matrix=bt709:"
    "in_range=full:out_range=limited:"
    "in_primaries=bt709:out_primaries=bt709:"
    "in_transfer=srgb:out_transfer=srgb:out_chroma_loc=center:"
    "flags=lanczos+accurate_rnd+full_chroma_int"
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_tool(name: str) -> str:
    resolved = shutil.which(name)
    if resolved is None:
        raise RuntimeError(f"missing required tool: {name}")
    return resolved


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stderr.strip()}"
        )
    return completed.stdout


def first_line(command: list[str]) -> str:
    output = run(command)
    return output.splitlines()[0] if output.splitlines() else "unknown"


def identify_master(magick: str, path: Path) -> tuple[int, int, int, str]:
    fields = run(
        [
            magick,
            "identify",
            "-format",
            "%w %h %z %[colorspace]",
            str(path),
        ]
    ).split()
    if len(fields) != 4:
        raise RuntimeError(f"unexpected ImageMagick identification: {fields}")
    return int(fields[0]), int(fields[1]), int(fields[2]), fields[3]


def plane_dimensions(pixel_format: str, width: int, height: int) -> list[tuple[int, int]]:
    if pixel_format == "yuv444p10le":
        return [(width, height)] * 3
    if pixel_format == "yuv420p10le":
        return [
            (width, height),
            ((width + 1) // 2, (height + 1) // 2),
            ((width + 1) // 2, (height + 1) // 2),
        ]
    raise RuntimeError(f"unsupported pixel format: {pixel_format}")


def analyze_yuv(path: Path, pixel_format: str, width: int, height: int) -> dict[str, object]:
    dimensions = plane_dimensions(pixel_format, width, height)
    expected_samples = sum(plane_width * plane_height for plane_width, plane_height in dimensions)
    expected_size = expected_samples * 2
    data = path.read_bytes()
    if len(data) != expected_size:
        raise RuntimeError(
            f"{path.name}: expected {expected_size} bytes, got {len(data)}"
        )
    samples = array("H")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    planes = []
    offset = 0
    for name, (plane_width, plane_height) in zip("YUV", dimensions):
        count = plane_width * plane_height
        values = memoryview(samples)[offset:offset + count]
        distinct = set(values)
        minimum = min(distinct)
        maximum = max(distinct)
        low_two_bits = sorted({value & 3 for value in distinct})
        if maximum > 1023:
            raise RuntimeError(f"{path.name}: {name} sample exceeds 10 bits")
        if low_two_bits != [0, 1, 2, 3]:
            raise RuntimeError(
                f"{path.name}: {name} does not exercise all low two bits"
            )
        planes.append(
            {
                "name": name,
                "width": plane_width,
                "height": plane_height,
                "offset_bytes": offset * 2,
                "size_bytes": count * 2,
                "minimum": minimum,
                "maximum": maximum,
                "distinct_values": len(distinct),
                "low_two_bits": low_two_bits,
            }
        )
        offset += count
    return {"size_bytes": expected_size, "planes": planes}


def generate_master(
    magick: str,
    source: Path,
    crop_geometry: str,
    width: int,
    output: Path,
) -> None:
    run(
        [
            magick,
            str(source),
            "-crop",
            crop_geometry,
            "+repage",
            "-auto-orient",
            "-filter",
            "Lanczos",
            "-resize",
            f"{width}x",
            "-depth",
            "16",
            "-strip",
            "-define",
            "png:color-type=2",
            "-define",
            "png:bit-depth=16",
            "-define",
            "png:exclude-chunk=date,time",
            str(output),
        ]
    )


def generate_yuv(ffmpeg: str, master: Path, pixel_format: str, output: Path) -> None:
    run(
        [
            ffmpeg,
            "-v",
            "error",
            "-y",
            "-i",
            str(master),
            "-vf",
            SCALE_FILTER,
            "-frames:v",
            "1",
            "-pix_fmt",
            pixel_format,
            "-f",
            "rawvideo",
            str(output),
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()

    source_directory = arguments.source_dir.resolve()
    output = arguments.output_dir.resolve()
    sources = []
    for source_specification in SOURCES:
        source = source_directory / str(source_specification["file"])
        if not source.is_file():
            raise RuntimeError(f"missing source DNG: {source}")
        source_sha256 = sha256_file(source)
        if source_sha256 != source_specification["sha256"]:
            raise RuntimeError(
                f"source SHA-256 mismatch for {source.name}: expected "
                f"{source_specification['sha256']}, got {source_sha256}"
            )
        sources.append((source_specification, source, source_sha256))

    magick = require_tool("magick")
    ffmpeg = require_tool("ffmpeg")
    output.mkdir(parents=True, exist_ok=True)
    build_directory = ROOT / "build"
    build_directory.mkdir(exist_ok=True)
    artifacts = []
    with tempfile.TemporaryDirectory(prefix="10bit-sources-", dir=build_directory) as temporary_text:
        temporary = Path(temporary_text)
        source_manifest = []
        for source_specification, source, source_sha256 in sources:
            source_name = str(source_specification["name"])
            source_manifest.append(
                {
                    "name": source_name,
                    "path": f"images/artifacts-raw/{source.name}",
                    "sha256": source_sha256,
                    "owner": "project author",
                    "camera": "Google Pixel 8 Pro",
                    "camera_module": source_specification["camera_module"],
                    "raw_dimensions": source_specification["raw_dimensions"],
                    "default_crop": source_specification["crop"],
                    "orientation": source_specification["orientation"],
                    "cfa": "RGGB",
                    "bits_per_sample": 16,
                    "white_level": source_specification["white_level"],
                }
            )
            for requested_width in WIDTHS:
                temporary_master = temporary / (
                    f"pixel8-{source_name}-{requested_width}-rgb16.png"
                )
                generate_master(
                    magick,
                    source,
                    str(source_specification["crop_geometry"]),
                    requested_width,
                    temporary_master,
                )
                width, height, depth, colorspace = identify_master(
                    magick, temporary_master
                )
                if (
                    width != requested_width
                    or depth != 16
                    or colorspace != "sRGB"
                ):
                    raise RuntimeError(
                        f"unexpected master contract: {width}x{height} "
                        f"depth={depth} colorspace={colorspace}"
                    )
                stem = f"pixel8-{source_name}-{width}x{height}"
                master_name = f"{stem}-rgb16.png"
                master_path = temporary / master_name
                temporary_master.rename(master_path)
                artifacts.append(
                    {
                        "file": master_name,
                        "source": source_name,
                        "role": "developed RGB master",
                        "format": "PNG RGB 16-bit big-endian samples",
                        "width": width,
                        "height": height,
                        "size_bytes": master_path.stat().st_size,
                        "sha256": sha256_file(master_path),
                    }
                )
                for pixel_format in ("yuv444p10le", "yuv420p10le"):
                    yuv_name = f"{stem}-{pixel_format}.yuv"
                    yuv_path = temporary / yuv_name
                    generate_yuv(ffmpeg, master_path, pixel_format, yuv_path)
                    analysis = analyze_yuv(yuv_path, pixel_format, width, height)
                    artifacts.append(
                        {
                            "file": yuv_name,
                            "source": source_name,
                            "role": "10-bit AV2 encoder source",
                            "format": pixel_format,
                            "width": width,
                            "height": height,
                            "size_bytes": analysis["size_bytes"],
                            "sha256": sha256_file(yuv_path),
                            "planes": analysis["planes"],
                        }
                    )
        manifest = {
            "schema": 2,
            "sources": source_manifest,
            "tools": {
                "imagemagick": first_line([magick, "-version"]),
                "ffmpeg": first_line([ffmpeg, "-version"]),
            },
            "development": {
                "crop": "each DNG default crop before orientation",
                "orientation": "apply DNG orientation",
                "resize": "ImageMagick Lanczos at 16-bit precision",
                "master": "stripped 16-bit RGB PNG in sRGB",
            },
            "yuv_contract": {
                "storage": "headerless planar unsigned 16-bit little-endian words",
                "significant_bits": 10,
                "sample_range": [0, 1023],
                "color_primaries": "BT.709 (CICP 1)",
                "transfer_characteristics": "IEC 61966-2-1 sRGB (CICP 13)",
                "matrix_coefficients": "BT.709 (CICP 1)",
                "range": "limited",
                "chroma_location_420": "center",
                "conversion": SCALE_FILTER,
                "plane_order": ["Y", "U", "V"],
            },
            "artifacts": artifacts,
        }
        manifest_path = temporary / "manifest.json"
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        for old_artifact in output.glob("pixel8-*"):
            if old_artifact.is_file():
                old_artifact.unlink()
        old_manifest = output / "manifest.json"
        if old_manifest.is_file():
            old_manifest.unlink()
        for generated in temporary.iterdir():
            shutil.copyfile(generated, output / generated.name)

    print(f"generated {len(artifacts)} artifacts in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
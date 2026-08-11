#!/usr/bin/env python3
"""Encode and decode the native 10-bit corpus with pinned AVM tools."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
import subprocess
import sys
from array import array
from pathlib import Path

from obu_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_DIR = ROOT / "images" / "artifacts" / "10bit"
DEFAULT_OUTPUT_DIR = DEFAULT_SOURCE_DIR / "avm-reference"
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm" / "avmenc"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_QPS = (96, 128, 160)
COMMON_DISABLED_CODING_TOOLS = (
    "film_grain",
    "lcr",
    "operating_point_sets",
    "atlas",
    "restoration",
)
FORMAT_DISABLED_CODING_TOOLS = {
    "yuv420p10le": (),
    "yuv444p10le": ("intrabc",),
}


def coding_tool_profile() -> dict[str, object]:
    return {
        "enabled": [],
        "disabled_for_all_formats": list(COMMON_DISABLED_CODING_TOOLS),
        "disabled_by_format": {
            pixel_format: list(tools)
            for pixel_format, tools in FORMAT_DISABLED_CODING_TOOLS.items()
        },
        "unlisted_tools": "pinned AVM defaults",
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_tool(value: Path) -> Path:
    if value.is_file():
        return value.resolve()
    resolved = shutil.which(str(value))
    if resolved is None:
        raise RuntimeError(f"missing required tool: {value}")
    return Path(resolved).resolve()


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
    return completed.stdout + completed.stderr


def tool_banner(tool: Path, marker: str) -> str:
    for line in run([str(tool), "--help"]).splitlines():
        if marker in line:
            return line.strip()
    raise RuntimeError(f"{tool} help does not contain {marker!r}")


def relative_command(command: list[str]) -> list[str]:
    result = []
    for argument in command:
        try:
            result.append(str(Path(argument).resolve().relative_to(ROOT)))
        except (OSError, ValueError):
            result.append(argument)
    return result


def plane_dimensions(
    pixel_format: str, width: int, height: int
) -> list[tuple[int, int]]:
    if pixel_format == "yuv444p10le":
        return [(width, height)] * 3
    if pixel_format == "yuv420p10le":
        return [
            (width, height),
            ((width + 1) // 2, (height + 1) // 2),
            ((width + 1) // 2, (height + 1) // 2),
        ]
    raise RuntimeError(f"unsupported source format: {pixel_format}")


def read_samples(path: Path, expected_samples: int) -> array[int]:
    data = path.read_bytes()
    expected_bytes = expected_samples * 2
    if len(data) != expected_bytes:
        raise RuntimeError(
            f"{path.name}: expected {expected_bytes} bytes, got {len(data)}"
        )
    samples = array("H")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    if samples and max(samples) > 1023:
        raise RuntimeError(f"{path.name}: sample exceeds 10 bits")
    return samples


def compare_frames(
    source: Path,
    decoded: Path,
    pixel_format: str,
    width: int,
    height: int,
) -> list[dict[str, int | float | str]]:
    dimensions = plane_dimensions(pixel_format, width, height)
    expected_samples = sum(
        plane_width * plane_height for plane_width, plane_height in dimensions
    )
    source_samples = read_samples(source, expected_samples)
    decoded_samples = read_samples(decoded, expected_samples)
    planes = []
    offset = 0
    for name, (plane_width, plane_height) in zip("YUV", dimensions):
        count = plane_width * plane_height
        source_plane = memoryview(source_samples)[offset : offset + count]
        decoded_plane = memoryview(decoded_samples)[offset : offset + count]
        absolute_sum = 0
        squared_sum = 0
        maximum_error = 0
        for source_value, decoded_value in zip(source_plane, decoded_plane):
            difference = int(decoded_value) - int(source_value)
            absolute = abs(difference)
            absolute_sum += absolute
            squared_sum += difference * difference
            maximum_error = max(maximum_error, absolute)
        mean_squared_error = squared_sum / count
        psnr = (
            math.inf
            if mean_squared_error == 0
            else 10.0 * math.log10((1023 * 1023) / mean_squared_error)
        )
        planes.append(
            {
                "name": name,
                "width": plane_width,
                "height": plane_height,
                "source_minimum": min(source_plane),
                "source_maximum": max(source_plane),
                "decoded_minimum": min(decoded_plane),
                "decoded_maximum": max(decoded_plane),
                "maximum_absolute_error": maximum_error,
                "mean_absolute_error": round(absolute_sum / count, 6),
                "mean_squared_error": round(mean_squared_error, 6),
                "psnr_db": "infinity" if math.isinf(psnr) else round(psnr, 6),
            }
        )
        offset += count
    return planes


def encode_command(
    encoder: Path,
    source: Path,
    output: Path,
    pixel_format: str,
    width: int,
    height: int,
    qp: int,
) -> list[str]:
    is_444 = pixel_format == "yuv444p10le"
    if pixel_format not in FORMAT_DISABLED_CODING_TOOLS:
        raise RuntimeError(f"unsupported source format: {pixel_format}")
    command = [
        str(encoder),
        "--debug",
        "--codec=av2",
        "--obu",
        "--passes=1",
        "--limit=1",
        "--good",
        "--cpu-used=9",
        "--threads=1",
        f"--profile={1 if is_444 else 0}",
        f"--width={width}",
        f"--height={height}",
        "--i444" if is_444 else "--i420",
        "--bit-depth=10",
        "--input-bit-depth=10",
        f"--input-chroma-subsampling-x={0 if is_444 else 1}",
        f"--input-chroma-subsampling-y={0 if is_444 else 1}",
    ]
    if not is_444:
        command.append("--chroma-sample-position=unspecified")
    command.extend(
        [
            "--force-video-mode=0",
            "--film-grain-test=0",
        ]
    )
    tool_flags = {
        "lcr": "--enable-lcr=0",
        "operating_point_sets": "--enable-operating-point-sets=0",
        "atlas": "--enable-atlas=0",
        "restoration": "--enable-restoration=0",
        "intrabc": "--enable-intrabc=0",
    }
    for tool in COMMON_DISABLED_CODING_TOOLS:
        if tool != "film_grain":
            command.append(tool_flags[tool])
    for tool in FORMAT_DISABLED_CODING_TOOLS[pixel_format]:
        command.append(tool_flags[tool])
    command.extend(
        [
            "--end-usage=q",
            f"--qp={qp}",
            "--test-decode=fatal",
            "--quiet",
            "-o",
            str(output),
            str(source),
        ]
    )
    return command


def decode_command(decoder: Path, source: Path, output: Path) -> list[str]:
    return [
        str(decoder),
        "--codec=av2",
        "--rawvideo",
        "--output-bit-depth=10",
        "--limit=1",
        "--threads=1",
        "-o",
        str(output),
        str(source),
    ]


def load_sources(source_directory: Path) -> tuple[str, list[dict[str, object]]]:
    manifest_path = source_directory / "manifest.json"
    manifest_hash = sha256_file(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema") != 2:
        raise RuntimeError("10-bit source manifest must use schema 2")
    sources = [
        entry
        for entry in manifest.get("artifacts", [])
        if entry.get("role") == "10-bit AV2 encoder source"
    ]
    if not sources:
        raise RuntimeError("10-bit source manifest contains no encoder inputs")
    return manifest_hash, sources


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    parser.add_argument("--qp", type=int, action="append", dest="qps")
    parser.add_argument(
        "--only",
        action="append",
        default=[],
        help="generate only source filenames containing this text",
    )
    parser.add_argument("--resume", action="store_true")
    arguments = parser.parse_args()

    qps = sorted(set(arguments.qps or DEFAULT_QPS))
    if any(qp < 90 or qp > 160 for qp in qps):
        raise RuntimeError("QP values must be in the requested range 90 through 160")
    source_directory = arguments.source_dir.resolve()
    output_directory = arguments.output_dir.resolve()
    encoder = require_tool(arguments.encoder)
    decoder = require_tool(arguments.decoder)
    source_manifest_hash, sources = load_sources(source_directory)
    if arguments.only:
        sources = [
            entry
            for entry in sources
            if any(text in str(entry["file"]) for text in arguments.only)
        ]
        if not sources:
            raise RuntimeError("--only filters selected no source files")

    output_directory.mkdir(parents=True, exist_ok=True)
    if not arguments.resume and not arguments.only:
        for pattern in ("pixel8-*.obu", "pixel8-*-decoded.yuv", "manifest.json"):
            for stale in output_directory.glob(pattern):
                stale.unlink()
    records = []
    total = len(sources) * len(qps)
    completed = 0
    for source_record in sources:
        source = source_directory / str(source_record["file"])
        if sha256_file(source) != source_record["sha256"]:
            raise RuntimeError(f"source hash mismatch: {source.name}")
        pixel_format = str(source_record["format"])
        width = int(source_record["width"])
        height = int(source_record["height"])
        expected_size = sum(
            plane_width * plane_height
            for plane_width, plane_height in plane_dimensions(
                pixel_format, width, height
            )
        ) * 2
        if source.stat().st_size != expected_size:
            raise RuntimeError(f"source size mismatch: {source.name}")
        for qp in qps:
            stem = f"{source.stem}-qp{qp:03d}"
            obu = output_directory / f"{stem}.obu"
            decoded = output_directory / f"{stem}-decoded.yuv"
            encode = encode_command(
                encoder, source, obu, pixel_format, width, height, qp
            )
            if not (arguments.resume and obu.is_file() and obu.stat().st_size > 0):
                run(encode)
            decode = decode_command(decoder, obu, decoded)
            if not (
                arguments.resume
                and decoded.is_file()
                and decoded.stat().st_size == expected_size
            ):
                run(decode)
            if decoded.stat().st_size != expected_size:
                raise RuntimeError(
                    f"decoded size mismatch for {obu.name}: expected "
                    f"{expected_size}, got {decoded.stat().st_size}"
                )
            obu_entries = parse_obus(obu.read_bytes())
            obu_types = [entry.name for entry in obu_entries]
            expected_types = [
                "temporal_delimiter",
                "sequence_header",
                "closed_loop_key",
            ]
            if obu_types != expected_types:
                raise RuntimeError(f"unexpected OBU sequence in {obu.name}: {obu_types}")
            records.append(
                {
                    "name": stem,
                    "source": source.name,
                    "source_sha256": source_record["sha256"],
                    "pixel_format": pixel_format,
                    "width": width,
                    "height": height,
                    "bit_depth": 10,
                    "qp": qp,
                    "obu": {
                        "file": obu.name,
                        "size_bytes": obu.stat().st_size,
                        "sha256": sha256_file(obu),
                        "units": [entry.ledger_entry() for entry in obu_entries],
                    },
                    "decoded": {
                        "file": decoded.name,
                        "size_bytes": decoded.stat().st_size,
                        "sha256": sha256_file(decoded),
                        "planes": compare_frames(
                            source, decoded, pixel_format, width, height
                        ),
                    },
                    "encode_command": relative_command(encode),
                    "decode_command": relative_command(decode),
                }
            )
            completed += 1
            print(f"[{completed}/{total}] {stem}", flush=True)

    revision = run(["git", "-C", str(ROOT / "vendor" / "avm"), "rev-parse", "HEAD"])
    manifest = {
        "schema": 2,
        "description": "Pinned AVM 10-bit still-image encode/decode references",
        "source_manifest": {
            "file": str((source_directory / "manifest.json").relative_to(ROOT)),
            "sha256": source_manifest_hash,
        },
        "qps": qps,
        "coding_tool_profile": coding_tool_profile(),
        "output_contract": {
            "storage": "headerless planar unsigned 16-bit little-endian words",
            "significant_bits": 10,
            "sample_range": [0, 1023],
            "plane_order": ["Y", "U", "V"],
        },
        "tools": {
            "avm_revision": revision.strip(),
            "avmenc": tool_banner(encoder, "AOMedia Project AV2 Encoder"),
            "avmenc_sha256": sha256_file(encoder),
            "avmdec": tool_banner(decoder, "AOMedia Project AV2 Decoder"),
            "avmdec_sha256": sha256_file(decoder),
        },
        "artifacts": sorted(records, key=lambda record: str(record["name"])),
    }
    manifest_path = output_directory / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"generated {len(records)} AVM reference pairs in {output_directory}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
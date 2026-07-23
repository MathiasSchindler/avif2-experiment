#!/usr/bin/env python3
"""Generate varied JPEG-derived AV2 reference artifact chains."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

from m0_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "images"
OUTPUT_DIR = IMAGE_DIR / "artifacts"
AVM_DIR = ROOT / "vendor" / "avm"
AVM_BUILD_DIR = ROOT / "build" / "reference" / "avm"
AVM_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
AVM_ENCODER_SHA256 = (
    "426a41472a45cbeda3376ba162731d39bc0e582c729098970a8167ccf8b022dd"
)


@dataclass(frozen=True)
class Variant:
    image: str
    long_edge: int
    qp: int


VARIANTS = (
    Variant("chimp", 111, 64),
    Variant("chimp", 287, 192),
    Variant("chinnathurai", 128, 112),
    Variant("chinnathurai", 333, 224),
    Variant("cuckoo", 157, 160),
    Variant("cuckoo", 431, 80),
    Variant("emerald", 192, 208),
    Variant("emerald", 480, 128),
    Variant("flycatcher", 223, 176),
    Variant("flycatcher", 397, 48),
    Variant("laarim", 256, 96),
    Variant("laarim", 448, 192),
    Variant("louis", 301, 144),
    Variant("louis", 463, 224),
    Variant("pastell", 320, 64),
    Variant("pastell", 512, 160),
    Variant("schwaebisch", 353, 112),
    Variant("schwaebisch", 417, 208),
    Variant("silene", 384, 80),
    Variant("silene", 479, 176),
)


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return completed.stdout


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tool_line(command: list[str], marker: str) -> str:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    for line in completed.stdout.splitlines():
        if marker in line:
            return line.strip()
    raise RuntimeError(f"tool output lacks marker {marker!r}: {command[0]}")


def dimensions(path: Path) -> tuple[int, int]:
    output = run(
        [
            "ffprobe",
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=width,height",
            "-of",
            "json",
            str(path),
        ]
    )
    streams = json.loads(output).get("streams", [])
    if len(streams) != 1:
        raise RuntimeError(f"expected one image stream in {path}")
    return int(streams[0]["width"]), int(streams[0]["height"])


def scaled_dimensions(width: int, height: int, long_edge: int) -> tuple[int, int]:
    if width >= height:
        scaled_width = long_edge
        scaled_height = max(1, (height * long_edge + width // 2) // width)
    else:
        scaled_height = long_edge
        scaled_width = max(1, (width * long_edge + height // 2) // height)
    return scaled_width, scaled_height


def expected_i420_size(width: int, height: int) -> int:
    chroma_width = (width + 1) // 2
    chroma_height = (height + 1) // 2
    return width * height + 2 * chroma_width * chroma_height


def require_i420_size(path: Path, width: int, height: int) -> None:
    expected = expected_i420_size(width, height)
    actual = path.stat().st_size
    if actual != expected:
        raise RuntimeError(
            f"I420 size mismatch for {path}: expected {expected}, got {actual}"
        )


def verify_reference_tools(encoder: Path, decoder: Path) -> dict[str, object]:
    if not encoder.is_file() or not decoder.is_file():
        raise RuntimeError("run scripts/setup-avm.sh before generating artifacts")
    revision = run(["git", "-C", str(AVM_DIR), "rev-parse", "HEAD"]).strip()
    if revision != AVM_REVISION:
        raise RuntimeError(
            f"vendor/avm is at {revision}, expected {AVM_REVISION}"
        )
    source_changes = run(
        ["git", "-C", str(AVM_DIR), "status", "--short"]
    ).splitlines()
    encoder_hash = sha256(encoder)
    if encoder_hash != AVM_ENCODER_SHA256:
        raise RuntimeError(
            f"avmenc hash is {encoder_hash}, expected {AVM_ENCODER_SHA256}"
        )
    return {
        "revision": revision,
        "source_checkout_clean": not source_changes,
        "source_checkout_changes": source_changes,
        "encoder": tool_line(
            [str(encoder), "--help"], "AOMedia Project AV2 Encoder"
        ),
        "decoder": tool_line(
            [str(decoder), "--help"], "AOMedia Project AV2 Decoder"
        ),
        "encoder_sha256": encoder_hash,
        "decoder_sha256": sha256(decoder),
    }


def encode(
    encoder: Path,
    source: Path,
    output: Path,
    width: int,
    height: int,
    qp: int,
) -> list[str]:
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
        "--profile=0",
        f"--width={width}",
        f"--height={height}",
        "--i420",
        "--bit-depth=8",
        "--input-bit-depth=8",
        "--input-chroma-subsampling-x=1",
        "--input-chroma-subsampling-y=1",
        "--chroma-sample-position=unspecified",
        "--force-video-mode=0",
        "--film-grain-test=0",
        "--enable-lcr=0",
        "--enable-operating-point-sets=0",
        "--enable-atlas=0",
        "--end-usage=q",
        f"--qp={qp}",
        "--test-decode=fatal",
        "--quiet",
        "-o",
        str(output),
        str(source),
    ]
    run(command)
    return command


def decode(
    decoder: Path,
    obu: Path,
    output: Path,
    width: int,
    height: int,
) -> list[str]:
    pattern = output.with_name(output.stem + "-%wx%h-%1.i420")
    for stale in output.parent.glob(output.stem + "-*.i420"):
        stale.unlink()
    command = [
        str(decoder),
        "--codec=av2",
        "--i420",
        "--limit=1",
        "-o",
        str(pattern),
        str(obu),
    ]
    run(command)
    matches = list(output.parent.glob(output.stem + "-*.i420"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one decoded I420 file for {obu}")
    match = re.fullmatch(
        re.escape(output.stem) + r"-(\d+)x(\d+)-1\.i420",
        matches[0].name,
    )
    if match is None:
        raise RuntimeError(f"unexpected decoded filename: {matches[0].name}")
    decoded_dimensions = int(match.group(1)), int(match.group(2))
    if decoded_dimensions != (width, height):
        raise RuntimeError(
            f"decoded dimensions {decoded_dimensions} do not match "
            f"{width}x{height}"
        )
    matches[0].replace(output)
    return command


def file_record(path: Path) -> dict[str, object]:
    return {
        "path": str(path.relative_to(ROOT)),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def generate_variant(
    variant: Variant,
    encoder: Path,
    decoder: Path,
) -> dict[str, object]:
    jpeg = IMAGE_DIR / f"{variant.image}.jpg"
    if not jpeg.is_file():
        raise RuntimeError(f"missing source image: {jpeg}")
    original_width, original_height = dimensions(jpeg)
    width, height = scaled_dimensions(
        original_width, original_height, variant.long_edge
    )
    stem = f"{variant.image}-{width}x{height}-qp{variant.qp:03d}"
    source_i420 = OUTPUT_DIR / f"{stem}-source.i420"
    source_bmp = OUTPUT_DIR / f"{stem}-source.bmp"
    obu = OUTPUT_DIR / f"{stem}.obu"
    decoded_i420 = OUTPUT_DIR / f"{stem}-decoded.i420"
    decoded_bmp = OUTPUT_DIR / f"{stem}-decoded.bmp"

    source_command = [
        "ffmpeg",
        "-nostdin",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-i",
        str(jpeg),
        "-vf",
        f"scale={width}:{height}:flags=lanczos,format=yuv420p",
        "-frames:v",
        "1",
        "-f",
        "rawvideo",
        str(source_i420),
    ]
    run(source_command)
    require_i420_size(source_i420, width, height)

    source_bmp_command = [
        "ffmpeg",
        "-nostdin",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "rawvideo",
        "-pixel_format",
        "yuv420p",
        "-video_size",
        f"{width}x{height}",
        "-i",
        str(source_i420),
        "-frames:v",
        "1",
        "-pix_fmt",
        "bgr24",
        str(source_bmp),
    ]
    run(source_bmp_command)
    if dimensions(source_bmp) != (width, height):
        raise RuntimeError(f"source BMP dimensions do not match for {stem}")

    encode_command = encode(
        encoder, source_i420, obu, width, height, variant.qp
    )
    obus = parse_obus(obu.read_bytes())
    obu_names = [entry.name for entry in obus]
    if obu_names != [
        "temporal_delimiter",
        "sequence_header",
        "closed_loop_key",
    ]:
        raise RuntimeError(f"unexpected OBU ledger for {obu}: {obu_names}")
    decode_command = decode(
        decoder, obu, decoded_i420, width, height
    )
    require_i420_size(decoded_i420, width, height)

    decoded_bmp_command = [
        "ffmpeg",
        "-nostdin",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-f",
        "rawvideo",
        "-pixel_format",
        "yuv420p",
        "-video_size",
        f"{width}x{height}",
        "-i",
        str(decoded_i420),
        "-frames:v",
        "1",
        "-pix_fmt",
        "bgr24",
        str(decoded_bmp),
    ]
    run(decoded_bmp_command)
    if dimensions(decoded_bmp) != (width, height):
        raise RuntimeError(f"decoded BMP dimensions do not match for {stem}")

    return {
        "name": variant.image,
        "source_jpeg": file_record(jpeg),
        "source_dimensions": {
            "width": original_width,
            "height": original_height,
        },
        "variant": {
            "long_edge": variant.long_edge,
            "width": width,
            "height": height,
            "qp": variant.qp,
            "cpu_used": 9,
            "profile": 0,
            "bit_depth": 8,
            "pixel_format": "I420",
        },
        "commands": {
            "source_i420": source_command,
            "source_bmp": source_bmp_command,
            "encode": encode_command,
            "decode": decode_command,
            "decoded_bmp": decoded_bmp_command,
        },
        "obu_ledger": [entry.ledger_entry() for entry in obus],
        "artifacts": {
            "source_i420": file_record(source_i420),
            "source_bmp": file_record(source_bmp),
            "obu": file_record(obu),
            "decoded_i420": file_record(decoded_i420),
            "decoded_bmp": file_record(decoded_bmp),
        },
    }


def main() -> None:
    encoder = AVM_BUILD_DIR / "avmenc"
    decoder = AVM_BUILD_DIR / "avmdec"
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    reference = verify_reference_tools(encoder, decoder)
    ffmpeg = tool_line(["ffmpeg", "-version"], "ffmpeg version")

    entries = []
    for index, variant in enumerate(VARIANTS, start=1):
        print(
            f"[{index:02d}/{len(VARIANTS):02d}] "
            f"{variant.image}, long edge {variant.long_edge}, QP {variant.qp}"
        )
        entries.append(generate_variant(variant, encoder, decoder))

    manifest = {
        "schema": "av2h-jpeg-artifact-matrix-v1",
        "description": (
            "Varied 8-bit I420 AV2 still-image reference artifacts generated "
            "from all JPEG sources except chalk.jpg."
        ),
        "tools": {
            "ffmpeg": ffmpeg,
            "avm": reference,
        },
        "encoder_contract": {
            "codec": "av2",
            "container": "obu",
            "frames": 1,
            "passes": 1,
            "deadline": "good",
            "cpu_used": 9,
            "threads": 1,
            "profile": 0,
            "bit_depth": 8,
            "input_bit_depth": 8,
            "chroma_subsampling": "4:2:0",
            "rate_control": "q",
        },
        "variants": entries,
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(entries)} variants and {manifest_path}")


if __name__ == "__main__":
    main()

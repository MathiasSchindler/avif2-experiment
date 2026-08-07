#!/usr/bin/env python3
"""Generate and validate 8-bit 4:4:4 AV2 artifacts from source JPEGs."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

from m0_carriage import parse_obus


ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "images"
DEFAULT_OUTPUT = IMAGE_DIR / "artifacts-8bit-444"
DEFAULT_ENCODER = ROOT / "build" / "reference" / "avm-arm64" / "avmenc.exe"
DEFAULT_DECODER = ROOT / "build" / "reference" / "avm-arm64" / "avmdec.exe"
DEFAULT_INSPECTOR = ROOT / "build" / "host-cmake" / "av2heif-artifact.exe"
DEFAULT_PROJECT_DECODER = (
    ROOT / "build" / "windows-x86_64" / "av2heif-decoder.exe"
)


def require_tool(value: Path | str) -> str:
    text = str(value)
    resolved = str(Path(text).resolve()) if Path(text).is_file() else shutil.which(text)
    if resolved is None:
        raise RuntimeError(f"missing required tool: {text}")
    return resolved


def run(command: list[str]) -> None:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{completed.stderr.strip()}"
        )


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def dimensions(ffprobe: str, source: Path) -> tuple[int, int]:
    completed = subprocess.run(
        [
            ffprobe,
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=width,height",
            "-of",
            "json",
            str(source),
        ],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    streams = json.loads(completed.stdout).get("streams", [])
    if len(streams) != 1:
        raise RuntimeError(f"expected one image stream in {source}")
    return int(streams[0]["width"]), int(streams[0]["height"])


def scaled_dimensions(width: int, height: int, maximum: int) -> tuple[int, int]:
    if max(width, height) <= maximum:
        return width, height
    if width >= height:
        return maximum, max(1, (height * maximum + width // 2) // width)
    return max(1, (width * maximum + height // 2) // height), maximum


def normalize_obus(obu: Path) -> None:
    entries = parse_obus(obu.read_bytes())
    normalized = [
        entry.encoded for entry in entries
        if entry.name != "content_interpretation"
    ]
    if len(normalized) != len(entries):
        obu.write_bytes(b"".join(normalized))


def validate_with_inspector(
    inspector: str,
    obu: Path,
    width: int,
    height: int,
    temporary: Path,
) -> dict[str, object]:
    entries = parse_obus(obu.read_bytes())
    if [entry.name for entry in entries] != [
        "temporal_delimiter",
        "sequence_header",
        "closed_loop_key",
    ]:
        raise RuntimeError(f"unexpected OBU ledger for {obu}")
    sequence = next(entry for entry in entries if entry.name == "sequence_header")
    frame = next(entry for entry in entries if entry.name == "closed_loop_key")
    config = temporary / f"{obu.stem}-config.obu"
    payload = temporary / f"{obu.stem}-payload.obu"
    container = temporary / f"{obu.stem}.av2.heif"
    report = temporary / f"{obu.stem}.json"
    config.write_bytes(sequence.encoded)
    payload.write_bytes(frame.encoded)
    run(
        [
            inspector,
            "pack",
            str(config),
            str(payload),
            str(container),
            "2",
            "2",
            "2",
            "limited",
        ]
    )
    run([inspector, "inspect", str(container), str(report)])
    inspection = json.loads(report.read_text(encoding="utf-8"))
    image = inspection["codec"]["effective"]
    sequence_info = inspection["codec"]["sequence"]
    if image["width"] != width or image["height"] != height:
        raise RuntimeError(f"inspector dimension mismatch for {obu}")
    if image["channels"] != 3 or image["bits_per_channel"] != 8:
        raise RuntimeError(f"inspector pixel-layout mismatch for {obu}")
    if (
        sequence_info["profile"] != 4
        or sequence_info["chroma_format_idc"] != 2
        or sequence_info["bit_depth_idc"] != 1
        or sequence_info["still_picture"] != 1
    ):
        raise RuntimeError(f"inspector sequence-profile mismatch for {obu}")
    return {
        "obu_types": [entry.name for entry in entries],
        "profile_idc": sequence_info["profile"],
        "chroma_format_idc": sequence_info["chroma_format_idc"],
        "bit_depth_idc": sequence_info["bit_depth_idc"],
        "still_picture": sequence_info["still_picture"],
        "channels": image["channels"],
        "bits_per_channel": image["bits_per_channel"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--decoder", type=Path, default=DEFAULT_DECODER)
    parser.add_argument("--inspector", type=Path, default=DEFAULT_INSPECTOR)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER
    )
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--max-long-edge", type=int, default=800)
    parser.add_argument("--qp", type=int, default=128)
    parser.add_argument(
        "--demo-compatible",
        action="store_true",
        help="Disable loop filters and require successful project decoding.",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Reuse nonempty OBU files already present in the output directory.",
    )
    parser.add_argument(
        "--source",
        action="append",
        help="Generate only the named JPEG stem; may be repeated.",
    )
    args = parser.parse_args()
    if args.max_long_edge < 1:
        raise ValueError("--max-long-edge must be positive")
    if not 0 <= args.qp <= 255:
        raise ValueError("--qp must be between 0 and 255")

    ffmpeg = require_tool("ffmpeg")
    ffprobe = require_tool("ffprobe")
    encoder = require_tool(args.encoder)
    decoder = require_tool(args.decoder)
    inspector = require_tool(args.inspector)
    project_decoder = (
        require_tool(args.project_decoder) if args.demo_compatible else None
    )
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    requested = set(args.source or [])
    sources = sorted(
        source for source in IMAGE_DIR.glob("*.jpg")
        if not requested or source.stem in requested
    )
    if not sources:
        raise RuntimeError(f"no matching JPEG sources found in {IMAGE_DIR}")
    missing = requested - {source.stem for source in sources}
    if missing:
        raise RuntimeError(f"unknown JPEG source stems: {', '.join(sorted(missing))}")

    records = []
    with tempfile.TemporaryDirectory(prefix="av2heif-444-artifacts-") as scratch:
        temporary = Path(scratch)
        for index, source in enumerate(sources, start=1):
            source_width, source_height = dimensions(ffprobe, source)
            width, height = scaled_dimensions(
                source_width, source_height, args.max_long_edge
            )
            stem = f"{source.stem}-{width}x{height}-qp{args.qp:03d}"
            raw = temporary / f"{stem}.i444"
            decoded = temporary / f"{stem}-decoded.i444"
            project_decoded = temporary / f"{stem}-project.i444"
            obu = output_dir / f"{stem}.obu"
            print(f"[{index:02d}/{len(sources):02d}] {source.name} -> {obu.name}")

            run(
                [
                    ffmpeg,
                    "-nostdin",
                    "-hide_banner",
                    "-loglevel",
                    "error",
                    "-y",
                    "-i",
                    str(source),
                    "-vf",
                    f"scale={width}:{height}:flags=lanczos,format=yuv444p",
                    "-frames:v",
                    "1",
                    "-pix_fmt",
                    "yuv444p",
                    "-f",
                    "rawvideo",
                    str(raw),
                ]
            )
            source_bytes = raw.read_bytes()
            expected_size = width * height * 3
            if len(source_bytes) != expected_size:
                raise RuntimeError(f"unexpected I444 frame size in {raw}")
            encode_arguments = [
                "--debug",
                "--codec=av2",
                "--obu",
                "--passes=1",
                "--limit=1",
                "--good",
                "--cpu-used=9",
                "--threads=1",
                "--profile=1",
                f"--width={width}",
                f"--height={height}",
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
                *(
                    [
                        "--enable-ccso=0",
                        "--enable-cdef=0",
                        "--enable-deblocking=0",
                        "--enable-gdf=0",
                    ]
                    if args.demo_compatible else []
                ),
                "--enable-intrabc=0",
                "--enable-restoration=0",
                "--end-usage=q",
                f"--qp={args.qp}",
                "--test-decode=fatal",
                "--quiet",
                "-o",
                str(obu),
                str(raw),
            ]
            if not (args.resume and obu.is_file() and obu.stat().st_size > 0):
                run([encoder, *encode_arguments])
            normalize_obus(obu)
            run(
                [
                    decoder,
                    "--codec=av2",
                    "--rawvideo",
                    "--output-bit-depth=8",
                    "--limit=1",
                    "-o",
                    str(decoded),
                    str(obu),
                ]
            )
            decoded_bytes = decoded.read_bytes()
            if len(decoded_bytes) != expected_size:
                raise RuntimeError(f"unexpected AVM decode size for {obu}")
            project_decoded_bytes = None
            if project_decoder is not None:
                run(
                    [
                        project_decoder,
                        str(obu),
                        "-o",
                        str(project_decoded),
                    ]
                )
                project_decoded_bytes = project_decoded.read_bytes()
                if len(project_decoded_bytes) != expected_size:
                    raise RuntimeError(f"unexpected project decode size for {obu}")
            verification = validate_with_inspector(
                inspector, obu, width, height, temporary
            )
            record = {
                    "name": source.stem,
                    "source": source.name,
                    "source_dimensions": {
                        "width": source_width,
                        "height": source_height,
                    },
                    "width": width,
                    "height": height,
                    "qp": args.qp,
                    "file": obu.name,
                    "bytes": obu.stat().st_size,
                    "sha256": sha256_file(obu),
                    "source_i444_sha256": sha256_bytes(source_bytes),
                    "decoded_i444_sha256": sha256_bytes(decoded_bytes),
                    "verification": verification,
                }
            if project_decoded_bytes is not None:
                record["project_decoded_i444_sha256"] = sha256_bytes(
                    project_decoded_bytes
                )
                record["project_matches_avm"] = (
                    project_decoded_bytes == decoded_bytes
                )
            records.append(record)

    manifest_path = output_dir / "manifest.json"
    existing_manifest = None
    if requested and manifest_path.is_file():
        existing_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        retained = [
            record for record in existing_manifest.get("artifacts", [])
            if record.get("name") not in requested
        ]
        records = sorted(retained + records, key=lambda record: record["name"])
    manifest = {
        "schema": "av2h-jpeg-8bit-444-artifacts-v1",
        "description": (
            "AV2 8-bit 4:4:4 still-image OBUs derived from the JPEG sources "
            f"in images/, with the longer edge limited to "
            f"{args.max_long_edge} pixels."
        ),
        "encoder": {
            "codec": "av2",
            "container": "obu",
            "frames": 1,
            "passes": 1,
            "deadline": "good",
            "cpu_used": 9,
            "threads": 1,
            "profile": 1,
            "sequence_profile_idc": 4,
            "bit_depth": 8,
            "chroma_format": "4:4:4",
            "rate_control": "q",
            "qp": args.qp,
        },
        "max_long_edge": args.max_long_edge,
        "artifacts": records,
    }
    if args.demo_compatible:
        manifest["description"] = manifest["description"][:-1] + (
            " with loop filters disabled and successful project-decoder "
            "validation."
        )
        manifest["encoder"].update({
            "enable_ccso": 0,
            "enable_cdef": 0,
            "enable_deblocking": 0,
            "enable_gdf": 0,
        })
    if existing_manifest is not None:
        manifest["description"] = (
            "AV2 8-bit 4:4:4 still-image OBUs derived from the JPEG sources "
            "in images/. Selected records may use smaller decoder-validated "
            "dimensions; every dimension remains at most 800 pixels."
        )
        manifest["max_long_edge"] = max(
            int(existing_manifest.get("max_long_edge", 0)), args.max_long_edge
        )
        manifest["encoder"] = existing_manifest.get("encoder", manifest["encoder"])
    generated_files = {record["file"] for record in records}
    for stale in output_dir.glob("*.obu"):
        if stale.name not in generated_files:
            stale.unlink()
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="ascii",
    )
    print(f"Wrote {len(records)} artifacts and {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3
"""Generate paired JPEG-derived AV2 cases and test the local decoder."""

from __future__ import annotations

import argparse
import concurrent.futures
import datetime as dt
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_IMAGES = ROOT / "images"
DEFAULT_AVMENC = ROOT / "build" / "reference" / "avm" / "avmenc.exe"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec.exe"
DEFAULT_LOCAL_DECODER = (
    ROOT / "build" / "windows-x86_64" / "av2heif-decoder.exe"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Create full-spectrum and realistic-QP AV2 cases per "
            "images/*.jpg, decode with AVM and the local decoder, and "
            "report parity."
        )
    )
    parser.add_argument("--images", type=Path, default=DEFAULT_IMAGES)
    parser.add_argument("--avmenc", type=Path, default=DEFAULT_AVMENC)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--local-decoder", type=Path, default=DEFAULT_LOCAL_DECODER
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output directory (default: experimental/yyyy-mm-dd-hh-mm).",
    )
    parser.add_argument(
        "--seed",
        type=int,
        help="Random seed. A system-random seed is generated when omitted.",
    )
    parser.add_argument("--min-long-edge", type=int, default=128)
    parser.add_argument("--max-long-edge", type=int, default=640)
    parser.add_argument("--min-qp", type=int, default=0)
    parser.add_argument("--max-qp", type=int, default=255)
    parser.add_argument("--realistic-min-qp", type=int, default=80)
    parser.add_argument("--realistic-max-qp", type=int, default=160)
    parser.add_argument(
        "--jobs",
        type=int,
        default=min(4, os.cpu_count() or 1),
        help="Number of image cases to process concurrently (default: %(default)s).",
    )
    return parser.parse_args()


def run(command: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if check and completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{detail}" if detail else "")
        )
    return completed


def run_quiet(command: list[str]) -> None:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
        )


def require_tool(path_or_name: Path | str) -> str:
    value = str(path_or_name)
    resolved = str(Path(value).resolve()) if Path(value).is_file() else shutil.which(value)
    if resolved is None:
        raise RuntimeError(f"missing required tool: {value}")
    return resolved


def dimensions(ffprobe: str, path: Path) -> tuple[int, int]:
    completed = run(
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
            str(path),
        ]
    )
    streams = json.loads(completed.stdout).get("streams", [])
    if len(streams) != 1:
        raise RuntimeError(f"expected one image stream in {path}")
    return int(streams[0]["width"]), int(streams[0]["height"])


def scaled_dimensions(width: int, height: int, long_edge: int) -> tuple[int, int]:
    if width >= height:
        return long_edge, max(1, (height * long_edge + width // 2) // width)
    return max(1, (width * long_edge + height // 2) // height), long_edge


def expected_i420_size(width: int, height: int) -> int:
    return width * height + 2 * ((width + 1) // 2) * ((height + 1) // 2)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def file_record(path: Path, output: Path) -> dict[str, Any]:
    return {
        "path": str(path.relative_to(output)),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def convert_to_i420(
    ffmpeg: str, jpeg: Path, output: Path, width: int, height: int
) -> list[str]:
    command = [
        ffmpeg,
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
        str(output),
    ]
    run(command)
    expected = expected_i420_size(width, height)
    if output.stat().st_size != expected:
        raise RuntimeError(
            f"unexpected I420 size for {output.name}: "
            f"{output.stat().st_size}, expected {expected}"
        )
    return command


def i420_to_bmp(
    ffmpeg: str, source: Path, output: Path, width: int, height: int
) -> list[str]:
    command = [
        ffmpeg,
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
        str(source),
        "-frames:v",
        "1",
        "-pix_fmt",
        "bgr24",
        str(output),
    ]
    run_quiet(command)
    return command


def encode(
    avmenc: str,
    source: Path,
    output: Path,
    width: int,
    height: int,
    qp: int,
) -> list[str]:
    command = [
        avmenc,
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
    run_quiet(command)
    return command


def decode_avm(
    avmdec: str, obu: Path, output: Path, width: int, height: int
) -> list[str]:
    pattern = output.with_name(output.stem + "-%wx%h-%1.i420")
    command = [
        avmdec,
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
        raise RuntimeError(f"expected one AVM output for {obu.name}")
    matches[0].replace(output)
    if output.stat().st_size != expected_i420_size(width, height):
        raise RuntimeError(f"unexpected AVM I420 size for {obu.name}")
    return command


def compare_i420(
    local: Path, reference: Path, width: int, height: int
) -> dict[str, Any]:
    local_bytes = local.read_bytes()
    reference_bytes = reference.read_bytes()
    if len(local_bytes) != len(reference_bytes):
        return {
            "exact": False,
            "size_match": False,
            "local_size": len(local_bytes),
            "reference_size": len(reference_bytes),
        }
    chroma_width = (width + 1) // 2
    chroma_height = (height + 1) // 2
    y_size = width * height
    uv_size = chroma_width * chroma_height
    planes = (
        ("Y", 0, y_size),
        ("U", y_size, uv_size),
        ("V", y_size + uv_size, uv_size),
    )
    result: dict[str, Any] = {"size_match": True, "planes": {}}
    total = 0
    for name, offset, size in planes:
        mismatches = 0
        maximum_difference = 0
        first = -1
        for index in range(size):
            difference = abs(
                local_bytes[offset + index] - reference_bytes[offset + index]
            )
            if difference == 0:
                continue
            if first < 0:
                first = index
            mismatches += 1
            maximum_difference = max(maximum_difference, difference)
        result["planes"][name] = {
            "mismatches": mismatches,
            "first_plane_offset": first,
            "max_difference": maximum_difference,
        }
        total += mismatches
    result["total_mismatches"] = total
    result["exact"] = total == 0
    return result


def choose_variant(
    rng: random.Random,
    original_width: int,
    original_height: int,
    minimum: int,
    maximum: int,
    min_qp: int,
    max_qp: int,
) -> tuple[int, int, int, int]:
    original_long_edge = max(original_width, original_height)
    upper = min(maximum, original_long_edge - 1)
    lower = min(minimum, upper)
    if upper < 1:
        raise RuntimeError("source image is too small to downscale")
    long_edge = rng.randint(max(1, lower), upper)
    width, height = scaled_dimensions(
        original_width, original_height, long_edge
    )
    return long_edge, width, height, rng.randint(min_qp, max_qp)


def generate_case(
    jpeg: Path,
    output: Path,
    rng: random.Random,
    tools: dict[str, str],
    args: argparse.Namespace,
    qp_profile: str,
    min_qp: int,
    max_qp: int,
) -> dict[str, Any]:
    original_width, original_height = dimensions(tools["ffprobe"], jpeg)
    long_edge, width, height, qp = choose_variant(
        rng,
        original_width,
        original_height,
        args.min_long_edge,
        args.max_long_edge,
        min_qp,
        max_qp,
    )
    stem = f"{jpeg.stem}-{qp_profile}-{width}x{height}-qp{qp:03d}"
    source_i420 = output / f"{stem}-source.i420"
    source_bmp = output / f"{stem}-source.bmp"
    obu = output / f"{stem}.obu"
    avm_i420 = output / f"{stem}-avm.i420"
    avm_bmp = output / f"{stem}-avm.bmp"
    local_i420 = output / f"{stem}-local.i420"
    local_bmp = output / f"{stem}-local.bmp"

    commands: dict[str, list[str]] = {}
    commands["source_i420"] = convert_to_i420(
        tools["ffmpeg"], jpeg, source_i420, width, height
    )
    commands["source_bmp"] = i420_to_bmp(
        tools["ffmpeg"], source_i420, source_bmp, width, height
    )
    commands["encode"] = encode(
        tools["avmenc"], source_i420, obu, width, height, qp
    )
    commands["avm_decode"] = decode_avm(
        tools["avmdec"], obu, avm_i420, width, height
    )
    commands["avm_bmp"] = i420_to_bmp(
        tools["ffmpeg"], avm_i420, avm_bmp, width, height
    )

    local_i420_command = [
        tools["local_decoder"], str(obu), "-o", str(local_i420)
    ]
    local_i420_result = run(local_i420_command, check=False)
    commands["local_i420"] = local_i420_command
    local_bmp_command = [
        tools["local_decoder"], str(obu), "-o", str(local_bmp)
    ]
    local_bmp_result = run(local_bmp_command, check=False)
    commands["local_bmp"] = local_bmp_command

    local_success = (
        local_i420_result.returncode == 0
        and local_bmp_result.returncode == 0
        and local_i420.is_file()
        and local_bmp.is_file()
    )
    parity = (
        compare_i420(local_i420, avm_i420, width, height)
        if local_success
        else None
    )
    artifact_paths = [
        source_i420,
        source_bmp,
        obu,
        avm_i420,
        avm_bmp,
    ]
    if local_i420.is_file():
        artifact_paths.append(local_i420)
    if local_bmp.is_file():
        artifact_paths.append(local_bmp)
    return {
        "name": f"{jpeg.stem}-{qp_profile}",
        "source_name": jpeg.stem,
        "qp_profile": qp_profile,
        "status": "exact" if parity and parity["exact"] else (
            "decoded_nonexact" if local_success else "decode_failed"
        ),
        "source_dimensions": [original_width, original_height],
        "variant": {
            "long_edge": long_edge,
            "width": width,
            "height": height,
            "qp": qp,
        },
        "local_decoder": {
            "success": local_success,
            "i420_exit_code": local_i420_result.returncode,
            "bmp_exit_code": local_bmp_result.returncode,
            "stderr": local_i420_result.stderr.strip(),
        },
        "parity": parity,
        "commands": commands,
        "artifacts": [file_record(path, output) for path in artifact_paths],
    }


def write_reports(
    output: Path,
    seed: int,
    tools: dict[str, str],
    cases: list[dict[str, Any]],
) -> None:
    totals = {
        "images": len(
            {case.get("source_name", case["name"]) for case in cases}
        ),
        "cases": len(cases),
        "exact": sum(case.get("status") == "exact" for case in cases),
        "decoded_nonexact": sum(
            case.get("status") == "decoded_nonexact" for case in cases
        ),
        "decode_failed": sum(
            case.get("status") == "decode_failed" for case in cases
        ),
        "generation_failed": sum(
            case.get("status") == "generation_failed" for case in cases
        ),
    }
    report = {
        "schema": "av2h-random-jpeg-experiment-v1",
        "created": dt.datetime.now().astimezone().isoformat(),
        "seed": seed,
        "tools": tools,
        "totals": totals,
        "cases": cases,
    }
    (output / "statistics.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    lines = [
        "Random JPEG-derived AV2 decoder experiment",
        f"Seed: {seed}",
        f"Source images: {totals['images']}",
        f"Cases: {totals['cases']}",
        f"Exact: {totals['exact']}",
        f"Decoded, nonexact: {totals['decoded_nonexact']}",
        f"Decode failed: {totals['decode_failed']}",
        f"Generation failed: {totals['generation_failed']}",
        "",
    ]
    for case in cases:
        variant = case.get("variant")
        detail = ""
        if variant:
            detail = (
                f" {variant['width']}x{variant['height']} QP {variant['qp']}"
            )
        if case.get("parity"):
            detail += f" mismatches={case['parity']['total_mismatches']}"
        lines.append(f"{case['name']}: {case['status']}{detail}")
    (output / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    if args.min_long_edge < 1 or args.max_long_edge < args.min_long_edge:
        raise RuntimeError("invalid long-edge range")
    if args.min_qp < 0 or args.max_qp > 255 or args.max_qp < args.min_qp:
        raise RuntimeError("QP range must be within 0..255")
    if (
        args.realistic_min_qp < 0
        or args.realistic_max_qp > 255
        or args.realistic_max_qp < args.realistic_min_qp
    ):
        raise RuntimeError("realistic QP range must be within 0..255")
    if args.jobs < 1:
        raise RuntimeError("jobs must be at least 1")
    images = args.images.resolve()
    jpegs = sorted(images.glob("*.jpg"), key=lambda path: path.name.lower())
    if not jpegs:
        raise RuntimeError(f"no .jpg files found in {images}")
    seed = args.seed if args.seed is not None else random.SystemRandom().randrange(2**63)
    rng = random.Random(seed)
    timestamp = dt.datetime.now().strftime("%Y-%m-%d-%H-%M")
    output = (args.output or (Path(__file__).parent / timestamp)).resolve()
    output.mkdir(parents=True, exist_ok=False)
    tools = {
        "ffmpeg": require_tool("ffmpeg"),
        "ffprobe": require_tool("ffprobe"),
        "avmenc": require_tool(args.avmenc.resolve()),
        "avmdec": require_tool(args.avmdec.resolve()),
        "local_decoder": require_tool(args.local_decoder.resolve()),
    }
    profiles = (
        ("full", args.min_qp, args.max_qp),
        ("realistic", args.realistic_min_qp, args.realistic_max_qp),
    )
    cases: list[dict[str, Any] | None] = [None] * (len(jpegs) * len(profiles))
    print(f"Output: {output}")
    print(f"Seed: {seed}")
    print(f"Jobs: {args.jobs}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        futures = {
            executor.submit(
                generate_case,
                jpeg,
                output,
                random.Random(rng.randrange(2**63)),
                tools,
                args,
                profile,
                min_qp,
                max_qp,
            ): (image_index * len(profiles) + profile_index, jpeg, profile)
            for image_index, jpeg in enumerate(jpegs)
            for profile_index, (profile, min_qp, max_qp) in enumerate(profiles)
        }
        for completed, future in enumerate(
            concurrent.futures.as_completed(futures), start=1
        ):
            index, jpeg, profile = futures[future]
            try:
                case = future.result()
            except Exception as error:
                case = {
                    "name": f"{jpeg.stem}-{profile}",
                    "source_name": jpeg.stem,
                    "qp_profile": profile,
                    "status": "generation_failed",
                    "error": str(error),
                }
            cases[index] = case
            print(
                f"[{completed:02d}/{len(cases):02d}] {jpeg.name} "
                f"({profile}): "
                f"{case['status']}",
                flush=True,
            )
            write_reports(
                output,
                seed,
                tools,
                [item for item in cases if item is not None],
            )
    completed_cases = [case for case in cases if case is not None]
    print((output / "summary.txt").read_text(encoding="utf-8"), end="")
    return 0 if all(
        case["status"] != "generation_failed" for case in completed_cases
    ) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(2)
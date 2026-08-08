#!/usr/bin/env python3
"""Generate the Palacio Pitti AV2 quality-comparison asset series."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import shutil
import subprocess
import threading
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEMO = Path(__file__).resolve().parent
DEFAULT_SOURCE = DEMO / "source" / "palacio-pitti-2000.jpg"
DEFAULT_OUTPUT = ROOT / "docs" / "assets"
QP_VALUES = (0, 4, *range(8, 217, 8), *range(220, 253, 4))
OUTPUT_SIZE = 960
ACTIVE_PROCESSES: set[subprocess.Popen[str]] = set()
ACTIVE_PROCESSES_LOCK = threading.Lock()

BASELINE_QUALITIES = (*range(1, 11), *range(15, 100, 5), 96, 97, 98)
AVIF_QUALITIES = (5, 6, 8, 10, *range(15, 100, 5), 96, 98)
BASELINE_FORMATS = (
    {
        "id": "jpeg",
        "format": "JPEG",
        "setting_suffix": "4:2:0",
        "extension": "jpg",
        "qualities": BASELINE_QUALITIES,
        "arguments_before": ("-sampling-factor", "4:2:0"),
        "arguments_after": (),
    },
    {
        "id": "webp",
        "format": "WebP",
        "setting_suffix": "method 6",
        "extension": "webp",
        "qualities": BASELINE_QUALITIES,
        "arguments_before": (),
        "arguments_after": ("-define", "webp:method=6"),
    },
    {
        "id": "avif",
        "format": "AVIF",
        "setting_suffix": "speed 6",
        "extension": "avif",
        "qualities": AVIF_QUALITIES,
        "arguments_before": (),
        "arguments_after": ("-define", "heic:speed=6"),
    },
)

SCENES = (
    {
        "id": "overview",
        "name": "Entire ceiling",
        "description": "The complete ceiling scaled to 960 x 960 pixels.",
        "filter": f"scale={OUTPUT_SIZE}:{OUTPUT_SIZE}:flags=lanczos,format=yuv420p",
        "transformation": "Scaled from the complete source image.",
    },
    {
        "id": "inscription",
        "name": "Inscription detail",
        "description": "A 50% linear crop with lettering, stucco, and fresco detail.",
        "filter": (
            "crop=1920:1920:0:920,transpose=2,"
            f"scale={OUTPUT_SIZE}:{OUTPUT_SIZE}:flags=lanczos,format=yuv420p"
        ),
        "transformation": (
            "Cropped to 1920 x 1920 pixels at source position (0, 920), "
            "rotated 90 degrees counter-clockwise, and scaled to 960 x 960."
        ),
    },
)


def default_encoder() -> Path:
    directory = ROOT / "build" / "reference" / "avm"
    names = (
        ("avmenc.exe", "avmenc")
        if os.name == "nt"
        else ("avmenc", "avmenc.exe")
    )
    for name in names:
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return directory / names[0]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--encoder", type=Path, default=default_encoder())
    parser.add_argument(
        "--jobs", type=int, default=min(4, os.cpu_count() or 1)
    )
    parser.add_argument(
        "--resume", action="store_true",
        help="Reuse nonempty encoded files already present in the output directory.",
    )
    return parser.parse_args()


def require_tool(value: Path | str) -> str:
    text = str(value)
    resolved = str(Path(text).resolve()) if Path(text).is_file() else shutil.which(text)
    if resolved is None:
        raise RuntimeError(f"missing required tool: {text}")
    return resolved


def run(command: list[str]) -> None:
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    with ACTIVE_PROCESSES_LOCK:
        ACTIVE_PROCESSES.add(process)
    try:
        _, stderr = process.communicate()
    finally:
        with ACTIVE_PROCESSES_LOCK:
            ACTIVE_PROCESSES.discard(process)
    if process.returncode != 0:
        raise RuntimeError(
            f"command failed ({process.returncode}): {' '.join(command)}\n"
            f"{stderr.strip()}"
        )


def terminate_active_processes() -> None:
    with ACTIVE_PROCESSES_LOCK:
        processes = list(ACTIVE_PROCESSES)
    for process in processes:
        if process.poll() is None:
            process.terminate()
    for process in processes:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def dimensions(ffprobe: str, source: Path) -> tuple[int, int]:
    completed = subprocess.run(
        [
            ffprobe, "-v", "error", "-select_streams", "v:0",
            "-show_entries", "stream=width,height", "-of", "json",
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


def make_source(ffmpeg: str, source: Path, output: Path, filter_graph: str) -> None:
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
            filter_graph,
            "-frames:v",
            "1",
            "-f",
            "rawvideo",
            str(output),
        ]
    )
    expected = OUTPUT_SIZE * OUTPUT_SIZE * 3 // 2
    if output.stat().st_size != expected:
        raise RuntimeError(
            f"unexpected I420 size for {output}: {output.stat().st_size}"
        )


def make_reference_source(ffmpeg: str, source: Path, output: Path) -> None:
    run(
        [
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
            f"{OUTPUT_SIZE}x{OUTPUT_SIZE}",
            "-i",
            str(source),
            "-frames:v",
            "1",
            str(output),
        ]
    )


def encode_baseline(
    magick: str,
    source: Path,
    output: Path,
    baseline: dict[str, object],
    resume: bool,
) -> dict[str, object]:
    if resume and output.is_file() and output.stat().st_size > 0:
        print(f"Reusing {output.name}", flush=True)
    else:
        temporary = output.with_name(
            f".{output.stem}.tmp{output.suffix}"
        )
        temporary.unlink(missing_ok=True)
        print(f"Encoding {output.name} ({baseline['setting']})", flush=True)
        try:
            run(
                [
                    magick,
                    str(source),
                    "-strip",
                    *baseline["arguments_before"],
                    "-quality",
                    str(baseline["quality"]),
                    *baseline["arguments_after"],
                    str(temporary),
                ]
            )
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)
        print(f"Encoded {output.name} ({output.stat().st_size} bytes)", flush=True)
    return {
        "id": baseline["id"],
        "format": baseline["format"],
        "quality": baseline["quality"],
        "setting": baseline["setting"],
        "file": output.name,
        "bytes": output.stat().st_size,
        "sha256": sha256(output),
    }


def encode_variant(
    encoder: str, source: Path, output: Path, qp: int, resume: bool
) -> dict[str, object]:
    if resume and output.is_file() and output.stat().st_size > 0:
        print(f"Reusing {output.name}", flush=True)
    else:
        temporary = output.with_name(f".{output.name}.tmp")
        temporary.unlink(missing_ok=True)
        print(f"Encoding {output.name} (QP {qp})", flush=True)
        try:
            run(
                [
                    encoder,
                    "--debug",
                    "--codec=av2",
                    "--obu",
                    "--passes=1",
                    "--limit=1",
                    "--good",
                    "--cpu-used=9",
                    "--threads=1",
                    "--profile=0",
                    f"--width={OUTPUT_SIZE}",
                    f"--height={OUTPUT_SIZE}",
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
                    str(temporary),
                    str(source),
                ]
            )
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)
        print(f"Encoded {output.name} ({output.stat().st_size} bytes)", flush=True)
    return {
        "qp": qp,
        "file": output.name,
        "bytes": output.stat().st_size,
        "sha256": sha256(output),
    }


def main() -> None:
    args = parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    if not source.is_file():
        raise RuntimeError(f"missing source image: {source}")
    ffmpeg = require_tool("ffmpeg")
    ffprobe = require_tool("ffprobe")
    magick = require_tool("magick")
    encoder = require_tool(args.encoder)
    source_width, source_height = dimensions(ffprobe, source)
    output.mkdir(parents=True, exist_ok=True)

    scene_records = []
    for scene in SCENES:
        scene_id = str(scene["id"])
        source_i420 = output / f"{scene_id}-source.i420"
        source_png = output / f"{scene_id}-source.png"
        print(f"Preparing {scene['name']}", flush=True)
        make_source(ffmpeg, source, source_i420, str(scene["filter"]))
        try:
            jobs = []
            with concurrent.futures.ThreadPoolExecutor(
                max_workers=args.jobs
            ) as pool:
                for qp in QP_VALUES:
                    target = output / f"{scene_id}-q{qp:03d}.obu"
                    jobs.append(
                        pool.submit(
                            encode_variant, encoder, source_i420, target, qp,
                            args.resume
                        )
                    )
                try:
                    variants = [job.result() for job in jobs]
                except BaseException:
                    for job in jobs:
                        job.cancel()
                    terminate_active_processes()
                    raise
            make_reference_source(ffmpeg, source_i420, source_png)
            baselines = []
            for baseline_format in BASELINE_FORMATS:
                for quality in baseline_format["qualities"]:
                    baseline = {
                        **baseline_format,
                        "id": f"{baseline_format['id']}-q{quality:02d}",
                        "quality": quality,
                        "setting": (
                            f"quality {quality} / "
                            f"{baseline_format['setting_suffix']}"
                        ),
                    }
                    target = output / (
                        f"{scene_id}-{baseline['id']}."
                        f"{baseline['extension']}"
                    )
                    baselines.append(
                        encode_baseline(
                            magick, source_png, target, baseline, args.resume
                        )
                    )
        finally:
            source_i420.unlink(missing_ok=True)
            source_png.unlink(missing_ok=True)
        scene_records.append(
            {
                "id": scene_id,
                "name": scene["name"],
                "description": scene["description"],
                "transformation": scene["transformation"],
                "width": OUTPUT_SIZE,
                "height": OUTPUT_SIZE,
                "variants": sorted(variants, key=lambda item: int(item["qp"])),
                "baselines": baselines,
            }
        )
        print(f"Generated {len(variants)} variants for {scene_id}")

    manifest = {
        "schema": "av2h-quality-viewer-v5",
        "title": "Palazzo Pitti ceiling",
        "source": {
            "description": (
                "Ceiling of Sala della Stufa, Galleria Palatina, "
                "Palazzo Pitti, Florence."
            ),
            "author": "Diego Delso",
            "author_url": "http://delso.photo/",
            "license": "CC BY-SA 4.0",
            "license_url": "https://creativecommons.org/licenses/by-sa/4.0/legalcode",
            "commons_url": (
                "https://commons.wikimedia.org/wiki/File:Palacio_Pitti,_Florencia,_"
                "Italia,_2022-09-18,_DD_163-165_HDR.jpg"
            ),
            "derivative_url": (
                "https://commons.wikimedia.org/wiki/Special:Redirect/file/"
                "Palacio_Pitti%2C_Florencia%2C_Italia%2C_2022-09-18%2C_"
                "DD_163-165_HDR.jpg?width=2000"
            ),
            "downloaded_dimensions": {
                "width": source_width,
                "height": source_height,
            },
            "sha256": sha256(source),
            "changes": (
                "Scaled, cropped, rotated, converted to I420, and encoded "
                "as AV2, JPEG, WebP, and AVIF."
            ),
        },
        "encoder": {
            "codec": "AV2",
            "implementation": "AVM",
            "cpu_used": 9,
            "profile": 0,
            "bit_depth": 8,
            "pixel_format": "I420",
            "qp_values": list(QP_VALUES),
        },
        "baseline_encoder": {
            "implementation": "ImageMagick",
            "qualities": {
                baseline_format["id"]: list(baseline_format["qualities"])
                for baseline_format in BASELINE_FORMATS
            },
            "note": (
                "Bitrate matching does not imply equal visual quality; "
                "quality scales are format-specific."
            ),
        },
        "scenes": scene_records,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Wrote {output / 'manifest.json'}")


if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""Freeze the deterministic still-image quality baseline."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import re
import statistics
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER_ROOT = ROOT / "experimental" / "encoder"
DEFAULT_ENCODER = ENCODER_ROOT / "build" / "av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build" / "host-cmake" / "av2heif-decoder"
DEFAULT_OUTPUT = ENCODER_ROOT / "metrics" / "encoder-quality-baseline.json"
DEFAULT_WORK = ENCODER_ROOT / "build" / "encoder-quality-baseline"
PREPARED_LOCK = ROOT / "images" / "recipes" / "prepared-lock.json"

QINDICES = (60, 100, 180)
SOURCES = (
    "chinnathurai-church",
    "elbrus-milky-way",
    "pastel-chalks",
    "pitti-ceiling",
)
FORMATS = ("i400", "i420", "i444")
PATTERNS = (
    "horizontal-edge",
    "vertical-edge",
    "diagonal-edge",
    "checkerboard-4",
    "horizontal-lines-2",
    "vertical-lines-2",
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def run(command: list[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    start = time.monotonic_ns()
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    return completed, time.monotonic_ns() - start


def plane_geometry(pixel_format: str, width: int, height: int) -> list[tuple[int, int]]:
    if pixel_format == "i400":
        return [(width, height)]
    if pixel_format == "i420":
        return [
            (width, height),
            ((width + 1) // 2, (height + 1) // 2),
            ((width + 1) // 2, (height + 1) // 2),
        ]
    return [(width, height), (width, height), (width, height)]


def split_planes(data: bytes, pixel_format: str, width: int, height: int) -> list[bytes]:
    planes = []
    offset = 0
    for plane_width, plane_height in plane_geometry(pixel_format, width, height):
        size = plane_width * plane_height
        planes.append(data[offset:offset + size])
        offset += size
    if offset != len(data):
        raise RuntimeError("decoded planar byte count mismatch")
    return planes


def pattern_sample(name: str, x: int, y: int, shift: int) -> int:
    if name == "horizontal-edge":
        return (32 + shift) & 255 if y < 32 else (224 - shift) & 255
    if name == "vertical-edge":
        return (32 + shift) & 255 if x < 32 else (224 - shift) & 255
    if name == "diagonal-edge":
        return (40 + shift) & 255 if x < y else (216 - shift) & 255
    if name == "checkerboard-4":
        return (48 + shift) & 255 if ((x // 4) ^ (y // 4)) & 1 else (208 - shift) & 255
    if name == "horizontal-lines-2":
        return (64 + shift) & 255 if (y // 2) & 1 else (192 - shift) & 255
    if name == "vertical-lines-2":
        return (64 + shift) & 255 if (x // 2) & 1 else (192 - shift) & 255
    raise RuntimeError(f"unknown pattern: {name}")


def generated_input(name: str, pixel_format: str) -> bytes:
    output = bytearray()
    for plane, (width, height) in enumerate(plane_geometry(pixel_format, 64, 64)):
        shift = (0, 19, 37)[plane]
        for y in range(height):
            for x in range(width):
                scale = 2 if pixel_format == "i420" and plane else 1
                output.append(pattern_sample(name, x * scale, y * scale, shift))
    return bytes(output)


def prepared_cases() -> list[dict[str, object]]:
    lock = json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))
    records = {
        item["source_id"]: item
        for item in lock["preparations"]
        if item["preparation_id"] == "le896"
    }
    cases = []
    for source_id in SOURCES:
        record = records[source_id]
        width, height = map(int, record["dimensions"])
        manifest_dir = ROOT / "images" / "prepared" / "le896" / source_id
        for pixel_format, output_name in (
            ("i400", "gray8"), ("i420", "yuv420p8"), ("i444", "yuv444p8")
        ):
            output = record["outputs"][output_name]
            cases.append({
                "id": f"full-{source_id}-{pixel_format}",
                "subset": "full-prepared",
                "source_id": source_id,
                "pixel_format": pixel_format,
                "width": width,
                "height": height,
                "path": manifest_dir / output["filename"],
                "expected_sha256": output["sha256"],
            })
    return cases


def all_cases(work: Path) -> list[dict[str, object]]:
    cases = prepared_cases()
    inputs = work / "inputs"
    inputs.mkdir(parents=True, exist_ok=True)
    for pattern in PATTERNS:
        for pixel_format in FORMATS:
            data = generated_input(pattern, pixel_format)
            path = inputs / f"{pattern}-64x64.{pixel_format}"
            path.write_bytes(data)
            cases.append({
                "id": f"target-{pattern}-{pixel_format}",
                "subset": "targeted",
                "pattern": pattern,
                "pixel_format": pixel_format,
                "width": 64,
                "height": 64,
                "path": path,
                "expected_sha256": sha256_bytes(data),
            })
    return cases


def psnr(sse: int, samples: int) -> str:
    if sse == 0:
        return "inf"
    return f"{10.0 * math.log10(255.0 * 255.0 * samples / sse):.6f}"


def encode_case(
    case: dict[str, object], qindex: int, encoder: Path, avmdec: Path,
    project_decoder: Path, work: Path, search: str,
) -> dict[str, object]:
    case_id = f"{case['id']}-q{qindex}"
    case_dir = work / "runs" / case_id
    case_dir.mkdir(parents=True, exist_ok=True)
    source_path = Path(case["path"])
    source = source_path.read_bytes()
    if sha256_bytes(source) != case["expected_sha256"]:
        raise RuntimeError(f"input hash mismatch: {case['id']}")
    attempts = []
    outputs = []
    metric = re.compile(
        rb"encoder: bytes=(\d+) sse_y=(\d+)(?: sse_u=(\d+) sse_v=(\d+))?"
        rb"(?: psnr_y=(?:inf|\d+\.\d+)dB)? workspace_bytes=(\d+) qindex=(\d+)"
    )
    for attempt in range(2):
        obu = case_dir / f"encoded-{attempt}.obu"
        command = [
            str(encoder), str(source_path), "--width", str(case["width"]),
            "--height", str(case["height"]), "--qindex", str(qindex),
            "--search", search, "--output", str(obu),
        ]
        completed, duration = run(command)
        if completed.returncode != 0:
            raise RuntimeError(
                f"encoder failed for {case_id}: {completed.stderr.decode(errors='replace')}"
            )
        match = metric.search(completed.stderr)
        if not match:
            raise RuntimeError(f"encoder metrics missing for {case_id}")
        attempts.append(duration)
        outputs.append(obu)
    obu_hashes = [sha256_file(path) for path in outputs]
    if obu_hashes[0] != obu_hashes[1]:
        raise RuntimeError(f"nondeterministic output: {case_id}")
    assert match is not None
    obu = outputs[0]
    avm_output = case_dir / "avm.raw"
    project_output = case_dir / f"project.{case['pixel_format']}"
    commands = (
        [
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm_output), str(obu),
        ],
        [str(project_decoder), str(obu), "-o", str(project_output)],
    )
    decode_times = {}
    for name, command in zip(("avm", "project"), commands):
        completed, duration = run(command)
        if completed.returncode != 0:
            raise RuntimeError(f"{name} decoder rejected {case_id}")
        decode_times[name] = duration
    avm = avm_output.read_bytes()
    project = project_output.read_bytes()
    if avm != project:
        raise RuntimeError(f"decoder mismatch: {case_id}")
    source_planes = split_planes(
        source, str(case["pixel_format"]), int(case["width"]), int(case["height"])
    )
    decoded_planes = split_planes(
        avm, str(case["pixel_format"]), int(case["width"]), int(case["height"])
    )
    plane_records = []
    for name, original, decoded in zip(("y", "u", "v"), source_planes, decoded_planes):
        sse = sum((a - b) ** 2 for a, b in zip(original, decoded))
        plane_records.append({
            "plane": name,
            "samples": len(original),
            "sse": sse,
            "psnr_db": psnr(sse, len(original)),
            "decoded_sha256": sha256_bytes(decoded),
        })
    reported = [int(match.group(2))]
    if match.group(3) is not None:
        reported.extend((int(match.group(3)), int(match.group(4))))
    decoded_sse = [item["sse"] for item in plane_records]
    return {
        "id": case_id,
        "source_case": case["id"],
        "subset": case["subset"],
        "pattern": case.get("pattern"),
        "source_id": case.get("source_id"),
        "pixel_format": case["pixel_format"],
        "width": case["width"],
        "height": case["height"],
        "qindex": qindex,
        "input_bytes": len(source),
        "input_sha256": sha256_bytes(source),
        "obu_bytes": obu.stat().st_size,
        "obu_sha256": obu_hashes[0],
        "decoded_sha256": sha256_bytes(avm),
        "planes": plane_records,
        "encoder_reported_sse": reported,
        "encoder_reported_sse_matches_decode": reported == decoded_sse,
        "workspace_bytes": int(match.group(5)),
        "encode_time_ns": {
            "samples": attempts,
            "median": int(statistics.median(attempts)),
            "worst": max(attempts),
        },
        "decode_time_ns": decode_times,
        "deterministic_rerun": True,
        "avm_project_exact": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    parser.add_argument(
        "--search", choices=("off", "directional", "intra", "plane-qdelta"),
        default="off")
    args = parser.parse_args()
    for path in (args.encoder, args.avmdec, args.project_decoder):
        if not path.is_file():
            raise SystemExit(f"missing executable: {path}")
    cases = all_cases(args.work_dir)
    records = [
        encode_case(case, qindex, args.encoder.resolve(), args.avmdec.resolve(),
                    args.project_decoder.resolve(), args.work_dir.resolve(),
                    args.search)
        for case in cases for qindex in QINDICES
    ]
    full = [item for item in records if item["subset"] == "full-prepared"]
    targeted = [item for item in records if item["subset"] == "targeted"]
    result = {
        "schema": "av2-encoder-quality-baseline-v1",
        "generated_by": "experimental/encoder/tools/encoder_quality_baseline.py",
        "configuration": {
            "qindices": list(QINDICES),
            "prepared_sources": list(SOURCES),
            "formats": list(FORMATS),
            "patterns": list(PATTERNS),
            "encode_repetitions": 2,
            "search": args.search,
        },
        "counts": {
            "prepared_sources": len(SOURCES),
            "full_format_images": len(SOURCES) * len(FORMATS),
            "targeted_format_images": len(PATTERNS) * len(FORMATS),
            "full_cases": len(full),
            "targeted_cases": len(targeted),
            "total_cases": len(records),
        },
        "tools": {
            "encoder": {
                "bytes": args.encoder.stat().st_size,
                "sha256": sha256_file(args.encoder),
            },
            "avmdec_sha256": sha256_file(args.avmdec),
            "project_decoder_sha256": sha256_file(args.project_decoder),
            "prepared_lock_sha256": sha256_file(PREPARED_LOCK),
            "python": platform.python_version(),
            "machine": platform.machine(),
            "system": platform.system(),
        },
        "summary": {
            "all_deterministic": all(item["deterministic_rerun"] for item in records),
            "all_validator_exact": all(item["avm_project_exact"] for item in records),
            "max_workspace_bytes": max(item["workspace_bytes"] for item in records),
            "median_encode_time_ns": int(statistics.median(
                item["encode_time_ns"]["median"] for item in records
            )),
            "worst_encode_time_ns": max(
                item["encode_time_ns"]["worst"] for item in records
            ),
            "total_obu_bytes": sum(item["obu_bytes"] for item in records),
            "total_sse": {
                plane: sum(
                    next((p["sse"] for p in item["planes"] if p["plane"] == plane), 0)
                    for item in records
                )
                for plane in ("y", "u", "v")
            },
        },
        "cases": records,
    }
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        f"cases={len(records)} full={len(full)} targeted={len(targeted)} "
        f"bytes={result['summary']['total_obu_bytes']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

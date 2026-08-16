#!/usr/bin/env python3
"""Diagnose Baya Weaver blockiness with a reproducible production sweep."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import time
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
DEFAULT_ENCODER = ENCODER / "build/av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT = ROOT / "build/host-cmake/av2heif-decoder"
DEFAULT_OUTPUT = ENCODER / "metrics/blockiness-m0.json"
DEFAULT_ARTIFACTS = ENCODER / "metrics/blockiness-m0"
DEFAULT_WORK = ENCODER / "build/blockiness-m0"
PREPARED_LOCK = ROOT / "images/recipes/prepared-lock.json"
PREPARED_DIR = ROOT / "images/prepared/le1440/baya-weaver"

QINDICES = (60, 80, 100, 120, 150, 165, 180, 220)
ANCHOR_MODES = ("off", "intra", "directional")
FORMATS = (
    ("i400", "gray8"),
    ("i420", "yuv420p8"),
    ("i444", "yuv444p8"),
)
BOUNDARIES = (4, 8, 16)
CROP = {"x": 448, "y": 192, "width": 512, "height": 576}
METRIC_RE = re.compile(
    rb"encoder: bytes=(\d+) sse_y=(\d+)"
    rb"(?: sse_u=(\d+) sse_v=(\d+))?"
    rb"(?: psnr_y=(?:inf|\d+\.\d+)dB)?"
    rb" workspace_bytes=(\d+) qindex=(\d+)"
)


class M0Error(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def executable(path: Path, label: str) -> Path:
    path = path.resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise M0Error(f"{label} is not executable: {path}")
    return path


def run(command: Sequence[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    start = time.monotonic_ns()
    completed = subprocess.run(
        command, cwd=ROOT, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )
    return completed, time.monotonic_ns() - start


def plane_geometry(
    pixel_format: str, width: int, height: int,
) -> list[tuple[int, int]]:
    if pixel_format == "i400":
        return [(width, height)]
    if pixel_format == "i420":
        return [
            (width, height),
            ((width + 1) // 2, (height + 1) // 2),
            ((width + 1) // 2, (height + 1) // 2),
        ]
    if pixel_format == "i444":
        return [(width, height)] * 3
    raise M0Error(f"unsupported pixel format: {pixel_format}")


def split_planes(
    data: bytes, pixel_format: str, width: int, height: int,
) -> list[bytes]:
    planes = []
    offset = 0
    for plane_width, plane_height in plane_geometry(
        pixel_format, width, height
    ):
        size = plane_width * plane_height
        planes.append(data[offset:offset + size])
        offset += size
    if offset != len(data):
        raise M0Error(
            f"{pixel_format} planar size mismatch: {len(data)} != {offset}"
        )
    return planes


def psnr(sse: int, samples: int) -> str:
    if sse == 0:
        return "inf"
    return f"{10.0 * math.log10(65025.0 * samples / sse):.6f}"


def ssim_q1e9(
    source: bytes, decoded: bytes, width: int, height: int,
    window: int = 8,
) -> int:
    """Return deterministic non-overlapping-window SSIM scaled by 1e9."""
    if len(source) != width * height or len(decoded) != len(source):
        raise M0Error("SSIM plane geometry mismatch")
    total = 0
    windows = 0
    c1_scaled = 65025
    c2_scaled = 9 * 65025
    scale = 10000
    for top in range(0, height, window):
        bottom = min(top + window, height)
        for left in range(0, width, window):
            right = min(left + window, width)
            sx = sy = sxx = syy = sxy = 0
            count = (right - left) * (bottom - top)
            for y in range(top, bottom):
                offset = y * width
                for x in range(left, right):
                    a = source[offset + x]
                    b = decoded[offset + x]
                    sx += a
                    sy += b
                    sxx += a * a
                    syy += b * b
                    sxy += a * b
            count_squared = count * count
            luminance_num = scale * 2 * sx * sy + c1_scaled * count_squared
            luminance_den = scale * (sx * sx + sy * sy) + \
                c1_scaled * count_squared
            covariance = count * sxy - sx * sy
            variance = count * sxx - sx * sx + count * syy - sy * sy
            contrast_num = scale * 2 * covariance + \
                c2_scaled * count_squared
            contrast_den = scale * variance + c2_scaled * count_squared
            total += (
                1_000_000_000 * luminance_num * contrast_num
                // (luminance_den * contrast_den)
            )
            windows += 1
    return total // windows


def boundary_diagnostic(
    source: bytes, decoded: bytes, width: int, height: int, block: int,
) -> dict[str, int]:
    """Measure reconstruction-gradient error at and away from block edges."""
    if len(source) != width * height or len(decoded) != len(source):
        raise M0Error("boundary plane geometry mismatch")
    boundary_error = interior_error = 0
    boundary_absolute_error = boundary_excess = 0
    boundary_count = interior_count = 0

    for y in range(height):
        row = y * width
        for x in range(1, width):
            source_jump = int(source[row + x]) - int(source[row + x - 1])
            decoded_jump = int(decoded[row + x]) - int(decoded[row + x - 1])
            error = decoded_jump - source_jump
            if x % block == 0:
                boundary_error += error * error
                boundary_absolute_error += abs(error)
                boundary_excess += max(
                    0, abs(decoded_jump) - abs(source_jump)
                )
                boundary_count += 1
            else:
                interior_error += error * error
                interior_count += 1

    for y in range(1, height):
        row = y * width
        previous = row - width
        for x in range(width):
            source_jump = int(source[row + x]) - int(source[previous + x])
            decoded_jump = int(decoded[row + x]) - int(decoded[previous + x])
            error = decoded_jump - source_jump
            if y % block == 0:
                boundary_error += error * error
                boundary_absolute_error += abs(error)
                boundary_excess += max(
                    0, abs(decoded_jump) - abs(source_jump)
                )
                boundary_count += 1
            else:
                interior_error += error * error
                interior_count += 1

    ratio_ppm = 0
    if boundary_error and interior_error:
        ratio_ppm = (
            boundary_error * interior_count * 1_000_000
            // (boundary_count * interior_error)
        )
    return {
        "block": block,
        "boundary_samples": boundary_count,
        "boundary_gradient_error_squared": boundary_error,
        "boundary_gradient_error_absolute": boundary_absolute_error,
        "boundary_excess_discontinuity": boundary_excess,
        "interior_samples": interior_count,
        "interior_gradient_error_squared": interior_error,
        "boundary_to_interior_error_ratio_ppm": ratio_ppm,
    }


def write_pgm(
    path: Path, plane: bytes, width: int, height: int,
    crop: dict[str, int],
) -> None:
    left = crop["x"]
    top = crop["y"]
    crop_width = min(crop["width"], width - left)
    crop_height = min(crop["height"], height - top)
    if left < 0 or top < 0 or crop_width <= 0 or crop_height <= 0:
        raise M0Error("representative crop is outside the luma plane")
    pixels = b"".join(
        plane[y * width + left:y * width + left + crop_width]
        for y in range(top, top + crop_height)
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        f"P5\n{crop_width} {crop_height}\n255\n".encode("ascii") + pixels
    )


def prepared_cases() -> tuple[list[dict[str, Any]], dict[str, str]]:
    lock_data = PREPARED_LOCK.read_bytes()
    lock = json.loads(lock_data)
    record = next(
        item for item in lock["preparations"]
        if item["preparation_id"] == "le1440"
        and item["source_id"] == "baya-weaver"
    )
    width, height = map(int, record["dimensions"])
    cases = []
    hashes = {
        "images/recipes/prepared-lock.json": sha256_bytes(lock_data),
        str((PREPARED_DIR / "manifest.json").relative_to(ROOT)):
            sha256_file(PREPARED_DIR / "manifest.json"),
    }
    for pixel_format, output_name in FORMATS:
        output = record["outputs"][output_name]
        path = PREPARED_DIR / output["filename"]
        if sha256_file(path) != output["sha256"]:
            raise M0Error(f"prepared input hash mismatch: {path}")
        relative = str(path.relative_to(ROOT))
        hashes[relative] = output["sha256"]
        cases.append({
            "pixel_format": pixel_format,
            "width": width,
            "height": height,
            "path": path,
            "input_bytes": output["byte_size"],
            "input_sha256": output["sha256"],
        })
    return cases, hashes


def encode_and_measure(
    case: dict[str, Any], qindex: int, search: str,
    encoder: Path, avmdec: Path, project: Path,
    work: Path, artifacts: Path,
) -> dict[str, Any]:
    pixel_format = case["pixel_format"]
    case_id = f"{pixel_format}-{search}-q{qindex}"
    case_dir = work / case_id
    case_dir.mkdir(parents=True)
    output_dir = artifacts / "streams" / pixel_format
    output_dir.mkdir(parents=True, exist_ok=True)
    retained_obu = output_dir / f"{search}-q{qindex}.obu"
    durations = []
    hashes = []
    matches = []
    for attempt in range(2):
        obu = case_dir / f"attempt-{attempt}.obu"
        command = [
            str(encoder), str(case["path"]),
            "--width", str(case["width"]),
            "--height", str(case["height"]),
            "--qindex", str(qindex),
            "--search", search,
            "--output", str(obu),
        ]
        completed, duration = run(command)
        if completed.returncode != 0:
            raise M0Error(
                f"encoder failed for {case_id}: "
                f"{completed.stderr.decode('utf-8', 'backslashreplace')}"
            )
        match = METRIC_RE.search(completed.stderr)
        if not match:
            raise M0Error(f"encoder metrics missing for {case_id}")
        durations.append(duration)
        hashes.append(sha256_file(obu))
        matches.append(match)
    if hashes[0] != hashes[1]:
        raise M0Error(f"nondeterministic stream: {case_id}")
    shutil.copyfile(case_dir / "attempt-0.obu", retained_obu)

    avm_output = case_dir / "avm.raw"
    project_output = case_dir / f"project.{pixel_format}"
    decode_commands = (
        [
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm_output),
            str(retained_obu),
        ],
        [str(project), str(retained_obu), "-o", str(project_output)],
    )
    decode_times: dict[str, int] = {}
    for name, command in zip(("avm", "project"), decode_commands):
        completed, duration = run(command)
        if completed.returncode != 0:
            raise M0Error(
                f"{name} decoder rejected {case_id}: "
                f"{completed.stderr.decode('utf-8', 'backslashreplace')}"
            )
        decode_times[name] = duration
    avm = avm_output.read_bytes()
    project_decoded = project_output.read_bytes()
    if avm != project_decoded:
        raise M0Error(f"decoder mismatch for {case_id}")

    source = case["path"].read_bytes()
    source_planes = split_planes(
        source, pixel_format, case["width"], case["height"]
    )
    decoded_planes = split_planes(
        avm, pixel_format, case["width"], case["height"]
    )
    plane_records = []
    for index, (name, source_plane, decoded_plane, geometry) in enumerate(zip(
        ("y", "u", "v"), source_planes, decoded_planes,
        plane_geometry(pixel_format, case["width"], case["height"]),
    )):
        width, height = geometry
        sse = sum(
            (int(left) - int(right)) ** 2
            for left, right in zip(source_plane, decoded_plane)
        )
        plane_records.append({
            "plane": name,
            "width": width,
            "height": height,
            "samples": len(source_plane),
            "sse": sse,
            "psnr_db": psnr(sse, len(source_plane)),
            "ssim_q1e9": ssim_q1e9(
                source_plane, decoded_plane, width, height
            ),
            "source_sha256": sha256_bytes(source_plane),
            "decoded_sha256": sha256_bytes(decoded_plane),
            "boundaries": [
                boundary_diagnostic(
                    source_plane, decoded_plane, width, height, block
                )
                for block in BOUNDARIES
            ],
        })
        if index == 0:
            crop_path = (
                artifacts / "crops" / pixel_format
                / f"{search}-q{qindex}-y.pgm"
            )
            write_pgm(
                crop_path, decoded_plane, width, height, CROP
            )

    match = matches[0]
    reported_sse = [int(match.group(2))]
    if match.group(3) is not None:
        reported_sse.extend(
            (int(match.group(3)), int(match.group(4)))
        )
    decoded_sse = [plane["sse"] for plane in plane_records]
    return {
        "id": case_id,
        "pixel_format": pixel_format,
        "width": case["width"],
        "height": case["height"],
        "qindex": qindex,
        "search": search,
        "command": [
            "av2-encoder-experiment", case["path"].name,
            "--width", str(case["width"]),
            "--height", str(case["height"]),
            "--qindex", str(qindex),
            "--search", search,
            "--output", retained_obu.name,
        ],
        "input_bytes": case["input_bytes"],
        "input_sha256": case["input_sha256"],
        "obu_bytes": retained_obu.stat().st_size,
        "obu_sha256": hashes[0],
        "retained_obu": str(retained_obu.relative_to(ENCODER)),
        "decoded_sha256": sha256_bytes(avm),
        "planes": plane_records,
        "encoder_reported_sse": reported_sse,
        "encoder_local_sse_matches_decode": reported_sse == decoded_sse,
        "workspace_bytes": int(match.group(5)),
        "encode_time_ns": {
            "samples": durations,
            "median": int(statistics.median(durations)),
            "worst": max(durations),
        },
        "decode_time_ns": decode_times,
        "deterministic": True,
        "avm_project_exact": True,
    }


def boundary_record(record: dict[str, Any], plane: int, block: int) -> dict[str, int]:
    return next(
        item for item in record["planes"][plane]["boundaries"]
        if item["block"] == block
    )


def classify(records: list[dict[str, Any]]) -> dict[str, Any]:
    bounded = {
        (record["pixel_format"], record["qindex"]): record
        for record in records if record["search"] == "bounded"
    }
    high_quality = [bounded[(pixel_format, min(QINDICES))]
                    for pixel_format, _ in FORMATS]
    continuity = [bounded[(pixel_format, 100)]
                  for pixel_format, _ in FORMATS]
    high_sse = sum(record["planes"][0]["sse"] for record in high_quality)
    continuity_sse = sum(record["planes"][0]["sse"] for record in continuity)
    high_bytes = sum(record["obu_bytes"] for record in high_quality)
    continuity_bytes = sum(record["obu_bytes"] for record in continuity)
    sse_improvement_ppm = (
        (continuity_sse - high_sse) * 1_000_000 // continuity_sse
    )
    rate_growth_ppm = (
        (high_bytes - continuity_bytes) * 1_000_000 // continuity_bytes
    )
    high_boundary_error = sum(
        boundary_record(record, 0, 8)[
            "boundary_gradient_error_squared"
        ] for record in high_quality
    )
    high_boundary_samples = sum(
        boundary_record(record, 0, 8)["boundary_samples"]
        for record in high_quality
    )
    high_interior_error = sum(
        boundary_record(record, 0, 8)[
            "interior_gradient_error_squared"
        ] for record in high_quality
    )
    high_interior_samples = sum(
        boundary_record(record, 0, 8)["interior_samples"]
        for record in high_quality
    )
    ratio_ppm = (
        high_boundary_error * high_interior_samples * 1_000_000
        // (high_boundary_samples * high_interior_error)
    )
    boundary_evidence = ratio_ppm >= 1_150_000
    residual_evidence = (
        sse_improvement_ppm >= 200_000 or
        (
            rate_growth_ppm >= 300_000 and
            sse_improvement_ppm < 100_000 and
            not boundary_evidence
        )
    )
    if residual_evidence and boundary_evidence:
        dominant = "both-residual-capacity-and-block-boundaries"
    elif residual_evidence:
        dominant = "residual-capacity"
    elif boundary_evidence:
        dominant = "block-boundaries"
    else:
        dominant = "inconclusive"
    return {
        "classification": dominant,
        "continuity_qindex": 100,
        "lower_quantization_qindex": min(QINDICES),
        "aggregate_luma_sse_improvement_ppm": sse_improvement_ppm,
        "aggregate_stream_rate_growth_ppm": rate_growth_ppm,
        "lower_quantization_8x8_boundary_to_interior_error_ratio_ppm":
            ratio_ppm,
        "thresholds": {
            "residual_capacity_minimum_sse_improvement_ppm": 200_000,
            "residual_saturation_minimum_rate_growth_ppm": 300_000,
            "residual_saturation_maximum_sse_improvement_ppm": 100_000,
            "block_boundary_minimum_error_ratio_ppm": 1_150_000,
        },
        "residual_capacity_evidence": residual_evidence,
        "block_boundary_evidence": boundary_evidence,
        "recommendation": (
            "continue with M1 combined prediction-plus-AC search, then retain "
            "M2 residual breadth before M3 bounded deblocking"
            if residual_evidence and boundary_evidence else
            "prioritize M1 and M2 residual breadth"
            if residual_evidence else
            "prioritize M1 validation followed by M3 bounded deblocking"
            if boundary_evidence else
            "expand the diagnostic corpus before changing codec tools"
        ),
    }


def equal_byte_matches(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    matches = []
    for pixel_format, _ in FORMATS:
        candidates = [
            record for record in records
            if record["pixel_format"] == pixel_format
            and record["search"] == "bounded"
        ]
        for mode in ANCHOR_MODES:
            anchor = next(
                record for record in records
                if record["pixel_format"] == pixel_format
                and record["search"] == mode
            )
            nearest = min(
                candidates,
                key=lambda item: (
                    abs(item["obu_bytes"] - anchor["obu_bytes"]),
                    item["qindex"],
                ),
            )
            matches.append({
                "pixel_format": pixel_format,
                "anchor_search": mode,
                "anchor_qindex": 100,
                "anchor_bytes": anchor["obu_bytes"],
                "bounded_qindex": nearest["qindex"],
                "bounded_bytes": nearest["obu_bytes"],
                "byte_difference": nearest["obu_bytes"] - anchor["obu_bytes"],
                "byte_difference_ppm": (
                    (nearest["obu_bytes"] - anchor["obu_bytes"]) * 1_000_000
                    // anchor["obu_bytes"]
                ),
                "anchor_plane_sse": [
                    plane["sse"] for plane in anchor["planes"]
                ],
                "bounded_plane_sse": [
                    plane["sse"] for plane in nearest["planes"]
                ],
                "anchor_plane_ssim_q1e9": [
                    plane["ssim_q1e9"] for plane in anchor["planes"]
                ],
                "bounded_plane_ssim_q1e9": [
                    plane["ssim_q1e9"] for plane in nearest["planes"]
                ],
            })
    return matches


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    result.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    result.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    result.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    result.add_argument("--artifacts", type=Path, default=DEFAULT_ARTIFACTS)
    result.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        encoder = executable(args.encoder, "experimental encoder")
        avmdec = executable(args.avmdec, "pinned avmdec")
        project = executable(args.project_decoder, "project decoder")
        cases, corpus = prepared_cases()
        shutil.rmtree(args.work_dir, ignore_errors=True)
        shutil.rmtree(args.artifacts, ignore_errors=True)
        args.work_dir.mkdir(parents=True)
        args.artifacts.mkdir(parents=True)
        records = []
        for case in cases:
            source_planes = split_planes(
                case["path"].read_bytes(), case["pixel_format"],
                case["width"], case["height"]
            )
            source_crop = (
                args.artifacts / "crops" / case["pixel_format"]
                / "source-y.pgm"
            )
            write_pgm(
                source_crop, source_planes[0],
                case["width"], case["height"], CROP
            )
            for qindex in QINDICES:
                records.append(encode_and_measure(
                    case, qindex, "bounded", encoder, avmdec, project,
                    args.work_dir, args.artifacts,
                ))
            for mode in ANCHOR_MODES:
                records.append(encode_and_measure(
                    case, 100, mode, encoder, avmdec, project,
                    args.work_dir, args.artifacts,
                ))

        gates = {
            "locked_inputs": len(cases) == 3 and all(
                case["width"] == 1440 and case["height"] == 960
                for case in cases
            ),
            "bounded_sweep_complete": len([
                record for record in records
                if record["search"] == "bounded"
            ]) == len(FORMATS) * len(QINDICES),
            "q100_anchors_complete": len([
                record for record in records
                if record["search"] in ANCHOR_MODES
            ]) == len(FORMATS) * len(ANCHOR_MODES),
            "all_deterministic": all(
                record["deterministic"] for record in records
            ),
            "all_local_sse_exact": all(
                record["encoder_local_sse_matches_decode"]
                for record in records
            ),
            "all_avm_project_exact": all(
                record["avm_project_exact"] for record in records
            ),
            "all_artifacts_retained": all(
                (ENCODER / record["retained_obu"]).is_file()
                for record in records
            ),
        }
        revision = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True,
            text=True, stdout=subprocess.PIPE,
        ).stdout.strip()
        diagnosis = classify(records)
        report = {
            "schema": "av2-encoder-blockiness-m0-v1",
            "status": "pass" if all(gates.values()) else "blocked",
            "configuration": {
                "source": "baya-weaver",
                "preparation": "le1440",
                "dimensions": [1440, 960],
                "formats": [item[0] for item in FORMATS],
                "bounded_qindices": list(QINDICES),
                "continuity_qindex": 100,
                "anchor_searches": list(ANCHOR_MODES),
                "boundary_sizes": list(BOUNDARIES),
                "ssim": {
                    "window": "non-overlapping 8x8 active-crop windows",
                    "scale": 1_000_000_000,
                    "constants": "C1=(0.01*255)^2, C2=(0.03*255)^2",
                },
                "boundary_metric": (
                    "squared decoded-minus-source adjacent-sample gradient "
                    "error; block-edge MSE is compared with non-boundary MSE"
                ),
                "representative_luma_crop": CROP,
            },
            "revision": {
                "git_commit": revision,
                "encoder_sha256": sha256_file(encoder),
                "avmdec_sha256": sha256_file(avmdec),
                "project_decoder_sha256": sha256_file(project),
            },
            "corpus": corpus,
            "gates": gates,
            "diagnosis": diagnosis,
            "equal_byte_comparisons": equal_byte_matches(records),
            "summary": {
                "case_count": len(records),
                "stream_count": len(records),
                "crop_count": len(list(
                    (args.artifacts / "crops").glob("*/*.pgm")
                )),
                "maximum_workspace_bytes": max(
                    record["workspace_bytes"] for record in records
                ),
                "worst_encode_time_ns": max(
                    record["encode_time_ns"]["worst"]
                    for record in records
                ),
            },
            "cases": records,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps({
            "status": report["status"],
            "classification": diagnosis["classification"],
            **report["summary"],
        }, sort_keys=True))
        return 0 if report["status"] == "pass" else 1
    except (M0Error, OSError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=os.sys.stderr)
        return 1
    finally:
        shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

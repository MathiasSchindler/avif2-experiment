#!/usr/bin/env python3
"""Run or summarize the bounded M4 partition/transform Baya gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import time
from pathlib import Path

from blockiness_m0 import boundary_diagnostic, split_planes, ssim_q1e9


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
ENCODER_BIN = ENCODER / "build/av2-encoder-experiment"
M1_REPORT = ENCODER / "metrics/blocky-m1-gate.json"
SOURCES = ROOT / "images/prepared/le1440/baya-weaver"
CASES = (
    ("i400", "source-1440x960-gray8.i400"),
    ("i420", "source-1440x960-yuv420p8.i420"),
    ("i444", "source-1440x960-yuv444p8.i444"),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def tokens(stderr: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for token in stderr.split():
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        if value.lstrip("-").replace(".", "", 1).isdigit():
            if "." not in value:
                result[key] = int(value)
    return result


def run(command: list[str], timeout_seconds: int = 3600) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["timeout", str(timeout_seconds), *command],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


def baseline_records() -> dict[str, dict]:
    report = json.loads(M1_REPORT.read_text(encoding="utf-8"))
    records: dict[str, dict] = {}
    for case in report["cases"]:
        if case["id"] == "baya-weaver-le1440-i400-q100":
            records["i400"] = case["searches"]["rd"]
        elif case["id"] == "baya-weaver-le1440-i420-q100":
            records["i420"] = case["searches"]["rd"]
        elif case["id"] == "baya-weaver-le1440-i444-q100":
            records["i444"] = case["searches"]["rd"]
    return records


def artifact_names(reuse: Path, pixel_format: str) -> tuple[Path, Path]:
    if pixel_format == "i400":
        return (
            reuse / "baya-i400-partition-rd.obu",
            reuse / "baya-i400-partition-rd.metrics",
        )
    return (
        reuse / f"baya-{pixel_format}-partition-rd-final.obu",
        reuse / f"baya-{pixel_format}-partition-rd-final.metrics",
    )


def encode_case(
    source: Path, pixel_format: str, artifacts: Path,
) -> tuple[Path, str, float]:
    output = artifacts / f"baya-{pixel_format}-partition-rd.obu"
    repeat = artifacts / f"baya-{pixel_format}-partition-rd-repeat.obu"
    started = time.monotonic()
    first = run([
        str(ENCODER_BIN), str(source), "--search", "partition-rd",
        "--qindex", "100", "--output", str(output),
    ])
    elapsed = time.monotonic() - started
    if first.returncode != 0:
        raise RuntimeError(first.stderr.decode("utf-8", errors="replace"))
    second = run([
        str(ENCODER_BIN), str(source), "--search", "partition-rd",
        "--qindex", "100", "--output", str(repeat),
    ])
    if second.returncode != 0:
        raise RuntimeError(second.stderr.decode("utf-8", errors="replace"))
    if output.read_bytes() != repeat.read_bytes():
        raise RuntimeError(f"{pixel_format}: nondeterministic stream")
    repeat.unlink()
    return output, first.stderr.decode("utf-8", errors="replace"), elapsed


def decode_exact(
    stream: Path, pixel_format: str, avmdec: Path, project: Path,
    artifacts: Path,
) -> bytes:
    avm = artifacts / f"decoded-avm.{pixel_format}"
    local = artifacts / f"decoded-project.{pixel_format}"
    commands = (
        [
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(stream),
        ],
        [str(project), str(stream), "-o", str(local)],
    )
    for command in commands:
        completed = run(command, 300)
        if completed.returncode != 0:
            raise RuntimeError(
                completed.stderr.decode("utf-8", errors="replace"))
    decoded = avm.read_bytes()
    if decoded != local.read_bytes():
        raise RuntimeError(f"{pixel_format}: decoder mismatch")
    avm.unlink()
    local.unlink()
    return decoded


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--avmdec", type=Path,
                        default=ROOT / "build/reference/avm/avmdec")
    parser.add_argument("--project-decoder", type=Path,
                        default=ROOT / "build/host-cmake/av2heif-decoder")
    parser.add_argument("--output", type=Path,
                        default=ENCODER / "metrics/blocky-m4-partition-gate.json")
    parser.add_argument("--artifacts-dir", type=Path,
                        default=ENCODER / "metrics/blocky-m4-partition-gate")
    parser.add_argument("--reuse-dir", type=Path)
    args = parser.parse_args()
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    baselines = baseline_records()
    records = []
    objective_improvements = 0
    for pixel_format, filename in CASES:
        source_path = SOURCES / filename
        source = source_path.read_bytes()
        if args.reuse_dir is None:
            stream_path, stderr, elapsed = encode_case(
                source_path, pixel_format, args.artifacts_dir)
        else:
            stream_path, metrics_path = artifact_names(
                args.reuse_dir, pixel_format)
            stderr = metrics_path.read_text(encoding="utf-8")
            parsed = tokens(stderr)
            elapsed = float(
                next((item.split("=", 1)[1] for item in stderr.split()
                      if item.startswith("elapsed=")), "0"))
        parsed = tokens(stderr)
        stream = stream_path.read_bytes()
        decoded = decode_exact(
            stream_path, pixel_format, args.avmdec,
            args.project_decoder, args.artifacts_dir)
        source_planes = split_planes(source, pixel_format, 1440, 960)
        decoded_planes = split_planes(decoded, pixel_format, 1440, 960)
        plane_records = []
        for plane, (original, reconstruction) in enumerate(
                zip(source_planes, decoded_planes)):
            plane_width = 1440 if pixel_format != "i420" or plane == 0 else 720
            plane_height = 960 if pixel_format != "i420" or plane == 0 else 480
            plane_records.append({
                "plane": "yuv"[plane],
                "sse": sum((a - b) ** 2
                           for a, b in zip(original, reconstruction)),
                "ssim_q1e9": ssim_q1e9(
                    original, reconstruction, plane_width, plane_height),
                "boundary_8": boundary_diagnostic(
                    original, reconstruction, plane_width, plane_height, 8),
                "decoded_sha256": sha256(reconstruction),
            })
        baseline = baselines[pixel_format]
        baseline_sse = [plane["decoded_sse"] for plane in baseline["planes"]]
        adaptive_sse = [plane["sse"] for plane in plane_records]
        strict_pareto = (
            len(stream) <= baseline["obu_bytes"] and
            all(a <= b for a, b in zip(adaptive_sse, baseline_sse)) and
            (len(stream) < baseline["obu_bytes"] or adaptive_sse != baseline_sse)
        )
        if strict_pareto:
            objective_improvements += 1
        candidate_large = parsed.get(
            "rd_candidate_transform_16x16",
            parsed.get("rd_transform_16x16", 0))
        candidate_small = parsed.get(
            "rd_candidate_transform_8x8",
            parsed.get("rd_transform_8x8", 0))
        records.append({
            "pixel_format": pixel_format,
            "input_sha256": sha256(source),
            "adaptive": {
                "bytes": len(stream),
                "stream_sha256": sha256(stream),
                "planes": plane_records,
                "runtime_seconds": elapsed,
                "workspace_bytes": parsed["workspace_bytes"],
                "candidate_trials": parsed.get("rd_trials", 0),
                "candidate_partition_split8": parsed.get(
                    "rd_candidate_partition_split8",
                    parsed.get("rd_partition_split8", 0)),
                "candidate_partition_keep16": parsed.get(
                    "rd_candidate_partition_keep16",
                    parsed.get("rd_partition_keep16", 0)),
                "candidate_transform_8x8": candidate_small,
                "candidate_transform_16x16": candidate_large,
                "decoder_exact": True,
            },
            "rd_baseline": {
                "bytes": baseline["obu_bytes"],
                "stream_sha256": baseline["obu_sha256"],
                "plane_sse": baseline_sse,
                "deterministic": baseline["deterministic"],
                "decoder_exact": baseline["avm_project_exact"],
            },
            "comparison": {
                "byte_delta": len(stream) - baseline["obu_bytes"],
                "plane_sse_delta": [
                    a - b for a, b in zip(adaptive_sse, baseline_sse)
                ],
                "strict_pareto": strict_pareto,
                "stream_matches_rd": sha256(stream) == baseline["obu_sha256"],
            },
        })
        if args.reuse_dir is not None:
            retained = args.artifacts_dir / stream_path.name
            shutil.copyfile(stream_path, retained)
    status = "pass" if objective_improvements else "blocked"
    report = {
        "schema": 1,
        "status": status,
        "reason": None if objective_improvements else (
            "All three final Baya streams match admitted rd byte-for-byte; "
            "candidate 16x16 trials do not produce a validator-exact admitted "
            "Baya objective improvement."
        ),
        "configuration": {
            "width": 1440, "height": 960, "qindex": 100,
            "baseline_search": "rd", "adaptive_search": "partition-rd",
        },
        "cases": records,
        "summary": {
            "strict_pareto_cases": objective_improvements,
            "all_per_plane_nonregressing": all(
                all(delta <= 0 for delta in case["comparison"]["plane_sse_delta"])
                for case in records),
            "all_decoder_exact": all(
                case["adaptive"]["decoder_exact"] for case in records),
            "all_match_rd": all(
                case["comparison"]["stream_matches_rd"] for case in records),
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    print(json.dumps(report["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

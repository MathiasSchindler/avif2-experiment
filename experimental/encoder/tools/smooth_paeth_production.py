#!/usr/bin/env python3
"""Validate the admitted smooth-family production search on locked model cases."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER_ROOT = Path(__file__).resolve().parents[1]
MODEL_PATH = Path(__file__).with_name("smooth_paeth_model.py")
VECTORS = ENCODER_ROOT / "tests/smooth_paeth_vectors.json"
DEFAULT_OUTPUT = ENCODER_ROOT / "metrics/encoder-smooth-paeth-production.json"
DEFAULT_WORK = ENCODER_ROOT / "build/encoder-smooth-paeth-production"
METRIC = re.compile(
    rb"bytes=(\d+) sse_y=(\d+)(?: sse_u=(\d+) sse_v=(\d+))?.*"
    rb"(?:intra_mode|luma_mode)=(\d+)"
)


def load_model():
    spec = importlib.util.spec_from_file_location("smooth_paeth_model", MODEL_PATH)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def geometry(pixel_format: str, width: int, height: int) -> list[tuple[int, int]]:
    if pixel_format == "i400":
        return [(width, height)]
    if pixel_format == "i420":
        return [(width, height), ((width + 1) // 2, (height + 1) // 2)] * 1 + [
            ((width + 1) // 2, (height + 1) // 2)
        ]
    return [(width, height)] * 3


def source_for_case(model, case: dict[str, object]) -> bytes:
    width, height = map(int, case["size"])
    name = str(case["source"])
    if not name.startswith("prepared:"):
        return bytes(model.generated_pattern(name, width, height))
    fmt = str(case["format"])
    luma_width, luma_height = map(int, case["luma_reference_size"])
    crops, _ = model.cardinal._prepared_crops(fmt, luma_width, luma_height)
    for plane, source, crop_width, crop_height in crops:
        if (plane == case["plane"] and
                crop_width == width and crop_height == height):
            return bytes(source)
    raise RuntimeError("prepared crop not found")


def planar_case(model, case: dict[str, object]) -> bytes:
    fmt = str(case["format"])
    width, height = map(int, case["luma_reference_size"])
    target = {"Y": 0, "U": 1, "V": 2}[str(case["plane"])]
    source = source_for_case(model, case)
    output = bytearray()
    for plane, (plane_width, plane_height) in enumerate(geometry(fmt, width, height)):
        output.extend(source if plane == target else bytes([128]) * (
            plane_width * plane_height))
    return bytes(output)


def run(command: list[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    start = time.monotonic_ns()
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed, time.monotonic_ns() - start


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--encoder", type=Path,
                        default=ENCODER_ROOT / "build/av2-encoder-experiment")
    parser.add_argument("--avmdec", type=Path,
                        default=ROOT / "build/reference/avm/avmdec")
    parser.add_argument("--project-decoder", type=Path,
                        default=ROOT / "build/host-cmake/av2heif-decoder")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()
    model = load_model()
    data = json.loads(VECTORS.read_text())
    args.work_dir.mkdir(parents=True, exist_ok=True)
    totals = {
        name: {"bytes": 0, "sse": [0, 0, 0], "time_ns": 0}
        for name in ("directional-exact", "intra")
    }
    wins = 0
    mode_names = {
        5: "smooth", 6: "smooth-v", 7: "smooth-h", 8: "paeth",
    }
    mode_wins = {name: 0 for name in mode_names.values()}
    records = []
    for index, case in enumerate(data["cases"]):
        fmt = str(case["format"])
        width, height = map(int, case["luma_reference_size"])
        source = args.work_dir / f"{index:03d}.{fmt}"
        source_data = planar_case(model, case)
        source.write_bytes(source_data)
        record = {"index": index, "format": fmt, "plane": case["plane"],
                  "size": case["size"], "source": case["source"]}
        outputs = {}
        for search in ("directional-exact", "intra"):
            attempts = []
            for attempt in range(2 if search == "intra" else 1):
                obu = args.work_dir / f"{index:03d}-{search}-{attempt}.obu"
                completed, elapsed = run([
                    str(args.encoder), str(source), "--width", str(width),
                    "--height", str(height), "--search", search, "-o", str(obu),
                ])
                if completed.returncode != 0:
                    raise RuntimeError(completed.stderr.decode(errors="replace"))
                match = METRIC.search(completed.stderr)
                if not match:
                    raise RuntimeError("encoder metrics missing")
                attempts.append((obu, elapsed, match))
            if len(attempts) == 2 and attempts[0][0].read_bytes() != attempts[1][0].read_bytes():
                raise RuntimeError(f"nondeterministic case {index}")
            obu, elapsed, match = attempts[0]
            reported = [int(match.group(2))]
            if match.group(3) is not None:
                reported.extend((int(match.group(3)), int(match.group(4))))
            outputs[search] = {
                "bytes": int(match.group(1)), "sse": reported,
                "mode": int(match.group(5)), "time_ns": elapsed,
                "sha256": hashlib.sha256(obu.read_bytes()).hexdigest(),
            }
            totals[search]["bytes"] += int(match.group(1))
            totals[search]["time_ns"] += elapsed
            for plane, value in enumerate(reported):
                totals[search]["sse"][plane] += value
        intra_obu = args.work_dir / f"{index:03d}-intra-0.obu"
        avm = args.work_dir / f"{index:03d}.avm"
        project = args.work_dir / f"{index:03d}.project.{fmt}"
        for command in (
            [str(args.avmdec), "--codec=av2", "--rawvideo", "--limit=1",
             "--output-bit-depth=8", "-o", str(avm), str(intra_obu)],
            [str(args.project_decoder), str(intra_obu), "-o", str(project)],
        ):
            completed, _ = run(command)
            if completed.returncode != 0:
                raise RuntimeError(f"decoder rejected case {index}")
        if avm.read_bytes() != project.read_bytes():
            raise RuntimeError(f"decoder mismatch case {index}")
        decoded = avm.read_bytes()
        decoded_sse = []
        offset = 0
        for plane_width, plane_height in geometry(fmt, width, height):
            size = plane_width * plane_height
            decoded_sse.append(sum(
                (left - right) ** 2 for left, right in zip(
                    source_data[offset:offset + size],
                    decoded[offset:offset + size])
            ))
            offset += size
        local_sse_matches_decode = decoded_sse == outputs["intra"]["sse"]
        if outputs["intra"]["mode"] >= 5:
            wins += 1
            mode_wins[mode_names[outputs["intra"]["mode"]]] += 1
        record["results"] = outputs
        record["local_sse_matches_decode"] = local_sse_matches_decode
        records.append(record)
    checks = {
        "bytes_nonincreasing":
            totals["intra"]["bytes"] <= totals["directional-exact"]["bytes"],
        "plane_sse_nonincreasing": all(
            after <= before for after, before in zip(
                totals["intra"]["sse"], totals["directional-exact"]["sse"])),
        "incremental_wins": wins > 0,
        "smooth_sse_matches_decode": all(
            record["local_sse_matches_decode"]
            for record in records
            if record["results"]["intra"]["mode"] >= 5),
        "runtime_ratio_at_most_4":
            totals["intra"]["time_ns"] <= totals["directional-exact"]["time_ns"] * 4,
    }
    result = {
        "schema": "encoder-smooth-paeth-production-v2",
        "cases": len(records), "baseline": totals["directional-exact"],
        "intra": totals["intra"], "incremental_wins": wins,
        "mode_wins": mode_wins, "gate": {"checks": checks,
                                         "passed": all(checks.values())},
        "records": records,
    }
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps({key: result[key] for key in (
        "cases", "baseline", "intra", "incremental_wins", "mode_wins", "gate"
    )}, sort_keys=True))
    return 0 if result["gate"]["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

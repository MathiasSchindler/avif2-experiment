#!/usr/bin/env python3
"""Freeze Smooth and Paeth search at the full production dimension limit."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import statistics
from pathlib import Path
from typing import Any

from ac8x8_color_gate import (
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT,
    ENCODER,
    FORMATS,
    PREPARED_LOCK,
    ROOT,
    encode,
)


DEFAULT_OUTPUT = ENCODER / "metrics/smooth-paeth-full-gate.json"
DEFAULT_WORK = ENCODER / "build/smooth-paeth-full-gate"
SOURCE_ID = "pitti-ceiling"
PREPARATION_ID = "le1440"
QINDEX = 100
MAX_WORKSPACE_BYTES = 24 * 1024 * 1024


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    lock = json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))
    record = next(
        item for item in lock["preparations"]
        if item["source_id"] == SOURCE_ID
        and item["preparation_id"] == PREPARATION_ID
    )
    width, height = map(int, record["dimensions"])
    output_names = {
        "i400": "gray8",
        "i420": "yuv420p8",
        "i444": "yuv444p8",
    }
    luma = bytes(
        min(255, ((x - width // 2) ** 2 + (y - height // 2) ** 2) // 4096)
        for y in range(height) for x in range(width)
    )
    cases = []
    sources = {}
    for pixel_format in FORMATS:
        if pixel_format == "i400":
            source = luma
        elif pixel_format == "i420":
            chroma = bytes([128]) * (((width + 1) // 2) *
                                     ((height + 1) // 2))
            source = luma + chroma + chroma
        else:
            chroma = bytes([128]) * (width * height)
            source = luma + chroma + chroma
        cases.append({
            "id": f"synthetic-center-{pixel_format}-{width}x{height}",
            "subset": "synthetic-center",
            "pixel_format": pixel_format,
            "width": width,
            "height": height,
            "qindex": QINDEX,
            "source": source,
            "source_sha256": sha256_bytes(source),
        })
    output = record["outputs"][output_names["i400"]]
    path = (ROOT / "images" / "prepared" / PREPARATION_ID /
            SOURCE_ID / output["filename"])
    source = path.read_bytes()
    if sha256_bytes(source) != output["sha256"]:
        raise RuntimeError(f"prepared input hash mismatch: {path}")
    relative = str(path.relative_to(ROOT))
    sources[relative] = output["sha256"]
    cases.append({
        "id": f"prepared-{SOURCE_ID}-i400-{width}x{height}",
        "subset": "prepared-image",
        "pixel_format": "i400",
        "width": width,
        "height": height,
        "qindex": QINDEX,
        "source": source,
        "source_sha256": output["sha256"],
        "prepared_source": relative,
    })
    return cases, {
        "prepared_lock_sha256": sha256_bytes(PREPARED_LOCK.read_bytes()),
        "prepared_sources": sources,
    }


def plane_sse(result: dict[str, Any]) -> list[int]:
    return [int(plane["sse"]) for plane in result.get("planes", [])]


def selected_mode(result: dict[str, Any]) -> int:
    stderr = str(result["attempts"][-1]["stderr"])
    for field in ("intra_mode", "luma_mode"):
        marker = f"{field}="
        if marker in stderr:
            return int(stderr.split(marker, 1)[1].split()[0])
    raise RuntimeError("selected luma mode missing")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()
    for path in (args.encoder, args.avmdec, args.project_decoder):
        if not path.is_file():
            raise SystemExit(f"missing executable: {path}")

    cases, corpus = fixture_cases()
    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True)
    records = []
    try:
        for case in cases:
            case_root = args.work_dir / case["id"]
            baseline = encode(
                case, "directional-exact", 1, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                case_root / "directional-exact")
            intra = encode(
                case, "intra", 2, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                case_root / "intra")
            before = plane_sse(baseline)
            after = plane_sse(intra)
            gains = [left - right for left, right in zip(before, after)]
            before_cost = sum(before) * 256 + baseline.get("obu_bytes", 0) * 8
            after_cost = sum(after) * 256 + intra.get("obu_bytes", 0) * 8
            record = {
                key: value for key, value in case.items() if key != "source"
            }
            record.update({
                "directional_exact": baseline,
                "intra": intra,
                "selected_luma_mode": selected_mode(intra),
                "sse_gain": gains,
                "rd_cost_gain": before_cost - after_cost,
            })
            records.append(record)

        successful = [
            record for record in records
            if all(record[name].get("status") == "pass"
                   for name in ("directional_exact", "intra"))
        ]
        strict_wins = [
            record for record in successful if sum(record["sse_gain"]) > 0
        ]
        modes = {
            record["selected_luma_mode"] for record in successful
        }
        intra_times = [
            attempt["duration_ns"] for record in successful
            for attempt in record["intra"]["attempts"]
        ]
        baseline_times = [
            attempt["duration_ns"] for record in successful
            for attempt in record["directional_exact"]["attempts"]
        ]
        gates = {
            "all_formats_at_1440": {
                record["pixel_format"] for record in successful
                if record["width"] == 1440 and record["height"] == 1440
                and record["subset"] == "synthetic-center"
            } == set(FORMATS),
            "all_cases_validated": len(successful) == len(records),
            "local_avm_project_exact": all(
                record["intra"].get("local_sse_matches_decode", False)
                and record["intra"].get("avm_project_exact", False)
                for record in successful),
            "deterministic": all(
                record["intra"].get("deterministic", False)
                for record in successful),
            "no_plane_sse_regression": all(
                all(gain >= 0 for gain in record["sse_gain"])
                for record in successful),
            "rd_cost_nonregression": all(
                record["rd_cost_gain"] >= 0 for record in successful),
            "smooth_family_win": any(
                record["selected_luma_mode"] >= 5
                and sum(record["sse_gain"]) > 0
                for record in successful),
            "workspace_bounded": all(
                record["intra"].get("workspace_bytes", MAX_WORKSPACE_BYTES + 1)
                <= MAX_WORKSPACE_BYTES
                for record in successful),
            "runtime_ratio_at_most_4": sum(intra_times) <=
                4 * sum(baseline_times),
        }
        report = {
            "schema": "av2-encoder-smooth-paeth-full-gate-v1",
            "status": "pass" if all(gates.values()) else "blocked",
            "configuration": {
                "formats": list(FORMATS),
                "dimensions": [1440, 1440],
                "qindex": QINDEX,
                "candidate_order": [
                    "DC", "V", "H", "Smooth", "Smooth-V", "Smooth-H",
                    "Paeth",
                ],
                "chroma_mode": "DC",
            },
            "corpus": corpus,
            "gates": gates,
            "summary": {
                "successful_cases": len(successful),
                "strict_wins": len(strict_wins),
                "total_sse_gain": sum(
                    sum(record["sse_gain"]) for record in successful),
                "total_byte_gain": sum(
                    record["directional_exact"].get("obu_bytes", 0) -
                    record["intra"].get("obu_bytes", 0)
                    for record in successful),
                "selected_luma_modes": sorted(modes),
                "median_intra_time_ns":
                    int(statistics.median(intra_times)) if intra_times else 0,
                "worst_intra_time_ns": max(intra_times, default=0),
                "maximum_workspace_bytes": max(
                    (record["intra"].get("workspace_bytes", 0)
                     for record in successful), default=0),
            },
            "cases": records,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        print(json.dumps({
            "status": report["status"],
            **report["summary"],
        }, sort_keys=True))
        return 0 if report["status"] == "pass" else 1
    finally:
        shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

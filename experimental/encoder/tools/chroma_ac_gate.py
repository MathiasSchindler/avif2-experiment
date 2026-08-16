#!/usr/bin/env python3
"""Validate production chroma AC across I420 and I444."""

from __future__ import annotations

import argparse
import json
import shutil
import statistics
from pathlib import Path

from ac8x8_color_gate import (
    ENCODER,
    FORMATS,
    ROOT,
    encode,
    fixture_cases,
)


def plane_sse(result: dict[str, object]) -> list[int]:
    planes = result.get("planes", [])
    if planes:
        return [int(plane["sse"]) for plane in planes]
    return [int(value) for value in result.get("reported_sse", [])]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--encoder", type=Path,
        default=ENCODER / "build/av2-encoder-experiment")
    parser.add_argument(
        "--avmdec", type=Path,
        default=ROOT / "build/reference/avm/avmdec")
    parser.add_argument(
        "--project-decoder", type=Path,
        default=ROOT / "build/host-cmake/av2heif-decoder")
    parser.add_argument(
        "--output", type=Path,
        default=ENCODER / "metrics/chroma-ac-gate.json")
    parser.add_argument(
        "--work-dir", type=Path,
        default=ENCODER / "results/chroma-ac-gate")
    args = parser.parse_args()
    cases, corpus = fixture_cases()
    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True)
    records = []
    try:
        for case in cases:
            root = args.work_dir / case["id"]
            off = encode(
                case, "off", 1, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                root / "off")
            bounded = encode(
                case, "bounded", 2, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                root / "bounded")
            default = encode(
                case, None, 1, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                root / "default")
            baseline = plane_sse(off)
            candidate = plane_sse(bounded)
            gains = [
                before - after
                for before, after in zip(baseline, candidate)
            ]
            records.append({
                key: case[key] for key in (
                    "id", "pixel_format", "width", "height", "qindex",
                    "texture", "chroma", "source_sha256")
            } | {
                "off": off,
                "bounded": bounded,
                "default": default,
                "baseline_sse": baseline,
                "candidate_sse": candidate,
                "sse_gain": gains,
                "default_matches_bounded":
                    default.get("obu_sha256") == bounded.get("obu_sha256"),
            })
        successful = [
            record for record in records
            if record["bounded"]["status"] == "pass"
        ]
        color = [
            record for record in successful
            if record["pixel_format"] != "i400"
        ]
        comparable_chroma = [
            record for record in color
            if len(record["baseline_sse"]) == 3
            and sum(record["baseline_sse"][1:]) > 0
        ]
        times = [
            attempt["duration_ns"]
            for record in successful
            for attempt in record["bounded"]["attempts"]
        ]
        gates = {
            "all_formats_reachable": all(
                any(record["pixel_format"] == pixel_format
                    and record["bounded"]["status"] == "pass"
                    for record in records)
                for pixel_format in FORMATS),
            "local_avm_project_exact": len(successful) == len(records)
                and all(
                    record["bounded"].get("avm_project_exact", False)
                    and record["bounded"].get(
                        "local_sse_matches_decode", False)
                    for record in successful),
            "deterministic": len(successful) == len(records)
                and all(record["bounded"].get("deterministic", False)
                        for record in successful),
            "default_is_bounded": all(
                record["default_matches_bounded"] for record in records),
            "no_plane_sse_regression": all(
                all(gain >= 0 for gain in record["sse_gain"])
                for record in records),
            "chroma_wins": len(comparable_chroma) >= 4
                and all(sum(record["sse_gain"][1:]) > 0
                        for record in comparable_chroma),
            "bounded_workspace": max(
                record["bounded"].get("workspace_bytes", 0)
                for record in successful) <= 16 * 1024 * 1024,
        }
        report = {
            "schema": "av2-encoder-chroma-ac-gate-v1",
            "status": "pass" if all(gates.values()) else "blocked",
            "configuration": {
                "case_count": len(records),
                "formats": list(FORMATS),
                "transform_sizes": [
                    "4x4", "4x8", "8x4", "8x8",
                    "8x16", "16x8", "16x16",
                ],
                "search": "bounded",
                "chroma_tools": {
                    "cctx": False,
                    "qmatrix": False,
                    "tcq": False,
                    "parity_hiding": False,
                },
            },
            "corpus": corpus,
            "gates": gates,
            "summary": {
                "bounded_successes": len(successful),
                "chroma_comparable_cases": len(comparable_chroma),
                "chroma_wins": sum(
                    sum(record["sse_gain"][1:]) > 0
                    for record in comparable_chroma),
                "chroma_sse_gain": sum(
                    sum(record["sse_gain"][1:]) for record in color),
                "total_byte_cost": sum(
                    record["bounded"].get("obu_bytes", 0) -
                    record["off"].get("obu_bytes", 0)
                    for record in records),
                "max_workspace_bytes": max(
                    record["bounded"].get("workspace_bytes", 0)
                    for record in successful),
                "median_bounded_time_ns":
                    int(statistics.median(times)) if times else 0,
                "worst_bounded_time_ns": max(times, default=0),
            },
            "cases": records,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(json.dumps({
            "status": report["status"],
            **report["summary"],
        }, sort_keys=True))
        return 0 if report["status"] == "pass" else 1
    finally:
        shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

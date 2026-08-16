#!/usr/bin/env python3
"""Measure the bounded I400 AC surface against the locked baseline."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
sys.path.insert(0, str(ENCODER / "tools"))
from encoder_quality_baseline import PATTERNS, QINDICES, generated_input


def main() -> int:
    binary = ENCODER / "build/av2-encoder-experiment"
    baseline_path = ENCODER / "metrics/encoder-quality-baseline.json"
    output = ENCODER / "metrics/ac8x8-production.json"
    work = ENCODER / "build/ac8x8-production-report"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    baseline = json.loads(baseline_path.read_text())
    locked = {
        case["id"]: case for case in baseline["cases"]
        if case["subset"] == "targeted" and case["pixel_format"] == "i400"
    }
    metric = re.compile(
        r"bytes=(\d+) sse_y=(\d+).*workspace_bytes=(\d+) qindex=(\d+)")
    records = []
    for pattern in PATTERNS:
        source = generated_input(pattern, "i400")
        raw = work / f"{pattern}-64x64.i400"
        raw.write_bytes(source)
        for qindex in QINDICES:
            obu = work / f"{pattern}-q{qindex}.obu"
            start = time.monotonic_ns()
            completed = subprocess.run([
                str(binary), str(raw), "--width", "64", "--height", "64",
                "--qindex", str(qindex), "--search", "bounded",
                "--output", str(obu),
            ], check=True, text=True, capture_output=True)
            elapsed = time.monotonic_ns() - start
            match = metric.search(completed.stderr)
            if not match:
                raise RuntimeError("encoder metric missing")
            case_id = f"target-{pattern}-i400-q{qindex}"
            old = locked[case_id]
            old_sse = old["planes"][0]["sse"]
            new_sse = int(match.group(2))
            records.append({
                "id": case_id,
                "baseline_bytes": old["obu_bytes"],
                "candidate_bytes": int(match.group(1)),
                "baseline_sse_y": old_sse,
                "candidate_sse_y": new_sse,
                "sse_gain": old_sse - new_sse,
                "improved": new_sse < old_sse,
                "workspace_bytes": int(match.group(3)),
                "encode_time_ns": elapsed,
            })
    subset = [
        record for record in records
        if any(name in record["id"] for name in
               ("checkerboard-4", "horizontal-lines-2", "vertical-lines-2"))
    ]
    baseline_subset_sse = sum(item["baseline_sse_y"] for item in subset)
    candidate_subset_sse = sum(item["candidate_sse_y"] for item in subset)
    result = {
        "schema": "av2-ac8x8-production-report-v1",
        "surface": "explicit --search bounded, I400 only",
        "cases": records,
        "summary": {
            "case_count": len(records),
            "wins": sum(item["improved"] for item in records),
            "baseline_bytes": sum(item["baseline_bytes"] for item in records),
            "candidate_bytes": sum(item["candidate_bytes"] for item in records),
            "baseline_sse_y": sum(item["baseline_sse_y"] for item in records),
            "candidate_sse_y": sum(item["candidate_sse_y"] for item in records),
            "ac_subset_cases": len(subset),
            "ac_subset_wins": sum(item["improved"] for item in subset),
            "ac_subset_gain_percent":
                100.0 * (baseline_subset_sse - candidate_subset_sse)
                / baseline_subset_sse,
            "byte_gate_pass": sum(item["candidate_bytes"] for item in records)
                <= sum(item["baseline_bytes"] for item in records),
            "gain_gate_pass": candidate_subset_sse * 100
                <= baseline_subset_sse * 92,
            "minimum_wins_pass": sum(item["improved"] for item in subset) >= 9,
            "max_workspace_bytes": max(item["workspace_bytes"] for item in records),
            "worst_encode_time_ns": max(item["encode_time_ns"] for item in records),
        },
    }
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    shutil.rmtree(work)
    print(json.dumps(result["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Select whole-image AC frontiers under the locked I400 byte budget."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
sys.path.insert(0, str(ENCODER / "tools"))
from encoder_quality_baseline import PATTERNS, QINDICES, generated_input


AC_PATTERNS = {
    "checkerboard-4", "horizontal-lines-2", "vertical-lines-2"
}


def build_driver() -> Path:
    output = ENCODER / "build/ac8x8-hardbudget-driver"
    sources = [
        "src/memory.c", "src/writer.c", "src/range.c", "src/cdf.c",
        "src/obu.c", "src/syntax_cdf.c", "src/syntax.c", "src/m1.c",
        "src/m3.c", "src/m4.c",
    ]
    command = [
        "cc", "-I", str(ENCODER / "include"), "-std=c11", "-Wall",
        "-Wextra", "-Wpedantic", "-Werror", "-O2", "-fno-builtin",
        "-o", str(output), str(ENCODER / "tests/ac8x8_hardbudget_driver.c"),
        *(str(ENCODER / source) for source in sources),
    ]
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(command, check=True)
    return output


def pareto(states: list[dict]) -> list[dict]:
    result = []
    for state in sorted(states, key=lambda item: (item["bytes"], item["sse"])):
        if not result or state["sse"] < result[-1]["sse"]:
            result.append(state)
    return result


def stable_report(value):
    if isinstance(value, dict):
        return {
            key: stable_report(item) for key, item in value.items()
            if not key.endswith("runtime_ns")
        }
    if isinstance(value, list):
        return [stable_report(item) for item in value]
    return value


def choose(cases: list[dict], budget: int) -> tuple[list[dict], dict]:
    dp = {(0, 0): (0, [])}
    for case in cases:
        next_dp = {}
        for (used, wins), (sse, choices) in dp.items():
            for state in case["frontier"]:
                total = used + state["bytes"]
                if total > budget:
                    continue
                win = int(case["ac_subset"] and
                          state["sse"] < case["baseline_sse_y"])
                key = (total, wins + win)
                value = (sse + state["sse"], choices + [state])
                old = next_dp.get(key)
                if old is None or value[0] < old[0]:
                    next_dp[key] = value
        dp = next_dp
    viable = [
        (sse, used, -wins, choices)
        for (used, wins), (sse, choices) in dp.items()
    ]
    sse, used, negative_wins, choices = min(viable)
    return choices, {
        "candidate_bytes": used,
        "candidate_sse_y": sse,
        "ac_subset_wins": -negative_wins,
    }


def subset_reachability(cases: list[dict]) -> dict:
    subset = [case for case in cases if case["ac_subset"]]
    baseline_sse = sum(case["baseline_sse_y"] for case in subset)
    baseline_bytes = sum(case["baseline_bytes"] for case in subset)
    dp = {(0, 0): 0}
    for case in subset:
        next_dp = {}
        for (used, wins), sse in dp.items():
            for state in case["frontier"]:
                win = int(state["sse"] < case["baseline_sse_y"])
                key = (used + state["bytes"], wins + win)
                value = sse + state["sse"]
                if value < next_dp.get(key, sys.maxsize):
                    next_dp[key] = value
        dp = next_dp
    gain_states = [
        (used, wins, sse) for (used, wins), sse in dp.items()
        if sse * 100 <= baseline_sse * 92
    ]
    max_wins = max(wins for _, wins, _ in gain_states)
    best = min(
        (used, sse) for used, wins, sse in gain_states
        if wins == max_wins)
    unavailable = [
        case["id"] for case in subset
        if not any(state["sse"] < case["baseline_sse_y"]
                   for state in case["frontier"])
    ]
    return {
        "maximum_reachable_wins_at_8_percent_gain": max_wins,
        "minimum_subset_bytes_at_maximum_wins": best[0],
        "minimum_subset_sse_at_that_rate": best[1],
        "locked_subset_bytes": baseline_bytes,
        "additional_subset_bytes_required": best[0] - baseline_bytes,
        "cases_without_any_winning_candidate": unavailable,
    }


def build_report() -> dict:
    driver = build_driver()
    baseline = json.loads(
        (ENCODER / "metrics/encoder-quality-baseline.json").read_text())
    locked = {
        case["id"]: case for case in baseline["cases"]
        if case["subset"] == "targeted" and case["pixel_format"] == "i400"
    }
    work = ENCODER / "build/ac8x8-hardbudget-model"
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    cases = []
    started = time.monotonic_ns()
    for pattern in PATTERNS:
        raw = work / f"{pattern}.i400"
        raw.write_bytes(generated_input(pattern, "i400"))
        for qindex in QINDICES:
            case_id = f"target-{pattern}-i400-q{qindex}"
            old = locked[case_id]
            begin = time.monotonic_ns()
            completed = subprocess.run(
                [str(driver), str(raw), "64", "64", str(qindex)],
                check=True, text=True, capture_output=True)
            states = [json.loads(line) for line in completed.stdout.splitlines()]
            baseline_state = {
                "step": -1,
                "bytes": old["obu_bytes"],
                "sse": old["planes"][0]["sse"],
                "source": "locked-dc",
            }
            for state in states:
                state["source"] = "exact-greedy"
            frontier = pareto([baseline_state, *states])
            cases.append({
                "id": case_id,
                "pattern": pattern,
                "qindex": qindex,
                "ac_subset": pattern in AC_PATTERNS,
                "baseline_bytes": baseline_state["bytes"],
                "baseline_sse_y": baseline_state["sse"],
                "frontier": frontier,
                "evaluated_states": len(states),
                "runtime_ns": time.monotonic_ns() - begin,
            })
    budget = sum(case["baseline_bytes"] for case in cases)
    choices, summary = choose(cases, budget)
    for case, choice in zip(cases, choices):
        case["selected"] = choice
    subset = [case for case in cases if case["ac_subset"]]
    baseline_subset_sse = sum(case["baseline_sse_y"] for case in subset)
    candidate_subset_sse = sum(case["selected"]["sse"] for case in subset)
    baseline_total_sse = sum(case["baseline_sse_y"] for case in cases)
    summary.update({
        "baseline_bytes": budget,
        "baseline_sse_y": baseline_total_sse,
        "byte_regression": summary["candidate_bytes"] - budget,
        "sse_regression": summary["candidate_sse_y"] - baseline_total_sse,
        "ac_subset_cases": len(subset),
        "baseline_ac_subset_sse_y": baseline_subset_sse,
        "candidate_ac_subset_sse_y": candidate_subset_sse,
        "ac_subset_gain_percent":
            100.0 * (baseline_subset_sse - candidate_subset_sse)
            / baseline_subset_sse,
        "minimum_wins": 9,
        "byte_gate_pass": summary["candidate_bytes"] <= budget,
        "gain_gate_pass": candidate_subset_sse * 100
            <= baseline_subset_sse * 92,
        "minimum_wins_pass": summary["ac_subset_wins"] >= 9,
        "total_runtime_ns": time.monotonic_ns() - started,
        "worst_case_runtime_ns": max(case["runtime_ns"] for case in cases),
        "evaluated_frontier_states":
            sum(case["evaluated_states"] for case in cases),
    })
    summary["status"] = (
        "done" if summary["byte_gate_pass"] and summary["gain_gate_pass"]
        and summary["minimum_wins_pass"] and summary["sse_regression"] <= 0
        else "blocked"
    )
    reachability = subset_reachability(cases)
    summary.update(reachability)
    summary["minimum_aggregate_bytes_for_reachable_frontier"] = (
        budget + reachability["additional_subset_bytes_required"])
    summary["selected_stream_parity"] = (
        "locked local/pinned-AVM/project baseline evidence")
    summary["per_case_dc_budget_wins"] = sum(
        any(state["bytes"] <= case["baseline_bytes"] and
            state["sse"] < case["baseline_sse_y"]
            for state in case["frontier"])
        for case in cases if case["ac_subset"])
    shutil.rmtree(work)
    return {
        "schema": "av2-ac8x8-hardbudget-model-v1",
        "algorithm": (
            "deterministic exact-stream greedy position-prefix frontier per "
            "block, followed by aggregate multiple-choice knapsack"
        ),
        "constraints": {
            "aggregate_bytes": budget,
            "maximum_frontier_steps_per_case": 32,
            "whole_image_candidate_blocks": 16,
            "maximum_positions_per_block": 16,
            "maximum_nonzeros_per_block": 8,
            "maximum_absolute_level": 63,
        },
        "cases": cases,
        "summary": summary,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = ENCODER / "metrics/ac8x8-hardbudget.json"
    report = build_report()
    text = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.check:
        frozen = json.loads(output.read_text()) if output.is_file() else None
        if frozen is None or stable_report(frozen) != stable_report(report):
            print(f"stale: {output}", file=sys.stderr)
            return 1
    else:
        output.write_text(text)
    print(json.dumps(report["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

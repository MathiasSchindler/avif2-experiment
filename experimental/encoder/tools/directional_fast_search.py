#!/usr/bin/env python3
"""Evaluate a bounded prefilter for the production DC/V/H stream search."""

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
ENCODER_ROOT = ROOT / "experimental" / "encoder"
BASELINE_PATH = ENCODER_ROOT / "metrics" / "encoder-quality-baseline.json"
DEFAULT_OUTPUT = ENCODER_ROOT / "metrics" / "directional-fast-search.json"
DEFAULT_WORK = ENCODER_ROOT / "build" / "directional-fast-search"
DEFAULT_ENCODER = ENCODER_ROOT / "build" / "av2-encoder-experiment"
BASELINE_TOOL = ENCODER_ROOT / "tools" / "encoder_quality_baseline.py"
MODEL_VECTORS = ENCODER_ROOT / "tests" / "directional_intra_vectors.json"
PREFILTER_RATIO_Q12 = 3686  # 0.8999
SMALL_IMAGE_PIXELS = 64 * 64
RUNTIME_GATE_NS = 600_000_000_000


def load_baseline_tool():
    spec = importlib.util.spec_from_file_location(
        "encoder_quality_baseline_for_directional", BASELINE_TOOL
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load baseline tool")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def split_luma(data: bytes, width: int, height: int) -> bytes:
    size = width * height
    if len(data) < size:
        raise RuntimeError("input is shorter than its luma plane")
    return data[:size]


def _prediction(
    mode: str, above: list[int] | None, left: list[int] | None, x: int, y: int
) -> int:
    edges = (above or []) + (left or [])
    dc = 128 if not edges else (sum(edges) + len(edges) // 2) // len(edges)
    if mode == "vertical" and above is not None:
        return above[x]
    if mode == "horizontal" and left is not None:
        return left[y]
    return dc


def predictor_sse(source: bytes, width: int, height: int, mode: str) -> int:
    """Source-edge SSE with the best leaf-local constant residual."""
    total = 0
    for block_y in range(0, height, 8):
        for block_x in range(0, width, 8):
            above = (
                [source[(block_y - 1) * width + min(block_x + x, width - 1)]
                 for x in range(8)]
                if block_y else None
            )
            left = (
                [source[min(block_y + y, height - 1) * width + block_x - 1]
                 for y in range(8)]
                if block_x else None
            )
            samples: list[int] = []
            predictions: list[int] = []
            for y in range(8):
                for x in range(8):
                    source_x = min(block_x + x, width - 1)
                    source_y = min(block_y + y, height - 1)
                    samples.append(source[source_y * width + source_x])
                    predictions.append(_prediction(mode, above, left, x, y))
            residual_sum = sum(
                sample - prediction
                for sample, prediction in zip(samples, predictions)
            )
            residual = (
                (residual_sum + 32) // 64
                if residual_sum >= 0 else -((-residual_sum + 32) // 64)
            )
            total += sum(
                (sample - max(0, min(255, prediction + residual))) ** 2
                for sample, prediction in zip(samples, predictions)
            )
    return total


def prefilter(source: bytes, width: int, height: int) -> dict[str, object]:
    scores = {
        mode: predictor_sse(source, width, height, mode)
        for mode in ("dc", "vertical", "horizontal")
    }
    directional = min(scores["vertical"], scores["horizontal"])
    dc = scores["dc"]
    ratio_q12 = 4096 if dc == 0 else directional * 4096 // dc
    small = width * height <= SMALL_IMAGE_PIXELS
    evaluate = small or ratio_q12 <= PREFILTER_RATIO_Q12
    return {
        "scores": scores,
        "directional_to_dc_q12": ratio_q12,
        "small_image": small,
        "evaluate_exact": evaluate,
        "early_pruned": not evaluate,
    }


METRIC = re.compile(
    rb"encoder: bytes=(\d+) sse_y=(\d+)(?: sse_u=(\d+) sse_v=(\d+))?"
)


def run_encoder(
    encoder: Path, case: dict[str, object], qindex: int, search: str, output: Path
) -> dict[str, object]:
    command = [
        str(encoder), str(case["path"]), "--width", str(case["width"]),
        "--height", str(case["height"]), "--qindex", str(qindex),
        "--search", search, "--output", str(output),
    ]
    start = time.monotonic_ns()
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    duration = time.monotonic_ns() - start
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.decode("utf-8", "replace"))
    match = METRIC.search(completed.stderr)
    if match is None:
        raise RuntimeError("encoder metrics missing")
    sse = [int(match.group(2))]
    if match.group(3) is not None:
        sse.extend((int(match.group(3)), int(match.group(4))))
    data = output.read_bytes()
    return {
        "obu_bytes": int(match.group(1)),
        "sse": sse,
        "obu_sha256": hashlib.sha256(data).hexdigest(),
        "encode_time_ns": duration,
    }


def model_pruning_agreement() -> dict[str, int]:
    vectors = json.loads(MODEL_VECTORS.read_text(encoding="utf-8"))
    agreement = 0
    pruned = 0
    evaluated = 0
    for case in vectors["cases"]:
        ordered = sorted(
            case["candidates"],
            key=lambda item: ("dc", "vertical", "horizontal").index(item["mode"]),
        )
        best = None
        for candidate in ordered:
            lower_bound = int(candidate["distortion"]) << 8
            if best is not None and lower_bound > int(best["cost_q8"]):
                pruned += 1
                continue
            evaluated += 1
            if best is None or (
                int(candidate["cost_q8"]), int(candidate["distortion"]),
                int(candidate["rate_tell_bits"]),
                ("dc", "vertical", "horizontal").index(candidate["mode"]),
            ) < (
                int(best["cost_q8"]), int(best["distortion"]),
                int(best["rate_tell_bits"]),
                ("dc", "vertical", "horizontal").index(best["mode"]),
            ):
                best = candidate
        if best["mode"] == case["candidates"][0]["mode"]:
            agreement += 1
    return {
        "cases": len(vectors["cases"]),
        "agreement": agreement,
        "evaluated_candidates": evaluated,
        "early_pruned_candidates": pruned,
    }


def evaluate(
    encoder: Path, work: Path, rerun_shortlist: bool = False
) -> dict[str, object]:
    baseline = json.loads(BASELINE_PATH.read_text(encoding="utf-8"))
    baseline_cases = {case["id"]: case for case in baseline["cases"]}
    baseline_tool = load_baseline_tool()
    source_cases = baseline_tool.all_cases(work)
    oracle_dir = work / "oracle"
    off_dir = work / "off"
    oracle_dir.mkdir(parents=True, exist_ok=True)
    off_dir.mkdir(parents=True, exist_ok=True)
    records = []
    exhaustive_start = time.monotonic_ns()
    for case in source_cases:
        source = Path(case["path"]).read_bytes()
        feature = prefilter(
            split_luma(source, int(case["width"]), int(case["height"])),
            int(case["width"]), int(case["height"]),
        )
        for qindex in baseline_tool.QINDICES:
            case_id = f"{case['id']}-q{qindex}"
            locked = baseline_cases[case_id]
            before = run_encoder(
                encoder, case, qindex, "off", off_dir / f"{case_id}.obu"
            )
            production = run_encoder(
                encoder, case, qindex, "directional",
                oracle_dir / f"{case_id}-production.obu"
            )
            exact = production
            if not feature["evaluate_exact"]:
                exact = run_encoder(
                    encoder, case, qindex, "directional-exact",
                    oracle_dir / f"{case_id}-exact.obu"
                )
            if feature["evaluate_exact"] and rerun_shortlist:
                rerun = run_encoder(
                    encoder, case, qindex, "directional",
                    oracle_dir / f"{case_id}-rerun.obu"
                )
                production["deterministic_rerun"] = (
                    production["obu_sha256"] == rerun["obu_sha256"]
                    and production["obu_bytes"] == rerun["obu_bytes"]
                    and production["sse"] == rerun["sse"]
                )
            else:
                production["deterministic_rerun"] = None
            exact["deterministic_rerun"] = production["deterministic_rerun"]
            before_sse = before["sse"]
            exact_admissible = (
                exact["obu_bytes"] <= before["obu_bytes"]
                and all(a <= b for a, b in zip(exact["sse"], before_sse))
            )
            exact_changed = (
                exact["obu_sha256"] != before["obu_sha256"]
                and (exact["obu_bytes"] != before["obu_bytes"]
                     or exact["sse"] != before_sse)
            )
            policy_admissible = (
                production["obu_bytes"] <= before["obu_bytes"]
                and all(a <= b for a, b in zip(production["sse"], before_sse))
            )
            policy_changed = (
                production["obu_sha256"] != before["obu_sha256"]
                and (production["obu_bytes"] != before["obu_bytes"]
                     or production["sse"] != before_sse)
            )
            oracle_commit = exact_admissible and exact_changed
            policy_commit = policy_admissible and policy_changed
            records.append({
                "id": case_id,
                "subset": locked["subset"],
                "pixel_format": locked["pixel_format"],
                "qindex": qindex,
                "prefilter": feature,
                "baseline": {
                    "obu_bytes": before["obu_bytes"],
                    "sse": before_sse,
                    "encode_time_ns": before["encode_time_ns"],
                    "obu_sha256": before["obu_sha256"],
                    "locked_obu_bytes": locked["obu_bytes"],
                    "locked_sse": [
                        int(plane["sse"]) for plane in locked["planes"]
                    ],
                },
                "production": production,
                "exhaustive": exact,
                "admissible": exact_admissible,
                "exhaustive_changed": exact_changed,
                "oracle_commit": oracle_commit,
                "policy_commit": policy_commit,
                "decision_agrees": policy_commit == oracle_commit,
                "search_off_locked": (
                    before["obu_bytes"] == locked["obu_bytes"]
                    and before["obu_sha256"] == locked["obu_sha256"]
                ),
            })
    exhaustive_wall = time.monotonic_ns() - exhaustive_start
    shortlisted = [record for record in records
                   if record["prefilter"]["evaluate_exact"]]
    committed = [record for record in records if record["policy_commit"]]
    projected_policy_ns = sum(
        int(record["production"]["encode_time_ns"])
        for record in records
    )

    def totals(items: list[dict[str, object]], policy: bool) -> tuple[int, list[int]]:
        byte_total = 0
        sse_total = [0, 0, 0]
        for record in items:
            chosen = (
                record["production"]
                if policy and record["policy_commit"] else record["baseline"]
            )
            byte_total += int(chosen["obu_bytes"])
            for index, value in enumerate(chosen["sse"]):
                sse_total[index] += int(value)
        return byte_total, sse_total

    target = [record for record in records if record["subset"] == "targeted"]
    directional_subset = [
        record for record in target
        if record["pixel_format"] == "i400"
        and any(name in record["id"] for name in (
            "horizontal-edge", "vertical-edge", "diagonal-edge"
        ))
    ]
    all_before = totals(records, False)
    all_after = totals(records, True)
    target_before = totals(target, False)
    target_after = totals(target, True)
    directional_before = totals(directional_subset, False)
    directional_after = totals(directional_subset, True)
    model = model_pruning_agreement()
    result = {
        "schema": "av2-directional-fast-search-v1",
        "generated_by": "experimental/encoder/tools/directional_fast_search.py",
        "configuration": {
            "prefilter_ratio_q12": PREFILTER_RATIO_Q12,
            "small_image_pixels": SMALL_IMAGE_PIXELS,
            "runtime_gate_ns": RUNTIME_GATE_NS,
            "cases": len(records),
            "encoder_sha256": hashlib.sha256(encoder.read_bytes()).hexdigest(),
            "locked_baseline_sha256": hashlib.sha256(
                BASELINE_PATH.read_bytes()
            ).hexdigest(),
        },
        "algorithm": {
            "prefilter": "source-edge DC/V/H SSE after best local constant residual",
            "exact_stage": "production full-stream DC/V/H result for shortlisted pictures",
            "transaction": "each trial rebuilds writer/CDF state; only the exact admissible winner is re-encoded and committed",
            "pruning": "always evaluate <=64x64; otherwise prune above the locked Q12 ratio",
            "lower_bound": "candidate distortion * 256 safely prunes exact rate comparison when it already exceeds the incumbent",
        },
        "summary": {
            "decision_agreement_count": sum(
                bool(record["decision_agrees"]) for record in records
            ),
            "decision_agreement_total": len(records),
            "decision_agreement_percent": round(
                100 * sum(bool(record["decision_agrees"]) for record in records)
                / len(records), 3
            ),
            "shortlisted": len(shortlisted),
            "early_pruned": len(records) - len(shortlisted),
            "committed_wins": len(committed),
            "safe_decisions": sum(
                bool(record["decision_agrees"]) for record in records
            ),
            "search_off_locked": all(
                bool(record["search_off_locked"]) for record in records
            ),
            "exhaustive_changed_outputs": sum(
                bool(record["exhaustive_changed"]) for record in records
            ),
            "rejected_regressing_outputs": sum(
                bool(record["exhaustive_changed"]) and not record["admissible"]
                for record in records
            ),
            "exhaustive_wall_time_ns": exhaustive_wall,
            "projected_policy_time_ns": projected_policy_ns,
            "within_runtime_gate": projected_policy_ns < RUNTIME_GATE_NS,
            "all_bytes_nonregressing": all_after[0] <= all_before[0],
            "all_plane_sse_nonregressing": all(
                after <= before
                for before, after in zip(all_before[1], all_after[1])
            ),
            "total_before": {"bytes": all_before[0], "sse": all_before[1]},
            "total_after": {"bytes": all_after[0], "sse": all_after[1]},
            "targeted_before": {
                "bytes": target_before[0], "sse": target_before[1]
            },
            "targeted_after": {
                "bytes": target_after[0], "sse": target_after[1]
            },
            "targeted_byte_gain_percent": round(
                100 * (target_before[0] - target_after[0]) / target_before[0], 3
            ),
            "directional_subset_cases": len(directional_subset),
            "directional_subset_wins": sum(
                bool(record["policy_commit"]) for record in directional_subset
            ),
            "directional_subset_before": {
                "bytes": directional_before[0], "sse": directional_before[1]
            },
            "directional_subset_after": {
                "bytes": directional_after[0], "sse": directional_after[1]
            },
            "directional_subset_byte_gain_percent": round(
                100 * (directional_before[0] - directional_after[0])
                / directional_before[0], 3
            ),
            "model_140_case_agreement": model,
        },
        "determinism": {
            "baseline_reruns_locked": baseline["summary"]["all_deterministic"],
            "shortlisted_reruns_requested": rerun_shortlist,
            "all_shortlisted_deterministic": (
                all(record["production"]["deterministic_rerun"]
                    for record in shortlisted)
                if rerun_shortlist else None
            ),
        },
        "cases": records,
    }
    return result


def check_report(report: dict[str, object]) -> None:
    summary = report["summary"]
    if report["schema"] != "av2-directional-fast-search-v1":
        raise RuntimeError("unexpected report schema")
    if summary["decision_agreement_total"] != 90:
        raise RuntimeError("report does not cover the locked 90 cases")
    if summary["safe_decisions"] != 90:
        raise RuntimeError("production policy does not make 90/90 safe decisions")
    if not summary["search_off_locked"]:
        raise RuntimeError("search-off output differs from the locked baseline")
    if not summary["within_runtime_gate"]:
        raise RuntimeError("projected policy misses the runtime gate")
    if not summary["all_bytes_nonregressing"]:
        raise RuntimeError("policy regresses aggregate bytes")
    if not summary["all_plane_sse_nonregressing"]:
        raise RuntimeError("policy regresses aggregate plane SSE")
    if summary["directional_subset_byte_gain_percent"] < 10:
        raise RuntimeError("policy retains less than 10% directional subset gain")
    if summary["directional_subset_wins"] < 3:
        raise RuntimeError("policy retains too few directional subset wins")
    model = summary["model_140_case_agreement"]
    if model["agreement"] != model["cases"] or model["cases"] != 140:
        raise RuntimeError("bounded model disagrees with exhaustive model")
    determinism = report["determinism"]
    if not determinism["baseline_reruns_locked"] or not determinism[
        "all_shortlisted_deterministic"
    ]:
        raise RuntimeError("determinism evidence is incomplete")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--rerun-shortlist", action=argparse.BooleanOptionalAction, default=True
    )
    args = parser.parse_args()
    if args.check:
        check_report(json.loads(args.output.read_text(encoding="utf-8")))
        print(f"directional fast-search report OK: {args.output}")
        return 0
    result = evaluate(
        args.encoder.resolve(), args.work_dir.resolve(), args.rerun_shortlist
    )
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    check_report(result)
    print(json.dumps(result["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

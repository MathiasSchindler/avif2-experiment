#!/usr/bin/env python3
"""Evaluate bounded AC qindex trials against the locked DC-only byte budget."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER_ROOT = ROOT / "experimental" / "encoder"
sys.path.insert(0, str(ENCODER_ROOT / "tools"))
from encoder_quality_baseline import (  # noqa: E402
    QINDICES,
    all_cases,
    split_planes,
)

DEFAULT_ENCODER = ENCODER_ROOT / "build" / "av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_PROJECT = ROOT / "build" / "host-cmake" / "av2heif-decoder"
DEFAULT_BASELINE = ENCODER_ROOT / "metrics" / "encoder-quality-baseline.json"
DEFAULT_OUTPUT = ENCODER_ROOT / "metrics" / "ac8x8-equalsize-qtrial.json"
DEFAULT_WORK = ENCODER_ROOT / "build" / "ac8x8-equalsize-qtrial"

# Dense near the requested value, then geometrically wider. Including 195
# guarantees that every requested baseline point reaches qindex 255.
Q_OFFSETS = (0, 1, 2, 3, 4, 5, 8, 12, 16, 24, 32, 48, 64, 96, 128, 195)
AC_PATTERNS = ("checkerboard-4", "horizontal-lines-2", "vertical-lines-2")
METRIC = re.compile(rb"bytes=(\d+) sse_y=(\d+).*qindex=(\d+)")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def qtrials(requested: int) -> list[int]:
    return sorted({min(255, requested + offset) for offset in Q_OFFSETS})


def run(command: list[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    start = time.monotonic_ns()
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    return completed, time.monotonic_ns() - start


def decode(
    obu: Path, case: dict[str, object], source: bytes, avmdec: Path,
    project: Path, prefix: Path,
) -> tuple[int, str, dict[str, int]]:
    avm = prefix.with_suffix(".avm.i400")
    own = prefix.with_suffix(".project.i400")
    times = {}
    commands = (
        ("avm", [
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(obu),
        ]),
        ("project", [str(project), str(obu), "-o", str(own)]),
    )
    for name, command in commands:
        completed, elapsed = run(command)
        if completed.returncode != 0:
            raise RuntimeError(f"{name} decoder rejected {obu.name}")
        times[name] = elapsed
    reconstructed = avm.read_bytes()
    if reconstructed != own.read_bytes():
        raise RuntimeError(f"decoder mismatch for {obu.name}")
    planes = split_planes(
        reconstructed, str(case["pixel_format"]),
        int(case["width"]), int(case["height"]),
    )
    if len(planes) != 1:
        raise RuntimeError("equal-size trial unexpectedly reached chroma")
    sse = sum((a - b) ** 2 for a, b in zip(source, reconstructed))
    return sse, sha256(reconstructed), times


def encode(
    binary: Path, source_path: Path, case: dict[str, object], qindex: int,
    search: str, output: Path,
) -> tuple[int, int, int]:
    completed, elapsed = run([
        str(binary), str(source_path), "--width", str(case["width"]),
        "--height", str(case["height"]), "--qindex", str(qindex),
        "--search", search, "--output", str(output),
    ])
    if completed.returncode != 0:
        raise RuntimeError(
            f"encoder failed for {case['id']} q{qindex}: "
            f"{completed.stderr.decode(errors='replace')}"
        )
    match = METRIC.search(completed.stderr)
    if not match:
        raise RuntimeError(f"encoder metrics missing for {case['id']} q{qindex}")
    byte_count, reported_sse, reported_q = map(int, match.groups())
    if byte_count != output.stat().st_size or reported_q != qindex:
        raise RuntimeError("encoder metric/stream mismatch")
    return reported_sse, elapsed, byte_count


def psnr(sse: int, samples: int) -> str:
    if sse == 0:
        return "inf"
    return f"{10 * math.log10(255 * 255 * samples / sse):.6f}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()
    for path in (args.encoder, args.avmdec, args.project_decoder):
        if not path.is_file():
            raise SystemExit(f"missing executable: {path}")

    baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
    locked = {
        item["id"]: item for item in baseline["cases"]
        if item["pixel_format"] == "i400"
    }
    shutil.rmtree(args.work_dir, ignore_errors=True)
    cases = [
        case for case in all_cases(args.work_dir)
        if case["pixel_format"] == "i400"
    ]
    records = []
    start = time.monotonic_ns()
    for case in cases:
        source_path = Path(case["path"])
        source = source_path.read_bytes()
        for requested in QINDICES:
            case_id = f"{case['id']}-q{requested}"
            old = locked[case_id]
            case_dir = args.work_dir / "runs" / case_id
            case_dir.mkdir(parents=True, exist_ok=True)

            fallback_obu = case_dir / "fallback.obu"
            fallback_reported_sse, fallback_time, fallback_bytes = encode(
                args.encoder.resolve(), source_path, case, requested, "off",
                fallback_obu,
            )
            fallback_sse, fallback_decoded_hash, fallback_decode_times = decode(
                fallback_obu, case, source, args.avmdec.resolve(),
                args.project_decoder.resolve(), case_dir / "fallback",
            )
            if (
                fallback_bytes != old["obu_bytes"]
                or sha256(fallback_obu.read_bytes()) != old["obu_sha256"]
                or fallback_decoded_hash != old["decoded_sha256"]
                or fallback_sse != old["planes"][0]["sse"]
                or fallback_reported_sse != fallback_sse
            ):
                raise RuntimeError(f"locked fallback drift: {case_id}")

            trials = []
            feasible = []
            for qindex in qtrials(requested):
                obu = case_dir / f"ac-q{qindex}.obu"
                reported_sse, encode_time, byte_count = encode(
                    args.encoder.resolve(), source_path, case, qindex, "bounded",
                    obu,
                )
                trial = {
                    "qindex": qindex,
                    "q_shift": qindex - requested,
                    "obu_bytes": byte_count,
                    "within_budget": byte_count <= fallback_bytes,
                    "encode_time_ns": encode_time,
                }
                if trial["within_budget"]:
                    try:
                        decoded_sse, decoded_hash, decode_times = decode(
                            obu, case, source, args.avmdec.resolve(),
                            args.project_decoder.resolve(),
                            case_dir / f"ac-q{qindex}",
                        )
                    except RuntimeError as error:
                        trial.update({
                            "decoder_parity": False,
                            "decoder_error": str(error),
                        })
                    else:
                        if reported_sse != decoded_sse:
                            raise RuntimeError(f"reported SSE mismatch: {case_id}")
                        trial.update({
                            "sse_y": decoded_sse,
                            "psnr_y_db": psnr(decoded_sse, len(source)),
                            "decoded_sha256": decoded_hash,
                            "decoder_parity": True,
                            "decode_time_ns": decode_times,
                            "obu_sha256": sha256(obu.read_bytes()),
                        })
                        feasible.append(
                            (decoded_sse, qindex - requested, qindex, trial)
                        )
                trials.append(trial)

            # Exact distortion first. On a tie, preserve the requested qindex
            # and the DC fallback rather than silently weakening its semantics.
            selected_kind = "dc_fallback"
            selected = {
                "qindex": requested,
                "q_shift": 0,
                "obu_bytes": fallback_bytes,
                "sse_y": fallback_sse,
                "psnr_y_db": psnr(fallback_sse, len(source)),
                "obu_sha256": sha256(fallback_obu.read_bytes()),
                "decoded_sha256": fallback_decoded_hash,
            }
            better = [item for item in feasible if item[0] < fallback_sse]
            if better:
                _, _, _, selected_trial = min(better)
                selected_kind = "bounded_ac"
                selected = {
                    key: selected_trial[key] for key in (
                        "qindex", "q_shift", "obu_bytes", "sse_y",
                        "psnr_y_db", "obu_sha256", "decoded_sha256",
                    )
                }
                rerun = case_dir / "selected-rerun.obu"
                encode(
                    args.encoder.resolve(), source_path, case,
                    int(selected["qindex"]), "bounded", rerun,
                )
                if sha256(rerun.read_bytes()) != selected["obu_sha256"]:
                    raise RuntimeError(f"nondeterministic selection: {case_id}")

            records.append({
                "id": case_id,
                "subset": case["subset"],
                "pattern": case.get("pattern"),
                "source_id": case.get("source_id"),
                "requested_qindex": requested,
                "byte_budget": fallback_bytes,
                "fallback": {
                    "obu_bytes": fallback_bytes,
                    "sse_y": fallback_sse,
                    "psnr_y_db": psnr(fallback_sse, len(source)),
                    "encode_time_ns": fallback_time,
                    "decode_time_ns": fallback_decode_times,
                    "validator_exact": True,
                    "locked_baseline_exact": True,
                },
                "trials": trials,
                "selection": {
                    "kind": selected_kind,
                    **selected,
                    "sse_gain": fallback_sse - int(selected["sse_y"]),
                    "improved": int(selected["sse_y"]) < fallback_sse,
                    "deterministic": True,
                    "validator_exact": True,
                },
            })

    targeted = [item for item in records if item["subset"] == "targeted"]
    prepared = [item for item in records if item["subset"] == "full-prepared"]
    ac_subset = [item for item in targeted if item["pattern"] in AC_PATTERNS]

    def total(items: list[dict[str, object]], field: str) -> int:
        if field == "fallback":
            return sum(int(item["fallback"]["sse_y"]) for item in items)
        return sum(int(item["selection"]["sse_y"]) for item in items)

    baseline_ac_sse = total(ac_subset, "fallback")
    selected_ac_sse = total(ac_subset, "selection")
    elapsed = time.monotonic_ns() - start
    encode_times = [
        int(trial["encode_time_ns"])
        for item in records for trial in item["trials"]
    ]
    selected_ac_shifts = [
        int(item["selection"]["q_shift"]) for item in records
        if item["selection"]["kind"] == "bounded_ac"
    ]

    def corpus_summary(items: list[dict[str, object]]) -> dict[str, object]:
        fallback_sse = total(items, "fallback")
        selected_sse = total(items, "selection")
        return {
            "cases": len(items),
            "fallback_bytes": sum(item["byte_budget"] for item in items),
            "selected_bytes": sum(
                item["selection"]["obu_bytes"] for item in items
            ),
            "fallback_sse_y": fallback_sse,
            "selected_sse_y": selected_sse,
            "sse_gain": fallback_sse - selected_sse,
            "sse_gain_percent":
                100.0 * (fallback_sse - selected_sse) / fallback_sse,
            "wins": sum(item["selection"]["improved"] for item in items),
        }

    result = {
        "schema": "av2-ac8x8-equalsize-qtrial-v1",
        "generated_by": "experimental/encoder/tools/ac8x8_equalsize_qtrial.py",
        "policy": {
            "quality_semantics": (
                "requested qindex is a quality anchor, not an exact emitted "
                "qindex; trials only increase qindex (nominally lower quality), "
                "and the selected emitted qindex is reported"
            ),
            "trial_offsets": list(Q_OFFSETS),
            "maximum_ac_trials": len(Q_OFFSETS),
            "direction": "requested qindex through 255, monotonically increasing",
            "budget": "that case's locked DC-only complete-stream byte count",
            "ranking": "minimum decoded Y SSE, then minimum qindex shift",
            "fallback": "locked DC-only stream at the requested qindex",
        },
        "cases": records,
        "summary": {
            "case_count": len(records),
            "targeted_cases": len(targeted),
            "prepared_cases": len(prepared),
            "trial_count": sum(len(item["trials"]) for item in records),
            "feasible_trial_count": sum(
                trial["within_budget"]
                for item in records for trial in item["trials"]
            ),
            "validator_rejected_trial_count": sum(
                trial["within_budget"] and not trial.get("decoder_parity", False)
                for item in records for trial in item["trials"]
            ),
            "selected_ac_count": sum(
                item["selection"]["kind"] == "bounded_ac" for item in records
            ),
            "wins": sum(item["selection"]["improved"] for item in records),
            "targeted_wins": sum(
                item["selection"]["improved"] for item in targeted
            ),
            "prepared_wins": sum(
                item["selection"]["improved"] for item in prepared
            ),
            "ac_subset_cases": len(ac_subset),
            "ac_subset_wins": sum(
                item["selection"]["improved"] for item in ac_subset
            ),
            "ac_subset_gain_percent":
                100.0 * (baseline_ac_sse - selected_ac_sse) / baseline_ac_sse,
            "baseline_bytes": sum(item["byte_budget"] for item in records),
            "selected_bytes": sum(
                item["selection"]["obu_bytes"] for item in records
            ),
            "all_cases_at_or_below_budget": all(
                item["selection"]["obu_bytes"] <= item["byte_budget"]
                for item in records
            ),
            "prepared_sse_regressions": sum(
                item["selection"]["sse_y"] > item["fallback"]["sse_y"]
                for item in prepared
            ),
            "corpora": {
                "all": corpus_summary(records),
                "targeted": corpus_summary(targeted),
                "prepared": corpus_summary(prepared),
                "locked_ac_subset": corpus_summary(ac_subset),
            },
            "all_selected_validator_exact": all(
                item["selection"]["validator_exact"] for item in records
            ),
            "all_selected_deterministic": all(
                item["selection"]["deterministic"] for item in records
            ),
            "q_shift": {
                "minimum": min(item["selection"]["q_shift"] for item in records),
                "median": statistics.median(
                    item["selection"]["q_shift"] for item in records
                ),
                "maximum": max(item["selection"]["q_shift"] for item in records),
            },
            "selected_ac_q_shift": {
                "minimum": min(selected_ac_shifts) if selected_ac_shifts else None,
                "median": (
                    statistics.median(selected_ac_shifts)
                    if selected_ac_shifts else None
                ),
                "maximum": max(selected_ac_shifts) if selected_ac_shifts else None,
            },
            "runtime_ns": elapsed,
            "median_trial_encode_time_ns": int(statistics.median(encode_times)),
            "worst_trial_encode_time_ns": max(encode_times),
            "gain_gate_pass": selected_ac_sse * 100 <= baseline_ac_sse * 92,
            "minimum_wins_pass": sum(
                item["selection"]["improved"] for item in ac_subset
            ) >= len(ac_subset),
            "byte_gate_pass": all(
                item["selection"]["obu_bytes"] <= item["byte_budget"]
                for item in records
            ),
            "prepared_gate_pass": all(
                item["selection"]["sse_y"] <= item["fallback"]["sse_y"]
                for item in prepared
            ),
        },
    }
    result["summary"]["success"] = all((
        result["summary"]["gain_gate_pass"],
        result["summary"]["minimum_wins_pass"],
        result["summary"]["byte_gate_pass"],
        result["summary"]["prepared_gate_pass"],
    ))
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    shutil.rmtree(args.work_dir)
    print(json.dumps(result["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

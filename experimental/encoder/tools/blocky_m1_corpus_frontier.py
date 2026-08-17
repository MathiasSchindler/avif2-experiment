#!/usr/bin/env python3
"""Sweep deterministic high-gradient prepared I400 crops for an exact RD win."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
sys.path.insert(0, str(ENCODER / "tools"))

from blocky_m1_diagnostic import crop_planar_source  # noqa: E402
from blocky_m1_gate import (  # noqa: E402
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT,
    LEGACY_ORDER,
    RD_SEARCH,
    GateError,
    case_record,
    display_path,
    encode_search,
    executable,
    legacy_searches_for_case,
    materialize_source,
    prepared_lock_data,
    rd_emitted_candidate,
    rd_winner_categories,
    selected_prepared_records,
    sha256_file,
    validate_result,
)


CROP_SIZE = 64
DEFAULT_OUTPUT = ENCODER / "metrics" / "blocky-m1-corpus-frontier-prepared-crops.json"
DEFAULT_WORK = ENCODER / "build" / "blocky-m1-corpus-frontier-prepared-crops"
DEFAULT_ARTIFACTS = ENCODER / "build" / "blocky-m1-corpus-frontier-prepared-crops-artifacts"


def parse_qindices(value: str) -> list[int]:
    values = []
    for item in value.split(","):
        item = item.strip()
        if not item:
            continue
        try:
            qindex = int(item)
        except ValueError as error:
            raise GateError(f"invalid qindex: {item}") from error
        if not 1 <= qindex <= 255:
            raise GateError(f"qindex is outside 1..255: {qindex}")
        values.append(qindex)
    if not values:
        raise GateError("at least one qindex is required")
    return sorted(set(values))


def high_gradient_crop(data: bytes, width: int, height: int) -> tuple[int, int, int]:
    if width < CROP_SIZE or height < CROP_SIZE:
        raise GateError(f"prepared image is smaller than {CROP_SIZE}x{CROP_SIZE}")
    best = (-1, 0, 0)
    for y in range(0, height - CROP_SIZE + 1, 16):
        for x in range(0, width - CROP_SIZE + 1, 16):
            score = 0
            for row in range(CROP_SIZE - 1):
                offset = (y + row) * width + x
                below = offset + width
                for column in range(CROP_SIZE - 1):
                    current = data[offset + column]
                    score += abs(current - data[offset + column + 1])
                    score += abs(current - data[below + column])
            candidate = (score, -y, -x)
            if candidate > (best[0], -best[1], -best[2]):
                best = (score, y, x)
    return best


def validator_exact(result: dict[str, Any]) -> bool:
    return bool(
        result.get("status") == "pass"
        and result.get("validated")
        and result.get("avm_project_exact")
        and result.get("local_sse_matches_decode")
    )


def decoded_sse(result: dict[str, Any]) -> list[int]:
    return [int(plane["decoded_sse"]) for plane in result["planes"]]


def dominates(left: dict[str, Any], right: dict[str, Any]) -> bool:
    left_sse = decoded_sse(left)
    right_sse = decoded_sse(right)
    return (
        int(left["obu_bytes"]) <= int(right["obu_bytes"])
        and all(left_value <= right_value for left_value, right_value in zip(left_sse, right_sse))
        and (
            int(left["obu_bytes"]) < int(right["obu_bytes"])
            or any(left_value < right_value for left_value, right_value in zip(left_sse, right_sse))
        )
    )


def best_rate_ceiling_baseline(searches: dict[str, dict[str, Any]]) -> dict[str, Any] | None:
    rd = searches[RD_SEARCH]
    if not validator_exact(rd):
        return None
    candidates = [
        result
        for name, result in searches.items()
        if name != RD_SEARCH
        and validator_exact(result)
        and int(result["obu_bytes"]) >= int(rd["obu_bytes"])
    ]
    if not candidates:
        return None
    return min(
        candidates,
        key=lambda result: (
            sum(decoded_sse(result)),
            int(result["obu_bytes"]),
            LEGACY_ORDER[result["search"]],
        ),
    )


def retain(artifacts: Path, case: dict[str, Any], result: dict[str, Any]) -> str:
    target = artifacts / "streams" / case["id"] / f"{result['search']}.obu"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(result["primary_output_path"], target)
    return display_path(target)


def prepared_cases(
    selection: str,
    qindices: Sequence[int],
    limit: int | None,
    full_source: str | None,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    lock = prepared_lock_data()
    records = selected_prepared_records(lock, selection)
    if full_source is not None:
        records = [record for record in records if record["source_id"] == full_source]
        if not records:
            raise GateError(f"prepared source is absent from {selection}: {full_source}")
    if limit is not None:
        records = records[:limit]
    cases = []
    sources: dict[str, str] = {}
    for record in records:
        width, height = map(int, record["dimensions"])
        output = record["outputs"]["gray8"]
        path = ROOT / "images" / "prepared" / record["preparation_id"] / record["source_id"] / output["filename"]
        data = path.read_bytes()
        if sha256_file(path) != output["sha256"]:
            raise GateError(f"prepared source hash mismatch: {path}")
        sources[display_path(path)] = output["sha256"]
        if full_source is None:
            score, y, x = high_gradient_crop(data, width, height)
            source = crop_planar_source(
                data, "i400", width, height, x, y, CROP_SIZE, CROP_SIZE,
            )
            case_width, case_height = CROP_SIZE, CROP_SIZE
            case_id = (
                f"prepared-{record['source_id']}-{record['preparation_id']}"
                f"-i400-crop-{x}x{y}"
            )
            tags = ("prepared", "real-image", "high-gradient-crop")
            crop_origin: dict[str, Any] = {
                "crop": {
                    "x": x, "y": y, "width": CROP_SIZE, "height": CROP_SIZE,
                },
                "gradient_score": score,
            }
        else:
            source = data
            case_width, case_height = width, height
            case_id = (
                f"prepared-{record['source_id']}-{record['preparation_id']}"
                "-i400-full"
            )
            tags = ("prepared", "real-image", "full-image", "multiple-superblocks")
            crop_origin = {}
        for qindex in qindices:
            cases.append(case_record(
                case_id=f"{case_id}-q{qindex}",
                subset="prepared-full" if full_source is not None else "prepared-crop",
                pixel_format="i400",
                width=case_width,
                height=case_height,
                qindex=qindex,
                tags=tags,
                expected_categories=(),
                source=source,
                origin={
                    "source_id": record["source_id"],
                    "preparation_id": record["preparation_id"],
                    "source": display_path(path),
                    "source_sha256": output["sha256"],
                    **crop_origin,
                },
            ))
    return cases, {
        "selection": selection,
        "crop_size": None if full_source is not None else CROP_SIZE,
        "full_source": full_source,
        "qindices": list(qindices),
        "source_count": len(records),
        "sources": sources,
    }


def record_case(
    case: dict[str, Any],
    encoder: Path,
    avmdec: Path,
    project: Path,
    work: Path,
    artifacts: Path,
) -> dict[str, Any]:
    source_path, source = materialize_source(case, work / case["id"])
    searches: dict[str, dict[str, Any]] = {}
    for search in legacy_searches_for_case(case["pixel_format"]):
        searches[search] = encode_search(case, source_path, source, search, 1, encoder, work / case["id"])
    searches[RD_SEARCH] = encode_search(case, source_path, source, RD_SEARCH, 2, encoder, work / case["id"])
    for search, result in searches.items():
        if result.get("status") == "pass":
            try:
                searches[search] = validate_result(
                    case, result, source, avmdec, project,
                )
            except GateError as error:
                result["validation_error"] = str(error)
                result["validated"] = False
                result["avm_project_exact"] = False
                result["local_sse_matches_decode"] = False
    baseline = best_rate_ceiling_baseline(searches)
    rd = searches[RD_SEARCH]
    record = {
        key: value for key, value in case.items()
        if key not in {"source", "source_path"}
    }
    record["searches"] = searches
    record["validator_exact_searches"] = sorted(
        name for name, result in searches.items() if validator_exact(result)
    )
    record["invalid_legacy_searches"] = sorted(
        name for name, result in searches.items()
        if name != RD_SEARCH and result.get("status") == "pass" and not validator_exact(result)
    )
    record["baseline_search"] = None if baseline is None else baseline["search"]
    record["rd_emitted_candidate"] = rd_emitted_candidate(
        case["pixel_format"], searches,
    )
    record["rd_winner_categories"] = (
        rd_winner_categories(rd.get("tokens", {}))
        if record["rd_emitted_candidate"]
        else []
    )
    if baseline is None:
        record["rd_vs_baseline"] = None
        return record
    record["rd_vs_baseline"] = {
        "baseline_search": baseline["search"],
        "byte_delta": int(rd["obu_bytes"]) - int(baseline["obu_bytes"]),
        "plane_sse_gain": [
            left - right for left, right in zip(decoded_sse(baseline), decoded_sse(rd))
        ],
        "nonregression": (
            int(rd["obu_bytes"]) <= int(baseline["obu_bytes"])
            and all(left <= right for left, right in zip(decoded_sse(rd), decoded_sse(baseline)))
        ),
        "strict_pareto_win": dominates(rd, baseline),
    }
    record["artifacts"] = {
        "rd": retain(artifacts, case, rd),
        "baseline": retain(artifacts, case, baseline),
    }
    return record


def summary(cases: Sequence[dict[str, Any]]) -> dict[str, Any]:
    comparable = [case for case in cases if case["rd_vs_baseline"] is not None]
    wins = [case for case in comparable if case["rd_vs_baseline"]["strict_pareto_win"]]
    trial_cases = [
        case for case in cases
        if int(case["searches"][RD_SEARCH].get("tokens", {}).get("rd_trials", "0")) > 0
    ]
    telemetry: dict[str, int] = {}
    for case in cases:
        for name, value in case["searches"][RD_SEARCH].get("tokens", {}).items():
            if name.startswith("rd_"):
                telemetry[name] = telemetry.get(name, 0) + int(value)
    return {
        "case_count": len(cases),
        "comparable_case_count": len(comparable),
        "strict_pareto_win_count": len(wins),
        "strict_pareto_wins": [case["id"] for case in wins],
        "strict_pareto_evidence": [
            {
                "id": case["id"],
                "origin": case["origin"],
                "qindex": case["qindex"],
                "baseline": {
                    "bytes": case["searches"][case["baseline_search"]]["obu_bytes"],
                    "decoded_sse": decoded_sse(
                        case["searches"][case["baseline_search"]]),
                },
                "rd": {
                    "bytes": case["searches"][RD_SEARCH]["obu_bytes"],
                    "decoded_sse": decoded_sse(case["searches"][RD_SEARCH]),
                },
            }
            for case in wins
        ],
        "rd_trial_case_count": len(trial_cases),
        "rd_zero_trial_cases": [
            case["id"] for case in cases if case not in trial_cases
        ],
        "rd_exact_case_count": sum(validator_exact(case["searches"][RD_SEARCH]) for case in cases),
        "legacy_nonexact_case_count": sum(bool(case["invalid_legacy_searches"]) for case in cases),
        "decoder_rejected_search_count": sum(
            1
            for case in cases
            for result in case["searches"].values()
            if "validation_error" in result
        ),
        "rd_telemetry_totals": dict(sorted(telemetry.items())),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    parser.add_argument("--artifacts-dir", type=Path, default=DEFAULT_ARTIFACTS)
    parser.add_argument("--selection", choices=("selected", "le896", "le1440"), default="selected")
    parser.add_argument("--qindices", default="60,100,141")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--full-source")
    args = parser.parse_args(argv)

    encoder = executable(args.encoder, "experimental encoder")
    avmdec = executable(args.avmdec, "pinned AVM decoder")
    project = executable(args.project_decoder, "project decoder")
    qindices = parse_qindices(args.qindices)
    cases, corpus = prepared_cases(
        args.selection, qindices, args.limit, args.full_source,
    )
    if not cases:
        raise GateError("prepared crop selection is empty")

    shutil.rmtree(args.work_dir, ignore_errors=True)
    shutil.rmtree(args.artifacts_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)
    try:
        records = [
            record_case(case, encoder, avmdec, project, args.work_dir, args.artifacts_dir)
            for case in cases
        ]
    finally:
        shutil.rmtree(args.work_dir, ignore_errors=True)

    report = {
        "schema": "av2-encoder-blocky-m1-corpus-frontier-prepared-crops-v1",
        "status": "pass" if any(
            case["rd_vs_baseline"] and case["rd_vs_baseline"]["strict_pareto_win"]
            for case in records
        ) else "blocked",
        "configuration": {
            "selection": args.selection,
            "qindices": qindices,
            "crop_selector": (
                "full source bytes"
                if args.full_source is not None
                else "highest 64x64 luma abs(horizontal)+abs(vertical) gradient on a 16-pixel grid; stable y/x tie order"
            ),
            "legacy_searches": list(legacy_searches_for_case("i400")),
            "rd_repetitions": 2,
            "legacy_repetitions": 1,
            "baseline_selector": "validator-exact legacy candidates at or above RD bytes; minimum decoded total SSE, bytes, then help order",
        },
        "tools": {
            "encoder": {"path": display_path(encoder), "sha256": sha256_file(encoder)},
            "avmdec": {"path": display_path(avmdec), "sha256": sha256_file(avmdec)},
            "project_decoder": {"path": display_path(project), "sha256": sha256_file(project)},
        },
        "corpus": corpus,
        "cases": records,
        "summary": summary(records),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": report["status"],
        "case_count": report["summary"]["case_count"],
        "strict_pareto_win_count": report["summary"]["strict_pareto_win_count"],
    }, sort_keys=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())

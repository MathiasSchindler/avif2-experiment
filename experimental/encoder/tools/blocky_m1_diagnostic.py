#!/usr/bin/env python3
"""Diagnose observable `--search rd` output classes on minimal M1 cases."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import statistics
from pathlib import Path
from typing import Any, Sequence

from blocky_m1_gate import (
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT,
    ENCODER,
    LEGACY_ORDER,
    LEGACY_SEARCHES,
    OUTPUT_NAMES,
    PREPARED_LOCK,
    RD_SEARCH,
    ROOT,
    GateError,
    case_record,
    display_path,
    encode_search,
    enrich_case_record,
    executable,
    legacy_searches_for_case,
    materialize_source,
    plane_geometry,
    prepared_lock_data,
    probe_rd_support,
    sha256_bytes,
    sha256_file,
    targeted_cases,
    validate_result,
)


DEFAULT_OUTPUT = ENCODER / "build" / "blocky-m1-diagnostic.json"
DEFAULT_ARTIFACTS = ENCODER / "build" / "blocky-m1-diagnostic-artifacts"
DEFAULT_WORK = ENCODER / "build" / "blocky-m1-diagnostic"
CROPS_FIXTURE = ENCODER / "fixtures" / "blocky-m1-diagnostic-crops.json"
TARGETED_IDS = {
    "skip-flat-odd-i400-q100",
    "ac-checker-i400-q100",
    "directional-horizontal-edge-i400-q60",
    "paeth-vertical-edge-i400-q60",
    "smooth-strong-center-i400-q60",
    "qdelta-tiled-gradient-i420-q141",
    "qdelta-tiled-gradient-i444-q141",
}
TARGETED_ROLES = {
    "skip-flat-odd-i400-q100": "skip-dc-collapse",
    "ac-checker-i400-q100": "bounded-ac-winner",
    "directional-horizontal-edge-i400-q60": "global-directional-win-not-rd",
    "paeth-vertical-edge-i400-q60": "global-paeth-win-not-rd",
    "smooth-strong-center-i400-q60": "global-smooth-win-not-rd",
    "qdelta-tiled-gradient-i420-q141": "color-rd-equals-bounded",
    "qdelta-tiled-gradient-i444-q141": "color-rd-unique-qdelta",
}
CORPUS_ORDER = {"targeted": 0, "baya-crop": 1}
CORPUS_NAMES = {"targeted": "targeted", "baya": "baya"}


def load_crop_fixture() -> dict[str, Any]:
    return json.loads(CROPS_FIXTURE.read_text(encoding="utf-8"))


def legacy_rank(search: str) -> int:
    return LEGACY_ORDER.get(search, len(LEGACY_SEARCHES))


def case_sort_key(case: dict[str, Any]) -> tuple[int, str]:
    return (CORPUS_ORDER.get(case["subset"], 99), str(case["id"]))


def parse_corpora(value: str) -> list[str]:
    selected = []
    for token in value.split(","):
        name = token.strip().lower()
        if not name:
            continue
        if name not in CORPUS_NAMES:
            raise GateError(f"unknown corpus selection: {token}")
        selected.append(CORPUS_NAMES[name])
    if not selected:
        raise GateError("at least one corpus is required")
    return selected


def filter_cases(cases: list[dict[str, Any]], pattern: str | None) -> list[dict[str, Any]]:
    if pattern is None:
        return cases
    compiled = re.compile(pattern)
    return [case for case in cases if compiled.search(case["id"])]


def crop_plane(
    data: bytes,
    source_width: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> bytes:
    return b"".join(
        data[(y + row) * source_width + x:(y + row) * source_width + x + width]
        for row in range(height)
    )


def crop_planar_source(
    data: bytes,
    pixel_format: str,
    source_width: int,
    source_height: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> bytes:
    planes = []
    cropped = []
    offset = 0
    for plane_width, plane_height in plane_geometry(
        pixel_format, source_width, source_height
    ):
        size = plane_width * plane_height
        planes.append((plane_width, plane_height, data[offset:offset + size]))
        offset += size
    if offset != len(data):
        raise GateError(
            f"{pixel_format} planar size mismatch: expected {offset}, got {len(data)}"
        )
    target_geometry = plane_geometry(pixel_format, width, height)
    for plane_index, ((plane_width, plane_height, plane_data), (crop_width, crop_height)) in enumerate(
        zip(planes, target_geometry)
    ):
        divisor = 2 if pixel_format == "i420" and plane_index != 0 else 1
        crop_x = x // divisor
        crop_y = y // divisor
        if (
            crop_x < 0
            or crop_y < 0
            or crop_x + crop_width > plane_width
            or crop_y + crop_height > plane_height
        ):
            raise GateError("crop is outside prepared source bounds")
        cropped.append(
            crop_plane(plane_data, plane_width, crop_x, crop_y, crop_width, crop_height)
        )
    return b"".join(cropped)


def selected_targeted_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    cases, meta = targeted_cases()
    selected = []
    for case in cases:
        if case["id"] not in TARGETED_IDS:
            continue
        origin = dict(case["origin"])
        origin["diagnostic_role"] = TARGETED_ROLES[case["id"]]
        selected.append({**case, "origin": origin})
    if {case["id"] for case in selected} != TARGETED_IDS:
        missing = sorted(TARGETED_IDS - {case["id"] for case in selected})
        raise GateError(f"missing targeted diagnostic cases: {missing}")
    return sorted(selected, key=case_sort_key), {
        **meta,
        "selected_ids": sorted(TARGETED_IDS),
        "targeted_case_count": len(selected),
    }


def baya_crop_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    fixture = load_crop_fixture()
    lock = prepared_lock_data()
    records = {
        (record["source_id"], record["preparation_id"]): record
        for record in lock["preparations"]
    }
    cases = []
    sources: dict[str, str] = {}
    for spec in fixture["crops"]:
        key = (spec["source_id"], spec["preparation_id"])
        if key not in records:
            raise GateError(f"prepared record missing for diagnostic crop: {key}")
        record = records[key]
        source_width, source_height = map(int, record["dimensions"])
        crop = spec["crop"]
        if (
            crop["x"] < 0
            or crop["y"] < 0
            or crop["x"] + crop["width"] > source_width
            or crop["y"] + crop["height"] > source_height
        ):
            raise GateError(f"diagnostic crop is out of bounds: {spec['id']}")
        output = record["outputs"][OUTPUT_NAMES[spec["pixel_format"]]]
        path = (
            ROOT
            / "images"
            / "prepared"
            / spec["preparation_id"]
            / spec["source_id"]
            / output["filename"]
        )
        data = path.read_bytes()
        if sha256_bytes(data) != output["sha256"]:
            raise GateError(f"prepared source hash mismatch: {path}")
        source = crop_planar_source(
            data,
            spec["pixel_format"],
            source_width,
            source_height,
            int(crop["x"]),
            int(crop["y"]),
            int(crop["width"]),
            int(crop["height"]),
        )
        sources[str(path.relative_to(ROOT))] = output["sha256"]
        cases.append(
            case_record(
                case_id=str(spec["id"]),
                subset="baya-crop",
                pixel_format=str(spec["pixel_format"]),
                width=int(crop["width"]),
                height=int(crop["height"]),
                qindex=int(spec["qindex"]),
                tags=tuple(spec["tags"]),
                expected_categories=(),
                source=source,
                origin={
                    "source_id": spec["source_id"],
                    "preparation_id": spec["preparation_id"],
                    "crop": {
                        "x": int(crop["x"]),
                        "y": int(crop["y"]),
                        "width": int(crop["width"]),
                        "height": int(crop["height"]),
                    },
                    "diagnostic_role": spec["role"],
                    "expected_rd_relation": spec["expected_rd_relation"],
                    "note": spec["note"],
                },
            )
        )
    return sorted(cases, key=case_sort_key), {
        "crop_fixture_sha256": sha256_file(CROPS_FIXTURE),
        "crop_count": len(cases),
        "prepared_lock_sha256": sha256_file(PREPARED_LOCK),
        "sources": sources,
    }


def build_case_matrix(corpora: Sequence[str]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    corpus: dict[str, Any] = {}
    for name in corpora:
        if name == "targeted":
            subset_cases, meta = selected_targeted_cases()
        elif name == "baya":
            subset_cases, meta = baya_crop_cases()
        else:
            raise GateError(f"unknown corpus: {name}")
        cases.extend(subset_cases)
        corpus[name] = meta
    return sorted(cases, key=case_sort_key), corpus


def result_signature(result: dict[str, Any]) -> tuple[str, int, tuple[int, ...]] | None:
    if result.get("status") != "pass":
        return None
    return (
        str(result["obu_sha256"]),
        int(result["obu_bytes"]),
        tuple(int(value) for value in result["reported_sse"]),
    )


def equivalence_classes(searches: dict[str, dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, int, tuple[int, ...]], list[str]] = {}
    for search, result in searches.items():
        signature = result_signature(result)
        if signature is None:
            continue
        grouped.setdefault(signature, []).append(search)
    classes = []
    for (sha256, obu_bytes, sse), names in grouped.items():
        ordered = sorted(names, key=legacy_rank)
        classes.append(
            {
                "searches": ordered,
                "obu_sha256": sha256,
                "obu_bytes": obu_bytes,
                "reported_sse": list(sse),
            }
        )
    classes.sort(
        key=lambda item: (
            int(item["obu_bytes"]),
            tuple(int(value) for value in item["reported_sse"]),
            legacy_rank(item["searches"][0]),
        )
    )
    return classes


def rd_matches(case: dict[str, Any], searches: dict[str, dict[str, Any]]) -> list[str]:
    signature = result_signature(searches[RD_SEARCH])
    if signature is None:
        return []
    return [
        name
        for name in legacy_searches_for_case(case["pixel_format"])
        if result_signature(searches[name]) == signature
    ]


def rd_relation(matches: Sequence[str]) -> str:
    if not matches:
        return "rd-unique"
    if list(matches) == ["bounded"]:
        return "rd-equals-bounded"
    if list(matches) == ["off"]:
        return "rd-equals-off"
    if set(matches) == {"off", "bounded"}:
        return "rd-equals-off-and-bounded"
    return "rd-collapsed"


def runtime_ratio(
    searches: dict[str, dict[str, Any]],
    left: str,
    right: str,
) -> float | None:
    left_result = searches.get(left)
    right_result = searches.get(right)
    if (
        left_result is None
        or right_result is None
        or left_result.get("status") != "pass"
        or right_result.get("status") != "pass"
    ):
        return None
    right_time = int(right_result["encode_time_ns"]["median"])
    if right_time == 0:
        return None
    return int(left_result["encode_time_ns"]["median"]) / right_time


def maybe_median(values: list[float]) -> float | None:
    if not values:
        return None
    return float(statistics.median(values))


def validate_all(
    case: dict[str, Any],
    searches: dict[str, dict[str, Any]],
    source: bytes,
    avmdec: Path,
    project: Path,
) -> dict[str, dict[str, Any]]:
    for search in sorted(searches):
        if searches[search].get("status") != "pass":
            continue
        searches[search] = validate_result(case, searches[search], source, avmdec, project)
    return searches


def summarize(records: list[dict[str, Any]]) -> dict[str, Any]:
    bounded = [record["id"] for record in records if record["rd_relation"] == "rd-equals-bounded"]
    off = [record["id"] for record in records if record["rd_equals_off"]]
    unique = [record["id"] for record in records if record["rd_relation"] == "rd-unique"]
    mismatched = [
        record["id"]
        for record in records
        if record.get("expected_rd_relation") is not None
        and not record.get("expected_rd_relation_matched", False)
    ]
    frontier_missed = [
        record["id"]
        for record in records
        if record["legacy_frontier"]
        and not set(record["legacy_frontier"]) & set(record["rd_matches"])
    ]
    rd_bounded_ratios = [
        record["rd_runtime_ratio_vs_bounded"]
        for record in records
        if record["rd_runtime_ratio_vs_bounded"] is not None
    ]
    rd_off_ratios = [
        record["rd_runtime_ratio_vs_off"]
        for record in records
        if record["rd_runtime_ratio_vs_off"] is not None
    ]
    by_format = {}
    for pixel_format in sorted({record["pixel_format"] for record in records}):
        format_records = [record for record in records if record["pixel_format"] == pixel_format]
        by_format[pixel_format] = {
            "case_count": len(format_records),
            "rd_equals_bounded_cases": [
                record["id"]
                for record in format_records
                if record["rd_relation"] == "rd-equals-bounded"
            ],
            "rd_unique_cases": [
                record["id"]
                for record in format_records
                if record["rd_relation"] == "rd-unique"
            ],
            "median_rd_runtime_ratio_vs_bounded": maybe_median(
                [
                    record["rd_runtime_ratio_vs_bounded"]
                    for record in format_records
                    if record["rd_runtime_ratio_vs_bounded"] is not None
                ]
            ),
        }
    return {
        "case_count": len(records),
        "rd_equals_bounded_cases": bounded,
        "rd_equals_off_cases": off,
        "rd_unique_cases": unique,
        "expected_relation_mismatches": mismatched,
        "legacy_frontier_not_matched_by_rd": frontier_missed,
        "median_rd_runtime_ratio_vs_bounded": maybe_median(rd_bounded_ratios),
        "max_rd_runtime_ratio_vs_bounded": max(rd_bounded_ratios, default=None),
        "median_rd_runtime_ratio_vs_off": maybe_median(rd_off_ratios),
        "by_format": by_format,
    }


def blocked_report(args: argparse.Namespace, probe: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": "av2-encoder-blocky-m1-diagnostic-v1",
        "status": "blocked",
        "reason": f"rd unavailable: {probe['reason']}",
        "configuration": {
            "corpora": parse_corpora(args.corpora),
            "case_filter": args.case_filter,
        },
        "support_probe": probe,
        "cases": [],
        "summary": {"case_count": 0},
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--artifacts-dir", type=Path, default=DEFAULT_ARTIFACTS)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    parser.add_argument(
        "--corpora",
        default="targeted,baya",
        help="comma-separated corpus list: targeted,baya",
    )
    parser.add_argument("--case-filter")
    parser.add_argument("--rd-repetitions", type=int, default=2)
    parser.add_argument("--legacy-repetitions", type=int, default=1)
    parser.add_argument("--skip-validation", action="store_true")
    parser.add_argument("--keep-work", action="store_true")
    args = parser.parse_args(argv)

    corpora = parse_corpora(args.corpora)
    encoder = executable(args.encoder, "experimental encoder")

    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    probe = probe_rd_support(encoder, args.work_dir)
    if not probe["available"]:
        report = blocked_report(args, probe)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({"status": report["status"], "reason": report["reason"]}, sort_keys=True))
        if not args.keep_work:
            shutil.rmtree(args.work_dir, ignore_errors=True)
        return 1

    avmdec = project = None
    if not args.skip_validation:
        avmdec = executable(args.avmdec, "pinned AVM decoder")
        project = executable(args.project_decoder, "project decoder")

    cases, corpus = build_case_matrix(corpora)
    cases = filter_cases(cases, args.case_filter)
    if not cases:
        raise GateError("no diagnostic cases matched the requested corpus/filter")

    shutil.rmtree(args.artifacts_dir, ignore_errors=True)
    args.artifacts_dir.mkdir(parents=True, exist_ok=True)

    records = []
    try:
        for case in cases:
            case_root = args.work_dir / case["id"]
            source_path, source = materialize_source(case, case_root)
            searches: dict[str, dict[str, Any]] = {}
            for search in legacy_searches_for_case(case["pixel_format"]):
                searches[search] = encode_search(
                    case,
                    source_path,
                    source,
                    search,
                    args.legacy_repetitions,
                    encoder,
                    case_root,
                )
            searches[RD_SEARCH] = encode_search(
                case,
                source_path,
                source,
                RD_SEARCH,
                args.rd_repetitions,
                encoder,
                case_root,
            )
            if avmdec is not None and project is not None:
                searches = validate_all(case, searches, source, avmdec, project)
            record = enrich_case_record(case, searches, args.artifacts_dir)
            matches = rd_matches(case, searches)
            relation = rd_relation(matches)
            record["equivalence_classes"] = equivalence_classes(searches)
            record["rd_matches"] = matches
            record["rd_relation"] = relation
            record["rd_equals_bounded"] = "bounded" in matches
            record["rd_equals_off"] = "off" in matches
            record["rd_unique"] = relation == "rd-unique"
            record["rd_runtime_ratio_vs_off"] = runtime_ratio(searches, RD_SEARCH, "off")
            record["rd_runtime_ratio_vs_bounded"] = runtime_ratio(
                searches, RD_SEARCH, "bounded"
            )
            expected_relation = case["origin"].get("expected_rd_relation")
            if expected_relation is not None:
                record["expected_rd_relation"] = expected_relation
                record["expected_rd_relation_matched"] = expected_relation == relation
            records.append(record)

        report = {
            "schema": "av2-encoder-blocky-m1-diagnostic-v1",
            "status": "pass",
            "configuration": {
                "corpora": corpora,
                "case_filter": args.case_filter,
                "legacy_repetitions": args.legacy_repetitions,
                "rd_repetitions": args.rd_repetitions,
                "skip_validation": args.skip_validation,
                "targeted_ids": sorted(TARGETED_IDS),
            },
            "tools": {
                "encoder": {
                    "path": display_path(encoder),
                    "sha256": sha256_file(encoder),
                },
            },
            "support_probe": probe,
            "corpus": corpus,
            "cases": records,
            "summary": summarize(records),
        }
        if avmdec is not None and project is not None:
            report["tools"]["avmdec"] = {
                "path": display_path(avmdec),
                "sha256": sha256_file(avmdec),
            }
            report["tools"]["project_decoder"] = {
                "path": display_path(project),
                "sha256": sha256_file(project),
            }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(
            json.dumps(
                {
                    "case_count": report["summary"]["case_count"],
                    "rd_equals_bounded_cases": report["summary"]["rd_equals_bounded_cases"],
                    "rd_unique_cases": report["summary"]["rd_unique_cases"],
                    "status": report["status"],
                },
                sort_keys=True,
            )
        )
        return 0
    finally:
        if not args.keep_work:
            shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

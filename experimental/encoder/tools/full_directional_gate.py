#!/usr/bin/env python3
"""Freeze the production classic-directional search surface."""

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
    plane_geometry,
)


DEFAULT_OUTPUT = ENCODER / "metrics/full-directional-gate.json"
DEFAULT_WORK = ENCODER / "build/full-directional-gate"
PREPARED_CROPS = {
    "chinnathurai-church": (416, 624),
    "elbrus-milky-way": (400, 416),
    "pastel-chalks": (624, 400),
    "pitti-ceiling": (208, 208),
}
PATTERNS = ("d45", "d135", "steep", "shallow")
CHROMA_PATTERNS = ("chroma-vertical", "chroma-horizontal")
QINDEX = 100


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pattern_sample(name: str, x: int, y: int, plane: int) -> int:
    shift = (0, 17, 31)[plane]
    if name == "chroma-vertical":
        if plane == 0:
            return 128
        return min(240, 24 + shift + x * 3)
    elif name == "chroma-horizontal":
        if plane == 0:
            return 128
        return min(240, 24 + shift + y * 3)
    elif name == "d45":
        low = x < y
    elif name == "d135":
        low = x + y < 63
    elif name == "steep":
        low = x * 2 < y
    elif name == "shallow":
        low = x < y * 2
    else:
        raise RuntimeError(f"unknown pattern: {name}")
    return (40 + shift) if low else (216 - shift)


def generated_case(name: str, pixel_format: str) -> dict[str, Any]:
    planes = []
    for plane, (width, height) in enumerate(
            plane_geometry(pixel_format, 64, 64)):
        scale = 2 if pixel_format == "i420" and plane != 0 else 1
        planes.append(bytes(
            pattern_sample(name, x * scale, y * scale, plane)
            for y in range(height) for x in range(width)
        ))
    source = b"".join(planes)
    return {
        "id": f"synthetic-{name}-{pixel_format}",
        "subset": "synthetic-directional",
        "pixel_format": pixel_format,
        "width": 64,
        "height": 64,
        "qindex": QINDEX,
        "source": source,
        "source_sha256": sha256_bytes(source),
    }


def crop_plane(data: bytes, source_width: int, x: int, y: int,
               width: int, height: int) -> bytes:
    return b"".join(
        data[(y + row) * source_width + x:
             (y + row) * source_width + x + width]
        for row in range(height)
    )


def prepared_cases() -> tuple[list[dict[str, Any]], dict[str, str]]:
    lock = json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))
    records = {
        item["source_id"]: item for item in lock["preparations"]
        if item["preparation_id"] == "le896"
        and item["source_id"] in PREPARED_CROPS
    }
    cases = []
    source_hashes = {}
    output_names = {
        "i400": "gray8",
        "i420": "yuv420p8",
        "i444": "yuv444p8",
    }
    for source_id, (crop_x, crop_y) in PREPARED_CROPS.items():
        record = records[source_id]
        source_width, source_height = map(int, record["dimensions"])
        for pixel_format in FORMATS:
            output = record["outputs"][output_names[pixel_format]]
            path = (ROOT / "images/prepared/le896" / source_id /
                    output["filename"])
            data = path.read_bytes()
            if sha256_bytes(data) != output["sha256"]:
                raise RuntimeError(f"prepared input hash mismatch: {path}")
            source_hashes[str(path.relative_to(ROOT))] = output["sha256"]
            offset = 0
            planes = []
            for plane, (plane_width, plane_height) in enumerate(
                    plane_geometry(pixel_format, source_width, source_height)):
                size = plane_width * plane_height
                plane_data = data[offset:offset + size]
                offset += size
                divisor = 2 if pixel_format == "i420" and plane != 0 else 1
                width = 64 // divisor
                height = 64 // divisor
                planes.append(crop_plane(
                    plane_data, plane_width, crop_x // divisor,
                    crop_y // divisor, width, height))
            if offset != len(data):
                raise RuntimeError(f"prepared plane size mismatch: {path}")
            source = b"".join(planes)
            cases.append({
                "id": f"prepared-{source_id}-{pixel_format}",
                "subset": "prepared-crop",
                "source_id": source_id,
                "crop": [crop_x, crop_y, 64, 64],
                "pixel_format": pixel_format,
                "width": 64,
                "height": 64,
                "qindex": QINDEX,
                "source": source,
                "source_sha256": sha256_bytes(source),
            })
    return cases, source_hashes


def fixture_cases() -> tuple[list[dict[str, Any]], dict[str, Any]]:
    prepared, source_hashes = prepared_cases()
    generated = [
        generated_case(pattern, pixel_format)
        for pattern in PATTERNS for pixel_format in FORMATS
    ]
    generated.extend(
        generated_case(pattern, pixel_format)
        for pattern in CHROMA_PATTERNS
        for pixel_format in ("i420", "i444")
    )
    return generated + prepared, {
        "prepared_lock_sha256": sha256_bytes(PREPARED_LOCK.read_bytes()),
        "prepared_sources": source_hashes,
    }


def plane_sse(result: dict[str, Any]) -> list[int]:
    return [int(plane["sse"]) for plane in result.get("planes", [])]


def selected_modes(result: dict[str, Any]) -> dict[str, int]:
    stderr = str(result["attempts"][-1]["stderr"])
    modes = {}
    for field in ("intra_mode", "luma_mode", "chroma_mode"):
        marker = f"{field}="
        if marker in stderr:
            modes[field] = int(stderr.split(marker, 1)[1].split()[0])
    return modes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()
    required = (args.encoder, args.avmdec, args.project_decoder)
    for path in required:
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
                case, "off", 1, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                case_root / "off")
            bounded = encode(
                case, "directional-full", 2, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                case_root / "bounded")
            before = plane_sse(baseline)
            after = plane_sse(bounded)
            gains = [left - right for left, right in zip(before, after)]
            before_cost = sum(before) * 256 + baseline.get("obu_bytes", 0) * 8
            after_cost = sum(after) * 256 + bounded.get("obu_bytes", 0) * 8
            record = {
                key: value for key, value in case.items() if key != "source"
            }
            record.update({
                "off": baseline,
                "bounded": bounded,
                "selected_modes": selected_modes(bounded),
                "sse_gain": gains,
                "rd_cost_gain": before_cost - after_cost,
            })
            records.append(record)

        successful = [
            record for record in records
            if all(record[name].get("status") == "pass"
                   for name in ("off", "bounded"))
        ]
        strict_wins = [
            record for record in successful if sum(record["sse_gain"]) > 0
        ]
        prepared_wins = [
            record for record in strict_wins
            if record["subset"] == "prepared-crop"
        ]
        chroma_wins = [
            record for record in strict_wins
            if len(record["sse_gain"]) == 3
            and sum(record["sse_gain"][1:]) > 0
        ]
        luma_modes = {
            record["selected_modes"].get(
                "luma_mode", record["selected_modes"].get("intra_mode", 0))
            for record in successful
        }
        chroma_modes = {
            record["selected_modes"].get("chroma_mode", 0)
            for record in successful if record["pixel_format"] != "i400"
        }
        times = [
            attempt["duration_ns"] for record in successful
            for attempt in record["bounded"]["attempts"]
        ]
        gates = {
            "all_cases_validated": len(successful) == len(records),
            "local_avm_project_exact": all(
                record["bounded"].get("local_sse_matches_decode", False)
                and record["bounded"].get("avm_project_exact", False)
                for record in successful),
            "deterministic": all(
                record["bounded"].get("deterministic", False)
                for record in successful),
            "no_plane_sse_regression": all(
                all(gain >= 0 for gain in record["sse_gain"])
                for record in successful),
            "rd_cost_nonregression": all(
                record["rd_cost_gain"] >= 0 for record in successful),
            "strict_directional_wins": len(strict_wins) >= 8,
            "prepared_image_wins": len(prepared_wins) >= 4,
            "chroma_dc_preserved": all(
                record["selected_modes"].get("chroma_mode", 0) == 0
                and sum(record["sse_gain"][1:]) == 0
                for record in successful
                if record["pixel_format"] != "i400"),
            "noncardinal_luma_reachable": any(
                mode not in (0, 26, 54) for mode in luma_modes),
        }
        report = {
            "schema": "av2-encoder-full-directional-gate-v1",
            "status": "pass" if all(gates.values()) else "blocked",
            "configuration": {
                "case_count": len(records),
                "formats": list(FORMATS),
                "qindex": QINDEX,
                "bounded_candidate_count": 29,
                "syntax_oracle_candidate_count": 56,
                "luma_interpolation": "four-tap IDIF",
                "chroma_interpolation": "two-sample linear",
                "production_chroma_mode": "DC",
                "mrl_index": 0,
                "dip": False,
                "ibp": False,
            },
            "corpus": corpus,
            "gates": gates,
            "summary": {
                "successful_cases": len(successful),
                "strict_wins": len(strict_wins),
                "prepared_wins": len(prepared_wins),
                "chroma_wins": len(chroma_wins),
                "total_sse_gain": sum(
                    sum(record["sse_gain"]) for record in successful),
                "total_byte_cost": sum(
                    record["bounded"].get("obu_bytes", 0) -
                    record["off"].get("obu_bytes", 0)
                    for record in successful),
                "selected_luma_modes": sorted(luma_modes),
                "selected_chroma_modes": sorted(chroma_modes),
                "median_bounded_time_ns":
                    int(statistics.median(times)) if times else 0,
                "worst_bounded_time_ns": max(times, default=0),
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

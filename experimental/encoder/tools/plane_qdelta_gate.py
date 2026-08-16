#!/usr/bin/env python3
"""Freeze production U/V frame quantizer-delta search."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path
from typing import Any

from ac8x8_color_gate import (
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT,
    ENCODER,
    PREPARED_LOCK,
    ROOT,
    encode,
)


DEFAULT_OUTPUT = ENCODER / "metrics/plane-qdelta-gate.json"
DEFAULT_WORK = ENCODER / "build/plane-qdelta-gate"
BLOCKS = ENCODER / "fixtures/m3-plane-qdelta-blocks.json"
QINDICES = (141, 190)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def tiled_plane(values: list[int], source_size: int, target_size: int) -> bytes:
    return bytes(
        values[(y % source_size) * source_size + x % source_size]
        for y in range(target_size) for x in range(target_size)
    )


def generated_cases() -> list[dict[str, Any]]:
    blocks = json.loads(BLOCKS.read_text(encoding="utf-8"))["blocks"]
    cases = []
    for pixel_format, chroma_size, block_name in (
        ("i420", 32, "color-gradient-4"),
        ("i444", 64, "color-gradient-8"),
    ):
        block = blocks[block_name]
        luma = bytes([128]) * (64 * 64)
        source = luma
        source += tiled_plane(block["u"], block["size"], chroma_size)
        source += tiled_plane(block["v"], block["size"], chroma_size)
        for qindex in QINDICES:
            cases.append({
                "id": f"tiled-gradient-{pixel_format}-q{qindex}",
                "subset": "generated",
                "pixel_format": pixel_format,
                "width": 64,
                "height": 64,
                "qindex": qindex,
                "source": source,
                "source_sha256": sha256_bytes(source),
            })
    return cases


def prepared_case() -> tuple[dict[str, Any], dict[str, str]]:
    lock = json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))
    record = next(
        item for item in lock["preparations"]
        if item["source_id"] == "pitti-ceiling"
        and item["preparation_id"] == "le896"
    )
    output = record["outputs"]["yuv444p8"]
    path = (ROOT / "images/prepared/le896/pitti-ceiling" /
            output["filename"])
    source = path.read_bytes()
    if sha256_bytes(source) != output["sha256"]:
        raise RuntimeError("prepared input hash mismatch")
    width, height = map(int, record["dimensions"])
    return {
        "id": "prepared-pitti-ceiling-i444-q100",
        "subset": "prepared",
        "pixel_format": "i444",
        "width": width,
        "height": height,
        "qindex": 100,
        "source": source,
        "source_sha256": output["sha256"],
    }, {
        "prepared_lock_sha256": sha256_bytes(PREPARED_LOCK.read_bytes()),
        str(path.relative_to(ROOT)): output["sha256"],
    }


def plane_sse(result: dict[str, Any]) -> list[int]:
    return [int(plane["sse"]) for plane in result.get("planes", [])]


def selected_deltas(result: dict[str, Any]) -> list[int]:
    stderr = str(result["attempts"][-1]["stderr"])
    values = []
    for name in ("qdelta_u", "qdelta_v"):
        marker = f"{name}="
        values.append(int(stderr.split(marker, 1)[1].split()[0]))
    return values


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

    prepared, corpus = prepared_case()
    cases = generated_cases() + [prepared]
    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True)
    records = []
    try:
        for case in cases:
            root = args.work_dir / case["id"]
            baseline = encode(
                case, "off", 1, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                root / "off")
            candidate = encode(
                case, "plane-qdelta", 2, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                root / "plane-qdelta")
            before = plane_sse(baseline)
            after = plane_sse(candidate)
            record = {
                key: value for key, value in case.items() if key != "source"
            }
            record.update({
                "off": baseline,
                "plane_qdelta": candidate,
                "selected_deltas": selected_deltas(candidate),
                "sse_gain": [
                    left - right for left, right in zip(before, after)
                ],
                "byte_delta":
                    candidate.get("obu_bytes", 0) -
                    baseline.get("obu_bytes", 0),
            })
            records.append(record)

        successful = [
            record for record in records
            if all(record[name].get("status") == "pass"
                   for name in ("off", "plane_qdelta"))
        ]
        strict = [
            record for record in successful
            if sum(record["sse_gain"][1:]) > 0
        ]
        gates = {
            "all_cases_validated": len(successful) == len(records),
            "local_avm_project_exact": all(
                record["plane_qdelta"].get(
                    "local_sse_matches_decode", False)
                and record["plane_qdelta"].get("avm_project_exact", False)
                for record in successful),
            "deterministic": all(
                record["plane_qdelta"].get("deterministic", False)
                for record in successful),
            "y_invariant": all(
                record["sse_gain"][0] == 0 for record in successful),
            "chroma_sse_nonregression": all(
                sum(record["sse_gain"][1:]) >= 0
                for record in successful),
            "per_case_rate_cap": all(
                record["byte_delta"] <= 2 for record in successful),
            "generated_strict_wins": len([
                record for record in strict
                if record["subset"] == "generated"
            ]) == 4,
            "both_formats_select_deltas": {
                record["pixel_format"] for record in successful
                if record["selected_deltas"] != [0, 0]
            } == {"i420", "i444"},
            "prepared_image_validated": any(
                record["subset"] == "prepared"
                and sum(record["sse_gain"][1:]) >= 0
                and record["byte_delta"] <= 0
                for record in successful),
        }
        report = {
            "schema": "av2-encoder-plane-qdelta-gate-v1",
            "status": "pass" if all(gates.values()) else "blocked",
            "configuration": {
                "candidate_deltas": [0, -16, -8, 8, 16],
                "maximum_added_bytes": 2,
                "formats": ["i420", "i444"],
                "qindices": list(QINDICES),
                "dc_ac_tied_per_plane": True,
            },
            "corpus": corpus,
            "gates": gates,
            "summary": {
                "successful_cases": len(successful),
                "strict_wins": len(strict),
                "total_chroma_sse_gain": sum(
                    sum(record["sse_gain"][1:]) for record in successful),
                "total_byte_delta": sum(
                    record["byte_delta"] for record in successful),
                "selected_delta_pairs": sorted({
                    tuple(record["selected_deltas"])
                    for record in successful
                }),
                "maximum_workspace_bytes": max(
                    (record["plane_qdelta"].get("workspace_bytes", 0)
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

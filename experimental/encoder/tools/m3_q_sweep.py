#!/usr/bin/env python3
"""Run the locked M3 qindex sweep with three-way reconstruction parity."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Sequence

from m3_benchmark import (
    BenchmarkError,
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT_DECODER,
    ROOT,
    decode_commands,
    distortion,
    executable,
    generated_pixels,
    sha256_bytes,
    sha256_file,
)
from m3_dc_model import choose_dc_level, dc_prediction


ENCODER_ROOT = ROOT / "experimental" / "encoder"
DEFAULT_SUITE = ENCODER_ROOT / "metrics" / "m3-q-sweep.json"
DEFAULT_OUTPUT = ENCODER_ROOT / "metrics" / "m3-q-sweep-results.json"
DEFAULT_WORK = ENCODER_ROOT / "build" / "m3-q-sweep"


def fixture_pixels(fixture: dict[str, Any]) -> bytes:
    if fixture["kind"] == "generated":
        return generated_pixels(fixture)
    if fixture["kind"] != "prepared-crop":
        raise BenchmarkError(f"unknown sweep fixture kind: {fixture['kind']}")
    source_path = ROOT / fixture["path"]
    if not source_path.is_file():
        raise BenchmarkError(f"prepared source is absent: {fixture['path']}")
    source = source_path.read_bytes()
    if sha256_bytes(source) != fixture["sha256"]:
        raise BenchmarkError(f"prepared source hash mismatch: {fixture['path']}")
    source_width = int(fixture["source_width"])
    source_height = int(fixture["source_height"])
    if len(source) != source_width * source_height:
        raise BenchmarkError(f"prepared source size mismatch: {fixture['path']}")
    x = int(fixture["x"])
    y = int(fixture["y"])
    width = int(fixture["width"])
    height = int(fixture["height"])
    if x < 0 or y < 0 or x + width > source_width or y + height > source_height:
        raise BenchmarkError(f"crop is outside prepared source: {fixture['id']}")
    return b"".join(
        source[row * source_width + x : row * source_width + x + width]
        for row in range(y, y + height)
    )


def local_reconstruction(
    source: bytes, width: int, height: int, qindex: int
) -> bytes:
    padded = bytearray(16 * 16)
    active = bytearray(width * height)
    for block_y in range(0, height, 8):
        for block_x in range(0, width, 8):
            above = (
                padded[(block_y - 1) * 16 + block_x:
                       (block_y - 1) * 16 + block_x + 8]
                if block_y else None
            )
            left = (
                [padded[(block_y + offset) * 16 + block_x - 1]
                 for offset in range(8)]
                if block_x else None
            )
            prediction = dc_prediction(above, left, 8)
            samples = [
                source[min(block_y + dy, height - 1) * width +
                       min(block_x + dx, width - 1)]
                for dy in range(8) for dx in range(8)
            ]
            value = choose_dc_level(
                samples, prediction, 8, qindex
            ).reconstruction
            for dy in range(8):
                start = (block_y + dy) * 16 + block_x
                padded[start:start + 8] = bytes([value]) * 8
    for y in range(height):
        active[y * width:(y + 1) * width] = \
            padded[y * 16:y * 16 + width]
    return bytes(active)


def run_command(command: Sequence[str]) -> subprocess.CompletedProcess[bytes]:
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    return subprocess.run(
        command, cwd=ROOT, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
    )


def run_sweep(
    suite: dict[str, Any], encoder: Path, avmdec: Path,
    project_decoder: Path, work: Path,
) -> dict[str, Any]:
    qindexes = [int(value) for value in suite["qindexes"]]
    if suite.get("quality_direction") != "lower_qindex_is_higher_quality":
        raise BenchmarkError("unsupported quality direction")
    if (not qindexes or any(not 1 <= value <= 255 for value in qindexes) or
            any(left <= right for left, right in zip(qindexes, qindexes[1:]))):
        raise BenchmarkError(
            "qindexes must be a strictly descending list within 1..255"
        )
    work.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, Any]] = []
    aggregate = {qindex: {"sse": 0, "obu_bytes": 0} for qindex in qindexes}
    for fixture in suite["fixtures"]:
        source = fixture_pixels(fixture)
        width = int(fixture["width"])
        height = int(fixture["height"])
        if len(source) != width * height:
            raise BenchmarkError(f"fixture size mismatch: {fixture['id']}")
        fixture_dir = work / fixture["id"]
        fixture_dir.mkdir(parents=True, exist_ok=True)
        source_path = fixture_dir / "source.i400"
        source_path.write_bytes(source)
        for qindex in qindexes:
            case_dir = fixture_dir / f"q{qindex}"
            case_dir.mkdir(parents=True, exist_ok=True)
            obu = case_dir / "encoded.obu"
            command = [
                str(encoder), "--i400-8", str(source_path),
                "--width", str(width), "--height", str(height),
                "--qindex", str(qindex), str(obu),
            ]
            encoded = run_command(command)
            if encoded.returncode != 0:
                raise BenchmarkError(
                    f"encoder failed for {fixture['id']} q={qindex}: "
                    f"{encoded.stderr.decode('utf-8', 'backslashreplace')}"
                )
            avm_command, project_command, avm_path, project_path = \
                decode_commands(avmdec, project_decoder, obu, case_dir)
            avm_result = run_command(avm_command)
            project_result = run_command(project_command)
            if avm_result.returncode != 0 or project_result.returncode != 0:
                raise BenchmarkError(
                    f"decoder rejected {fixture['id']} q={qindex}"
                )
            avm = avm_path.read_bytes()
            project = project_path.read_bytes()
            local = local_reconstruction(source, width, height, qindex)
            if not (local == avm == project):
                raise BenchmarkError(
                    f"local/AVM/project mismatch for {fixture['id']} q={qindex}"
                )
            metrics = distortion(source, local)
            obu_bytes = obu.stat().st_size
            aggregate[qindex]["sse"] += int(metrics["sse"])
            aggregate[qindex]["obu_bytes"] += obu_bytes
            records.append({
                "fixture": fixture["id"],
                "kind": fixture["kind"],
                "width": width,
                "height": height,
                "qindex": qindex,
                "source_sha256": sha256_bytes(source),
                "reconstruction_sha256": sha256_bytes(local),
                "local_avm_project_exact": True,
                "obu_bytes": obu_bytes,
                "obu_sha256": sha256_file(obu),
                "distortion": metrics,
            })
    aggregates = [
        {"qindex": qindex, **aggregate[qindex]} for qindex in qindexes
    ]
    monotonic = all(
        current["sse"] <= previous["sse"]
        for previous, current in zip(aggregates, aggregates[1:])
    )
    if not monotonic:
        raise BenchmarkError(
            "aggregate SSE increased while quality increased (qindex decreased)"
        )
    return {
        "schema": "av2-encoder-m3-q-sweep-results-v1",
        "quality_direction": suite["quality_direction"],
        "qindexes": qindexes,
        "fixture_count": len(suite["fixtures"]),
        "case_count": len(records),
        "local_avm_project_parity": True,
        "aggregate_sse_non_increasing_as_quality_increases": True,
        "aggregates": aggregates,
        "cases": records,
        "tools": {
            "encoder_sha256": sha256_file(encoder),
            "avmdec_sha256": sha256_file(avmdec),
            "project_decoder_sha256": sha256_file(project_decoder),
        },
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    result.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    result.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    result.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER)
    result.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    result.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        suite = json.loads(args.suite.read_text(encoding="utf-8"))
        result = run_sweep(
            suite,
            executable(args.encoder, "experimental encoder"),
            executable(args.avmdec, "pinned avmdec"),
            executable(args.project_decoder, "project decoder"),
            args.work_dir,
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except (BenchmarkError, OSError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"{result['case_count']} cases: parity PASS, aggregate SSE monotonic PASS"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Compare local and AVM decoder output for viewer demo OBU files."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ASSETS = ROOT / "docs" / "assets"
DEFAULT_LOCAL = ROOT / "build" / (
    "windows-x86_64/av2heif-decoder.exe"
    if os.name == "nt" else "native/av2heif-decoder"
)
DEFAULT_AVM = ROOT / "build" / "reference" / "avm" / (
    "avmdec.exe" if os.name == "nt" else "avmdec"
)
DEFAULT_OUTPUT = ROOT / "build" / "viewerdemo-parity"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=DEFAULT_ASSETS)
    parser.add_argument("--local-decoder", type=Path, default=DEFAULT_LOCAL)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVM)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--jobs", type=int, default=4)
    return parser.parse_args()


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def failure_record(completed: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    return {
        "exit_code": completed.returncode,
        "stdout": completed.stdout.strip(),
        "stderr": completed.stderr.strip(),
    }


def compare_planes(local: bytes, avm: bytes, width: int, height: int) -> dict[str, Any]:
    chroma_size = ((width + 1) // 2) * ((height + 1) // 2)
    planes = (
        ("Y", 0, width * height, width),
        ("U", width * height, chroma_size, (width + 1) // 2),
        ("V", width * height + chroma_size, chroma_size, (width + 1) // 2),
    )
    result: dict[str, Any] = {}
    for name, offset, size, stride in planes:
        mismatches = 0
        maximum_difference = 0
        first_offset = -1
        for index in range(size):
            difference = abs(local[offset + index] - avm[offset + index])
            if difference == 0:
                continue
            if first_offset < 0:
                first_offset = index
            mismatches += 1
            maximum_difference = max(maximum_difference, difference)
        result[name] = {
            "mismatches": mismatches,
            "max_difference": maximum_difference,
            "first_offset": first_offset,
            "first_x": first_offset % stride if first_offset >= 0 else -1,
            "first_y": first_offset // stride if first_offset >= 0 else -1,
        }
    return result


def process_variant(
    scene: dict[str, Any],
    variant: dict[str, Any],
    assets: Path,
    output: Path,
    local_decoder: Path,
    avmdec: Path,
) -> dict[str, Any]:
    width = int(scene["width"])
    height = int(scene["height"])
    expected_size = width * height + 2 * ((width + 1) // 2) * ((height + 1) // 2)
    obu = assets / variant["file"]
    stem = obu.stem
    local_output = output / f"{stem}-local.i420"
    avm_output = output / f"{stem}-avm.i420"
    avm_pattern = output / f"{stem}-avm-%wx%h-%1.i420"
    record: dict[str, Any] = {
        "scene": scene["id"],
        "qp": variant["qp"],
        "obu": obu.name,
    }

    local_completed = run(
        [str(local_decoder), str(obu), "-o", str(local_output)]
    )
    if local_completed.returncode != 0:
        record["result"] = "local_decode_failed"
        record["local"] = failure_record(local_completed)
        return record

    for stale in output.glob(f"{stem}-avm-*x*-*.i420"):
        stale.unlink()
    avm_completed = run(
        [
            str(avmdec), "--codec=av2", "--i420", "--limit=1",
            "-o", str(avm_pattern), str(obu),
        ]
    )
    if avm_completed.returncode != 0:
        record["result"] = "avm_decode_failed"
        record["avm"] = failure_record(avm_completed)
        return record
    matches = list(output.glob(f"{stem}-avm-*x*-1.i420"))
    if len(matches) != 1:
        record["result"] = "avm_output_missing"
        record["avm"] = failure_record(avm_completed)
        return record
    matches[0].replace(avm_output)

    local_bytes = local_output.read_bytes()
    avm_bytes = avm_output.read_bytes()
    record["local_size"] = len(local_bytes)
    record["avm_size"] = len(avm_bytes)
    record["expected_size"] = expected_size
    if len(local_bytes) != expected_size or len(avm_bytes) != expected_size:
        record["result"] = "output_size_mismatch"
        return record

    planes = compare_planes(local_bytes, avm_bytes, width, height)
    record["planes"] = planes
    record["total_mismatches"] = sum(
        plane["mismatches"] for plane in planes.values()
    )
    record["result"] = "exact" if record["total_mismatches"] == 0 else "mismatch"
    return record


def main() -> None:
    args = parse_args()
    assets = args.assets.resolve()
    output = args.output.resolve()
    local_decoder = args.local_decoder.resolve()
    avmdec = args.avmdec.resolve()
    for tool in (local_decoder, avmdec):
        if not tool.is_file():
            raise RuntimeError(f"missing decoder: {tool}")
    manifest = json.loads(
        (assets / "manifest.json").read_text(encoding="utf-8")
    )
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    work = [
        (scene, variant)
        for scene in manifest["scenes"]
        for variant in scene["variants"]
    ]
    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {
            pool.submit(
                process_variant, scene, variant, assets, output,
                local_decoder, avmdec,
            ): (scene["id"], variant["qp"])
            for scene, variant in work
        }
        for future in concurrent.futures.as_completed(futures):
            record = future.result()
            records.append(record)
            print(
                f"{record['scene']:11s} QP {record['qp']:3d}: "
                f"{record['result']}"
                + (
                    f" ({record.get('total_mismatches', 0)} bytes)"
                    if record["result"] == "mismatch" else ""
                )
            )

    records.sort(key=lambda item: (item["scene"], item["qp"]))
    counts: dict[str, int] = {}
    for record in records:
        counts[record["result"]] = counts.get(record["result"], 0) + 1
    report = {
        "schema": "av2h-viewerdemo-decoder-parity-v1",
        "local_decoder": str(local_decoder),
        "avmdec": str(avmdec),
        "counts": counts,
        "cases": records,
    }
    report_path = output / "report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Summary: {json.dumps(counts, sort_keys=True)}")
    print(f"Report: {report_path}")


if __name__ == "__main__":
    main()
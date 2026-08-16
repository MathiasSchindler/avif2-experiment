#!/usr/bin/env python3
"""Validate explicit production luma AC across I400, I420, and I444."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import re
import shutil
import statistics
import subprocess
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
DEFAULT_ENCODER = ENCODER / "build" / "av2-encoder-experiment"
DEFAULT_DRIVER = ENCODER / "build" / "ac8x8-color-gate-driver"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_PROJECT = ROOT / "build" / "host-cmake" / "av2heif-decoder"
DEFAULT_OUTPUT = ENCODER / "metrics" / "ac8x8-color-gate.json"
DEFAULT_WORK = ENCODER / "build" / "ac8x8-color-gate"
PREPARED_LOCK = ROOT / "images" / "recipes" / "prepared-lock.json"
AC_HEADER = ENCODER / "include" / "encoder" / "ac8x8.h"

FORMATS = ("i400", "i420", "i444")
SPECS = (
    ("odd-detail", 9, 15, 100, "detail", "neutral"),
    ("even-texture", 16, 16, 60, "texture", "non-neutral"),
    ("edge", 64, 64, 180, "edge", "neutral"),
    ("multisuperblock-prepared", 72, 64, 100, "prepared", "non-neutral"),
)
MAX_OBU_BYTES = 8192
MAX_CASE_TIME_NS = 5_000_000_000
MAX_WORKSPACE_BYTES = 1_048_576
MAX_WORK_BYTES = 33_554_432

METRIC = re.compile(
    rb"encoder: bytes=(\d+) sse_y=(\d+)(?: sse_u=(\d+) sse_v=(\d+))?"
    rb"(?: psnr_y=(?:inf|\d+\.\d+)dB)? workspace_bytes=(\d+) qindex=(\d+)"
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def plane_geometry(pixel_format: str, width: int,
                   height: int) -> list[tuple[int, int]]:
    if pixel_format == "i400":
        return [(width, height)]
    if pixel_format == "i420":
        chroma = ((width + 1) // 2, (height + 1) // 2)
        return [(width, height), chroma, chroma]
    return [(width, height)] * 3


def split_planes(data: bytes, pixel_format: str, width: int,
                 height: int) -> list[bytes]:
    result = []
    offset = 0
    for plane_width, plane_height in plane_geometry(
            pixel_format, width, height):
        size = plane_width * plane_height
        result.append(data[offset:offset + size])
        offset += size
    if offset != len(data):
        raise RuntimeError("planar byte count mismatch")
    return result


def prepared_luma() -> tuple[bytes, int, int, dict[str, str]]:
    lock = json.loads(PREPARED_LOCK.read_text(encoding="utf-8"))
    record = next(
        item for item in lock["preparations"]
        if item["source_id"] == "pitti-ceiling"
        and item["preparation_id"] == "le896"
    )
    width, height = map(int, record["dimensions"])
    output = record["outputs"]["gray8"]
    path = (ROOT / "images" / "prepared" / "le896" / "pitti-ceiling"
            / output["filename"])
    data = path.read_bytes()
    if sha256_bytes(data) != output["sha256"]:
        raise RuntimeError("locked prepared luma hash mismatch")
    return data, width, height, {
        "prepared_lock_sha256": sha256_file(PREPARED_LOCK),
        "prepared_plane": str(path.relative_to(ROOT)),
        "prepared_plane_sha256": output["sha256"],
    }


def make_luma(kind: str, width: int, height: int,
              prepared: tuple[bytes, int, int, dict[str, str]]) -> bytes:
    if kind == "prepared":
        data, source_width, source_height, _ = prepared
        x0 = (source_width - width) // 2
        y0 = (source_height - height) // 2
        return b"".join(
            data[(y0 + y) * source_width + x0:
                 (y0 + y) * source_width + x0 + width]
            for y in range(height)
        )
    output = bytearray()
    for y in range(height):
        for x in range(width):
            if kind == "edge":
                value = 36 if x * height < y * width else 220
            elif kind == "texture":
                value = 48 if ((x // 2) ^ (y // 3)) & 1 else 208
            else:
                value = (x * 37 + y * 73 + x * y * 3 + 19) & 255
            output.append(value)
    return bytes(output)


def make_chroma(width: int, height: int, plane: int,
                variant: str) -> bytes:
    if variant == "neutral":
        return bytes([128]) * (width * height)
    return bytes(
        (41 + plane * 83 + x * (11 + plane * 2)
         + y * (17 + plane * 4)) & 255
        for y in range(height) for x in range(width)
    )


def fixture_cases() -> tuple[list[dict[str, Any]], dict[str, str]]:
    prepared = prepared_luma()
    cases = []
    for name, width, height, qindex, kind, chroma in SPECS:
        luma = make_luma(kind, width, height, prepared)
        for pixel_format in FORMATS:
            planes = [luma]
            for plane, geometry in enumerate(
                    plane_geometry(pixel_format, width, height)[1:], 1):
                planes.append(make_chroma(*geometry, plane, chroma))
            source = b"".join(planes)
            cases.append({
                "id": f"{name}-{pixel_format}",
                "pixel_format": pixel_format,
                "width": width,
                "height": height,
                "qindex": qindex,
                "texture": kind,
                "chroma": "none" if pixel_format == "i400" else chroma,
                "source": source,
                "source_sha256": sha256_bytes(source),
                "luma_sha256": sha256_bytes(luma),
            })
    return cases, prepared[3]


def run(command: list[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    start = time.monotonic_ns()
    completed = subprocess.run(
        command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return completed, time.monotonic_ns() - start


def decode(obu: Path, pixel_format: str, avmdec: Path, project: Path,
           directory: Path) -> tuple[bytes, dict[str, int]]:
    avm = directory / "decoded.avm"
    local_project = directory / f"decoded.project.{pixel_format}"
    durations = {}
    for name, command in (
        ("avm", [
            str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(avm), str(obu),
        ]),
        ("project", [str(project), str(obu), "-o", str(local_project)]),
    ):
        completed, duration = run(command)
        durations[name] = duration
        if completed.returncode != 0:
            raise RuntimeError(f"{name} decoder rejected stream")
    avm_data = avm.read_bytes()
    if avm_data != local_project.read_bytes():
        raise RuntimeError("AVM/project reconstruction mismatch")
    return avm_data, durations


def encode(case: dict[str, Any], search: str | None, repetitions: int,
           encoder: Path, avmdec: Path, project: Path,
           directory: Path) -> dict[str, Any]:
    directory.mkdir(parents=True)
    source_path = directory / f"source.{case['pixel_format']}"
    source_path.write_bytes(case["source"])
    attempts = []
    output_hashes = []
    first_output = directory / "encoded-0.obu"
    metric_match = None
    for attempt in range(repetitions):
        output = directory / f"encoded-{attempt}.obu"
        command = [
            str(encoder), str(source_path), "--width", str(case["width"]),
            "--height", str(case["height"]), "--qindex", str(case["qindex"]),
        ]
        if search is not None:
            command.extend(("--search", search))
        command.extend(("--output", str(output)))
        completed, duration = run(command)
        metric_match = METRIC.search(completed.stderr)
        attempts.append({
            "returncode": completed.returncode,
            "duration_ns": duration,
            "stderr": completed.stderr.decode(errors="replace").strip(),
            "output_exists": output.is_file(),
        })
        if completed.returncode != 0:
            return {
                "status": "unsupported" if completed.returncode == 5 else "error",
                "attempts": attempts,
                "deterministic": len(attempts) < 2,
            }
        if metric_match is None:
            return {"status": "error", "reason": "encoder metrics missing",
                    "attempts": attempts}
        output_hashes.append(sha256_file(output))
    if len(set(output_hashes)) != 1:
        return {"status": "error", "reason": "nondeterministic OBU",
                "attempts": attempts}
    try:
        decoded, decode_times = decode(
            first_output, case["pixel_format"], avmdec, project, directory)
    except RuntimeError as error:
        failed = {
            "status": "error",
            "reason": str(error),
            "attempts": attempts,
            "deterministic": True,
            "obu_bytes": first_output.stat().st_size,
            "obu_sha256": output_hashes[0],
        }
        if metric_match is not None:
            failed["reported_sse"] = [
                int(value) for value in metric_match.groups()[1:4]
                if value is not None
            ]
            failed["workspace_bytes"] = int(metric_match.group(5))
        return failed
    source_planes = split_planes(
        case["source"], case["pixel_format"], case["width"], case["height"])
    decoded_planes = split_planes(
        decoded, case["pixel_format"], case["width"], case["height"])
    plane_records = []
    for name, source_plane, decoded_plane in zip(
            ("y", "u", "v"), source_planes, decoded_planes):
        plane_records.append({
            "plane": name,
            "sse": sum((a - b) ** 2
                       for a, b in zip(source_plane, decoded_plane)),
            "sha256": sha256_bytes(decoded_plane),
        })
    assert metric_match is not None
    reported = [int(metric_match.group(2))]
    if metric_match.group(3) is not None:
        reported.extend((
            int(metric_match.group(3)), int(metric_match.group(4))))
    return {
        "status": "pass",
        "attempts": attempts,
        "deterministic": True,
        "obu_bytes": first_output.stat().st_size,
        "obu_sha256": output_hashes[0],
        "decoded_sha256": sha256_bytes(decoded),
        "planes": plane_records,
        "local_sse_matches_decode":
            reported == [plane["sse"] for plane in plane_records],
        "avm_project_exact": True,
        "workspace_bytes": int(metric_match.group(5)),
        "decode_time_ns": decode_times,
    }


def api_i400(case: dict[str, Any], driver: Path,
             bounded_dir: Path) -> dict[str, Any]:
    source = bounded_dir / "source.i400"
    stream = bounded_dir / "api.obu"
    reconstruction = bounded_dir / "api.i400"
    completed, duration = run([
        str(driver), str(source), str(case["width"]), str(case["height"]),
        str(case["qindex"]), str(stream), str(reconstruction),
    ])
    if completed.returncode != 0:
        return {"pass": False, "returncode": completed.returncode}
    cli_stream = bounded_dir / "encoded-0.obu"
    avm = bounded_dir / "decoded.avm"
    return {
        "pass": (stream.read_bytes() == cli_stream.read_bytes()
                 and reconstruction.read_bytes() == avm.read_bytes()),
        "duration_ns": duration,
        "stream_sha256": sha256_file(stream),
        "reconstruction_sha256": sha256_file(reconstruction),
    }


def negative_gates(encoder: Path, api_test: Path, work: Path) -> dict[str, Any]:
    invalid_source = work / "invalid.i400"
    invalid_source.write_bytes(bytes(64))
    invalid, _ = run([
        str(encoder), str(invalid_source), "--width", "8", "--height", "8",
        "--search", "invalid", "--output", str(work / "invalid.obu"),
    ])
    sentinel = work / "sentinel.obu"
    source = work / "unsupported.i420"
    sentinel.write_bytes(b"sentinel")
    source.write_bytes(bytes([128]) * 96)
    unsupported, _ = run([
        str(encoder), str(source), "--width", "8", "--height", "8",
        "--search", "bounded", "--output", str(sentinel),
    ])
    api_capacity, _ = run([str(api_test)])
    atomic, _ = run([
        "sh", str(ENCODER / "tests" / "atomic_output.sh"), str(encoder),
        str(work / "atomic"),
    ])
    return {
        "invalid_search_rejected": invalid.returncode == 3,
        "bounded_color_reachable": (
            unsupported.returncode == 0 and sentinel.read_bytes() != b"sentinel"
        ),
        "api_capacity_and_atomic_result": api_capacity.returncode == 0,
        "cli_atomic_output": atomic.returncode == 0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--api-driver", type=Path, default=DEFAULT_DRIVER)
    parser.add_argument(
        "--api-test", type=Path, default=ENCODER / "build" / "ac8x8-test")
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    args = parser.parse_args()
    required = (
        args.encoder, args.api_driver, args.api_test,
        args.avmdec, args.project_decoder,
    )
    for path in required:
        if not path.is_file():
            raise SystemExit(f"missing executable: {path}")

    shutil.rmtree(args.work_dir, ignore_errors=True)
    args.work_dir.mkdir(parents=True)
    try:
        cases, corpus = fixture_cases()
        records = []
        for case in cases:
            case_root = args.work_dir / case["id"]
            default = encode(
                case, None, 1, args.encoder.resolve(), args.avmdec.resolve(),
                args.project_decoder.resolve(), case_root / "default")
            off = encode(
                case, "off", 1, args.encoder.resolve(), args.avmdec.resolve(),
                args.project_decoder.resolve(), case_root / "off")
            bounded = encode(
                case, "bounded", 2, args.encoder.resolve(),
                args.avmdec.resolve(), args.project_decoder.resolve(),
                case_root / "bounded")
            record = {
                key: value for key, value in case.items() if key != "source"
            }
            record.update({"default": default, "off": off, "bounded": bounded})
            record["default_off_legacy_exact"] = (
                default.get("obu_sha256") is not None
                and default.get("obu_sha256") == off.get("obu_sha256")
            )
            if bounded.get("status") == "pass":
                off_y_sse = (
                    off["planes"][0]["sse"] if "planes" in off
                    else off["reported_sse"][0]
                )
                record["luma_sse_gain"] = (
                    off_y_sse - bounded["planes"][0]["sse"])
                record["luma_win"] = record["luma_sse_gain"] > 0
                record["byte_cost"] = (
                    bounded["obu_bytes"] - off["obu_bytes"])
                record["uv_unchanged"] = (
                    case["pixel_format"] == "i400"
                    or ("planes" in off
                        and [plane["sha256"]
                             for plane in bounded["planes"][1:]]
                        == [plane["sha256"]
                            for plane in off["planes"][1:]])
                    or (case["chroma"] == "neutral"
                        and all(plane["sse"] == 0
                                for plane in bounded["planes"][1:]))
                )
                if case["pixel_format"] == "i400":
                    record["api"] = api_i400(
                        case, args.api_driver.resolve(),
                        case_root / "bounded")
            records.append(record)

        header = AC_HEADER.read_text(encoding="utf-8")
        api_surface = {
            "i400": "enc_ac8x8_i400_q_workspace" in header,
            "i420": "enc_ac8x8_yuv8_q_workspace" in header,
            "i444": "enc_ac8x8_yuv8_q_workspace" in header,
        }
        negative = negative_gates(
            args.encoder.resolve(), args.api_test.resolve(), args.work_dir)
        successful = [
            item for item in records if item["bounded"]["status"] == "pass"]
        color = [item for item in successful
                 if item["pixel_format"] != "i400"]
        times = [
            attempt["duration_ns"] for item in successful
            for attempt in item["bounded"]["attempts"]
        ]
        work_bytes = sum(
            path.stat().st_size for path in args.work_dir.rglob("*")
            if path.is_file())
        gates = {
            "cli_reachability_all_formats": all(
                any(item["pixel_format"] == pixel_format
                    and item["bounded"]["status"] == "pass"
                    for item in records)
                for pixel_format in FORMATS),
            "api_reachability_all_formats": all(api_surface.values()),
            "local_avm_project_exact": all(
                item["bounded"].get("local_sse_matches_decode", False)
                and item["bounded"].get("avm_project_exact", False)
                and (item["pixel_format"] != "i400"
                     or item.get("api", {}).get("pass", False))
                for item in successful
            ) and len(successful) == len(records),
            "luma_wins": all(
                item.get("luma_sse_gain", 0) >= 0 for item in records
            ) and all(
                any(item.get("luma_win", False)
                    for item in records
                    if item["pixel_format"] == pixel_format)
                for pixel_format in FORMATS
            ),
            "uv_unchanged": len(color) == 8
                and all(item.get("uv_unchanged", False) for item in color),
            "deterministic_hashes": all(
                item["bounded"].get("deterministic", False)
                for item in records),
            "bounded_resources": (
                bool(successful)
                and max(item["bounded"]["obu_bytes"]
                        for item in successful) <= MAX_OBU_BYTES
                and max(item["bounded"]["workspace_bytes"]
                        for item in successful) <= MAX_WORKSPACE_BYTES
                and max(times) <= MAX_CASE_TIME_NS
                and work_bytes <= MAX_WORK_BYTES
            ),
            "legacy_hashes": all(
                item["default_off_legacy_exact"] for item in records),
            "error_capacity_atomic": all(negative.values()),
        }
        blockers = [name for name, passed in gates.items() if not passed]
        by_format = {}
        for pixel_format in FORMATS:
            format_records = [
                item for item in records
                if item["pixel_format"] == pixel_format]
            comparable = [
                item for item in format_records
                if item["bounded"]["status"] == "pass"]
            encoded = [
                item for item in format_records
                if item["bounded"].get("obu_bytes") is not None]
            by_format[pixel_format] = {
                "case_count": 4,
                "bounded_successes": len(comparable),
                "bounded_encoded_cases": len(encoded),
                "wins": sum(item.get("luma_win", False)
                            for item in comparable),
                "off_bytes_all_cases": sum(
                    item["off"].get("obu_bytes", 0)
                    for item in format_records),
                "off_bytes_comparable": sum(
                    item["off"]["obu_bytes"] for item in comparable),
                "bounded_encoded_bytes": sum(
                    item["bounded"]["obu_bytes"] for item in encoded),
                "bounded_comparable_bytes": sum(
                    item["bounded"]["obu_bytes"] for item in comparable),
                "byte_cost": sum(item.get("byte_cost", 0)
                                 for item in comparable),
            }
        report = {
            "schema": "av2-encoder-ac8x8-color-gate-v1",
            "generated_by":
                "experimental/encoder/tools/ac8x8_color_gate.py",
            "status": "pass" if not blockers else "blocked",
            "acceptance": {
                "default_on_equal_rate_required": False,
                "limits": {
                    "max_obu_bytes": MAX_OBU_BYTES,
                    "max_case_time_ns": MAX_CASE_TIME_NS,
                    "max_workspace_bytes": MAX_WORKSPACE_BYTES,
                    "max_work_bytes": MAX_WORK_BYTES,
                },
            },
            "corpus": corpus,
            "configuration": {
                "formats": list(FORMATS),
                "case_specs": [list(item) for item in SPECS],
                "case_count": len(records),
            },
            "tools": {
                "encoder_sha256": sha256_file(args.encoder),
                "api_driver_sha256": sha256_file(args.api_driver),
                "avmdec_sha256": sha256_file(args.avmdec),
                "project_decoder_sha256": sha256_file(args.project_decoder),
                "python": platform.python_version(),
            },
            "api_surface": api_surface,
            "negative_paths": negative,
            "gates": gates,
            "blockers": blockers,
            "summary": {
                "case_count": len(records),
                "bounded_successes": len(successful),
                "bounded_failures": len(records) - len(successful),
                "luma_wins": sum(item.get("luma_win", False)
                                 for item in records),
                "max_workspace_bytes": max(
                    item["bounded"]["workspace_bytes"]
                    for item in successful),
                "median_bounded_time_ns": int(statistics.median(times)),
                "worst_bounded_time_ns": max(times),
                "work_bytes": work_bytes,
                "by_format": by_format,
            },
            "cases": records,
        }
        args.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8")
        print(json.dumps({
            "status": report["status"],
            "cases": len(records),
            "bounded_successes": len(successful),
            "blockers": blockers,
        }, sort_keys=True))
        return 0
    finally:
        shutil.rmtree(args.work_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())

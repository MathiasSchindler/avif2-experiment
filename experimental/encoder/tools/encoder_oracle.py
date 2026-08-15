#!/usr/bin/env python3
"""Compare an experimental AV2 OBU against the AVM reference decoder."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_OUTPUT_ROOT = ROOT / "build/encoder-oracle"
PROJECT_DECODER_CANDIDATES = (
    ROOT / "build/av2heif-decoder",
    ROOT / "build/diagnostics/av2heif-decoder",
    ROOT / "build/host/av2heif-decoder",
    ROOT / "build/host-sanitized/av2heif-decoder",
)


@dataclass(frozen=True)
class RawFormat:
    name: str
    extension: str
    bytes_per_sample: int
    chroma_x_shift: int | None
    chroma_y_shift: int | None


FORMATS = {
    "gray8": RawFormat("gray8", ".i400", 1, None, None),
    "yuv420p8": RawFormat("yuv420p8", ".i420", 1, 1, 1),
    "yuv444p8": RawFormat("yuv444p8", ".i444", 1, 0, 0),
    "yuv420p10le": RawFormat("yuv420p10le", ".yuv420p10le", 2, 1, 1),
    "yuv444p10le": RawFormat("yuv444p10le", ".yuv444p10le", 2, 0, 0),
}
FORMAT_ALIASES = {
    "i400": "gray8",
    "i420": "yuv420p8",
    "i444": "yuv444p8",
    "yuv420p": "yuv420p8",
    "yuv444p": "yuv444p8",
}


class OracleError(RuntimeError):
    pass


def resolve_format(name: str) -> RawFormat:
    canonical = FORMAT_ALIASES.get(name.lower(), name.lower())
    try:
        return FORMATS[canonical]
    except KeyError as error:
        supported = ", ".join(sorted(FORMATS))
        raise OracleError(f"unsupported raw format {name!r}; expected one of: {supported}") from error


def plane_layout(width: int, height: int, raw_format: RawFormat) -> list[dict[str, int | str]]:
    if width <= 0 or height <= 0:
        raise OracleError("width and height must be positive")
    dimensions = [("Y", width, height)]
    if raw_format.chroma_x_shift is not None:
        chroma_width = (width + (1 << raw_format.chroma_x_shift) - 1) >> raw_format.chroma_x_shift
        chroma_height = (height + (1 << raw_format.chroma_y_shift) - 1) >> raw_format.chroma_y_shift
        dimensions.extend((("U", chroma_width, chroma_height), ("V", chroma_width, chroma_height)))
    offset = 0
    layout: list[dict[str, int | str]] = []
    for plane, plane_width, plane_height in dimensions:
        size = plane_width * plane_height * raw_format.bytes_per_sample
        layout.append(
            {
                "plane": plane,
                "width": plane_width,
                "height": plane_height,
                "offset_bytes": offset,
                "size_bytes": size,
            }
        )
        offset += size
    return layout


def normalize_raw(path: Path, width: int, height: int, raw_format: RawFormat) -> tuple[bytes, list[dict[str, int | str]]]:
    layout = plane_layout(width, height, raw_format)
    expected_size = sum(int(plane["size_bytes"]) for plane in layout)
    data = path.read_bytes()
    if len(data) != expected_size:
        raise OracleError(
            f"{path}: raw size {len(data)} does not match expected single-frame "
            f"{expected_size} bytes for {width}x{height} {raw_format.name}"
        )
    if raw_format.bytes_per_sample == 2:
        for offset in range(0, len(data), 2):
            sample = data[offset] | (data[offset + 1] << 8)
            if sample > 1023:
                raise OracleError(f"{path}: non-10-bit sample {sample} at byte offset {offset}")
    return b"".join(
        data[int(plane["offset_bytes"]) : int(plane["offset_bytes"]) + int(plane["size_bytes"])]
        for plane in layout
    ), layout


def append_jsonl(path: Path, record: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as output:
        output.write(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n")
        output.flush()
        os.fsync(output.fileno())


def run_decoder(name: str, command: Sequence[str], log_path: Path, run_id: str) -> int:
    started = time.monotonic_ns()
    completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    duration = time.monotonic_ns() - started
    append_jsonl(
        log_path,
        {
            "schema": "av2-encoder-oracle-command-v1",
            "run_id": run_id,
            "decoder": name,
            "command": list(command),
            "returncode": completed.returncode,
            "duration_ns": duration,
            "stdout": completed.stdout.decode("utf-8", "backslashreplace"),
            "stderr": completed.stderr.decode("utf-8", "backslashreplace"),
        },
    )
    if completed.returncode != 0:
        raise OracleError(f"{name} decoder failed with exit code {completed.returncode}; see {log_path}")
    return duration


def find_project_decoder(explicit: Path | None) -> Path:
    if explicit is not None:
        return explicit
    for candidate in PROJECT_DECODER_CANDIDATES:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise OracleError(
        "project decoder not found; pass --project-decoder PATH or use --reference-only"
    )


def executable(path: Path, label: str) -> Path:
    if not path.is_file():
        raise OracleError(f"{label} does not exist: {path}")
    if not os.access(path, os.X_OK):
        raise OracleError(f"{label} is not executable: {path}")
    return path.resolve()


def compare(args: argparse.Namespace) -> dict[str, Any]:
    input_path = args.input.resolve()
    if not input_path.is_file():
        raise OracleError(f"input OBU does not exist: {input_path}")
    raw_format = resolve_format(args.format)
    avmdec = executable(args.avmdec, "AVM decoder")
    project_decoder = None if args.reference_only else executable(
        find_project_decoder(args.project_decoder), "project decoder"
    )
    run_id = f"{time.time_ns()}-{os.getpid()}"
    run_dir = args.work_dir.resolve() if args.work_dir else (args.output_root / "runs" / run_id).resolve()
    run_dir.mkdir(parents=True, exist_ok=False)
    reference_output = run_dir / "reference.raw"
    project_output = run_dir / f"project{raw_format.extension}"
    bit_depth = 8 if raw_format.bytes_per_sample == 1 else 10
    reference_command = [
        str(avmdec),
        "--codec=av2",
        "--rawvideo",
        "--limit=1",
        f"--output-bit-depth={bit_depth}",
        "-o",
        str(reference_output),
        str(input_path),
    ]
    durations = {
        "reference": run_decoder("reference", reference_command, args.log, run_id)
    }
    reference, layout = normalize_raw(reference_output, args.width, args.height, raw_format)
    reference_hash = hashlib.sha256(reference).hexdigest()
    project_hash = None
    exact_equal = None
    if project_decoder is not None:
        project_command = [
            str(project_decoder),
            str(input_path),
            "-o",
            str(project_output),
        ]
        durations["project"] = run_decoder("project", project_command, args.log, run_id)
        project, _ = normalize_raw(project_output, args.width, args.height, raw_format)
        project_hash = hashlib.sha256(project).hexdigest()
        exact_equal = reference == project
    record = {
        "schema": "av2-encoder-oracle-metrics-v1",
        "run_id": run_id,
        "status": "pass" if exact_equal is not False else "mismatch",
        "input": str(input_path),
        "input_sha256": hashlib.sha256(input_path.read_bytes()).hexdigest(),
        "width": args.width,
        "height": args.height,
        "format": raw_format.name,
        "planes": layout,
        "normalized_size_bytes": len(reference),
        "reference_sha256": reference_hash,
        "project_sha256": project_hash,
        "exact_equal": exact_equal,
        "duration_ns": durations,
        "run_directory": str(run_dir),
    }
    append_jsonl(args.metrics, record)
    return record


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("input", type=Path, help="experimental raw .obu stream")
    result.add_argument("--width", required=True, type=int)
    result.add_argument("--height", required=True, type=int)
    result.add_argument("--format", required=True, help="expected planar pixel format")
    result.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    result.add_argument("--project-decoder", type=Path)
    result.add_argument("--reference-only", action="store_true")
    result.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    result.add_argument("--work-dir", type=Path, help="new directory for this run")
    result.add_argument("--metrics", type=Path, default=DEFAULT_OUTPUT_ROOT / "metrics.jsonl")
    result.add_argument("--log", type=Path, default=DEFAULT_OUTPUT_ROOT / "commands.jsonl")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        record = compare(args)
    except (OracleError, OSError) as error:
        append_jsonl(
            args.metrics,
            {
                "schema": "av2-encoder-oracle-metrics-v1",
                "status": "error",
                "input": str(args.input),
                "error": str(error),
            },
        )
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(json.dumps(record, sort_keys=True))
    return 0 if record["exact_equal"] is not False else 1


if __name__ == "__main__":
    raise SystemExit(main())

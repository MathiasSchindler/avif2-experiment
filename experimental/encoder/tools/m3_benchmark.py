#!/usr/bin/env python3
"""Run the experimental encoder M3 gray-image benchmark and write JSONL."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import subprocess
import sys
import time
from decimal import Decimal, ROUND_HALF_UP, localcontext
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[3]
ENCODER_ROOT = ROOT / "experimental" / "encoder"
DEFAULT_SUITE = ENCODER_ROOT / "metrics" / "fixtures.json"
DEFAULT_ENCODER = ENCODER_ROOT / "build" / "av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build" / "reference" / "avm" / "avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build" / "host-cmake" / "av2heif-decoder"
DEFAULT_OUTPUT = ENCODER_ROOT / "results" / "m3-benchmark.jsonl"
DEFAULT_WORK = ENCODER_ROOT / "build" / "m3-benchmark"


class BenchmarkError(RuntimeError):
    pass


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def generated_pixels(fixture: dict[str, Any]) -> bytes:
    width = int(fixture["width"])
    height = int(fixture["height"])
    generator = fixture["generator"]
    if generator == "flat":
        return bytes([int(fixture.get("value", 128))]) * (width * height)
    if generator == "ramp-x":
        denominator = max(width - 1, 1)
        return bytes((x * 255 + denominator // 2) // denominator
                     for _y in range(height) for x in range(width))
    if generator == "impulse":
        center_x, center_y = width // 2, height // 2
        return bytes(255 if (x, y) == (center_x, center_y) else 0
                     for y in range(height) for x in range(width))
    if generator == "odd-edge":
        return bytes(224 if x == width - 1 or y == height - 1 else 32
                     for y in range(height) for x in range(width))
    raise BenchmarkError(f"unknown generator: {generator}")


def distortion(reference: bytes, candidate: bytes) -> dict[str, Any]:
    if len(reference) != len(candidate):
        raise BenchmarkError(
            f"raw output size mismatch: {len(candidate)} != {len(reference)}"
        )
    sse = sum((left - right) ** 2 for left, right in zip(reference, candidate))
    samples = len(reference)
    divisor = math.gcd(sse, samples)
    result: dict[str, Any] = {
        "samples": samples,
        "sse": sse,
        "mse_numerator": sse // divisor,
        "mse_denominator": samples // divisor,
        "exact": sse == 0,
        "psnr_millidb": None,
        "psnr_infinite": sse == 0,
    }
    if sse:
        with localcontext() as context:
            context.prec = 60
            ratio = Decimal(255 * 255 * samples) / Decimal(sse)
            value = Decimal(10000) * ratio.log10()
            result["psnr_millidb"] = int(
                value.to_integral_value(rounding=ROUND_HALF_UP)
            )
    return result


def run(command: Sequence[str]) -> tuple[subprocess.CompletedProcess[bytes], int]:
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    started = time.monotonic_ns()
    completed = subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    return completed, time.monotonic_ns() - started


def command_record(
    command: Sequence[str], completed: subprocess.CompletedProcess[bytes], duration: int
) -> dict[str, Any]:
    return {
        "argv": list(command),
        "returncode": completed.returncode,
        "time_ns": duration,
        "stdout": completed.stdout.decode("utf-8", "backslashreplace"),
        "stderr": completed.stderr.decode("utf-8", "backslashreplace"),
    }


def executable(path: Path, label: str) -> Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise BenchmarkError(f"{label} is not executable: {path}")
    return path.resolve()


def input_for_fixture(fixture: dict[str, Any], fixture_dir: Path) -> tuple[Path, bytes]:
    width = int(fixture["width"])
    height = int(fixture["height"])
    expected_size = width * height
    if fixture["kind"] == "generated":
        pixels = generated_pixels(fixture)
        path = fixture_dir / "source.i400"
        path.write_bytes(pixels)
    elif fixture["kind"] == "prepared":
        path = ROOT / fixture["path"]
        if not path.is_file():
            raise BenchmarkError(f"prepared input is absent: {fixture['path']}")
        pixels = path.read_bytes()
        expected_hash = fixture.get("sha256")
        if expected_hash and sha256_bytes(pixels) != expected_hash:
            raise BenchmarkError(f"prepared input hash mismatch: {fixture['path']}")
    else:
        raise BenchmarkError(f"unknown fixture kind: {fixture['kind']}")
    if len(pixels) != expected_size:
        raise BenchmarkError(
            f"input size {len(pixels)} != {expected_size} for {width}x{height}"
        )
    return path.resolve(), pixels


def decode_commands(
    avmdec: Path, project_decoder: Path, obu: Path, fixture_dir: Path
) -> tuple[list[str], list[str], Path, Path]:
    avm_output = fixture_dir / "avm.i400"
    project_output = fixture_dir / "project.i400"
    avm = [
        str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
        "--output-bit-depth=8", "-o", str(avm_output), str(obu),
    ]
    project = [str(project_decoder), str(obu), "-o", str(project_output)]
    return avm, project, avm_output, project_output


def run_fixture(
    fixture: dict[str, Any],
    encoder: Path,
    avmdec: Path,
    project_decoder: Path,
    work_root: Path,
    reconstruction_dir: Path | None,
) -> dict[str, Any]:
    fixture_id = fixture["id"]
    fixture_dir = work_root / fixture_id
    fixture_dir.mkdir(parents=True, exist_ok=True)
    record: dict[str, Any] = {
        "schema": "av2-encoder-m3-benchmark-v1",
        "fixture": fixture_id,
        "kind": fixture["kind"],
        "profile": fixture["profile"],
        "width": int(fixture["width"]),
        "height": int(fixture["height"]),
        "commands": {},
    }
    try:
        input_path, source = input_for_fixture(fixture, fixture_dir)
    except (BenchmarkError, OSError) as error:
        record.update({"status": "blocked", "reason": str(error)})
        return record
    record["source_sha256"] = sha256_bytes(source)
    if fixture.get("metadata"):
        record["metadata"] = fixture["metadata"]
    obu = fixture_dir / "encoded.obu"
    obu.unlink(missing_ok=True)
    if fixture["encoder_mode"] == "constant-128-8x8":
        encoder_command = [str(encoder), "--constant-128-8x8", str(obu)]
    else:
        encoder_command = [
            str(encoder), "--i400-8", str(input_path),
            "--width", str(fixture["width"]), "--height", str(fixture["height"]),
            str(obu),
        ]
    completed, duration = run(encoder_command)
    record["commands"]["encoder"] = command_record(
        encoder_command, completed, duration
    )
    record["encode_time_ns"] = duration
    if completed.returncode in (3, 5):
        record.update({
            "status": "unsupported",
            "reason": "encoder rejected or does not implement this dimension/profile",
            "encoder_returncode": completed.returncode,
        })
        return record
    if completed.returncode != 0 or not obu.is_file():
        record.update({
            "status": "error",
            "reason": "encoder failed without a supported-case classification",
            "encoder_returncode": completed.returncode,
        })
        return record
    obu_bytes = obu.stat().st_size
    pixels = int(fixture["width"]) * int(fixture["height"])
    record.update({
        "obu_bytes": obu_bytes,
        "obu_sha256": sha256_file(obu),
        "bpp_numerator": obu_bytes * 8,
        "bpp_denominator": pixels,
    })
    avm_command, project_command, avm_output, project_output = decode_commands(
        avmdec, project_decoder, obu, fixture_dir
    )
    for name, command in (("avmdec", avm_command), ("project", project_command)):
        decoded, decode_time = run(command)
        record["commands"][name] = command_record(command, decoded, decode_time)
        if decoded.returncode != 0:
            record.update({
                "status": "unsupported",
                "reason": f"{name} rejected the encoded dimension/profile",
            })
            return record
    try:
        avm = avm_output.read_bytes()
        project = project_output.read_bytes()
        if len(avm) != pixels or len(project) != pixels:
            raise BenchmarkError("decoder did not emit one exact-size I400 frame")
        record["decoder_exact"] = avm == project
        record["avm_sha256"] = sha256_bytes(avm)
        record["project_sha256"] = sha256_bytes(project)
        record["source_vs_decode"] = distortion(source, avm)
        if reconstruction_dir is not None:
            reconstruction = reconstruction_dir / f"{fixture_id}.i400"
            if reconstruction.is_file():
                rebuilt = reconstruction.read_bytes()
                record["reconstruction"] = {
                    "path": str(reconstruction),
                    "sha256": sha256_bytes(rebuilt),
                    "exact_decoder_match": rebuilt == avm,
                    "source_distortion": distortion(source, rebuilt),
                }
            else:
                record["reconstruction"] = {
                    "status": "not_provided",
                    "path": str(reconstruction),
                }
    except (BenchmarkError, OSError) as error:
        record.update({"status": "error", "reason": str(error)})
        return record
    record["status"] = "pass" if record["decoder_exact"] else "mismatch"
    return record


def append_record(path: Path, record: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as output:
        output.write(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n")
        output.flush()
        os.fsync(output.fileno())


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    result.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    result.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    result.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER)
    result.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    result.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    result.add_argument("--fixture", action="append", help="run only this fixture ID")
    result.add_argument("--append", action="store_true")
    result.add_argument(
        "--reconstruction-dir", type=Path,
        help="optionally compare FIXTURE.i400 encoder reconstructions",
    )
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        encoder = executable(args.encoder, "experimental encoder")
        avmdec = executable(args.avmdec, "pinned avmdec")
        project = executable(args.project_decoder, "project decoder")
        suite = json.loads(args.suite.read_text(encoding="utf-8"))
    except (BenchmarkError, OSError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    selected = set(args.fixture or ())
    fixtures = [
        fixture for fixture in suite["fixtures"]
        if not selected or fixture["id"] in selected
    ]
    if selected - {fixture["id"] for fixture in fixtures}:
        print("error: unknown fixture selection", file=sys.stderr)
        return 2
    args.work_dir.mkdir(parents=True, exist_ok=True)
    if not args.append:
        args.output.unlink(missing_ok=True)
    failed = False
    for fixture in fixtures:
        record = run_fixture(
            fixture, encoder, avmdec, project, args.work_dir,
            args.reconstruction_dir,
        )
        record["tools"] = {
            "encoder_sha256": sha256_file(encoder),
            "avmdec_sha256": sha256_file(avmdec),
            "project_decoder_sha256": sha256_file(project),
        }
        append_record(args.output, record)
        print(f"{fixture['id']}: {record['status']}")
        failed |= record["status"] in ("error", "mismatch", "blocked")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())

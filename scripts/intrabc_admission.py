#!/usr/bin/env python3
"""Run the pinned 8-bit I420 IntraBC admission matrix."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import sys
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

from obu_carriage import parse_obus, split_carriage  # noqa: E402


AVM_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
MANIFEST_SCHEMA = "av2-intrabc-admission-arm/v1"
REPORT_SCHEMA = "av2-intrabc-admission-report/v1"
PASTEL_QPS = (0, 48, 208, 255)
BASELINE_QPS = (96, 152)
print_lock = threading.Lock()


@dataclass(frozen=True)
class Case:
    case_id: str
    source_id: str
    preparation_id: str
    width: int
    height: int
    qp: int
    source: Path
    source_sha256: str
    requires_intrabc_occurrence: bool


@dataclass(frozen=True)
class Arm:
    case: Case
    intrabc: bool

    @property
    def name(self) -> str:
        return "on" if self.intrabc else "off"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        while chunk := input_file.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def file_record(path: Path, matrix_root: Path) -> dict[str, Any]:
    return {
        "path": path.relative_to(matrix_root).as_posix(),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def atomic_json(path: Path, value: Any) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    temporary.replace(path)


def run(command: list[str]) -> str:
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{detail[-4000:]}"
        )
    return completed.stdout


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def build_cases(lock: dict[str, Any]) -> list[Case]:
    cases: list[Case] = []
    for preparation in lock["preparations"]:
        preparation_id = preparation["preparation_id"]
        source_id = preparation["source_id"]
        qps: tuple[int, ...]
        if preparation_id == "le1440":
            qps = BASELINE_QPS
        elif preparation_id == "le896" and source_id == "pastel-chalks":
            qps = PASTEL_QPS
        else:
            continue
        width, height = preparation["dimensions"]
        output = preparation["outputs"]["yuv420p8"]
        source = ROOT / "images" / "prepared" / preparation_id / source_id
        source /= output["filename"]
        for qp in qps:
            case_id = f"{source_id}-{preparation_id}-i420-qp{qp:03d}"
            cases.append(
                Case(
                    case_id=case_id,
                    source_id=source_id,
                    preparation_id=preparation_id,
                    width=width,
                    height=height,
                    qp=qp,
                    source=source,
                    source_sha256=output["sha256"],
                    requires_intrabc_occurrence=not (
                        preparation_id == "le896"
                        and source_id == "pastel-chalks"
                        and qp == 255
                    ),
                )
            )
    cases.sort(key=lambda case: case.case_id)
    if len(cases) != 32:
        raise RuntimeError(f"admission recipe must contain 32 cases, got {len(cases)}")
    return cases


def build_arms(cases: list[Case]) -> list[Arm]:
    return [Arm(case, intrabc) for case in cases for intrabc in (False, True)]


def normalize_obu(path: Path) -> list[dict[str, Any]]:
    entries = parse_obus(path.read_bytes())
    normalized = [entry.encoded for entry in entries if entry.obu_type != 24]
    path.write_bytes(b"".join(normalized))
    entries = parse_obus(path.read_bytes())
    names = [entry.name for entry in entries]
    if names != ["temporal_delimiter", "sequence_header", "closed_loop_key"]:
        raise RuntimeError(f"unexpected normalized OBU ledger for {path}: {names}")
    return [entry.ledger_entry() for entry in entries]


def intrabc_occurrences(census: dict[str, Any]) -> dict[str, Any]:
    files = census.get("files")
    if not isinstance(files, list) or len(files) != 1:
        raise RuntimeError("census must describe exactly one input file")
    features = files[0].get("features")
    if not isinstance(features, list):
        raise RuntimeError("census features are absent")
    total = 0
    modes: dict[str, int] = {}
    skipped = 0
    residual = 0
    for feature in features:
        if feature.get("use_intrabc") != 1:
            continue
        count = int(feature["count"])
        total += count
        mode = str(feature.get("intrabc_mode", 0))
        modes[mode] = modes.get(mode, 0) + count
        if feature.get("skip_transform") == 1:
            skipped += count
        else:
            residual += count
    return {
        "block_count": total,
        "mode_counts": modes,
        "skipped_block_count": skipped,
        "residual_block_count": residual,
    }


def enforce_occurrence_policy(
    intrabc: bool,
    evidence: dict[str, Any],
    require_enabled_occurrence: bool = True,
) -> None:
    count = int(evidence["block_count"])
    if intrabc and require_enabled_occurrence and count == 0:
        raise RuntimeError("IntraBC-on arm contains no IntraBC blocks")
    if not intrabc and count != 0:
        raise RuntimeError(f"IntraBC-off arm contains {count} IntraBC blocks")


def manifest_complete(
    manifest_path: Path, arm: Arm, matrix_root: Path
) -> dict[str, Any] | None:
    if not manifest_path.is_file():
        return None
    try:
        manifest = load_json(manifest_path)
        if (
            manifest.get("schema") != MANIFEST_SCHEMA
            or manifest.get("status") != "complete"
            or manifest.get("case_id") != arm.case.case_id
            or manifest.get("intrabc") is not arm.intrabc
        ):
            return None
        for record in manifest["outputs"].values():
            path = matrix_root / record["path"]
            if (
                not path.is_file()
                or path.stat().st_size != record["bytes"]
                or sha256(path) != record["sha256"]
            ):
                return None
        enforce_occurrence_policy(
            arm.intrabc,
            manifest["census"],
            arm.case.requires_intrabc_occurrence,
        )
        recorded_expectation = manifest.get("requires_intrabc_occurrence")
        if recorded_expectation is None:
            manifest["requires_intrabc_occurrence"] = (
                arm.case.requires_intrabc_occurrence
            )
            atomic_json(manifest_path, manifest)
        elif recorded_expectation is not arm.case.requires_intrabc_occurrence:
            return None
        return manifest
    except (KeyError, OSError, TypeError, ValueError, RuntimeError):
        return None


def reusable_encoded(
    manifest_path: Path, encoded: Path, matrix_root: Path
) -> bool:
    if not manifest_path.is_file() or not encoded.is_file():
        return False
    try:
        manifest = load_json(manifest_path)
        record = manifest["outputs"]["obu"]
        recorded_path = matrix_root / record["path"]
        return (
            recorded_path == encoded
            and encoded.stat().st_size == record["bytes"]
            and sha256(encoded) == record["sha256"]
        )
    except (KeyError, OSError, TypeError, ValueError):
        return False


def encode_command(
    encoder: Path,
    arm: Arm,
    output: Path,
    encoder_threads: int,
) -> list[str]:
    case = arm.case
    intrabc_arguments = (
        ["--enable-intrabc=1", "--enable-intrabc-ext=1"]
        if arm.intrabc
        else ["--enable-intrabc=0"]
    )
    return [
        str(encoder),
        "--debug",
        "--codec=av2",
        "--obu",
        "--passes=1",
        "--limit=1",
        "--good",
        "--cpu-used=9",
        f"--threads={encoder_threads}",
        "--row-mt=1",
        "--profile=0",
        "--bit-depth=8",
        "--input-bit-depth=8",
        f"--width={case.width}",
        f"--height={case.height}",
        "--i420",
        "--input-chroma-subsampling-x=1",
        "--input-chroma-subsampling-y=1",
        "--chroma-sample-position=unspecified",
        "--force-video-mode=0",
        "--film-grain-test=0",
        "--enable-lcr=0",
        "--enable-operating-point-sets=0",
        "--enable-atlas=0",
        *intrabc_arguments,
        "--enable-restoration=0",
        "--enable-tcq=0",
        "--enable-parity-hiding=0",
        "--enable-qm=0",
        "--aq-mode=0",
        "--deltaq-mode=0",
        "--end-usage=q",
        f"--qp={case.qp}",
        "--test-decode=fatal",
        "--quiet",
        "-o",
        str(output),
        str(case.source),
    ]


def expected_i420_size(case: Case) -> int:
    chroma = ((case.width + 1) // 2) * ((case.height + 1) // 2)
    return case.width * case.height + 2 * chroma


def require_equal(first: Path, second: Path, label: str) -> None:
    if first.read_bytes() != second.read_bytes():
        raise RuntimeError(f"{label} mismatch: {first} != {second}")


def process_arm(
    arm: Arm,
    args: argparse.Namespace,
    tools: dict[str, Path],
) -> tuple[dict[str, Any], bool]:
    arm_dir = args.output_root / arm.case.case_id / arm.name
    arm_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = arm_dir / "manifest.json"
    if not args.force:
        complete = manifest_complete(manifest_path, arm, args.output_root)
        if complete is not None:
            with print_lock:
                print(f"reuse {arm.case.case_id}/{arm.name}", flush=True)
            return complete, True

    encoded = arm_dir / "encoded.obu"
    repeat = arm_dir / "encoded-repeat.obu"
    old_encoded_valid = not args.force and reusable_encoded(
        manifest_path, encoded, args.output_root
    )
    if manifest_path.exists():
        manifest_path.unlink()
    commands: dict[str, Any] = {}
    if not old_encoded_valid:
        commands["encode"] = encode_command(
            tools["encoder"], arm, encoded, args.encoder_threads
        )
        run(commands["encode"])
        normalize_obu(encoded)
    else:
        normalize_obu(encoded)

    commands["deterministic_encode"] = encode_command(
        tools["encoder"], arm, repeat, args.encoder_threads
    )
    run(commands["deterministic_encode"])
    ledger = normalize_obu(repeat)
    require_equal(encoded, repeat, "deterministic encode")
    repeat.unlink()

    census_path = arm_dir / "census.json"
    commands["census"] = [
        str(tools["census"]), "-o", str(census_path), str(encoded)
    ]
    run(commands["census"])
    census = intrabc_occurrences(load_json(census_path))
    enforce_occurrence_policy(
        arm.intrabc, census, arm.case.requires_intrabc_occurrence
    )

    avm_output = arm_dir / "avm.i420"
    local_output = arm_dir / "local.i420"
    commands["avm_decode"] = [
        str(tools["avm_decoder"]),
        "--codec=av2",
        "--rawvideo",
        "--output-bit-depth=8",
        "--limit=1",
        "-o",
        str(avm_output),
        str(encoded),
    ]
    commands["local_decode"] = [
        str(tools["local_decoder"]), str(encoded), "-o", str(local_output)
    ]
    run(commands["avm_decode"])
    run(commands["local_decode"])
    if avm_output.stat().st_size != expected_i420_size(arm.case):
        raise RuntimeError(f"unexpected AVM output size: {avm_output}")
    require_equal(avm_output, local_output, "native decoder parity")

    config = arm_dir / "config.obu"
    payload = arm_dir / "payload.obu"
    config_bytes, payload_bytes, _, _ = split_carriage(encoded.read_bytes())
    config.write_bytes(config_bytes)
    payload.write_bytes(payload_bytes)
    container = arm_dir / "encoded.heic"
    public_output = arm_dir / "public.i420"
    commands["public_heif"] = [
        [
            str(tools["artifact"]),
            "pack",
            str(config),
            str(payload),
            str(container),
            "1",
            "13",
            "1",
            "full",
        ],
        [str(tools["artifact"]), "decode", str(container), str(public_output)],
    ]
    for command in commands["public_heif"]:
        run(command)
    require_equal(avm_output, public_output, "public HEIF parity")

    outputs = {
        "obu": file_record(encoded, args.output_root),
        "census": file_record(census_path, args.output_root),
        "avm_decode": file_record(avm_output, args.output_root),
        "native_decode": file_record(local_output, args.output_root),
        "heif": file_record(container, args.output_root),
        "public_decode": file_record(public_output, args.output_root),
    }
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "status": "complete",
        "case_id": arm.case.case_id,
        "source_id": arm.case.source_id,
        "preparation_id": arm.case.preparation_id,
        "dimensions": [arm.case.width, arm.case.height],
        "pixel_format": "i420",
        "qp": arm.case.qp,
        "intrabc": arm.intrabc,
        "requires_intrabc_occurrence": arm.case.requires_intrabc_occurrence,
        "source": {
            "path": arm.case.source.relative_to(ROOT).as_posix(),
            "sha256": arm.case.source_sha256,
        },
        "deterministic_encode": True,
        "obu_ledger": ledger,
        "census": census,
        "commands": commands,
        "outputs": outputs,
        "parity": {"native": True, "public_heif": True},
    }
    atomic_json(manifest_path, manifest)
    with print_lock:
        print(
            f"pass  {arm.case.case_id}/{arm.name} "
            f"intrabc={census['block_count']}",
            flush=True,
        )
    return manifest, False


def tool_paths(args: argparse.Namespace) -> dict[str, Path]:
    return {
        "encoder": args.avm_build / "avmenc",
        "avm_decoder": args.avm_build / "avmdec",
        "census": args.census,
        "local_decoder": args.host_build / "av2heif-decoder",
        "artifact": args.host_build / "av2heif-artifact",
    }


def verify_environment(cases: list[Case], tools: dict[str, Path]) -> dict[str, Any]:
    for name, path in tools.items():
        if not path.is_file():
            raise RuntimeError(f"missing {name}: {path}")
    for case in cases:
        if not case.source.is_file():
            raise RuntimeError(f"missing prepared source: {case.source}")
        if sha256(case.source) != case.source_sha256:
            raise RuntimeError(f"prepared source hash mismatch: {case.source}")
    revision = run(["git", "-C", str(ROOT / "vendor" / "avm"), "rev-parse", "HEAD"])
    revision = revision.strip()
    if revision != AVM_REVISION:
        raise RuntimeError(f"AVM revision is {revision}, expected {AVM_REVISION}")
    return {
        "avm_revision": revision,
        "tools": {name: {"path": str(path), "sha256": sha256(path)}
                  for name, path in tools.items()},
    }


def build_report(
    manifests: list[dict[str, Any]], provenance: dict[str, Any], reused: int
) -> dict[str, Any]:
    by_case: dict[str, dict[str, dict[str, Any]]] = {}
    for manifest in manifests:
        by_case.setdefault(manifest["case_id"], {})[
            "on" if manifest["intrabc"] else "off"
        ] = manifest
    comparisons = []
    for case_id in sorted(by_case):
        pair = by_case[case_id]
        if set(pair) != {"off", "on"}:
            raise RuntimeError(f"incomplete off/on pair: {case_id}")
        off_bytes = pair["off"]["outputs"]["obu"]["bytes"]
        on_bytes = pair["on"]["outputs"]["obu"]["bytes"]
        comparisons.append(
            {
                "case_id": case_id,
                "off_bytes": off_bytes,
                "on_bytes": on_bytes,
                "byte_delta": on_bytes - off_bytes,
                "intrabc_blocks": pair["on"]["census"]["block_count"],
            }
        )
    return {
        "schema": REPORT_SCHEMA,
        "status": "complete",
        "case_count": len(comparisons),
        "arm_count": len(manifests),
        "reused_arm_count": reused,
        "executed_arm_count": len(manifests) - reused,
        "provenance": provenance,
        "comparisons": comparisons,
        "totals": {
            "off_bytes": sum(item["off_bytes"] for item in comparisons),
            "on_bytes": sum(item["on_bytes"] for item in comparisons),
            "intrabc_blocks": sum(item["intrabc_blocks"] for item in comparisons),
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--prepared-lock",
        type=Path,
        default=ROOT / "images" / "recipes" / "prepared-lock.json",
    )
    parser.add_argument(
        "--output-root", type=Path, default=ROOT / "build" / "intrabc-admission"
    )
    parser.add_argument(
        "--avm-build", type=Path, default=ROOT / "build" / "reference" / "avm"
    )
    parser.add_argument(
        "--census",
        type=Path,
        default=ROOT / "build" / "reference" / "census-accounting" / "av2-census",
    )
    parser.add_argument(
        "--host-build", type=Path, default=ROOT / "build" / "host-cmake"
    )
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--encoder-threads", type=int, default=8)
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    for name in ("prepared_lock", "output_root", "avm_build", "census", "host_build"):
        path = getattr(args, name)
        if not path.is_absolute():
            setattr(args, name, (ROOT / path).resolve())
    if args.jobs < 1 or args.encoder_threads < 1:
        parser.error("--jobs and --encoder-threads must be positive")
    return args


def main() -> int:
    args = parse_args()
    try:
        cases = build_cases(load_json(args.prepared_lock))
        if args.case:
            selected = set(args.case)
            cases = [case for case in cases if case.case_id in selected]
            missing = selected - {case.case_id for case in cases}
            if missing:
                raise RuntimeError(f"unknown cases: {', '.join(sorted(missing))}")
        arms = build_arms(cases)
        tools = tool_paths(args)
        provenance = verify_environment(cases, tools)
        args.output_root.mkdir(parents=True, exist_ok=True)
        results: list[dict[str, Any]] = []
        reused = 0
        failures: list[str] = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
            pending = {
                executor.submit(process_arm, arm, args, tools): arm for arm in arms
            }
            for future in concurrent.futures.as_completed(pending):
                arm = pending[future]
                try:
                    manifest, was_reused = future.result()
                    results.append(manifest)
                    reused += int(was_reused)
                except Exception as error:  # noqa: BLE001
                    message = f"{arm.case.case_id}/{arm.name}: {error}"
                    failures.append(message)
                    with print_lock:
                        print(f"FAIL  {message}", file=sys.stderr, flush=True)
        if failures:
            atomic_json(
                args.output_root / "report.json",
                {
                    "schema": REPORT_SCHEMA,
                    "status": "failed",
                    "failures": failures,
                },
            )
            return 1
        report = build_report(results, provenance, reused)
        atomic_json(args.output_root / "report.json", report)
        print(
            f"IntraBC admission passed: {report['arm_count']} arms, "
            f"{report['totals']['intrabc_blocks']} IntraBC blocks"
        )
        return 0
    except Exception as error:  # noqa: BLE001
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
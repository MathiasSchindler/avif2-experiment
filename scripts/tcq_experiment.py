#!/usr/bin/env python3
"""Run resumable, single-source AV2 TCQ experiments with pinned AVM."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
TOOLS = ROOT / "tools"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(TOOLS))

import intrabc_admission as common  # noqa: E402
from obu_carriage import split_carriage  # noqa: E402


MANIFEST_SCHEMA = "av2-tcq-experiment-arm/v1"
REPORT_SCHEMA = "av2-tcq-experiment-report/v1"


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


@dataclass(frozen=True)
class Arm:
    case: Case
    tcq: bool
    cpu_used: int

    @property
    def name(self) -> str:
        return "tcq-on" if self.tcq else "tcq-off"


def select_case(
    lock: dict[str, Any], source_id: str, preparation_id: str, qp: int
) -> Case:
    for preparation in lock["preparations"]:
        if (
            preparation["source_id"] != source_id
            or preparation["preparation_id"] != preparation_id
        ):
            continue
        width, height = preparation["dimensions"]
        output = preparation["outputs"]["yuv420p8"]
        source = (
            ROOT
            / "images"
            / "prepared"
            / preparation_id
            / source_id
            / output["filename"]
        )
        return Case(
            case_id=f"{source_id}-{preparation_id}-i420-qp{qp:03d}",
            source_id=source_id,
            preparation_id=preparation_id,
            width=width,
            height=height,
            qp=qp,
            source=source,
            source_sha256=output["sha256"],
        )
    raise RuntimeError(f"unknown prepared source: {source_id}/{preparation_id}")


def build_arms(cases: list[Case], tcq_mode: str, cpu_used: int) -> list[Arm]:
    modes = {
        "off": (False,),
        "on": (True,),
        "both": (False, True),
    }[tcq_mode]
    return [Arm(case, tcq, cpu_used) for case in cases for tcq in modes]


def configuration(arm: Arm, encoder_threads: int) -> dict[str, Any]:
    return {
        "cpu_used": arm.cpu_used,
        "encoder_threads": encoder_threads,
        "tcq": arm.tcq,
        "intra_dip": False,
        "transform_partitioning": False,
        "intrabc": False,
        "restoration": False,
        "parity_hiding": False,
        "quantization_matrices": False,
        "aq_mode": 0,
        "deltaq_mode": 0,
    }


def encode_command(
    encoder: Path, arm: Arm, output: Path, encoder_threads: int
) -> list[str]:
    case = arm.case
    return [
        str(encoder),
        "--debug",
        "--codec=av2",
        "--obu",
        "--passes=1",
        "--limit=1",
        "--good",
        f"--cpu-used={arm.cpu_used}",
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
        "--enable-intra-dip=0",
        "--enable-tx-partition=0",
        "--enable-intrabc=0",
        "--enable-restoration=0",
        f"--enable-tcq={int(arm.tcq)}",
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


def run_timed(command: list[str]) -> tuple[str, float]:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    elapsed = time.perf_counter() - started
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"{detail[-4000:]}"
        )
    return completed.stdout, elapsed


def tool_paths(args: argparse.Namespace) -> dict[str, Path]:
    tools = {
        "encoder": args.avm_build / "avmenc",
        "avm_decoder": args.avm_build / "avmdec",
        "census": args.census,
    }
    if not args.reference_only:
        tools.update(
            {
                "local_decoder": args.host_build / "av2heif-decoder",
                "artifact": args.host_build / "av2heif-artifact",
            }
        )
    return tools


def verify_environment(case: Case, tools: dict[str, Path]) -> dict[str, Any]:
    for name, path in tools.items():
        if not path.is_file():
            raise RuntimeError(f"missing {name}: {path}")
    if not case.source.is_file():
        raise RuntimeError(f"missing prepared source: {case.source}")
    if common.sha256(case.source) != case.source_sha256:
        raise RuntimeError(f"prepared source hash mismatch: {case.source}")
    revision = common.run(
        ["git", "-C", str(ROOT / "vendor" / "avm"), "rev-parse", "HEAD"]
    ).strip()
    if revision != common.AVM_REVISION:
        raise RuntimeError(
            f"AVM revision is {revision}, expected {common.AVM_REVISION}"
        )
    return {
        "avm_revision": revision,
        "tools": {
            name: {"path": str(path), "sha256": common.sha256(path)}
            for name, path in tools.items()
        },
    }


def encode_contract(
    arm: Arm, encoder_threads: int, provenance: dict[str, Any]
) -> dict[str, Any]:
    return {
        "source": {
            "path": arm.case.source.relative_to(ROOT).as_posix(),
            "sha256": arm.case.source_sha256,
        },
        "avm_revision": provenance["avm_revision"],
        "encoder_sha256": provenance["tools"]["encoder"]["sha256"],
        "configuration": configuration(arm, encoder_threads),
    }


def valid_record(record: dict[str, Any], output_root: Path) -> bool:
    try:
        path = output_root / record["path"]
        return (
            path.is_file()
            and path.stat().st_size == record["bytes"]
            and common.sha256(path) == record["sha256"]
        )
    except (KeyError, OSError, TypeError, ValueError):
        return False


def reusable_checkpoint(
    manifest_path: Path,
    contract: dict[str, Any],
    output_root: Path,
    require_complete: bool = False,
    required_reference_only: bool | None = None,
    required_provenance: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    if not manifest_path.is_file():
        return None
    try:
        manifest = common.load_json(manifest_path)
        if (
            manifest.get("schema") != MANIFEST_SCHEMA
            or manifest.get("encode_contract") != contract
            or (require_complete and manifest.get("status") != "complete")
            or (
                required_reference_only is not None
                and manifest.get("reference_only") is not required_reference_only
            )
            or (
                required_provenance is not None
                and manifest.get("provenance") != required_provenance
            )
        ):
            return None
        records = manifest["outputs"]
        records_to_validate = (
            records.values() if require_complete else (records["obu"],)
        )
        if not all(valid_record(record, output_root) for record in records_to_validate):
            return None
        return manifest
    except (KeyError, OSError, TypeError, ValueError):
        return None


def expected_i420_size(case: Case) -> int:
    chroma = ((case.width + 1) // 2) * ((case.height + 1) // 2)
    return case.width * case.height + 2 * chroma


def write_checkpoint(
    manifest_path: Path,
    arm: Arm,
    contract: dict[str, Any],
    commands: dict[str, Any],
    timings: dict[str, float],
    output_root: Path,
    encoded: Path,
    deterministic: bool,
) -> dict[str, Any]:
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "status": "encoded",
        "case_id": arm.case.case_id,
        "tcq": arm.tcq,
        "encode_contract": contract,
        "deterministic_encode": deterministic,
        "commands": commands,
        "timings_seconds": timings,
        "outputs": {"obu": common.file_record(encoded, output_root)},
    }
    common.atomic_json(manifest_path, manifest)
    return manifest


def process_arm(
    arm: Arm,
    args: argparse.Namespace,
    tools: dict[str, Path],
    provenance: dict[str, Any],
) -> tuple[dict[str, Any], bool]:
    arm_dir = args.output_root / arm.case.case_id / arm.name
    arm_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = arm_dir / "manifest.json"
    contract = encode_contract(arm, args.encoder_threads, provenance)
    if not args.force:
        complete = reusable_checkpoint(
            manifest_path,
            contract,
            args.output_root,
            require_complete=True,
            required_reference_only=args.reference_only,
            required_provenance=provenance,
        )
        if complete is not None:
            print(f"reuse {arm.case.case_id}/{arm.name}", flush=True)
            return complete, True

    encoded = arm_dir / "encoded.obu"
    checkpoint = None if args.force else reusable_checkpoint(
        manifest_path, contract, args.output_root
    )
    commands: dict[str, Any] = {}
    timings: dict[str, float] = {}
    if checkpoint is not None:
        commands.update(checkpoint.get("commands", {}))
        timings.update(checkpoint.get("timings_seconds", {}))
        deterministic = checkpoint.get("deterministic_encode") is True
        print(f"resume {arm.case.case_id}/{arm.name}", flush=True)
    else:
        if manifest_path.exists():
            manifest_path.unlink()
        commands["encode"] = encode_command(
            tools["encoder"], arm, encoded, args.encoder_threads
        )
        _, timings["encode"] = run_timed(commands["encode"])
        common.normalize_obu(encoded)
        deterministic = False
        write_checkpoint(
            manifest_path,
            arm,
            contract,
            commands,
            timings,
            args.output_root,
            encoded,
            deterministic,
        )

    if not deterministic:
        repeat = arm_dir / "encoded-repeat.obu"
        commands["deterministic_encode"] = encode_command(
            tools["encoder"], arm, repeat, args.encoder_threads
        )
        _, timings["deterministic_encode"] = run_timed(
            commands["deterministic_encode"]
        )
        ledger = common.normalize_obu(repeat)
        common.require_equal(encoded, repeat, "deterministic encode")
        repeat.unlink()
        deterministic = True
        write_checkpoint(
            manifest_path,
            arm,
            contract,
            commands,
            timings,
            args.output_root,
            encoded,
            deterministic,
        )
    else:
        ledger = common.normalize_obu(encoded)

    census_path = arm_dir / "census.json"
    commands["census"] = [
        str(tools["census"]), "-o", str(census_path), str(encoded)
    ]
    _, timings["census"] = run_timed(commands["census"])

    avm_output = arm_dir / "avm.i420"
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
    _, timings["avm_decode"] = run_timed(commands["avm_decode"])
    if avm_output.stat().st_size != expected_i420_size(arm.case):
        raise RuntimeError(f"unexpected AVM output size: {avm_output}")

    outputs = {
        "obu": common.file_record(encoded, args.output_root),
        "census": common.file_record(census_path, args.output_root),
        "avm_decode": common.file_record(avm_output, args.output_root),
    }
    parity: dict[str, bool | None] = {"native": None, "public_heif": None}
    if not args.reference_only:
        local_output = arm_dir / "local.i420"
        commands["local_decode"] = [
            str(tools["local_decoder"]), str(encoded), "-o", str(local_output)
        ]
        _, timings["local_decode"] = run_timed(commands["local_decode"])
        common.require_equal(avm_output, local_output, "native decoder parity")

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
            [
                str(tools["artifact"]),
                "decode",
                str(container),
                str(public_output),
            ],
        ]
        public_time = 0.0
        for command in commands["public_heif"]:
            _, elapsed = run_timed(command)
            public_time += elapsed
        timings["public_heif"] = public_time
        common.require_equal(avm_output, public_output, "public HEIF parity")
        outputs.update(
            {
                "native_decode": common.file_record(
                    local_output, args.output_root
                ),
                "heif": common.file_record(container, args.output_root),
                "public_decode": common.file_record(
                    public_output, args.output_root
                ),
            }
        )
        parity = {"native": True, "public_heif": True}

    manifest = {
        "schema": MANIFEST_SCHEMA,
        "status": "complete",
        "case_id": arm.case.case_id,
        "source_id": arm.case.source_id,
        "preparation_id": arm.case.preparation_id,
        "dimensions": [arm.case.width, arm.case.height],
        "pixel_format": "i420",
        "qp": arm.case.qp,
        "tcq": arm.tcq,
        "reference_only": args.reference_only,
        "encode_contract": contract,
        "provenance": provenance,
        "deterministic_encode": True,
        "obu_ledger": ledger,
        "commands": commands,
        "timings_seconds": timings,
        "outputs": outputs,
        "parity": parity,
    }
    common.atomic_json(manifest_path, manifest)
    print(
        f"pass  {arm.case.case_id}/{arm.name} "
        f"bytes={outputs['obu']['bytes']} encode={timings['encode']:.2f}s",
        flush=True,
    )
    return manifest, False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--prepared-lock",
        type=Path,
        default=ROOT / "images" / "recipes" / "prepared-lock.json",
    )
    parser.add_argument("--source-id", default="pastel-chalks")
    parser.add_argument("--preparation-id", default="le896")
    parser.add_argument("--qp", action="append", type=int)
    parser.add_argument("--tcq", choices=("off", "on", "both"), default="off")
    parser.add_argument("--cpu-used", type=int, default=1)
    parser.add_argument("--encoder-threads", type=int, default=16)
    parser.add_argument("--reference-only", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument(
        "--output-root", type=Path, default=ROOT / "build" / "tcq-experiment"
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
    args = parser.parse_args()
    for name in ("prepared_lock", "output_root", "avm_build", "census", "host_build"):
        path = getattr(args, name)
        if not path.is_absolute():
            setattr(args, name, (ROOT / path).resolve())
    qps = args.qp if args.qp is not None else [152]
    if any(qp < 0 or qp > 255 for qp in qps):
        parser.error("--qp must be between 0 and 255")
    args.qp = qps
    if args.cpu_used < 0 or args.cpu_used > 9:
        parser.error("--cpu-used must be between 0 and 9")
    if args.tcq in ("on", "both") and args.cpu_used > 1:
        parser.error("pinned AVM requires --cpu-used=1 or lower for TCQ")
    if args.encoder_threads < 1:
        parser.error("--encoder-threads must be positive")
    return args


def main() -> int:
    args = parse_args()
    try:
        lock = common.load_json(args.prepared_lock)
        cases = [
            select_case(lock, args.source_id, args.preparation_id, qp)
            for qp in args.qp
        ]
        arms = build_arms(cases, args.tcq, args.cpu_used)
        tools = tool_paths(args)
        args.output_root.mkdir(parents=True, exist_ok=True)
        manifests = []
        reused = 0
        for arm in arms:
            provenance = verify_environment(arm.case, tools)
            manifest, was_reused = process_arm(arm, args, tools, provenance)
            manifests.append(manifest)
            reused += int(was_reused)
        report = {
            "schema": REPORT_SCHEMA,
            "status": "complete",
            "arm_count": len(manifests),
            "reused_arm_count": reused,
            "executed_arm_count": len(manifests) - reused,
            "arms": [
                {
                    "case_id": manifest["case_id"],
                    "tcq": manifest["tcq"],
                    "bytes": manifest["outputs"]["obu"]["bytes"],
                    "timings_seconds": manifest["timings_seconds"],
                    "parity": manifest["parity"],
                }
                for manifest in manifests
            ],
        }
        common.atomic_json(args.output_root / "report.json", report)
        print(f"TCQ experiment passed: {len(manifests)} arm(s)")
        return 0
    except Exception as error:  # noqa: BLE001
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
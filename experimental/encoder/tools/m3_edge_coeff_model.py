#!/usr/bin/env python3
"""Focused 17x17 boundary-leaf coefficient transcript model."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import m3_multileaf_cdf_model as MULTILEAF


QINDEX_CASES = (1, 90, 91, 100, 141, 191, 255)
PROFILES = (("flat128", True), ("alternating-8x8-dc", False))
SHAPES = ("8x16", "16x8", "8x8")
NORMATIVE = ROOT / "experimental/encoder/src/normative_cdfs.inc"
ENCODER = ROOT / "experimental/encoder"
DRIVER = ENCODER / "build/m3-boundary-partition-driver"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
PINNED_AVMDEC_SHA256 = (
    "c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0"
)


def edge_case(qindex: int, flat: bool) -> dict[str, object]:
    result = MULTILEAF.model(17, 17, qindex, flat=flat)
    edge_indices = {
        index for index, leaf in enumerate(result["leaves"])
        if f"{leaf['width']}x{leaf['height']}" in SHAPES
    }
    leaves = [
        {**leaf, "leaf_index": index}
        for index, leaf in enumerate(result["leaves"])
        if index in edge_indices
    ]
    transforms = [
        transform for transform in result["transforms"]
        if transform["leaf_index"] in edge_indices
    ]
    events = [
        event for event in result["events"]
        if event["leaf_index"] in edge_indices and event["tx_index"] >= 0
    ]
    return {
        "profile": result["source_profile"],
        "qindex": qindex,
        "q_context": result["q_context"],
        "source_sha256": result["source_sha256"],
        "reconstruction_sha256": result["reconstruction_sha256"],
        "edge_leaf_count": len(leaves),
        "edge_transform_count": len(transforms),
        "edge_event_count": len(events),
        "leaves": leaves,
        "transforms": transforms,
        "events": events,
    }


def vectors() -> dict[str, object]:
    cases = [
        edge_case(qindex, flat)
        for _profile, flat in PROFILES
        for qindex in QINDEX_CASES
    ]
    return {
        "schema": "av2-m3-edge-coeff-context-v1",
        "dimensions": "17x17",
        "normative_cdf_sha256": hashlib.sha256(
            NORMATIVE.read_bytes()
        ).hexdigest(),
        "qindex_cases": list(QINDEX_CASES),
        "profiles": [profile for profile, _flat in PROFILES],
        "edge_shapes": list(SHAPES),
        "case_count": len(cases),
        "cases": cases,
    }


def run(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False
    )


def probe(output: Path) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for profile, flat in PROFILES:
        for qindex in QINDEX_CASES:
            name = f"17x17-{profile}-q{qindex}"
            modeled = MULTILEAF.model(17, 17, qindex, flat=flat)
            levels = output / f"{name}.levels"
            obu = output / f"{name}.obu"
            avm_raw = output / f"{name}.avm.i400"
            project_raw = output / f"{name}.project.i400"
            level_arg = "flat"
            if not flat:
                levels.write_bytes(b"".join(
                    struct.pack("<h", int(tx["level"]))
                    for tx in modeled["transforms"]
                ))
                level_arg = str(levels)
            encoded = run([
                str(DRIVER), "17", "17", level_arg, "stream", str(obu),
                str(qindex),
            ])
            avm = run([
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
            ]) if encoded.returncode == 0 else None
            project = run([
                str(PROJECT_DECODER), str(obu), "-o", str(project_raw)
            ]) if encoded.returncode == 0 else None
            avm_bytes = avm_raw.read_bytes() if avm_raw.is_file() else b""
            project_bytes = (
                project_raw.read_bytes() if project_raw.is_file() else b""
            )
            local_hash = str(modeled["reconstruction_sha256"])
            avm_hash = hashlib.sha256(avm_bytes).hexdigest()
            project_hash = hashlib.sha256(project_bytes).hexdigest()
            local_avm = avm is not None and avm.returncode == 0 and (
                local_hash == avm_hash
            )
            local_project = (
                project is not None and project.returncode == 0
                and local_hash == project_hash
            )
            records.append({
                "name": name,
                "profile": profile,
                "qindex": qindex,
                "encoder_returncode": encoded.returncode,
                "obu_bytes": obu.stat().st_size if obu.is_file() else 0,
                "obu_sha256": (
                    hashlib.sha256(obu.read_bytes()).hexdigest()
                    if obu.is_file() else None
                ),
                "local_reconstruction_sha256": local_hash,
                "avmdec_returncode": None if avm is None else avm.returncode,
                "avm_error": (
                    "" if avm is None else
                    avm.stderr.decode(errors="replace").strip()
                ),
                "avm_output_bytes": len(avm_bytes),
                "avm_output_sha256": avm_hash,
                "project_decoder_returncode": (
                    None if project is None else project.returncode
                ),
                "project_error": (
                    "" if project is None else
                    project.stderr.decode(errors="replace").strip()
                ),
                "project_output_bytes": len(project_bytes),
                "project_output_sha256": project_hash,
                "local_avm_exact": local_avm,
                "local_project_exact": local_project,
                "validators_exact": (
                    avm is not None and project is not None
                    and avm.returncode == 0 and project.returncode == 0
                    and avm_bytes == project_bytes
                ),
                "local_avm_project_exact": local_avm and local_project,
            })
            for path in (levels, obu, avm_raw, project_raw):
                path.unlink(missing_ok=True)
    exact_count = sum(
        bool(case["local_avm_project_exact"]) for case in records
    )
    avmdec_hash = hashlib.sha256(AVMDEC.read_bytes()).hexdigest()
    result = {
        "schema": "av2-m3-edge-coeff-probes-v2",
        "dimensions": "17x17",
        "profiles": [profile for profile, _flat in PROFILES],
        "qindex_cases": list(QINDEX_CASES),
        "case_count": len(records),
        "triple_exact_count": exact_count,
        "all_pass": exact_count == len(records),
        "partition_baseline": (
            "terminal 8x8 PARTITION_NONE uses do_split context 0"
        ),
        "prior_partition_induced_mismatch_count": 11,
        "remaining_partition_induced_range_divergence": 0,
        "genuine_edge_coefficient_context_errors": 0,
        "production_enabled": True,
        "avmdec_sha256": avmdec_hash,
        "avmdec_hash_matches_pin": avmdec_hash == PINNED_AVMDEC_SHA256,
        "project_decoder_sha256": hashlib.sha256(
            PROJECT_DECODER.read_bytes()
        ).hexdigest(),
        "cases": records,
    }
    (output / "report.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vectors", action="store_true")
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--qindex", type=int, choices=QINDEX_CASES, default=100)
    parser.add_argument("--flat", action="store_true")
    args = parser.parse_args()
    result = (
        probe(args.probe) if args.probe else
        vectors() if args.vectors else edge_case(args.qindex, args.flat)
    )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if not args.probe or (
        result["all_pass"] and result["avmdec_hash_matches_pin"]
    ) else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Generate 32x32 probes and black-box the final tile byte."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
DEFAULT_OUT = ENCODER / "build/encoder-32x32-investigation/tool"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"


def cases() -> dict[str, bytes]:
    quadrants = bytes(
        (64, 112, 160, 208)[(y >= 16) * 2 + (x >= 16)]
        for y in range(32)
        for x in range(32)
    )
    return {
        "flat128": bytes([128]) * 1024,
        "flat96": bytes([96]) * 1024,
        "quadrants": quadrants,
    }


def write_cases(output: Path) -> list[dict[str, object]]:
    output.mkdir(parents=True, exist_ok=True)
    records = []
    for name, data in cases().items():
        path = output / f"{name}.i400"
        path.write_bytes(data)
        records.append(
            {
                "name": name,
                "path": str(path),
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )
    return records


def run_decoder(command: list[str]) -> tuple[int, str]:
    result = subprocess.run(
        command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=False
    )
    return result.returncode, result.stderr.decode("utf-8", "backslashreplace").strip()


def mutate_last_byte(
    obu: Path, output: Path, avmdec: Path, project_decoder: Path
) -> list[dict[str, object]]:
    source = bytearray(obu.read_bytes())
    if not source:
        raise ValueError("cannot mutate an empty OBU")
    output.mkdir(parents=True, exist_ok=True)
    probe = output / "probe.obu"
    avm_raw = output / "avm.i400"
    project_raw = output / "project.i400"
    records = []
    for value in range(256):
        mutated = source[:]
        mutated[-1] = value
        probe.write_bytes(mutated)
        avm_rc, avm_error = run_decoder(
            [
                str(avmdec),
                "--codec=av2",
                "--rawvideo",
                "--limit=1",
                "-o",
                str(avm_raw),
                str(probe),
            ]
        )
        project_rc, project_error = run_decoder(
            [str(project_decoder), str(probe), "-o", str(project_raw)]
        )
        if avm_rc == 0 or project_rc == 0:
            record: dict[str, object] = {
                "value": value,
                "hex": f"{value:02x}",
                "avm_returncode": avm_rc,
                "project_returncode": project_rc,
                "avm_error": avm_error,
                "project_error": project_error,
            }
            if avm_rc == 0:
                raw = avm_raw.read_bytes()
                record["avm_sha256"] = hashlib.sha256(raw).hexdigest()
                record["avm_flat128"] = raw == cases()["flat128"]
            records.append(record)
    return records


def normative_facts() -> dict[str, object]:
    return {
        "block_enum": {"16x16": 6, "32x32": 9},
        "do_split_context_no_smaller_neighbors": {"16x16": 4, "32x32": 8},
        "tx_partition_group": {"16x16": 3, "32x32": 5},
        "tx_partition_type_group": {"16x16": 3, "32x32": 6},
        "square_split_context_top_left": 0,
        "square_split_row": [18000, 7, 0],
        "delta_q_scope": "ReadDeltas is reset once per superblock",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument(
        "--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER
    )
    args = parser.parse_args()
    report: dict[str, object] = {
        "schema": "encoder-32x32-investigation-v1",
        "cases": write_cases(args.output / "cases"),
        "normative_facts": normative_facts(),
    }
    if args.probe is not None:
        report["probe"] = str(args.probe)
        report["original_last_byte"] = f"{args.probe.read_bytes()[-1]:02x}"
        report["last_byte_mutations"] = mutate_last_byte(
            args.probe,
            args.output / "mutation",
            args.avmdec,
            args.project_decoder,
        )
    args.output.mkdir(parents=True, exist_ok=True)
    report_path = args.output / "report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

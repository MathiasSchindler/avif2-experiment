#!/usr/bin/env python3
"""Generate and validate the frozen M1/M3/M4 full-stream matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
FIXTURES = ENCODER / "fixtures/m2-full-streams"
MANIFEST = FIXTURES / "manifest.json"
DEFAULT_ENCODER = ENCODER / "build/av2-encoder-experiment"
DEFAULT_AVMDEC = ROOT / "build/reference/avm/avmdec"
DEFAULT_PROJECT = ROOT / "build/host-cmake/av2heif-decoder"
PINNED_AVMDEC_SHA256 = (
    "c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def quadrants() -> bytes:
    values = ((129, 135), (139, 120))
    output = bytearray(16 * 16)
    for block_y in range(2):
        for block_x in range(2):
            for y in range(8):
                offset = (block_y * 8 + y) * 16 + block_x * 8
                output[offset:offset + 8] = bytes(
                    [values[block_y][block_x]]) * 8
    return bytes(output)


CASES = (
    {
        "name": "m1-coefficient-free",
        "mode": "m1",
        "width": 8,
        "height": 8,
        "coverage": ["coefficient-free", "M1"],
        "source": bytes([128]) * 64,
    },
    {
        "name": "m3-dc-base",
        "mode": "m3",
        "width": 8,
        "height": 8,
        "coverage": ["DC", "base", "M3"],
        "source": bytes([129]) * 64,
    },
    {
        "name": "m3-dc-br",
        "mode": "m3",
        "width": 8,
        "height": 8,
        "coverage": ["DC", "base", "BR", "M3"],
        "source": bytes([135]) * 64,
    },
    {
        "name": "m3-dc-remainder",
        "mode": "m3",
        "width": 8,
        "height": 8,
        "coverage": ["DC", "base", "BR", "remainder", "M3"],
        "source": bytes([139]) * 64,
    },
    {
        "name": "m3-multi-block-adaptation",
        "mode": "m3",
        "width": 16,
        "height": 16,
        "coverage": ["multi-block", "CDF-adaptation", "M3"],
        "source": quadrants(),
    },
    {
        "name": "m4-vertical-ac",
        "mode": "m4",
        "width": 8,
        "height": 8,
        "coverage": ["DC", "AC(1,0)", "sign", "M4"],
        "source": b"".join(
            bytes([value]) * 8
            for value in (128, 128, 128, 129, 129, 129, 130, 130)
        ),
    },
)


def require_executable(path: Path, label: str) -> Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f"{label} is not executable: {path}")
    return path


def encode(case: dict[str, object], encoder: Path, output: Path,
           source_path: Path) -> None:
    mode = case["mode"]
    if mode == "m1":
        command = [str(encoder), "--constant-128-8x8", str(output)]
    elif mode == "m3":
        command = [
            str(encoder), "--i400-8", str(source_path),
            "--width", str(case["width"]), "--height", str(case["height"]),
            str(output),
        ]
    else:
        command = [
            str(encoder), "--experimental-m4-ac",
            str(source_path), str(output),
        ]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)


def decode(decoder: Path, kind: str, obu: Path, output: Path) -> bytes:
    if kind == "avmdec":
        command = [
            str(decoder), "--codec=av2", "--rawvideo", "--limit=1",
            "--output-bit-depth=8", "-o", str(output), str(obu),
        ]
    else:
        command = [str(decoder), str(obu), "-o", str(output)]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    return output.read_bytes()


def generate(encoder: Path, destination: Path) -> list[dict[str, object]]:
    require_executable(encoder, "experimental encoder")
    destination.mkdir(parents=True, exist_ok=True)
    records = []
    for case in CASES:
        name = str(case["name"])
        source_path = destination / f"{name}.i400"
        obu_path = destination / f"{name}.obu"
        source = bytes(case["source"])
        source_path.write_bytes(source)
        encode(case, encoder, obu_path, source_path)
        obu = obu_path.read_bytes()
        records.append({
            "name": name,
            "mode": case["mode"],
            "width": case["width"],
            "height": case["height"],
            "coverage": case["coverage"],
            "source": {
                "path": f"experimental/encoder/fixtures/m2-full-streams/{name}.i400",
                "bytes": len(source),
                "sha256": sha256(source),
            },
            "obu": {
                "path": f"experimental/encoder/fixtures/m2-full-streams/{name}.obu",
                "bytes": len(obu),
                "sha256": sha256(obu),
            },
        })
    return records


def validate(records: list[dict[str, object]], avmdec: Path,
             project: Path, work: Path) -> list[dict[str, object]]:
    require_executable(avmdec, "pinned avmdec")
    require_executable(project, "project decoder")
    avm_hash = sha256(avmdec.read_bytes())
    if avm_hash != PINNED_AVMDEC_SHA256:
        raise RuntimeError(
            f"avmdec sha256 {avm_hash}, expected {PINNED_AVMDEC_SHA256}")
    work.mkdir(parents=True, exist_ok=True)
    validation = []
    for record in records:
        name = str(record["name"])
        obu = FIXTURES / f"{name}.obu"
        avm = decode(avmdec, "avmdec", obu, work / f"{name}.avm.i400")
        local = decode(
            project, "project", obu, work / f"{name}.project.i400")
        source = (FIXTURES / f"{name}.i400").read_bytes()
        if avm != local:
            raise RuntimeError(f"{name}: validator outputs differ")
        validation.append({
            "name": name,
            "bytes": len(avm),
            "sha256": sha256(avm),
            "avmdec_project_exact": True,
            "source_exact": avm == source,
        })
    return validation


def check_generated(encoder: Path) -> None:
    expected = json.loads(MANIFEST.read_text(encoding="utf-8"))
    work = ENCODER / "build/m2-full-stream-regeneration"
    shutil.rmtree(work, ignore_errors=True)
    actual = generate(encoder, work)
    try:
        if actual != expected["cases"]:
            raise RuntimeError("full-stream manifest is stale")
        for record in actual:
            name = str(record["name"])
            for suffix in (".i400", ".obu"):
                if ((work / f"{name}{suffix}").read_bytes() !=
                        (FIXTURES / f"{name}{suffix}").read_bytes()):
                    raise RuntimeError(f"{name}{suffix}: fixture is stale")
    finally:
        shutil.rmtree(work, ignore_errors=True)


def write(encoder: Path, avmdec: Path, project: Path) -> None:
    FIXTURES.mkdir(parents=True, exist_ok=True)
    records = generate(encoder, FIXTURES)
    work = ENCODER / "build/m2-full-stream-validation"
    shutil.rmtree(work, ignore_errors=True)
    try:
        decoded = validate(records, avmdec, project, work)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    manifest = {
        "schema": "av2-encoder-m2-full-stream-matrix-v1",
        "description": (
            "Generated current M1/M3/M4 streams; both decoders must produce "
            "byte-exact matching reconstructed planes."
        ),
        "validators": {
            "avmdec": {
                "revision": "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d",
                "version": "v1.0.0",
                "sha256": sha256(avmdec.read_bytes()),
            },
            "project_decoder_generation_sha256": sha256(project.read_bytes()),
        },
        "cases": records,
        "decoded": decoded,
    }
    MANIFEST.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    parser.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    parser.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    if args.write:
        write(args.encoder, args.avmdec, args.project_decoder)
    else:
        check_generated(args.encoder)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

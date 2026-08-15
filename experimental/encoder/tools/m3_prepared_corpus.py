#!/usr/bin/env python3
"""Inventory and gate full prepared I400 images for the experimental encoder."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Sequence

from m3_benchmark import (
    BenchmarkError,
    DEFAULT_AVMDEC,
    DEFAULT_ENCODER,
    DEFAULT_PROJECT_DECODER,
    ROOT,
    command_record,
    decode_commands,
    distortion,
    executable,
    run,
    sha256_bytes,
    sha256_file,
)
from m3_dc_model import choose_dc_level, dc_prediction
from m3_multisuperblock_model import model as multisuperblock_model


ENCODER_ROOT = ROOT / "experimental" / "encoder"
PREPARED_LOCK = ROOT / "images" / "recipes" / "prepared-lock.json"
DEFAULT_MANIFEST = ENCODER_ROOT / "metrics" / "m3-prepared-corpus.json"
DEFAULT_OUTPUT = ENCODER_ROOT / "results" / "m3-prepared-corpus.json"
DEFAULT_WORK = ENCODER_ROOT / "build" / "m3-prepared-corpus"


def corpus_manifest(lock_path: Path = PREPARED_LOCK) -> dict[str, Any]:
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    records = sorted(
        lock["preparations"],
        key=lambda item: (item["source_id"], item["preparation_id"]),
    )
    selected: dict[str, tuple[int, str]] = {}
    for record in records:
        width, height = map(int, record["dimensions"])
        candidate = (width * height, record["preparation_id"])
        previous = selected.get(record["source_id"])
        if previous is None or candidate < previous:
            selected[record["source_id"]] = candidate

    inventory = []
    for record in records:
        width, height = map(int, record["dimensions"])
        gray = record["outputs"]["gray8"]
        source_manifest_path = (
            Path("images") / "sources" / "commons"
            / record["source_id"] / "source.json"
        )
        source_manifest = json.loads(
            (ROOT / source_manifest_path).read_text(encoding="utf-8")
        )
        original = source_manifest["original"]
        if (
            source_manifest["id"] != record["source_id"]
            or original["sha256"] != record["source_sha256"]
            or [original["width"], original["height"]]
            != record["source_dimensions"]
        ):
            raise BenchmarkError(
                f"source catalog disagrees with prepared lock: {record['source_id']}"
            )
        manifest_path = Path("images") / record["manifest"]
        plane_path = manifest_path.parent / gray["filename"]
        inventory.append({
            "id": f"{record['source_id']}-{record['preparation_id']}",
            "source_id": record["source_id"],
            "preparation_id": record["preparation_id"],
            "selected": (
                selected[record["source_id"]]
                == (width * height, record["preparation_id"])
            ),
            "pixel_format": "i400-8",
            "width": width,
            "height": height,
            "stride": width,
            "byte_size": int(gray["byte_size"]),
            "sha256": gray["sha256"],
            "path": str(plane_path),
            "preparation_manifest": str(manifest_path),
            "preparation_manifest_sha256": record["manifest_sha256"],
            "source_manifest": str(source_manifest_path),
            "source_manifest_sha256": sha256_file(ROOT / source_manifest_path),
            "source_sha256": record["source_sha256"],
            "source_dimensions": record["source_dimensions"],
            "source_original": {
                "filename": original["filename"],
                "mime_type": original["mime_type"],
                "width": int(original["width"]),
                "height": int(original["height"]),
                "byte_size": int(original["byte_size"]),
                "sha1": original["sha1"],
                "sha256": original["sha256"],
            },
            "formats": {
                name: {
                    "filename": output["filename"],
                    "byte_size": int(output["byte_size"]),
                    "sha256": output["sha256"],
                }
                for name, output in sorted(record["outputs"].items())
            },
        })
    return {
        "schema": "av2-encoder-m3-prepared-corpus-v1",
        "generated_by": "experimental/encoder/tools/m3_prepared_corpus.py",
        "prepared_lock": str(lock_path.relative_to(ROOT)),
        "prepared_lock_sha256": sha256_file(lock_path),
        "selection": {
            "policy": "minimum_pixel_area_then_preparation_id_per_source",
            "purpose": "one smallest full prepared I400 image for every source",
        },
        "inventory_count": len(inventory),
        "benchmark_count": sum(item["selected"] for item in inventory),
        "inventory": inventory,
    }


def verify_manifest(path: Path, expected: dict[str, Any]) -> None:
    actual = json.loads(path.read_text(encoding="utf-8"))
    if actual != expected:
        raise BenchmarkError(
            f"prepared corpus manifest is stale; regenerate with --write-manifest"
        )


def local_reconstruction(
    source: bytes, width: int, height: int, qindex: int
) -> bytes:
    padded_width = (width + 7) // 8 * 8
    padded_height = (height + 7) // 8 * 8
    padded = bytearray(padded_width * padded_height)
    traversal = multisuperblock_model(width, height)
    if not traversal["transform_exact_cover"]:
        raise BenchmarkError("multisuperblock model did not cover coded canvas")
    for block_x, block_y in traversal["transform_order"]:
        above = (
            padded[(block_y - 1) * padded_width + block_x:
                   (block_y - 1) * padded_width + block_x + 8]
            if block_y else None
        )
        left = (
            [padded[(block_y + offset) * padded_width + block_x - 1]
             for offset in range(8)]
            if block_x else None
        )
        prediction = dc_prediction(above, left, 8)
        samples = [
            source[min(block_y + dy, height - 1) * width
                   + min(block_x + dx, width - 1)]
            for dy in range(8) for dx in range(8)
        ]
        value = choose_dc_level(
            samples, prediction, 8, qindex
        ).reconstruction
        row = bytes([value]) * 8
        for dy in range(8):
            start = (block_y + dy) * padded_width + block_x
            padded[start:start + 8] = row
    return b"".join(
        padded[y * padded_width:y * padded_width + width]
        for y in range(height)
    )


def source_record(case: dict[str, Any]) -> tuple[Path, bytes]:
    path = ROOT / case["path"]
    manifest_path = ROOT / case["preparation_manifest"]
    if not path.is_file():
        raise BenchmarkError(f"prepared full-image luma is absent: {case['path']}")
    if not manifest_path.is_file():
        raise BenchmarkError(
            f"prepared image manifest is absent: {case['preparation_manifest']}"
        )
    if sha256_file(manifest_path) != case["preparation_manifest_sha256"]:
        raise BenchmarkError(
            f"prepared image manifest hash mismatch: {case['preparation_manifest']}"
        )
    metadata = json.loads(manifest_path.read_text(encoding="utf-8"))
    gray = metadata.get("outputs", {}).get("gray8", {})
    if (
        metadata.get("dimensions") != [case["width"], case["height"]]
        or metadata.get("source_sha256") != case["source_sha256"]
        or gray.get("filename") != path.name
        or gray.get("byte_size") != case["byte_size"]
        or gray.get("sha256") != case["sha256"]
    ):
        raise BenchmarkError(f"prepared metadata changed: {case['id']}")
    source = path.read_bytes()
    if len(source) != case["stride"] * case["height"]:
        raise BenchmarkError(f"prepared luma byte size changed: {case['id']}")
    if sha256_bytes(source) != case["sha256"]:
        raise BenchmarkError(f"prepared luma hash mismatch: {case['id']}")
    return path.resolve(), source


def run_case(
    case: dict[str, Any], qindex: int, encoder: Path, avmdec: Path,
    project_decoder: Path, work_root: Path,
) -> dict[str, Any]:
    record: dict[str, Any] = {
        "id": case["id"],
        "status": "error",
        "qindex": qindex,
        "input": {
            key: case[key] for key in (
                "path", "pixel_format", "width", "height", "stride",
                "byte_size", "sha256", "source_id", "source_sha256",
                "source_dimensions", "preparation_id",
                "preparation_manifest", "preparation_manifest_sha256",
                "source_manifest", "source_manifest_sha256",
            )
        },
        "attempts": [],
    }
    try:
        source_path, source = source_record(case)
    except (BenchmarkError, OSError, json.JSONDecodeError) as error:
        record.update({"status": "blocked", "reason": str(error)})
        return record

    case_dir = (work_root / case["id"]).resolve()
    case_dir.mkdir(parents=True, exist_ok=True)
    outputs = [case_dir / "encoded-1.obu", case_dir / "encoded-2.obu"]
    for output in outputs:
        output.unlink(missing_ok=True)
        command = [
            str(encoder), "--i400-8", str(source_path),
            "--width", str(case["width"]), "--height", str(case["height"]),
            "--stride", str(case["stride"]), "--qindex", str(qindex),
            str(output),
        ]
        completed, duration = run(command)
        attempt = command_record(command, completed, duration)
        if output.is_file():
            attempt["obu_bytes"] = output.stat().st_size
            attempt["obu_sha256"] = sha256_file(output)
        record["attempts"].append(attempt)

    returncodes = [attempt["returncode"] for attempt in record["attempts"]]
    if returncodes == [5, 5] and not any(output.exists() for output in outputs):
        record.update({
            "status": "expected_blocked",
            "reason": "full-image dimensions exceed the current 896-pixel M3 bound",
            "deterministic_rerun": True,
        })
        return record
    if returncodes != [0, 0] or not all(output.is_file() for output in outputs):
        record.update({
            "reason": "encoder reruns did not both succeed or expected-block",
            "deterministic_rerun": False,
        })
        return record
    hashes = [sha256_file(output) for output in outputs]
    if hashes[0] != hashes[1]:
        record.update({
            "reason": "encoder reruns produced different OBU hashes",
            "deterministic_rerun": False,
        })
        return record
    record.update({
        "deterministic_rerun": True,
        "obu_bytes": outputs[0].stat().st_size,
        "obu_sha256": hashes[0],
    })

    avm_command, project_command, avm_path, project_path = decode_commands(
        avmdec, project_decoder, outputs[0], case_dir
    )
    record["validators"] = {}
    for name, command in (("avm", avm_command), ("project", project_command)):
        completed, duration = run(command)
        record["validators"][name] = command_record(command, completed, duration)
        if completed.returncode != 0:
            record.update({
                "status": "error",
                "reason": f"{name} validator rejected after encoder success",
            })
            return record
    try:
        avm = avm_path.read_bytes()
        project = project_path.read_bytes()
        local = local_reconstruction(
            source, int(case["width"]), int(case["height"]), qindex
        )
        if not (len(local) == len(avm) == len(project) == case["byte_size"]):
            raise BenchmarkError("reconstruction byte size mismatch")
        record["reconstruction"] = {
            "local_sha256": sha256_bytes(local),
            "avm_sha256": sha256_bytes(avm),
            "project_sha256": sha256_bytes(project),
            "local_avm_project_exact": local == avm == project,
        }
        record["distortion"] = distortion(source, avm)
        if not record["reconstruction"]["local_avm_project_exact"]:
            raise BenchmarkError("local/AVM/project reconstruction mismatch")
    except (BenchmarkError, OSError) as error:
        record.update({"status": "error", "reason": str(error)})
        return record
    record["status"] = "pass"
    return record


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    result.add_argument("--write-manifest", action="store_true")
    result.add_argument("--inventory-only", action="store_true")
    result.add_argument("--encoder", type=Path, default=DEFAULT_ENCODER)
    result.add_argument("--avmdec", type=Path, default=DEFAULT_AVMDEC)
    result.add_argument("--project-decoder", type=Path, default=DEFAULT_PROJECT_DECODER)
    result.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    result.add_argument("--work-dir", type=Path, default=DEFAULT_WORK)
    result.add_argument("--source", action="append")
    result.add_argument("--qindex", type=int, default=100)
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        expected = corpus_manifest()
        if args.write_manifest:
            args.manifest.parent.mkdir(parents=True, exist_ok=True)
            args.manifest.write_text(
                json.dumps(expected, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        verify_manifest(args.manifest, expected)
        selected = [item for item in expected["inventory"] if item["selected"]]
        requested = set(args.source or ())
        if requested:
            selected = [item for item in selected if item["source_id"] in requested]
            if requested != {item["source_id"] for item in selected}:
                raise BenchmarkError("unknown source selection")
        availability = {
            item["id"]: (ROOT / item["path"]).is_file()
            for item in expected["inventory"]
        }
        if args.inventory_only:
            print(
                f"{len(expected['inventory'])} preparations, "
                f"{sum(availability.values())} local full-image I400 planes, "
                f"{len(selected)} benchmark selections"
            )
            return 0
        if not 1 <= args.qindex <= 255:
            raise BenchmarkError("qindex must be in 1..255")
        encoder = executable(args.encoder, "experimental encoder")
        avmdec = executable(args.avmdec, "pinned avmdec")
        project = executable(args.project_decoder, "project decoder")
        args.work_dir.mkdir(parents=True, exist_ok=True)
        records = [
            run_case(item, args.qindex, encoder, avmdec, project, args.work_dir)
            for item in selected
        ]
        result = {
            "schema": "av2-encoder-m3-prepared-corpus-results-v1",
            "manifest_sha256": sha256_file(args.manifest),
            "qindex": args.qindex,
            "tools": {
                "encoder_sha256": sha256_file(encoder),
                "avmdec_sha256": sha256_file(avmdec),
                "project_decoder_sha256": sha256_file(project),
            },
            "inventory": {
                "preparations": len(expected["inventory"]),
                "local_full_image_i400": sum(availability.values()),
                "benchmark_cases": len(selected),
            },
            "cases": records,
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    except (BenchmarkError, OSError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    counts = {
        status: sum(record["status"] == status for record in records)
        for status in ("pass", "expected_blocked", "blocked", "error")
    }
    print(", ".join(f"{name}={count}" for name, count in counts.items()))
    return 1 if any(
        counts[status] for status in ("expected_blocked", "blocked", "error")
    ) else 0


if __name__ == "__main__":
    raise SystemExit(main())

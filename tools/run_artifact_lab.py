#!/usr/bin/env python3
"""Run the AV2-HEIF artifact laboratory offline."""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import platform
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Any

from generate_i420_fixtures import FIXTURES, Fixture
from obu_carriage import hash_i420, parse_obus
from run_carriage_reference import artifact, decoded_output, reference_binary, verify_reference


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], commands: list[dict[str, object]]) -> None:
    commands.append({"argv": command, "display": shlex.join(command)})
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    subprocess.run(command, cwd=ROOT, env=environment, check=True)


def read_json(path: Path) -> Any:
    with path.open("r", encoding="ascii") as source:
        return json.load(source)


def command_output(command: list[str]) -> str:
    return subprocess.run(
        command,
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout.strip()


def obu_ledger(path: Path) -> list[dict[str, int | str]]:
    return [obu.ledger_entry() for obu in parse_obus(path.read_bytes())]


def require_identical(left: Path, right: Path, label: str) -> None:
    if left.read_bytes() != right.read_bytes():
        raise RuntimeError(f"{label} bytes differ: {left} and {right}")


def normalized_signature(entries: list[dict[str, int | str]]) -> list[dict[str, Any]]:
    keys = (
        "type",
        "temporal_id",
        "mlayer_id",
        "xlayer_id",
        "obu_size",
        "header_bytes",
        "sha256",
    )
    return [{key: entry[key] for key in keys} for entry in entries]


def encoded_byte_comparison(original_sha256: str, extracted_sha256: str) -> dict[str, object]:
    match = original_sha256 == extracted_sha256
    return {
        "classification": "identical" if match else "different-after-normalization",
        "match": match,
        "original_sha256": original_sha256,
        "extracted_sha256": extracted_sha256,
    }


def interpreted_metadata_comparison(
    source_metadata: dict[str, Any], effective: dict[str, Any]
) -> dict[str, object]:
    expected = {
        "width": source_metadata["width"],
        "height": source_metadata["height"],
        "pixel_format": source_metadata["pixel_format"],
        "bits_per_channel": source_metadata["bit_depth"],
        "range": source_metadata["range"],
        "cicp": source_metadata["cicp"],
    }
    actual = {key: effective.get(key) for key in expected}
    match = actual == expected
    return {
        "classification": "matching" if match else "mismatch",
        "match": match,
        "expected": expected,
        "actual": actual,
    }


def decode(
    decoder: Path,
    stream: Path,
    fixture_dir: Path,
    prefix: str,
    commands: list[dict[str, object]],
) -> Path:
    for stale in fixture_dir.glob(f"{prefix}-*.i420"):
        stale.unlink()
    output_pattern = fixture_dir / f"{prefix}-%wx%h-%1.i420"
    run(
        [
            str(decoder),
            "--codec=av2",
            "--i420",
            "--limit=1",
            "-o",
            str(output_pattern),
            str(stream),
        ],
        commands,
    )
    output, width, height = decoded_output(fixture_dir, prefix)
    return output, width, height


def run_fixture(
    fixture: Fixture,
    source_dir: Path,
    output_dir: Path,
    encoder: Path,
    decoder: Path,
    artifact_tool: Path,
) -> dict[str, object]:
    fixture_dir = output_dir / fixture.name
    fixture_dir.mkdir(parents=True, exist_ok=True)
    source = source_dir / f"{fixture.name}-{fixture.width}x{fixture.height}.i420"
    original_obu = fixture_dir / "original.obu"
    normalized_config = fixture_dir / "normalized-config.obu"
    normalized_payload = fixture_dir / "normalized-payload.obu"
    normalized_stream = fixture_dir / "normalized.obu"
    normalization_ledger = fixture_dir / "normalization-ledger.json"
    container = fixture_dir / "canonical.av2h"
    inspection_path = fixture_dir / "inspection.json"
    extracted_config = fixture_dir / "extracted-config.obu"
    extracted_payload = fixture_dir / "extracted-payload.obu"
    extracted_stream = fixture_dir / "extracted.obu"
    commands: list[dict[str, object]] = []

    run(
        [
            str(encoder),
            "--debug",
            "--codec=av2",
            "--obu",
            "--passes=1",
            "--limit=1",
            "--good",
            "--cpu-used=9",
            "--threads=1",
            "--profile=0",
            f"--width={fixture.width}",
            f"--height={fixture.height}",
            "--i420",
            "--bit-depth=8",
            "--input-bit-depth=8",
            "--input-chroma-subsampling-x=1",
            "--input-chroma-subsampling-y=1",
            "--chroma-sample-position=unspecified",
            "--force-video-mode=0",
            "--film-grain-test=0",
            "--enable-lcr=0",
            "--enable-operating-point-sets=0",
            "--enable-atlas=0",
            "--end-usage=q",
            "--qp=128",
            "--test-decode=fatal",
            "--quiet",
            "-o",
            str(original_obu),
            str(source),
        ],
        commands,
    )
    run(
        [
            sys.executable,
            str(ROOT / "tools" / "obu_carriage.py"),
            "split",
            str(original_obu),
            "--config",
            str(normalized_config),
            "--payload",
            str(normalized_payload),
            "--reconstructed",
            str(normalized_stream),
            "--ledger",
            str(normalization_ledger),
        ],
        commands,
    )
    run(
        [
            str(artifact_tool),
            "pack",
            str(normalized_config),
            str(normalized_payload),
            str(container),
            "2",
            "2",
            "2",
            "limited",
        ],
        commands,
    )
    run(
        [str(artifact_tool), "inspect", str(container), str(inspection_path)],
        commands,
    )
    run(
        [
            str(artifact_tool),
            "extract",
            str(container),
            str(extracted_config),
            str(extracted_payload),
            str(extracted_stream),
        ],
        commands,
    )
    require_identical(normalized_config, extracted_config, "configuration")
    require_identical(normalized_payload, extracted_payload, "item payload")
    require_identical(normalized_stream, extracted_stream, "logical stream")

    original_decode, original_width, original_height = decode(
        decoder, original_obu, fixture_dir, "original", commands
    )
    extracted_decode, extracted_width, extracted_height = decode(
        decoder, extracted_stream, fixture_dir, "extracted", commands
    )
    dimensions = (fixture.width, fixture.height)
    if (original_width, original_height) != dimensions or (
        extracted_width,
        extracted_height,
    ) != dimensions:
        raise RuntimeError(f"decoded dimensions differ for {fixture.name}")
    require_identical(original_decode, extracted_decode, "decoded I420 samples")

    source_metadata = {
        "width": fixture.width,
        "height": fixture.height,
        "pixel_format": "I420",
        "bit_depth": 8,
        "range": "limited",
        "cicp": {
            "color_primaries": 2,
            "transfer_characteristics": 2,
            "matrix_coefficients": 2,
        },
        "pattern": fixture.pattern,
    }
    inspection = read_json(inspection_path)
    effective = inspection["codec"]["effective"]
    metadata_comparison = interpreted_metadata_comparison(source_metadata, effective)
    if not metadata_comparison["match"]:
        raise RuntimeError(
            f"interpreted container metadata differs for {fixture.name}: "
            f"{metadata_comparison['actual']}"
        )

    original_ledger = obu_ledger(original_obu)
    normalized_ledger = obu_ledger(normalized_stream)
    extracted_ledger = obu_ledger(extracted_stream)
    if normalized_signature(normalized_ledger) != normalized_signature(
        extracted_ledger
    ):
        raise RuntimeError(f"normalized OBU structure differs for {fixture.name}")
    decoded = hash_i420(original_decode.read_bytes(), fixture.width, fixture.height)
    decoded["range"] = "not-reported-by-avmdec"
    decoded["cicp"] = "not-reported-by-avmdec"
    if inspection["query"]["output_bytes"] != decoded["bytes"]:
        raise RuntimeError(f"query output size differs for {fixture.name}")
    chroma_width = (fixture.width + 1) // 2
    chroma_height = (fixture.height + 1) // 2
    y_size = fixture.width * fixture.height
    chroma_size = chroma_width * chroma_height
    expected_planes = [
        {
            "index": 0,
            "width": fixture.width,
            "height": fixture.height,
            "row_bytes": fixture.width,
            "offset": 0,
            "bytes": y_size,
        },
        {
            "index": 1,
            "width": chroma_width,
            "height": chroma_height,
            "row_bytes": chroma_width,
            "offset": y_size,
            "bytes": chroma_size,
        },
        {
            "index": 2,
            "width": chroma_width,
            "height": chroma_height,
            "row_bytes": chroma_width,
            "offset": y_size + chroma_size,
            "bytes": chroma_size,
        },
    ]
    if inspection["query"]["planes"] != expected_planes:
        raise RuntimeError(f"query plane layout differs for {fixture.name}")
    original_obu_artifact = artifact(original_obu, ROOT)
    extracted_stream_artifact = artifact(extracted_stream, ROOT)
    result: dict[str, object] = {
        "name": fixture.name,
        "status": "pass",
        "source": artifact(source, ROOT),
        "source_metadata": source_metadata,
        "commands": commands,
        "artifacts": {
            "original_obu": original_obu_artifact,
            "normalization_ledger": artifact(normalization_ledger, ROOT),
            "normalized_config_obus": artifact(normalized_config, ROOT),
            "normalized_item_payload": artifact(normalized_payload, ROOT),
            "normalized_stream": artifact(normalized_stream, ROOT),
            "container": artifact(container, ROOT),
            "inspection": artifact(inspection_path, ROOT),
            "extracted_config_obus": artifact(extracted_config, ROOT),
            "extracted_item_payload": artifact(extracted_payload, ROOT),
            "extracted_stream": extracted_stream_artifact,
            "original_decode": artifact(original_decode, ROOT),
            "extracted_decode": artifact(extracted_decode, ROOT),
        },
        "container_inspection": inspection,
        "obu_ledgers": {
            "original": original_ledger,
            "normalized": normalized_ledger,
            "extracted": extracted_ledger,
        },
        "decoded": decoded,
        "comparison": {
            "encoded_bytes": encoded_byte_comparison(
                str(original_obu_artifact["sha256"]),
                str(extracted_stream_artifact["sha256"]),
            ),
            "normalized_obu_structure": {
                "classification": "identical",
                "match": True,
            },
            "interpreted_metadata": metadata_comparison,
            "decoded_planes": {
                "classification": "sample-exact",
                "match": True,
            },
            "lossy_quality": {
                "classification": "not-measured",
            },
        },
    }
    result_path = fixture_dir / "result.json"
    result_path.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    result["artifacts"]["result_manifest"] = artifact(result_path, ROOT)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir", type=Path, default=ROOT / "build" / "reference" / "avm"
    )
    parser.add_argument(
        "--artifact-tool",
        type=Path,
        default=ROOT / "build" / "host" / "av2heif-artifact",
    )
    parser.add_argument(
        "--output-dir", type=Path, default=ROOT / "build" / "m3" / "laboratory"
    )
    parser.add_argument(
        "--fixture", choices=[fixture.name for fixture in FIXTURES], action="append"
    )
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    build_dir = args.build_dir.resolve()
    artifact_tool = args.artifact_tool.resolve()
    output_dir = args.output_dir.resolve()
    source_dir = output_dir / "sources"
    output_dir.mkdir(parents=True, exist_ok=True)
    if not artifact_tool.is_file():
        raise SystemExit(f"error: missing artifact tool: {artifact_tool}")

    encoder = reference_binary(build_dir, "avmenc")
    decoder = reference_binary(build_dir, "avmdec")
    reference = verify_reference(ROOT / "vendor" / "avm", build_dir, encoder, decoder)
    selected = [
        fixture
        for fixture in FIXTURES
        if not args.fixture or fixture.name in args.fixture
    ]
    generation_command = [
        sys.executable,
        str(ROOT / "tools" / "generate_i420_fixtures.py"),
        "--output-dir",
        str(source_dir),
    ]
    for fixture in selected:
        generation_command.extend(["--fixture", fixture.name])
    source_commands: list[dict[str, object]] = []
    run(generation_command, source_commands)

    with concurrent.futures.ThreadPoolExecutor(
        max_workers=min(args.jobs, len(selected))
    ) as executor:
        results = list(executor.map(
            lambda fixture: run_fixture(
                fixture, source_dir, output_dir, encoder, decoder, artifact_tool
            ),
            selected,
        ))
    compiler = os.environ.get("ARTIFACT_LAB_COMPILER", os.environ.get("CC", "cc"))
    compiler_argv = shlex.split(compiler)
    if not compiler_argv:
        raise SystemExit("error: empty artifact compiler command")
    tool_identity = artifact(artifact_tool, ROOT)
    tool_identity.update(
        {
            "name": "av2heif-artifact",
            "revision": command_output(["git", "rev-parse", "HEAD"]),
            "worktree": command_output(["git", "status", "--short"]),
            "compiler": {
                "command": compiler,
                "argv": compiler_argv,
                "identity": command_output(
                    [*compiler_argv, "--version"]
                ).splitlines()[0],
            },
            "build_options": os.environ.get(
                "ARTIFACT_LAB_BUILD_OPTIONS",
                "unknown (run through `make test-artifact-lab` to record exact flags)",
            ),
            "sources": {
                path: artifact(ROOT / path, ROOT)
                for path in (
                    "tools/av2heif_artifact.c",
                    "include/av2heif.h",
                    "src/base.c",
                    "src/carriage.c",
                    "src/carriage.h",
                    "src/bmff.c",
                    "src/container.c",
                    "src/container.h",
                    "src/container_write.c",
                    "src/decoder/obu.c",
                    "src/decoder/internal/obu.h",
                    "src/decoder/sequence.c",
                    "src/decoder/sequence.h",
                    "src/decoder/content_interpretation.c",
                )
            },
        }
    )
    manifest = {
        "schema": "av2heif-artifact-laboratory-v1",
        "experiment": "AV2-HEIF experimental v0 artifact laboratory",
        "status": "pass",
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "tools": {
            "reference": reference,
            "artifact_tool": tool_identity,
        },
        "source_generation_commands": source_commands,
        "generated_artifacts": {
            "source_manifest": artifact(source_dir / "sources.json", ROOT),
        },
        "fixtures": results,
    }
    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    print(f"artifact laboratory: {len(results)} fixtures passed")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
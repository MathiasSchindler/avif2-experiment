#!/usr/bin/env python3
"""Run the AVM differential carriage experiment for all carriage experiment fixtures."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from generate_i420_fixtures import FIXTURES, Fixture


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_AVM_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact(path: Path, base: Path) -> dict[str, object]:
    return {
        "path": str(path.relative_to(base)),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def run(command: list[str], commands: list[list[str]], *, capture: bool = False) -> str:
    commands.append(command)
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    result = subprocess.run(
        command,
        cwd=ROOT,
        env=environment,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )
    return result.stdout if capture else ""


def git_output(vendor_dir: Path, *arguments: str) -> str:
    return subprocess.run(
        ["git", "-C", str(vendor_dir), *arguments],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout.strip()


def cmake_build_identity(build_dir: Path) -> dict[str, object]:
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        raise RuntimeError(f"missing AVM CMake cache: {cache_path}")
    cache: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        key_and_type, value = line.split("=", 1)
        key = key_and_type.split(":", 1)[0]
        cache[key] = value

    selected_keys = (
        "CMAKE_BUILD_TYPE",
        "CMAKE_GENERATOR",
        "CMAKE_C_COMPILER",
        "CMAKE_CXX_COMPILER",
        "CONFIG_AV2_DECODER",
        "CONFIG_AV2_ENCODER",
        "CONFIG_TENSORFLOW_LITE",
        "CONFIG_WEBM_IO",
        "ENABLE_DOCS",
        "ENABLE_EXAMPLES",
        "ENABLE_TESTS",
        "ENABLE_TOOLS",
    )
    configuration = {key: cache[key] for key in selected_keys if key in cache}
    for language in ("C", "CXX"):
        key = f"CMAKE_{language}_COMPILER"
        if key in configuration:
            continue
        metadata_files = list(
            (build_dir / "CMakeFiles").glob(
                f"*/CMake{language}Compiler.cmake"
            )
        )
        for metadata_path in metadata_files:
            match = re.search(
                rf'^set\({key} "([^"]+)"\)$',
                metadata_path.read_text(encoding="utf-8"),
                re.MULTILINE,
            )
            if match is not None:
                configuration[key] = match.group(1)
                break
    compiler = configuration.get("CMAKE_C_COMPILER")
    if compiler is None:
        raise RuntimeError("AVM CMake cache does not identify the C compiler")
    compiler_identity = subprocess.run(
        [compiler, "--version"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout.splitlines()[0]
    return {
        "configuration": configuration,
        "c_compiler_identity": compiler_identity,
    }


def binary_identity(path: Path) -> dict[str, object]:
    identity = artifact(path, ROOT)
    file_command = shutil.which("file")
    if file_command is not None:
        identity["file_description"] = subprocess.run(
            [file_command, "-b", str(path)],
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ).stdout.strip()
    else:
        with path.open("rb") as binary:
            identity["file_description"] = (
                "PE executable" if binary.read(2) == b"MZ" else "unknown"
            )
    return identity


def reference_binary(build_dir: Path, name: str) -> Path:
    binary = build_dir / name
    if binary.is_file():
        return binary
    windows_binary = binary.with_suffix(".exe")
    return windows_binary if windows_binary.is_file() else binary


def verify_reference(
    vendor_dir: Path, build_dir: Path, encoder: Path, decoder: Path
) -> dict[str, object]:
    revision = git_output(vendor_dir, "rev-parse", "HEAD")
    if revision != EXPECTED_AVM_REVISION:
        raise RuntimeError(
            f"AVM revision mismatch: expected {EXPECTED_AVM_REVISION}, got {revision}"
        )
    dirty = git_output(vendor_dir, "status", "--short")
    if dirty:
        raise RuntimeError("vendored AVM checkout has local modifications")
    for binary in (encoder, decoder):
        if not binary.is_file():
            raise RuntimeError(f"missing reference binary: {binary}")

    encoder_help = subprocess.run(
        [str(encoder), "--help"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout
    decoder_help = subprocess.run(
        [str(decoder), "--help"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout
    for marker in ("--obu", "--passes=<arg>"):
        if marker not in encoder_help:
            raise RuntimeError(f"pinned avmenc help lacks expected marker: {marker}")
    for marker in ("--i420", "AOMedia Project AV2 Decoder 1.0.0"):
        if marker not in decoder_help:
            raise RuntimeError(f"pinned avmdec help lacks expected marker: {marker}")

    return {
        "name": "AVM",
        "tag": git_output(vendor_dir, "describe", "--tags", "--exact-match"),
        "revision": revision,
        "checkout_clean": True,
        "build": cmake_build_identity(build_dir),
        "encoder": binary_identity(encoder),
        "decoder": binary_identity(decoder),
    }


def decoded_output(directory: Path, prefix: str) -> tuple[Path, int, int]:
    matches = list(directory.glob(f"{prefix}-*.i420"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one {prefix} decode, found {len(matches)}")
    match = re.fullmatch(
        rf"{re.escape(prefix)}-(\d+)x(\d+)-\d+\.i420", matches[0].name
    )
    if match is None:
        raise RuntimeError(f"decoder produced unexpected filename: {matches[0].name}")
    return matches[0], int(match.group(1)), int(match.group(2))


def run_fixture(
    fixture: Fixture,
    source_dir: Path,
    output_dir: Path,
    encoder: Path,
    decoder: Path,
) -> dict[str, object]:
    fixture_dir = output_dir / fixture.name
    fixture_dir.mkdir(parents=True, exist_ok=True)
    source = source_dir / f"{fixture.name}-{fixture.width}x{fixture.height}.i420"
    original_obu = fixture_dir / "original.obu"
    config_obu = fixture_dir / "config.obu"
    payload_obu = fixture_dir / "payload.obu"
    reconstructed_obu = fixture_dir / "reconstructed.obu"
    ledger = fixture_dir / "obu-ledger.json"
    commands: list[list[str]] = []

    encode_command = [
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
    ]
    run(encode_command, commands)

    split_command = [
        sys.executable,
        str(ROOT / "tools" / "obu_carriage.py"),
        "split",
        str(original_obu),
        "--config",
        str(config_obu),
        "--payload",
        str(payload_obu),
        "--reconstructed",
        str(reconstructed_obu),
        "--ledger",
        str(ledger),
    ]
    run(split_command, commands)

    for prefix in ("original", "reconstructed"):
        for stale in fixture_dir.glob(f"{prefix}-*.i420"):
            stale.unlink()
    original_pattern = fixture_dir / "original-%wx%h-%1.i420"
    reconstructed_pattern = fixture_dir / "reconstructed-%wx%h-%1.i420"
    run(
        [
            str(decoder),
            "--codec=av2",
            "--i420",
            "--limit=1",
            "-o",
            str(original_pattern),
            str(original_obu),
        ],
        commands,
    )
    run(
        [
            str(decoder),
            "--codec=av2",
            "--i420",
            "--limit=1",
            "-o",
            str(reconstructed_pattern),
            str(reconstructed_obu),
        ],
        commands,
    )

    original_decode, original_width, original_height = decoded_output(
        fixture_dir, "original"
    )
    reconstructed_decode, reconstructed_width, reconstructed_height = decoded_output(
        fixture_dir, "reconstructed"
    )
    expected_dimensions = (fixture.width, fixture.height)
    if (original_width, original_height) != expected_dimensions:
        raise RuntimeError(f"original decode dimensions differ for {fixture.name}")
    if (reconstructed_width, reconstructed_height) != expected_dimensions:
        raise RuntimeError(f"reconstructed decode dimensions differ for {fixture.name}")
    if original_decode.read_bytes() != reconstructed_decode.read_bytes():
        raise RuntimeError(f"decoded I420 samples differ for {fixture.name}")

    decoded_hash_command = [
        sys.executable,
        str(ROOT / "tools" / "obu_carriage.py"),
        "hash-i420",
        str(original_decode),
        f"--width={fixture.width}",
        f"--height={fixture.height}",
    ]
    decoded = json.loads(run(decoded_hash_command, commands, capture=True))
    ledger_data = json.loads(ledger.read_text(encoding="ascii"))
    result = {
        "name": fixture.name,
        "status": "pass",
        "source": artifact(source, ROOT),
        "source_metadata": {
            "format": "I420",
            "bit_depth": 8,
            "width": fixture.width,
            "height": fixture.height,
            "pattern": fixture.pattern,
        },
        "commands": [
            {"argv": command, "display": shlex.join(command)} for command in commands
        ],
        "artifacts": {
            "original_obu": artifact(original_obu, ROOT),
            "config_obus": artifact(config_obu, ROOT),
            "item_payload": artifact(payload_obu, ROOT),
            "reconstructed_obu": artifact(reconstructed_obu, ROOT),
            "original_decode": artifact(original_decode, ROOT),
            "reconstructed_decode": artifact(reconstructed_decode, ROOT),
        },
        "obu_ledger": ledger_data,
        "decoded": decoded,
        "comparison": {
            "dimensions_identical": True,
            "format_identical": True,
            "samples_byte_identical": True,
        },
    }
    (fixture_dir / "result.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--build-dir", type=Path, default=ROOT / "build" / "reference" / "avm"
    )
    parser.add_argument(
        "--output-dir", type=Path, default=ROOT / "build" / "m0" / "matrix"
    )
    parser.add_argument(
        "--fixture", choices=[fixture.name for fixture in FIXTURES], action="append"
    )
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    build_dir = args.build_dir.resolve()
    output_dir = args.output_dir.resolve()
    source_dir = output_dir / "sources"
    output_dir.mkdir(parents=True, exist_ok=True)

    encoder = reference_binary(build_dir, "avmenc")
    decoder = reference_binary(build_dir, "avmdec")
    reference = verify_reference(
        ROOT / "vendor" / "avm", build_dir, encoder, decoder
    )
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
    generation_commands: list[list[str]] = []
    run(generation_command, generation_commands)

    with concurrent.futures.ThreadPoolExecutor(
        max_workers=min(args.jobs, len(selected))
    ) as executor:
        results = list(executor.map(
            lambda fixture: run_fixture(
                fixture, source_dir, output_dir, encoder, decoder
            ),
            selected,
        ))
    manifest = {
        "experiment": "AV2-HEIF experimental v0 carriage experiment",
        "assumption": "A2",
        "status": "pass",
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "reference": reference,
        "source_generation_commands": generation_commands,
        "fixtures": results,
    }
    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    print(f"carriage experiment: {len(results)} fixtures passed")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3
"""Validate the tracked spatial-still-8bit release-admission corpus."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "tests" / "spatial-still-profile.json"
PINNED_AVM_REVISION = "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d"
FORMATS = {"i400": 1, "i420": 0, "i444": 2}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def expected_size(pixel_format: str, width: int, height: int) -> int:
    if pixel_format == "i400":
        return width * height
    if pixel_format == "i420":
        return width * height + 2 * ((width + 1) // 2) * ((height + 1) // 2)
    if pixel_format == "i444":
        return width * height * 3
    raise ValueError(f"unsupported pixel format: {pixel_format}")


def validate_manifest(
    manifest: dict[str, object],
) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    if manifest.get("schema") != "av2h-spatial-still-profile-v1":
        raise ValueError("unsupported spatial-still profile manifest schema")
    if manifest.get("avm_revision") != PINNED_AVM_REVISION:
        raise ValueError("spatial-still profile does not use the pinned AVM revision")
    required = manifest.get("required_feature_families")
    cases = manifest.get("cases")
    witnesses = manifest.get("focused_witnesses")
    if not isinstance(required, list) or not required or len(required) != len(set(required)):
        raise ValueError("required_feature_families must be a unique nonempty list")
    if not isinstance(cases, list) or not cases:
        raise ValueError("cases must be a nonempty list")
    if not isinstance(witnesses, list) or not witnesses:
        raise ValueError("focused_witnesses must be a nonempty list")
    covered: set[str] = set()
    identifiers: set[str] = set()
    formats: set[str] = set()
    for case in cases:
        if not isinstance(case, dict):
            raise ValueError("case records must be objects")
        identifier = case.get("id")
        features = case.get("features")
        if not isinstance(identifier, str) or not identifier or identifier in identifiers:
            raise ValueError(f"invalid or duplicate case id: {identifier}")
        if not isinstance(features, list) or not features:
            raise ValueError(f"{identifier}: features must be a nonempty list")
        if case.get("pixel_format") not in FORMATS:
            raise ValueError(f"{identifier}: unsupported pixel format")
        if not isinstance(case.get("width"), int) or case["width"] < 1:
            raise ValueError(f"{identifier}: invalid width")
        if not isinstance(case.get("height"), int) or case["height"] < 1:
            raise ValueError(f"{identifier}: invalid height")
        for field in ("encoded_sha256", "decoded_sha256"):
            value = case.get(field)
            if not isinstance(value, str) or len(value) != 64:
                raise ValueError(f"{identifier}: invalid {field}")
        provenance = case.get("provenance")
        if not isinstance(provenance, str) or not provenance:
            raise ValueError(f"{identifier}: missing provenance")
        identifiers.add(identifier)
        formats.add(str(case["pixel_format"]))
        covered.update(str(feature) for feature in features)
    witness_identifiers: set[str] = set()
    for witness in witnesses:
        if not isinstance(witness, dict):
            raise ValueError("focused witness records must be objects")
        identifier = witness.get("id")
        features = witness.get("features")
        source_sha256 = witness.get("source_sha256")
        if (
            not isinstance(identifier, str)
            or not identifier
            or identifier in witness_identifiers
        ):
            raise ValueError(f"invalid or duplicate focused witness id: {identifier}")
        if not isinstance(features, list) or not features:
            raise ValueError(f"{identifier}: witness features must be a nonempty list")
        if not isinstance(source_sha256, str) or len(source_sha256) != 64:
            raise ValueError(f"{identifier}: invalid source_sha256")
        if not isinstance(witness.get("test"), str) or not witness["test"]:
            raise ValueError(f"{identifier}: missing test binary")
        if not isinstance(witness.get("source"), str) or not witness["source"]:
            raise ValueError(f"{identifier}: missing witness source")
        witness_identifiers.add(identifier)
        covered.update(str(feature) for feature in features)
    missing = set(str(feature) for feature in required) - covered
    if missing:
        raise ValueError("missing feature families: " + ", ".join(sorted(missing)))
    if formats != set(FORMATS):
        raise ValueError("profile corpus must cover I400, I420, and I444")
    return cases, witnesses


def run_witness(host_build: Path, witness: dict[str, object]) -> None:
    identifier = str(witness["id"])
    source = ROOT / str(witness["source"])
    if not source.is_file():
        raise ValueError(f"{identifier}: missing witness source {source}")
    digest = sha256(source)
    if digest != witness["source_sha256"]:
        raise ValueError(
            f"{identifier}: source SHA-256 {digest}, expected "
            f"{witness['source_sha256']}"
        )
    test = host_build / str(witness["test"])
    completed = subprocess.run(
        [str(test)], cwd=ROOT, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{identifier}: {test} failed with {completed.returncode}: "
            f"{completed.stderr.strip()}"
        )
    print(f"{identifier}: focused witness passed")


def decode_case(decoder: Path, output_dir: Path, case: dict[str, object]) -> None:
    identifier = str(case["id"])
    encoded = ROOT / str(case["path"])
    if not encoded.is_file():
        raise ValueError(f"{identifier}: missing fixture {encoded}")
    encoded_digest = sha256(encoded)
    if encoded_digest != case["encoded_sha256"]:
        raise ValueError(
            f"{identifier}: encoded SHA-256 {encoded_digest}, expected "
            f"{case['encoded_sha256']}"
        )
    pixel_format = str(case["pixel_format"])
    output = output_dir / f"{identifier}.{pixel_format}"
    completed = subprocess.run(
        [str(decoder), str(encoded), "-o", str(output)],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{identifier}: decoder exit {completed.returncode}: "
            f"{completed.stderr.strip()}"
        )
    size = expected_size(pixel_format, int(case["width"]), int(case["height"]))
    if output.stat().st_size != size:
        raise RuntimeError(f"{identifier}: decoded {output.stat().st_size} bytes, expected {size}")
    digest = sha256(output)
    if digest != case["decoded_sha256"]:
        raise RuntimeError(
            f"{identifier}: decoded SHA-256 {digest}, expected {case['decoded_sha256']}"
        )
    print(f"{identifier}: {size} bytes exact")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--decoder", type=Path)
    parser.add_argument("--host-build", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    cases, witnesses = validate_manifest(manifest)
    if args.decoder is None and args.host_build is None:
        print(
            f"spatial-still profile manifest: {len(cases)} cases and "
            f"{len(witnesses)} focused witnesses complete"
        )
        return 0
    if args.decoder is not None:
        decoder = args.decoder.resolve()
        with tempfile.TemporaryDirectory(prefix="av2h-spatial-still-") as scratch:
            for case in cases:
                decode_case(decoder, Path(scratch), case)
        print(f"spatial-still profile decode: {len(cases)} cases exact")
    if args.host_build is not None:
        host_build = args.host_build.resolve()
        for witness in witnesses:
            run_witness(host_build, witness)
        print(f"spatial-still focused witnesses: {len(witnesses)} passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

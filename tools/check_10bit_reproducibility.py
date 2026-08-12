#!/usr/bin/env python3
"""Regenerate the retained 10-bit corpus and require byte-identical output."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CORPUS = ROOT / "tests" / "fixtures" / "10bit"
DEFAULT_GENERATOR = ROOT / "tools" / "generate_10bit_synthetic_references.py"


def snapshot(directory: Path, *, exclude_manifest: bool = False) -> dict[str, str]:
    return {
        path.relative_to(directory).as_posix(): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(directory.rglob("*"))
        if path.is_file() and not (exclude_manifest and path.name == "manifest.json")
    }


def normalize_manifest_paths(value: Any, old: str, new: str) -> Any:
    if isinstance(value, dict):
        return {
            key: normalize_manifest_paths(item, old, new)
            for key, item in value.items()
        }
    if isinstance(value, list):
        return [normalize_manifest_paths(item, old, new) for item in value]
    if isinstance(value, str):
        return value.replace(old, new)
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument(
        "--generator",
        type=Path,
        default=DEFAULT_GENERATOR,
        help="deterministic generator to rerun before comparing the corpus",
    )
    args = parser.parse_args()

    corpus = args.corpus.resolve()
    generator = args.generator.resolve()
    if not corpus.is_dir():
        raise SystemExit(f"10-bit corpus directory does not exist: {corpus}")
    if not generator.is_file():
        raise SystemExit(f"10-bit corpus generator does not exist: {generator}")
    retained = corpus / "avm-reference"
    if not retained.is_dir():
        raise SystemExit(f"retained reference directory does not exist: {retained}")
    build_directory = ROOT / "build"
    build_directory.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="10bit-reproducibility-", dir=build_directory
    ) as temporary_text:
        generated = Path(temporary_text) / "avm-reference"
        subprocess.run(
            [
                sys.executable,
                str(generator),
                "--source-dir",
                str(corpus),
                "--output-dir",
                str(generated),
            ],
            cwd=ROOT,
            check=True,
        )
        generated_hashes = snapshot(generated, exclude_manifest=True)
        retained_hashes = snapshot(retained, exclude_manifest=True)
        failed = False
        for path in sorted(generated_hashes):
            if path not in retained_hashes:
                print(f"missing retained output: {path}")
                failed = True
            elif generated_hashes[path] != retained_hashes[path]:
                print(f"changed generated output: {path}")
                failed = True

        generated_relative = generated.relative_to(ROOT).as_posix()
        retained_relative = retained.relative_to(ROOT).as_posix()
        generated_manifest = normalize_manifest_paths(
            json.loads((generated / "manifest.json").read_text(encoding="utf-8")),
            generated_relative,
            retained_relative,
        )
        retained_manifest = json.loads(
            (retained / "manifest.json").read_text(encoding="utf-8")
        )
        if generated_manifest != retained_manifest:
            print("changed generated output: manifest.json")
            failed = True
        if failed:
            return 1

    print(
        "10-bit corpus reproducible: "
        f"{len(generated_hashes) + 1} generator-owned files exact"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

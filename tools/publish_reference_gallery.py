#!/usr/bin/env python3
"""Publish the local AVM photographic reference corpus for the docs gallery."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import tempfile
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "images"
LOCK_PATH = IMAGES / "recipes" / "reference-lock.json"
SOURCE_ROOT = IMAGES / "sources" / "commons"
LICENSE_ROOT = IMAGES / "licenses"
DEFAULT_OUTPUT = ROOT / "docs" / "reference"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_verified(source: Path, destination: Path, expected: dict[str, object]) -> None:
    if not source.is_file():
        raise RuntimeError(f"missing reference file: {source}")
    if source.stat().st_size != expected["byte_size"]:
        raise RuntimeError(f"size mismatch: {source}")
    if sha256(source) != expected["sha256"]:
        raise RuntimeError(f"SHA-256 mismatch: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def display_name(source_id: str) -> str:
    return " ".join(word.capitalize() for word in source_id.split("-"))


def publish(output: Path) -> None:
    lock = json.loads(LOCK_PATH.read_text(encoding="utf-8"))
    references = lock["references"]
    by_source: dict[str, list[dict[str, object]]] = defaultdict(list)

    with tempfile.TemporaryDirectory(prefix="reference-gallery-", dir=output.parent) as temporary:
        staging = Path(temporary) / output.name
        streams = staging / "streams"
        metadata_root = staging / "sources" / "commons"
        streams.mkdir(parents=True)

        for reference in references:
            source_id = reference["source_id"]
            case_id = reference["case_id"]
            output_record = reference["outputs"]["encoded_obu"]
            decoded_record = reference["outputs"]["decoded"]
            case_manifest = IMAGES / reference["manifest"]
            source_obu = case_manifest.parent / output_record["filename"]
            relative_obu = Path("streams") / f"{case_id}.obu"
            copy_verified(source_obu, staging / relative_obu, output_record)
            width, height = reference["dimensions"]
            by_source[source_id].append(
                {
                    "id": case_id,
                    "file": relative_obu.as_posix(),
                    "preparation": reference["preparation_id"],
                    "pixel_format": reference["pixel_format"],
                    "qp": reference["qp"],
                    "width": width,
                    "height": height,
                    "bytes": output_record["byte_size"],
                    "sha256": output_record["sha256"],
                    "decoded_bytes": decoded_record["byte_size"],
                    "decoded_sha256": decoded_record["sha256"],
                }
            )

        sources = []
        copied_licenses: set[str] = set()
        for source_id, variants in sorted(by_source.items()):
            source_path = SOURCE_ROOT / source_id / "source.json"
            source = json.loads(source_path.read_text(encoding="utf-8"))
            metadata_path = metadata_root / source_id / "source.json"
            metadata_path.parent.mkdir(parents=True)
            shutil.copyfile(source_path, metadata_path)

            license_name = Path(source["license"]["local_file"]).name
            if license_name not in copied_licenses:
                license_source = LICENSE_ROOT / license_name
                license_destination = staging / "licenses" / license_name
                license_destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(license_source, license_destination)
                copied_licenses.add(license_name)

            variants.sort(
                key=lambda item: (
                    item["preparation"], item["pixel_format"], item["qp"]
                )
            )
            sources.append(
                {
                    "id": source_id,
                    "name": display_name(source_id),
                    "title": source["title"],
                    "creator": source["creator"],
                    "attribution": source["attribution"],
                    "commons_url": source["commons"]["file_page_url"],
                    "license": {
                        "name": source["license"]["name"],
                        "spdx": source["license"]["spdx"],
                        "url": source["license"]["url"],
                        "local_file": f"licenses/{license_name}",
                    },
                    "metadata": f"sources/commons/{source_id}/source.json",
                    "variants": variants,
                }
            )

        manifest = {
            "schema": "av2h-reference-gallery-v1",
            "avm_revision": lock["avm"]["revision"],
            "source_count": len(sources),
            "variant_count": len(references),
            "sources": sources,
        }
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        if output.exists():
            shutil.rmtree(output)
        staging.replace(output)

    total_bytes = sum(path.stat().st_size for path in output.rglob("*.obu"))
    print(
        f"Published {len(sources)} sources and {len(references)} OBUs "
        f"to {output} ({total_bytes} bytes)"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args()
    output = arguments.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    publish(output)


if __name__ == "__main__":
    main()
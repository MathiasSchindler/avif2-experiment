#!/usr/bin/env python3
"""Validate a built AV2 quality viewer site."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


EXPECTED_QPS = list(range(16, 241, 16))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    site = args.site.resolve()
    required = ("index.html", "styles.css", "app.js", "decoder.wasm", "manifest.json")
    for name in required:
        if not (site / name).is_file():
            raise RuntimeError(f"missing site file: {name}")

    manifest = json.loads((site / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != "av2h-quality-viewer-v1":
        raise RuntimeError("unexpected manifest schema")
    scenes = manifest.get("scenes", [])
    if [scene.get("id") for scene in scenes] != ["overview", "inscription"]:
        raise RuntimeError("unexpected scene set")
    for scene in scenes:
        if (scene.get("width"), scene.get("height")) != (960, 960):
            raise RuntimeError(f"unexpected dimensions for {scene.get('id')}")
        variants = scene.get("variants", [])
        if [variant.get("qp") for variant in variants] != EXPECTED_QPS:
            raise RuntimeError(f"incomplete QP series for {scene.get('id')}")
        for variant in variants:
            path = site / variant["file"]
            if not path.is_file():
                raise RuntimeError(f"missing OBU: {path.name}")
            if path.stat().st_size != variant["bytes"]:
                raise RuntimeError(f"size mismatch: {path.name}")
            if sha256(path) != variant["sha256"]:
                raise RuntimeError(f"hash mismatch: {path.name}")

    intermediates = list(site.glob("*.i420"))
    if intermediates:
        raise RuntimeError(f"intermediate I420 files remain: {intermediates}")
    print(f"Validated {len(scenes)} scenes and {len(EXPECTED_QPS) * len(scenes)} OBUs")


if __name__ == "__main__":
    main()
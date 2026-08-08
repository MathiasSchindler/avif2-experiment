#!/usr/bin/env python3
"""Validate a built AV2 quality viewer site."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


EXPECTED_QPS = [0, 4, *range(8, 217, 8), *range(220, 253, 4)]
EXPECTED_QUALITIES = [*range(1, 11), *range(15, 100, 5), 96, 97, 98]
EXPECTED_AVIF_QUALITIES = [5, 6, 8, 10, *range(15, 100, 5), 96, 98]
EXPECTED_FORMATS = {
    "jpeg": ("JPEG", "4:2:0", ".jpg", EXPECTED_QUALITIES),
    "webp": ("WebP", "method 6", ".webp", EXPECTED_QUALITIES),
    "avif": ("AVIF", "speed 6", ".avif", EXPECTED_AVIF_QUALITIES),
}
EXPECTED_BASELINE_IDS = [
    f"{format_id}-q{quality:02d}"
    for format_id, (*_, qualities) in EXPECTED_FORMATS.items()
    for quality in qualities
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def has_expected_signature(path: Path, format_name: str) -> bool:
    header = path.read_bytes()[:32]
    if format_name == "JPEG":
        return header.startswith(b"\xff\xd8\xff")
    if format_name == "WebP":
        return header.startswith(b"RIFF") and header[8:12] == b"WEBP"
    if format_name == "AVIF":
        return header[4:8] == b"ftyp" and b"avif" in header[8:32]
    return False


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("site", type=Path)
    args = parser.parse_args()
    site = args.site.resolve()
    assets = site / "assets"
    required = ("index.html", "viewer.css", "viewer.js", "decoder.wasm")
    for name in required:
        if not (site / name).is_file():
            raise RuntimeError(f"missing site file: {name}")
    if not (assets / "manifest.json").is_file():
        raise RuntimeError("missing site file: assets/manifest.json")

    manifest = json.loads(
        (assets / "manifest.json").read_text(encoding="utf-8")
    )
    if manifest.get("schema") != "av2h-quality-viewer-v5":
        raise RuntimeError("unexpected manifest schema")
    if manifest.get("encoder", {}).get("qp_values") != EXPECTED_QPS:
        raise RuntimeError("unexpected AV2 QP metadata")
    baseline_encoder = manifest.get("baseline_encoder", {})
    if (
        baseline_encoder.get("implementation") != "ImageMagick"
        or baseline_encoder.get("qualities") != {
            format_id: qualities
            for format_id, (*_, qualities) in EXPECTED_FORMATS.items()
        }
        or "does not imply equal visual quality" not in baseline_encoder.get("note", "")
    ):
        raise RuntimeError("unexpected baseline encoder metadata")
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
            path = assets / variant["file"]
            if not path.is_file():
                raise RuntimeError(f"missing OBU: {path.name}")
            if path.stat().st_size != variant["bytes"]:
                raise RuntimeError(f"size mismatch: {path.name}")
            if sha256(path) != variant["sha256"]:
                raise RuntimeError(f"hash mismatch: {path.name}")
        baselines = scene.get("baselines", [])
        if [baseline.get("id") for baseline in baselines] != EXPECTED_BASELINE_IDS:
            raise RuntimeError(f"incomplete baselines for {scene.get('id')}")
        for baseline in baselines:
            format_id = baseline["id"].split("-", 1)[0]
            format_name, setting_suffix, suffix, qualities = EXPECTED_FORMATS[format_id]
            quality = baseline["quality"]
            if (
                baseline.get("format") != format_name
                or quality not in qualities
                or baseline.get("setting") != f"quality {quality} / {setting_suffix}"
                or Path(baseline["file"]).suffix != suffix
            ):
                raise RuntimeError(f"invalid baseline metadata: {baseline['id']}")
            path = assets / baseline["file"]
            if not path.is_file():
                raise RuntimeError(f"missing baseline: {path.name}")
            if path.stat().st_size != baseline["bytes"]:
                raise RuntimeError(f"size mismatch: {path.name}")
            if sha256(path) != baseline["sha256"]:
                raise RuntimeError(f"hash mismatch: {path.name}")
            if not has_expected_signature(path, format_name):
                raise RuntimeError(f"format mismatch: {path.name}")
        for format_id in EXPECTED_FORMATS:
            series = [
                baseline for baseline in baselines
                if baseline["id"].startswith(f"{format_id}-")
            ]
            if any(
                current["bytes"] >= following["bytes"]
                for current, following in zip(series, series[1:])
            ):
                raise RuntimeError(
                    f"non-increasing {format_id} series for {scene.get('id')}"
                )

    intermediates = list(assets.glob("*.i420"))
    if intermediates:
        raise RuntimeError(f"intermediate I420 files remain: {intermediates}")
    expected_assets = {"manifest.json"}
    for scene in scenes:
        expected_assets.update(variant["file"] for variant in scene["variants"])
        expected_assets.update(baseline["file"] for baseline in scene["baselines"])
    encoded_assets = {
        path.name for pattern in ("*.obu", "*.jpg", "*.webp", "*.avif")
        for path in assets.glob(pattern)
    }
    unexpected_assets = sorted(encoded_assets - expected_assets)
    if unexpected_assets:
        raise RuntimeError(f"unexpected encoded assets: {unexpected_assets}")
    print(
        f"Validated {len(scenes)} scenes, "
        f"{len(EXPECTED_QPS) * len(scenes)} OBUs, and "
        f"{len(EXPECTED_BASELINE_IDS) * len(scenes)} baselines"
    )


if __name__ == "__main__":
    main()
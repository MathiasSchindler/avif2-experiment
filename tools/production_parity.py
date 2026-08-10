"""Read the tracked production-scale 8-bit decoded-output baseline."""

from __future__ import annotations

from pathlib import Path


def load_production_baseline(
    path: Path, pixel_format: str
) -> dict[str, tuple[int, int, str]]:
    records: dict[str, tuple[int, int, str]] = {}
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("|")
        if len(fields) != 5:
            raise RuntimeError(f"{path}:{line_number}: malformed baseline record")
        record_format, name, width_text, height_text, digest = fields
        if record_format != pixel_format:
            continue
        if name in records:
            raise RuntimeError(f"{path}:{line_number}: duplicate {pixel_format}/{name}")
        if len(digest) != 64 or any(character not in "0123456789abcdef" for character in digest):
            raise RuntimeError(f"{path}:{line_number}: invalid SHA-256")
        width = int(width_text)
        height = int(height_text)
        if width <= 0 or height <= 0:
            raise RuntimeError(f"{path}:{line_number}: invalid dimensions")
        records[name] = (width, height, digest)
    if len(records) != 12:
        raise RuntimeError(
            f"{path}: expected 12 {pixel_format} records, found {len(records)}"
        )
    return records
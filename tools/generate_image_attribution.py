#!/usr/bin/env python3
"""Generate images/ATTRIBUTION.md from reviewed image source records."""

from __future__ import annotations

import json
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
IMAGES_ROOT = REPOSITORY_ROOT / "images"
SOURCES_ROOT = IMAGES_ROOT / "sources" / "commons"
OUTPUT_PATH = IMAGES_ROOT / "ATTRIBUTION.md"


def load_records() -> list[dict[str, object]]:
    records = []
    for source_path in sorted(SOURCES_ROOT.glob("*/source.json")):
        record = json.loads(source_path.read_text(encoding="utf-8"))
        if record["id"] != source_path.parent.name:
            raise ValueError(
                f"source ID {record['id']!r} does not match {source_path.parent.name!r}"
            )
        records.append(record)
    return records


def generate(records: list[dict[str, object]]) -> str:
    lines = [
        "# Image corpus attribution",
        "",
        "This file is generated from `sources/commons/*/source.json` by",
        "`tools/generate_image_attribution.py`. Do not edit individual entries by",
        "hand; update the reviewed source record and regenerate this file.",
        "",
        "The repository's CC0 license does not apply to these photographs or to",
        "derivatives of them. Each work remains under the license named below.",
        "Prepared images, raw planes, encoded files, previews, and decoded reference",
        "outputs must retain the applicable attribution, license, source link, and a",
        "description of changes. The downloaded originals listed here are unchanged.",
        "",
    ]

    for record in records:
        commons = record["commons"]
        license_record = record["license"]
        attribution = record["attribution"]
        original = record["original"]
        local_license = Path(license_record["local_file"]).name
        lines.extend(
            [
                f"## {record['title']}",
                "",
                f"- **Source ID:** `{record['id']}`",
                f"- **Credit:** {attribution['credit_line']}",
                f"- **Creator:** [{record['creator']['name']}]({record['creator']['url']})",
                f"- **Source:** [Wikimedia Commons]({commons['file_page_url']})",
                f"  ([reviewed revision {commons['reviewed_revision']}]({commons['reviewed_revision_url']}))",
                f"- **License:** [{license_record['name']}]({license_record['url']})",
                f"  ([local legal code](licenses/{local_license}))",
                f"- **Original SHA-256:** `{original['sha256']}`",
                "- **Changes to downloaded original:** none",
            ]
        )
        copyright_notice = attribution.get("copyright_notice")
        if copyright_notice:
            lines.append(f"- **Copyright notice:** {copyright_notice}")
        courtesy = record.get("courtesy")
        if courtesy:
            lines.extend(
                [
                    f"- **Courtesy notice:** [author notification]({courtesy['notice_url']})",
                    "  (not additional permission; this project will replace the image if",
                    "  the author objects)",
                ]
            )
        lines.append("")

    return "\n".join(lines)


def main() -> None:
    OUTPUT_PATH.write_text(generate(load_records()), encoding="utf-8")


if __name__ == "__main__":
    main()
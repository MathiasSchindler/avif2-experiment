#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


TABLES = (
    ("do_split", "default_do_split_cdf.h"),
    ("do_square_split", "default_do_square_split_cdf.h"),
    ("rect_type", "default_rect_type_cdf.h"),
    ("do_ext_partition", "default_do_ext_partition_cdf.h"),
    ("do_uneven_4way_partition",
     "default_do_uneven_4way_partition_cdf.h"),
)

ROW = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-dir", type=Path,
        default=Path("build/reference/av2-spec-v1.0.0"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    lines = [
        "/* Generated from hash-locked AV2 v1.0.0 normative attachments. */",
        "",
    ]
    for name, filename in TABLES:
        rows = [
            tuple(map(int, row))
            for row in ROW.findall((args.spec_dir / filename).read_text())
        ]
        lines.append(
            f"static const Av2hBinaryCdfSpec {name}_defaults[] = {{")
        lines.extend(
            f"    {{ {threshold}U, {parameter}U, {count}U }},"
            for threshold, parameter, count in rows
        )
        lines.extend(("};", ""))
    args.output.write_text("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
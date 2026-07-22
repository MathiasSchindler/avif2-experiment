#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


TABLES = (
    (
        "gdf_alpha.h", "gdf_alpha", "uint16_t", 6 * 6 * 22 * 4,
        0, 511,
        "8286d7fe2a4feadec5ef776123791a253cd5e70e658bafb8a40827148ecf3600",
    ),
    (
        "gdf_weight.h", "gdf_weight", "int16_t", 6 * 6 * 3 * 22 * 4,
        -512, 511,
        "58b154df018156ca88e8320009e0c4ebd024e7fbd6351b2215a3af637862f666",
    ),
    (
        "gdf_bias.h", "gdf_bias", "int16_t", 6 * 6 * 3,
        -32768, 32767,
        "9c9790f96d0a9067c3ee55c16fe96c1d6296ff4279747924b30e547cd18b2372",
    ),
    (
        "gdf_intra_error.h", "gdf_intra_error", "int8_t", 6 * 4096,
        -128, 127,
        "5c92199161ce38a981805f444cd2906917502231fd2a9b1f8c6377ea336f13e5",
    ),
    (
        "gdf_inter_error.h", "gdf_inter_error", "int8_t", 5 * 6 * 1000,
        -128, 127,
        "4ca252f213b3f230ef1eb40683f3066d304fc54f43b132133f691aabb9c5376f",
    ),
    (
        "side_thresholds.h", "deblock_side_threshold", "int16_t", 296,
        -32768, 32767,
        "9880d3ac26c5f31372ebeeaeb9302ac07bae51c514c89e58e37e0e8ba763bed0",
    ),
)


def read_values(path: Path, expected_hash: str) -> list[int]:
    source = path.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != expected_hash:
        raise ValueError(f"{path}: sha256 {digest}, expected {expected_hash}")
    text = source.decode("ascii")
    initializer = text.find("=")
    if initializer < 0:
        raise ValueError(f"{path}: initializer not found")
    return [int(value) for value in re.findall(r"-?\d+", text[initializer:])]


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
    for filename, name, c_type, count, minimum, maximum, digest in TABLES:
        values = read_values(args.spec_dir / filename, digest)
        if len(values) != count:
            raise ValueError(
                f"{filename}: expected {count} values, got {len(values)}")
        if min(values) < minimum or max(values) > maximum:
            raise ValueError(
                f"{filename}: range {min(values)}..{max(values)} does not "
                f"fit {c_type}")
        lines.append(f"static const {c_type} {name}[{count}] = {{")
        for start in range(0, count, 16):
            row = ", ".join(str(value) for value in values[start:start + 16])
            lines.append(f"    {row},")
        lines.extend(("};", ""))
    args.output.write_text("\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
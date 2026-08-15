#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


TABLES = (
    (
        "dip_weights.h",
        "av2h_intra_dip_weights",
        "int16_t",
        6 * 64 * 11,
        -32768,
        32767,
        "978a840baf471b1c6a8b59811692e4b6eafa8f31d4a18617ee955a3231a74eac",
        "intra_dip_data.inc",
    ),
    (
        "pc_wiener_filters.h",
        "av2h_pc_wiener_filters",
        "int16_t",
        4 * 64 * 13,
        -32768,
        32767,
        "d368e9d2831497833b59c4516ff6b11f6925240474e86b6c3e7d27f285393a63",
        "pc_wiener_data.inc",
    ),
    (
        "pc_wiener_lut_to_class.h",
        "av2h_pc_wiener_lut_to_class",
        "uint8_t",
        4096,
        0,
        255,
        "2b5a7364cb1abd9020a861b20183231de45eb49a00d0f106712ef9988d664a46",
        "pc_wiener_data.inc",
    ),
    (
        "pc_wiener_sub_classify.h",
        "av2h_pc_wiener_sub_classify",
        "uint8_t",
        4 * 256,
        0,
        63,
        "2641737152ba5e461fed148d4b66aa79e3fa56bf5cf5b4f1723b6eb063bd479e",
        "pc_wiener_data.inc",
    ),
    (
        "pc_wiener_sub_classify2.h",
        "av2h_pc_wiener_sub_classify2",
        "uint8_t",
        4 * 8 * 256,
        0,
        15,
        "ae1f16ce7fd10076976aff653f869e7611c15c75e00b9437ee17b3ce0aae3362",
        "pc_wiener_data.inc",
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


def render_table(c_type: str, name: str, values: list[int]) -> list[str]:
    lines = [f"static const {c_type} {name}[{len(values)}] = {{"]
    for start in range(0, len(values), 16):
        row = ", ".join(str(value) for value in values[start:start + 16])
        lines.append(f"    {row},")
    lines.extend(("};", ""))
    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-dir", type=Path,
        default=Path("docs/spec/av2-v1.0.0"))
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()

    outputs: dict[str, list[str]] = {}
    for filename, name, c_type, count, minimum, maximum, digest, output in TABLES:
        values = read_values(args.spec_dir / filename, digest)
        if len(values) != count:
            raise ValueError(
                f"{filename}: expected {count} values, got {len(values)}")
        if min(values) < minimum or max(values) > maximum:
            raise ValueError(
                f"{filename}: range {min(values)}..{max(values)} does not "
                f"fit {c_type}")
        outputs.setdefault(output, [
            "/* Generated from hash-locked AV2 v1.0.0 normative attachments. */",
            "",
        ]).extend(render_table(c_type, name, values))

    args.output_directory.mkdir(parents=True, exist_ok=True)
    for filename, lines in outputs.items():
        (args.output_directory / filename).write_text(
            "\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

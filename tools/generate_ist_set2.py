#!/usr/bin/env python3
"""Extract the AV2 IST set-2 tables from the pinned AVM source."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "vendor" / "avm" / "av2" / "common" / "secondary_tx.h"
OUTPUT = ROOT / "src" / "decoder" / "ist_set2_data.inc"
SET9_TYPE1_OUTPUT = (
    ROOT / "src" / "decoder" / "ist_set9_type1_data.inc"
)
SET5_TYPE1_OUTPUT = (
    ROOT / "src" / "decoder" / "ist_set5_type1_data.inc"
)
SET4_TYPE3_OUTPUT = (
    ROOT / "src" / "decoder" / "ist_set4_type3_data.inc"
)


def parse_initializer(source: str, declaration: str) -> list:
    start = source.index(declaration)
    start = source.index("{", start)
    tokens = re.findall(r"[{}]|-?\d+", re.sub(r"//.*", "", source[start:]))
    stack: list[list] = []
    root: list = []
    for token in tokens:
        if token == "{":
            value: list = []
            if stack:
                stack[-1].append(value)
            else:
                root = value
            stack.append(value)
        elif token == "}":
            stack.pop()
            if not stack:
                return root
        else:
            stack[-1].append(int(token))
    raise ValueError(f"unterminated initializer for {declaration}")


def format_array(name: str, rows: list[list[int]]) -> str:
    lines = [f"static const int16_t {name}[32U][48U] = {{"]
    for row in rows:
        lines.append("    { " + ", ".join(map(str, row)) + " },")
    lines.append("};")
    return "\n".join(lines)


def format_4x4_array(name: str, rows: list[list[int]]) -> str:
    lines = [f"static const int16_t {name}[8U][16U] = {{"]
    for row in rows:
        lines.append("    { " + ", ".join(map(str, row)) + " },")
    lines.append("};")
    return "\n".join(lines)


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    kernels_4x4 = parse_initializer(source, "ist_4x4_kernel")
    kernels = parse_initializer(source, "ist_8x8_kernel")
    mappings = parse_initializer(source, "coeff8x8_mapping")
    sections = ["/* Generated from pinned AVM v1.0.0 normative tables. */"]
    for type_index in (0, 2):
        type_number = type_index + 1
        kernel = kernels[2][type_index]
        scan_map = mappings[2 * 3 + type_index]
        if len(kernel) != 32 or any(len(row) != 48 for row in kernel):
            raise ValueError(
                f"unexpected set-2/type-{type_number} kernel dimensions"
            )
        if len(scan_map) != 48:
            raise ValueError(
                f"unexpected set-2/type-{type_number} scan-map dimensions"
            )
        sections.append(
            format_array(
                f"av2h_ist_8x8_set2_type{type_number}_kernel", kernel
            )
            + f"\n\nstatic const uint8_t av2h_ist_8x8_set2_type{type_number}_scan_map[48U] = {{\n"
            + "    " + ", ".join(f"{value}U" for value in scan_map) + "\n};"
        )
    content = "\n".join(sections) + "\n"
    OUTPUT.write_text(content, encoding="ascii", newline="\n")

    set9_type1_kernel = kernels[9][0]
    set9_type1_scan_map = mappings[9 * 3]
    if (len(set9_type1_kernel) != 32 or
            any(len(row) != 48 for row in set9_type1_kernel)):
        raise ValueError("unexpected set-9/type-1 kernel dimensions")
    if len(set9_type1_scan_map) != 48:
        raise ValueError("unexpected set-9/type-1 scan-map dimensions")
    set9_type1_content = (
        "/* Generated from pinned AVM v1.0.0 normative tables. */\n"
        + format_array(
            "av2h_ist_8x8_set9_type1_kernel", set9_type1_kernel
        )
        + "\n\nstatic const uint8_t "
        "av2h_ist_8x8_set9_type1_scan_map[48U] = {\n"
        + "    "
        + ", ".join(f"{value}U" for value in set9_type1_scan_map)
        + "\n};\n\n"
    )
    set10_type1_kernel = kernels_4x4[10][0]
    if (len(set10_type1_kernel) != 8 or
            any(len(row) != 16 for row in set10_type1_kernel)):
        raise ValueError("unexpected 4x4 set-10/type-1 kernel dimensions")
    set9_type1_content += format_4x4_array(
        "av2h_ist_4x4_set10_type1_kernel", set10_type1_kernel
    ) + "\n\n"
    set12_type3_kernel = kernels_4x4[12][2]
    if (len(set12_type3_kernel) != 8 or
            any(len(row) != 16 for row in set12_type3_kernel)):
        raise ValueError("unexpected 4x4 set-12/type-3 kernel dimensions")
    set9_type1_content += format_4x4_array(
        "av2h_ist_4x4_set12_type3_kernel", set12_type3_kernel
    ) + "\n"
    SET9_TYPE1_OUTPUT.write_text(
        set9_type1_content, encoding="ascii", newline="\n"
    )

    set5_type1_kernel = kernels_4x4[5][0]
    if (len(set5_type1_kernel) != 8 or
            any(len(row) != 16 for row in set5_type1_kernel)):
        raise ValueError("unexpected 4x4 set-5/type-1 kernel dimensions")
    SET5_TYPE1_OUTPUT.write_text(
        "/* Generated from pinned AVM v1.0.0 normative tables. */\n"
        + format_4x4_array(
            "av2h_ist_4x4_set5_type1_kernel", set5_type1_kernel
        )
        + "\n",
        encoding="ascii",
        newline="\n",
    )

    set4_type3_kernel = kernels_4x4[4][2]
    if (len(set4_type3_kernel) != 8 or
            any(len(row) != 16 for row in set4_type3_kernel)):
        raise ValueError("unexpected 4x4 set-4/type-3 kernel dimensions")
    SET4_TYPE3_OUTPUT.write_text(
        "/* Generated from pinned AVM v1.0.0 normative tables. */\n"
        + format_4x4_array(
            "av2h_ist_4x4_set4_type3_kernel", set4_type3_kernel
        )
        + "\n",
        encoding="ascii",
        newline="\n",
    )


if __name__ == "__main__":
    main()
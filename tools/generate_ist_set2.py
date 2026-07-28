#!/usr/bin/env python3
"""Extract the AV2 IST set-2/type-3 tables from the pinned AVM source."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "vendor" / "avm" / "av2" / "common" / "secondary_tx.h"
OUTPUT = ROOT / "src" / "decoder" / "ist_set2_data.inc"


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


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    kernels = parse_initializer(source, "ist_8x8_kernel")
    mappings = parse_initializer(source, "coeff8x8_mapping")
    kernel = kernels[2][2]
    scan_map = mappings[2 * 3 + 2]
    if len(kernel) != 32 or any(len(row) != 48 for row in kernel):
        raise ValueError("unexpected set-2/type-3 kernel dimensions")
    if len(scan_map) != 48:
        raise ValueError("unexpected set-2/type-3 scan-map dimensions")
    content = (
        "/* Generated from pinned AVM v1.0.0 normative tables. */\n"
        + format_array("av2h_ist_8x8_set2_type3_kernel", kernel)
        + "\n\nstatic const uint8_t av2h_ist_8x8_set2_type3_scan_map[48U] = {\n"
        + "    " + ", ".join(f"{value}U" for value in scan_map) + "\n};\n"
    )
    OUTPUT.write_text(content, encoding="ascii", newline="\n")


if __name__ == "__main__":
    main()
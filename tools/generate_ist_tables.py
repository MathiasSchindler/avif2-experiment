#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


IST_SET_COUNT = 11
IST_TYPE_COUNT = 3
IST_HEIGHT = 32
IST_WIDTH = 48
IST_4X4_SET_COUNT = 14
IST_4X4_HEIGHT = 8
IST_4X4_WIDTH = 16
SELECTED_KERNELS = ((0, 3), (1, 1), (1, 2), (1, 3), (3, 1), (3, 3), (4, 1), (4, 2), (4, 3), (5, 1), (5, 2), (5, 3), (6, 1), (6, 2))
SELECTED_SCAN_MAPS = ((0, 1), (0, 2), (1, 1), (1, 2), (1, 3), (3, 1), (3, 3), (4, 1), (4, 3), (5, 1), (5, 2), (5, 3), (6, 1), (6, 2), (6, 3))
REACHED_4X4_KERNELS = (
    (0, 2), (1, 1), (2, 2), (6, 2), (7, 2), (7, 3), (13, 2),
    (13, 3)
)
REACHED_8X8_KERNELS = (
    (2, 2), (7, 1), (7, 3), (8, 2), (8, 3), (9, 2), (9, 3),
    (10, 1), (10, 2)
)
KERNEL_HASH = "65d17c0e9a34843da3f5fe48262c7170383b59c15e477ab55733796de871fcc4"
KERNEL_4X4_HASH = "9fafc6bc5086c992dabc1b26816713f9a56e5ccdba386130034165d6df985404"
SCAN_MAP_HASH = "8825c77db8b394c9274df8cb368c14271d12eed218eb228aae133956c4ec7146"


def read_initializer(path: Path, expected_hash: str, expected_count: int) -> list[int]:
    source = path.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != expected_hash:
        raise ValueError(f"{path}: sha256 {digest}, expected {expected_hash}")
    text = source.decode("ascii")
    initializer = text.find("=")
    if initializer < 0:
        raise ValueError(f"{path}: initializer not found")
    text = re.sub(r"/\*.*?\*/", "", text[initializer + 1:], flags=re.S)
    text = re.sub(r"//.*", "", text)
    values = [int(value) for value in re.findall(r"-?\d+", text)]
    if len(values) != expected_count:
        raise ValueError(
            f"{path}: expected {expected_count} values, got {len(values)}")
    return values


def selected_slice(values: list[int], selected_set: int,
                   selected_type: int, stride: int) -> list[int]:
    offset = ((selected_set * IST_TYPE_COUNT) + selected_type - 1) * stride
    return values[offset:offset + stride]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-dir", type=Path,
        default=Path("docs/spec/av2-v1.0.0/attachments"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reached-output", type=Path)
    args = parser.parse_args()

    kernel = read_initializer(
        args.spec_dir / "ist_8x8_kernel.h", KERNEL_HASH,
        IST_SET_COUNT * IST_TYPE_COUNT * IST_HEIGHT * IST_WIDTH)
    scan_map = read_initializer(
        args.spec_dir / "stx_scan_map.h", SCAN_MAP_HASH,
        IST_SET_COUNT * IST_TYPE_COUNT * IST_WIDTH)
    kernels = {
        (selected_set, selected_type): selected_slice(
            kernel, selected_set, selected_type, IST_HEIGHT * IST_WIDTH)
        for selected_set, selected_type in SELECTED_KERNELS
    }
    scan_maps = {
        (selected_set, selected_type): selected_slice(
            scan_map, selected_set, selected_type, IST_WIDTH)
        for selected_set, selected_type in SELECTED_SCAN_MAPS
    }
    if any(min(values) < -32768 or max(values) > 32767
           for values in kernels.values()):
        raise ValueError("selected IST kernel does not fit int16_t")
    if any(min(values) < 0 or max(values) >= 64 or
           len(set(values)) != IST_WIDTH for values in scan_maps.values()):
        raise ValueError("selected IST scan map is invalid")

    lines = [
        "/* Generated from hash-locked AV2 v1.0.0 normative attachments. */",
    ]
    for (selected_set, selected_type), kernel_values in kernels.items():
        lines.append(
            f"static const int16_t av2h_ist_8x8_set{selected_set}_type{selected_type}_kernel[32U][48U] = {{")
        for row in range(IST_HEIGHT):
            start = row * IST_WIDTH
            values = ", ".join(
                str(value) for value in
                kernel_values[start:start + IST_WIDTH])
            lines.append(f"    {{ {values} }},")
        lines.extend(("};", ""))
    for (selected_set, selected_type), scan_values in scan_maps.items():
        lines.append(
            f"static const uint8_t av2h_ist_8x8_set{selected_set}_type{selected_type}_scan_map[48U] = {{")
        for start in range(0, IST_WIDTH, 16):
            values = ", ".join(
                str(value) for value in scan_values[start:start + 16])
            lines.append(f"    {values},")
        lines.extend(("};", ""))
    args.output.write_text("\n".join(lines), encoding="ascii")
    if args.reached_output is not None:
        kernel_4x4 = read_initializer(
            args.spec_dir / "ist_4x4_kernel.h", KERNEL_4X4_HASH,
            IST_4X4_SET_COUNT * IST_TYPE_COUNT *
            IST_4X4_HEIGHT * IST_4X4_WIDTH)
        reached_lines = [
            "/* Generated from hash-locked AV2 v1.0.0 normative attachments. */",
        ]
        for selected_set, selected_type in REACHED_4X4_KERNELS:
            values = selected_slice(
                kernel_4x4, selected_set, selected_type,
                IST_4X4_HEIGHT * IST_4X4_WIDTH)
            reached_lines.append(
                f"static const int16_t av2h_ist_4x4_set{selected_set}_type{selected_type}_kernel[8U][16U] = {{")
            for row in range(IST_4X4_HEIGHT):
                start = row * IST_4X4_WIDTH
                row_values = ", ".join(
                    str(value) for value in
                    values[start:start + IST_4X4_WIDTH])
                reached_lines.append(f"    {{ {row_values} }},")
            reached_lines.extend(("};", ""))
        for selected_set, selected_type in REACHED_8X8_KERNELS:
            values = selected_slice(
                kernel, selected_set, selected_type,
                IST_HEIGHT * IST_WIDTH)
            reached_lines.append(
                f"static const int16_t av2h_ist_8x8_set{selected_set}_type{selected_type}_kernel[32U][48U] = {{")
            for row in range(IST_HEIGHT):
                start = row * IST_WIDTH
                row_values = ", ".join(
                    str(value) for value in values[start:start + IST_WIDTH])
                reached_lines.append(f"    {{ {row_values} }},")
            reached_lines.extend(("};", ""))
            scan_values = selected_slice(
                scan_map, selected_set, selected_type, IST_WIDTH)
            reached_lines.append(
                f"static const uint8_t av2h_ist_8x8_set{selected_set}_type{selected_type}_scan_map[48U] = {{")
            for start in range(0, IST_WIDTH, 16):
                row_values = ", ".join(
                    str(value) for value in scan_values[start:start + 16])
                reached_lines.append(f"    {row_values},")
            reached_lines.extend(("};", ""))
        args.reached_output.write_text(
            "\n".join(reached_lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
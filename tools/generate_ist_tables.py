#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


IST_SET_COUNT = 11
IST_TYPE_COUNT = 3
IST_HEIGHT = 32
IST_WIDTH = 48
SELECTED_KERNELS = ((0, 3), (1, 1), (1, 2), (1, 3), (4, 3), (5, 1), (5, 3), (6, 1))
SELECTED_SCAN_MAPS = ((0, 1), (0, 2), (1, 1), (1, 2), (1, 3), (4, 3), (5, 1), (5, 3), (6, 1), (6, 3))
KERNEL_HASH = "65d17c0e9a34843da3f5fe48262c7170383b59c15e477ab55733796de871fcc4"
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
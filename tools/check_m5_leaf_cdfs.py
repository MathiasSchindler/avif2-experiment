#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_HASHES = {
    "default_ccso_blk_cdf.h":
        "0314f72d107795b21429460ec07519ac1c2adfb77ccc65a8fd5dacb0576fd66f",
    "default_intrabc_cdf.h":
        "4fdf29f330db6ab9982d8e30340a7527e99dd819ccc349d11db4133678c4d8e0",
    "default_fsc_mode_cdf.h":
        "64e8bc4636afbcaf712c1ff220be0dff2ad0d67909669a6a8f4e396a63670fb0",
    "default_y_mode_index_cdf.h":
        "4490c02782275216bda5d75367a13a2b8aa434e891d2558c593e091a5a50f294",
    "default_y_mode_offset_cdf.h":
        "53279fa370568aa6d098755566252bca56e4f4d26fa25fe76e3c654a737db59c",
    "default_y_mode_set_cdf.h":
        "90aef3ee9186b3244092611a287ffd2f690acf8449bb685ed8a816c79703fcd3",
    "default_mrl_index_cdf.h":
        "07763f90a8d3bea6d382ac40318a3ffd7f906b2e46e261277c04b2cc51659aca",
    "default_mrl_sec_index_cdf.h":
        "42edaf55429854a15be6ca82d660e39da9192afe300590702d0b1b04d7043d5b",
    "default_is_cfl_cdf.h":
        "ad9a994a9e00791516eac75cbf18df0d49c9c2437cc6fdecb8d58d3048ab8ea7",
    "default_uv_mode_cfl_not_allowed_cdf.h":
        "4cce070ef9b76498769313a9caa2e5e04f516331b3a60e689d576f2f61b68927",
    "para_adjustment_list.h":
        "0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80",
}

ADJUSTMENT_ROW = re.compile(
    r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}")


def verify_hash(path: Path) -> None:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    expected = EXPECTED_HASHES[path.name]
    if digest != expected:
        raise ValueError(f"{path}: sha256 {digest}, expected {expected}")


def integers_after_equals(path: Path) -> list[int]:
    source = path.read_text()
    return [int(value) for value in re.findall(r"\d+", source.split("=", 1)[1])]


def avm_table(source: str, name: str) -> str:
    start = source.find(name)
    if start < 0:
        raise ValueError(f"AVM table not found: {name}")
    end = source.find(";\n", start)
    if end < 0:
        raise ValueError(f"AVM table terminator not found: {name}")
    return source[start:end]


def parse_avm_rows(source: str, name: str, symbols: int) -> list[tuple]:
    table = avm_table(source, name)
    pattern = re.compile(
        rf"AVM_CDF{symbols}\(([^)]*)\)\s*,\s*"
        rf"AVM_PARA{symbols}\(([^)]*)\)")
    rows = []
    for thresholds, parameters in pattern.findall(table):
        rows.append((
            tuple(int(value) for value in re.findall(r"\d+", thresholds)),
            tuple(int(value) for value in re.findall(r"-?\d+", parameters)),
        ))
    return rows


def check_rows(name: str, specification: list[tuple], avm: list[tuple],
               adjustments: list[tuple]) -> None:
    if len(specification) != len(avm):
        raise ValueError(
            f"{name}: spec has {len(specification)} rows, AVM has {len(avm)}")
    for index, (spec_row, avm_row) in enumerate(zip(specification, avm)):
        thresholds, parameter_index, count = spec_row
        avm_thresholds, avm_parameters = avm_row
        if count != 0 or thresholds != avm_thresholds or \
                adjustments[parameter_index] != avm_parameters:
            raise ValueError(
                f"{name}[{index}] differs: spec={spec_row}, AVM={avm_row}")
    print(f"{name}: {len(specification)} rows match")


def binary_rows(values: list[int]) -> list[tuple]:
    if len(values) % 3:
        raise ValueError("binary attachment does not contain three-word rows")
    return [((values[index],), values[index + 1], values[index + 2])
            for index in range(0, len(values), 3)]


def multi_rows(values: list[int], symbols: int) -> list[tuple]:
    row_size = symbols + 1
    if len(values) % row_size:
        raise ValueError(f"{symbols}-symbol attachment has a partial row")
    rows = []
    for index in range(0, len(values), row_size):
        row = values[index:index + row_size]
        rows.append((tuple(row[:symbols - 1]), row[-2], row[-1]))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-dir", type=Path,
        default=Path("build/reference/av2-spec-v1.0.0"))
    parser.add_argument(
        "--avm-defaults", type=Path,
        default=Path("vendor/avm/av2/common/entropy_inits_modes.h"))
    args = parser.parse_args()
    for filename in EXPECTED_HASHES:
        verify_hash(args.spec_dir / filename)
    adjustments = [
        tuple(map(int, row))
        for row in ADJUSTMENT_ROW.findall(
            (args.spec_dir / "para_adjustment_list.h").read_text())
    ]
    if len(adjustments) != 125:
        raise ValueError(f"expected 125 adjustment rows, got {len(adjustments)}")
    avm_source = args.avm_defaults.read_text()
    checks = (
        ("intrabc", "default_intrabc_cdf.h", "default_intrabc_cdf", 2,
         binary_rows),
        ("ccso", "default_ccso_blk_cdf.h", "default_ccso_cdf", 2,
         binary_rows),
        ("fsc", "default_fsc_mode_cdf.h", "default_fsc_mode_cdf", 2,
         binary_rows),
        ("y_mode_set", "default_y_mode_set_cdf.h",
         "default_y_mode_set_cdf", 4, multi_rows),
        ("y_mode_index", "default_y_mode_index_cdf.h",
         "default_y_mode_idx_cdf", 8, multi_rows),
        ("y_mode_offset", "default_y_mode_offset_cdf.h",
         "default_y_mode_idx_offset_cdf", 6, multi_rows),
        ("mrl_index", "default_mrl_index_cdf.h",
         "default_mrl_index_cdf", 4, multi_rows),
        ("mrl_secondary", "default_mrl_sec_index_cdf.h",
         "default_multi_line_mrl_cdf", 2, binary_rows),
        ("is_cfl", "default_is_cfl_cdf.h",
         "default_cfl_cdf", 2, binary_rows),
        ("uv_mode", "default_uv_mode_cfl_not_allowed_cdf.h",
         "default_uv_mode_cdf", 8, multi_rows),
    )
    total = 0
    for name, filename, avm_name, symbols, parser_function in checks:
        values = integers_after_equals(args.spec_dir / filename)
        specification = (parser_function(values) if symbols == 2 else
                         parser_function(values, symbols))
        avm = parse_avm_rows(avm_source, avm_name, symbols)
        check_rows(name, specification, avm, adjustments)
        total += len(specification)
    print(f"first-leaf CDF evidence: {total} rows match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
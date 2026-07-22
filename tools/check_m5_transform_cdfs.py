#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_HASHES = {
    "default_intra_tx_type_set1_cdf.h":
        "81cfe731ba9cedd0b19edab3f0dbdfe19457bd37b93921bf67344db8edaca6a8",
    "default_sec_tx_type_cdf.h":
        "02cbe3ded9ee46ef807d11e96098191e2c66831a511a1f028ad9e7a5d996173b",
    "default_cctx_type_cdf.h":
        "b42332c842ecbceae351958a60014be890cd3ad8f3b82520c042023db3da42e7",
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


def compact_rows(path: Path, symbols: int) -> list[tuple]:
    values = [
        int(value)
        for value in re.findall(r"\d+", path.read_text().split("=", 1)[1])
    ]
    row_size = symbols + 1
    if len(values) % row_size:
        raise ValueError(f"{path}: partial {symbols}-symbol row")
    rows = []
    for index in range(0, len(values), row_size):
        row = values[index:index + row_size]
        rows.append((tuple(row[:symbols - 1]), row[-2], row[-1]))
    return rows


def avm_rows(source: str, name: str, symbols: int) -> list[tuple]:
    start = source.find(name)
    if start < 0:
        raise ValueError(f"AVM table not found: {name}")
    end = source.find(";\n", start)
    if end < 0:
        raise ValueError(f"AVM table terminator not found: {name}")
    pattern = re.compile(
        rf"AVM_CDF{symbols}\(([^)]*)\)\s*,\s*"
        rf"AVM_PARA{symbols}\(([^)]*)\)")
    return [
        (tuple(int(value) for value in re.findall(r"\d+", thresholds)),
         tuple(int(value) for value in re.findall(r"-?\d+", parameters)))
        for thresholds, parameters in pattern.findall(source[start:end])
    ]


def check_table(name: str, specification: list[tuple], avm: list[tuple],
                adjustments: list[tuple]) -> None:
    if len(avm) < len(specification):
        raise ValueError(
            f"{name}: spec has {len(specification)} rows, AVM has {len(avm)}")
    for index, (spec_row, avm_row) in enumerate(
            zip(specification, avm[:len(specification)])):
        thresholds, parameter_index, count = spec_row
        avm_thresholds, parameters = avm_row
        if count != 0 or thresholds != avm_thresholds or \
                adjustments[parameter_index] != parameters:
            raise ValueError(
                f"{name}[{index}] differs: spec={spec_row}, AVM={avm_row}")
    print(f"{name}: {len(specification)} rows match")


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
    source = args.avm_defaults.read_text()
    tables = (
        ("primary_intra_set1", "default_intra_tx_type_set1_cdf.h",
         "default_intra_ext_tx_cdf", 7),
        ("secondary_type", "default_sec_tx_type_cdf.h",
         "default_stx_cdf", 4),
        ("chroma_cross_type", "default_cctx_type_cdf.h",
         "default_cctx_type_cdf", 7),
    )
    total = 0
    for name, filename, avm_name, symbols in tables:
        specification = compact_rows(args.spec_dir / filename, symbols)
        implementation = avm_rows(source, avm_name, symbols)
        check_table(name, specification, implementation, adjustments)
        total += len(specification)
    print(f"transform CDF evidence: {total} rows match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
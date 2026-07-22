#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_HASHES = {
    "default_txb_skip_cdf.h":
        "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
    "default_eob_pt_32_cdf.h":
        "5337967e95ac20b02bab8916ae33d2799db877dd5b3f237a31971b75c9f9206a",
    "default_eob_pt_64_cdf.h":
        "cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d",
    "default_eob_pt_1024_cdf.h":
        "6e5f1b9801ce22911aeecc9b21cea783d8ee23c2610a31b914772e7fc4dc0ff2",
    "default_eob_extra_cdf.h":
        "0c0e719e415956b36d25cf326f7914aac890477791bb6d61edf987786914dba1",
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
    if len(specification) != len(avm):
        raise ValueError(
            f"{name}: spec has {len(specification)} rows, AVM has {len(avm)}")
    for index, (spec_row, avm_row) in enumerate(zip(specification, avm)):
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
        default=Path("vendor/avm/av2/common/entropy_inits_coeffs.h"))
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
        ("txb_skip", "default_txb_skip_cdf.h", "default_txb_skip_cdf", 2),
        ("eob_pt_32", "default_eob_pt_32_cdf.h",
         "av2_default_eob_multi32_cdfs", 6),
        ("eob_pt_64", "default_eob_pt_64_cdf.h",
         "av2_default_eob_multi64_cdfs", 7),
        ("eob_pt_1024", "default_eob_pt_1024_cdf.h",
         "av2_default_eob_multi1024_cdfs", 8),
        ("eob_extra", "default_eob_extra_cdf.h",
         "av2_default_eob_extra_cdfs", 2),
    )
    total = 0
    for name, filename, avm_name, symbols in tables:
        specification = compact_rows(args.spec_dir / filename, symbols)
        implementation = avm_rows(source, avm_name, symbols)
        check_table(name, specification, implementation, adjustments)
        total += len(specification)
    print(f"coefficient CDF evidence: {total} rows match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
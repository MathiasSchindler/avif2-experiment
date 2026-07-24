#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_HASHES = {
    "default_do_ext_partition_cdf.h":
        "34e128f47a3331f44e41eec3faaeac3f68dc66a02d94d69707623e07451b7372",
    "default_do_split_cdf.h":
        "bb40c072c47561c4742cb96cd6e91cb1ea18ba153862e3e6bd1f88ef19c0b692",
    "default_do_square_split_cdf.h":
        "73cf5e0851fc98be518dc6d47a31657d2c503f34512e1428403e6ccf19eb4ace",
    "default_do_uneven_4way_partition_cdf.h":
        "a7644325eb682179358f27dc1d788d1c996efec6e23f00e9f512de312ff2d3b3",
    "default_rect_type_cdf.h":
        "f5fab585693d795883ff86519a9fa544a02dec1b768e88e7afe8e4b26a3f3c8a",
    "para_adjustment_list.h":
        "0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80",
}

TABLES = (
    "default_do_split_cdf",
    "default_do_square_split_cdf",
    "default_rect_type_cdf",
    "default_do_ext_partition_cdf",
    "default_do_uneven_4way_partition_cdf",
)

SPEC_ROW = re.compile(r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}")
ADJUSTMENT_ROW = re.compile(
    r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}")
AVM_ROW = re.compile(
    r"AVM_CDF2\(\s*(\d+)\s*\)\s*,\s*"
    r"AVM_PARA2\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)"
)


def verify_hash(path: Path) -> None:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    expected = EXPECTED_HASHES[path.name]
    if digest != expected:
        raise ValueError(f"{path}: sha256 {digest}, expected {expected}")


def avm_table(source: str, name: str) -> str:
    start = source.find(name)
    if start < 0:
        raise ValueError(f"AVM table not found: {name}")
    end = source.find(";\n\nstatic const", start)
    if end < 0:
        raise ValueError(f"AVM table terminator not found: {name}")
    return source[start:end]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-dir", type=Path,
        default=Path("docs/spec/av2-v1.0.0"))
    parser.add_argument(
        "--avm-defaults", type=Path,
        default=Path("vendor/avm/av2/common/entropy_inits_modes.h"))
    args = parser.parse_args()

    for filename in EXPECTED_HASHES:
        verify_hash(args.spec_dir / filename)

    adjustment_rows = [
        tuple(map(int, row))
        for row in ADJUSTMENT_ROW.findall(
            (args.spec_dir / "para_adjustment_list.h").read_text())
    ]
    if len(adjustment_rows) != 125:
        raise ValueError(
            f"expected 125 adjustment rows, got {len(adjustment_rows)}")

    avm_source = args.avm_defaults.read_text()
    total_rows = 0
    for name in TABLES:
        spec_rows = [
            tuple(map(int, row))
            for row in SPEC_ROW.findall(
                (args.spec_dir / f"{name}.h").read_text())
        ]
        avm_rows = [
            tuple(map(int, row))
            for row in AVM_ROW.findall(avm_table(avm_source, name))
        ]
        if len(spec_rows) != len(avm_rows):
            raise ValueError(
                f"{name}: spec has {len(spec_rows)} rows, "
                f"AVM has {len(avm_rows)}")
        for index, (specification, avm) in enumerate(
                zip(spec_rows, avm_rows)):
            threshold, parameter_index, count = specification
            avm_threshold, first, second, third = avm
            if count != 0 or threshold != avm_threshold or \
                    adjustment_rows[parameter_index] != \
                    (first, second, third):
                raise ValueError(
                    f"{name}[{index}] differs: spec={specification}, "
                    f"AVM={avm}")
        total_rows += len(spec_rows)
        print(f"{name}: {len(spec_rows)} rows match")
    print(f"partition CDF evidence: {total_rows} rows match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
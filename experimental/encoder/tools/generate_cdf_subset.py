#!/usr/bin/env python3
"""Generate the AV2 v1.0.0 CDF subset used by grayscale DC encoding."""

from __future__ import annotations

import argparse
import hashlib
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Table:
    name: str
    filename: str
    sha256: str
    symbols: int
    rows: int


TABLES = (
    Table("do_split", "default_do_split_cdf.h",
          "bb40c072c47561c4742cb96cd6e91cb1ea18ba153862e3e6bd1f88ef19c0b692",
          2, 128),
    Table("rect_type", "default_rect_type_cdf.h",
          "f5fab585693d795883ff86519a9fa544a02dec1b768e88e7afe8e4b26a3f3c8a",
          2, 128),
    Table("do_ext_partition", "default_do_ext_partition_cdf.h",
          "34e128f47a3331f44e41eec3faaeac3f68dc66a02d94d69707623e07451b7372",
          2, 128),
    Table("delta_q", "default_delta_q_cdf.h",
          "cfdf6d6220bc37e8b29839948c4762502c408777fb73caaf1a1c85761bf74aa2",
          8, 1),
    Table("y_mode_set", "default_y_mode_set_cdf.h",
          "90aef3ee9186b3244092611a287ffd2f690acf8449bb685ed8a816c79703fcd3",
          4, 1),
    Table("y_mode_index", "default_y_mode_index_cdf.h",
          "4490c02782275216bda5d75367a13a2b8aa434e891d2558c593e091a5a50f294",
          8, 3),
    Table("mrl_index", "default_mrl_index_cdf.h",
          "07763f90a8d3bea6d382ac40318a3ffd7f906b2e46e261277c04b2cc51659aca",
          4, 3),
    Table("fsc_mode", "default_fsc_mode_cdf.h",
          "64e8bc4636afbcaf712c1ff220be0dff2ad0d67909669a6a8f4e396a63670fb0",
          2, 24),
    Table("tx_do_partition", "default_tx_do_partition_cdf.h",
          "64e71f317f42e265436a00b75a1ef83091fdba4869fd54c5c46d73547fe45ad1",
          2, 36),
    Table("tx_partition_type", "default_tx_partition_type_cdf.h",
          "a34d25e1c82a2e791e542e6d13c6a17a126dedadc1a60d35dddb0272c527449d",
          7, 56),
    Table("palette_y_mode", "default_palette_y_mode_cdf.h",
          "a6bd3157e7092a7acb4219510360b809056cd9f4c7f7477601f17df0a12e0a40",
          2, 1),
    Table("txb_skip", "default_txb_skip_cdf.h",
          "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
          2, 400),
    Table("eob_pt_64", "default_eob_pt_64_cdf.h",
          "cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d",
          7, 12),
    Table("eob_extra", "default_eob_extra_cdf.h",
          "0c0e719e415956b36d25cf326f7914aac890477791bb6d61edf987786914dba1",
          2, 4),
    Table("coeff_base_eob", "default_coeff_base_eob_cdf.h",
          "77e0545f4a068d7cc194be884aff763af5a51af73fbb6dbbff97588dfd31f0a2",
          3, 80),
    Table("coeff_base_lf_eob", "default_coeff_base_lf_eob_cdf.h",
          "36dcaa7a6e355098c737a3187ec0038c54e0c609e080912cb620c3eda88d0e26",
          5, 80),
    Table("coeff_base_lf", "default_coeff_base_lf_cdf.h",
          "45f66ff809ad075fc1b17f2e8366c254290e6e505deef9c69904a338802a2f55",
          6, 1320),
    Table("coeff_base", "default_coeff_base_cdf.h",
          "9f415dbf859f39c062c49c836eba6a1e9646c1f654ed998574761de942c42819",
          4, 800),
    Table("coeff_br_lf", "default_coeff_br_lf_cdf.h",
          "fee755c055bd50681616f80618fb39ae47468f99a284259aac118b938d56943a",
          4, 56),
    Table("coeff_br", "default_coeff_br_cdf.h",
          "e3f0ebd02b0f01086a7ca2129ac861078440450442ea53f1309a31a97993c7a0",
          4, 28),
    Table("dc_sign", "default_dc_sign_cdf.h",
          "17d4545c3d60dbab3e10a2ca3a26d852c224a1ccd8170ce3daf089a2eb6161fa",
          2, 48),
    Table("intra_tx_type_set1", "default_intra_tx_type_set1_cdf.h",
          "81cfe731ba9cedd0b19edab3f0dbdfe19457bd37b93921bf67344db8edaca6a8",
          7, 3),
    Table("intra_tx_type_set2", "default_intra_tx_type_set2_cdf.h",
          "59e1e5d33ce1d2f613de0317617794b40586a52204486fae126faa3fa449a832",
          2, 3),
    Table("intra_tx_type_long", "default_intra_tx_type_long_cdf.h",
          "37ca65c41d95f1d27fb3d8530286e4c6f8b88d72ef20618e40680a4a54e7fed8",
          4, 4),
)

PARA_ADJUSTMENT_HASH = \
    "0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80"


def verify_parameter_adjustments(spec_dir: Path) -> None:
    path = spec_dir / "para_adjustment_list.h"
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != PARA_ADJUSTMENT_HASH:
        raise ValueError(
            f"{path.name}: sha256 {digest}, expected {PARA_ADJUSTMENT_HASH}")
    source = path.read_text()
    rows = [tuple(map(int, row)) for row in re.findall(
        r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}", source)]
    digits = (0, -1, -2, 1, 1)
    expected = [(digits[index // 25], digits[(index // 5) % 5],
                 digits[index % 5]) for index in range(125)]
    if rows != expected:
        raise ValueError(f"{path.name}: expected 125 base-5 adjustment rows")


def parse_table(spec_dir: Path, table: Table) -> list[list[int]]:
    path = spec_dir / table.filename
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != table.sha256:
        raise ValueError(
            f"{table.filename}: sha256 {digest}, expected {table.sha256}")
    source = data.decode("ascii")
    if "=" not in source:
        raise ValueError(f"{table.filename}: missing initializer")
    values = [int(value) for value in
              re.findall(r"\d+", source.split("=", 1)[1])]
    width = table.symbols + 1
    if len(values) != table.rows * width:
        raise ValueError(
            f"{table.filename}: got {len(values)} values, expected "
            f"{table.rows * width}")
    rows = [values[index:index + width]
            for index in range(0, len(values), width)]
    for index, row in enumerate(rows):
        thresholds = row[:table.symbols - 1]
        parameter, count = row[-2:]
        if any(value <= 0 or value >= 32768 for value in thresholds):
            raise ValueError(f"{table.filename}[{index}]: threshold range")
        if thresholds != sorted(set(thresholds)):
            raise ValueError(f"{table.filename}[{index}]: threshold order")
        if not 0 <= parameter < 125 or not 0 <= count <= 32:
            raise ValueError(f"{table.filename}[{index}]: metadata range")
    return rows


def render(spec_dir: Path) -> str:
    verify_parameter_adjustments(spec_dir)
    lines = [
        "/* Generated by tools/generate_cdf_subset.py; do not edit. */",
        "/* Source: hash-locked AV2 v1.0.0 normative attachments. */",
        f"/* para_adjustment_list.h sha256: {PARA_ADJUSTMENT_HASH} */",
        "",
    ]
    for table in TABLES:
        rows = parse_table(spec_dir, table)
        lines.append(f"/* {table.filename} sha256: {table.sha256} */")
        lines.append(
            f"static const uint16_t enc_norm_{table.name}"
            f"[{table.rows}U][{table.symbols + 1}U] = {{")
        for row in rows:
            lines.append("    {" + ", ".join(f"{value}U" for value in row) +
                         "},")
        lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    encoder = Path(__file__).resolve().parents[1]
    default_spec = encoder.parents[1] / "docs/spec/av2-v1.0.0/attachments"
    parser = argparse.ArgumentParser()
    parser.add_argument("--spec-dir", type=Path, default=default_spec)
    parser.add_argument(
        "--output", type=Path,
        default=encoder / "src/normative_cdfs.inc")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = render(args.spec_dir)
    if args.check:
        if not args.output.is_file() or args.output.read_text() != generated:
            raise SystemExit(f"{args.output}: generated data is stale")
    else:
        args.output.write_text(generated)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

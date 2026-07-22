#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


EXPECTED_HASHES = {
    "default_txb_skip_cdf.h":
        "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
    "default_eob_pt_16_cdf.h":
        "a06828f9166ee168f5e40fbe5025da2bc169d489648cbdc8ffe54277e853f883",
    "default_eob_pt_32_cdf.h":
        "5337967e95ac20b02bab8916ae33d2799db877dd5b3f237a31971b75c9f9206a",
    "default_eob_pt_64_cdf.h":
        "cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d",
    "default_eob_pt_1024_cdf.h":
        "6e5f1b9801ce22911aeecc9b21cea783d8ee23c2610a31b914772e7fc4dc0ff2",
    "default_eob_extra_cdf.h":
        "0c0e719e415956b36d25cf326f7914aac890477791bb6d61edf987786914dba1",
    "default_coeff_base_eob_cdf.h":
        "77e0545f4a068d7cc194be884aff763af5a51af73fbb6dbbff97588dfd31f0a2",
    "default_coeff_base_lf_eob_cdf.h":
        "36dcaa7a6e355098c737a3187ec0038c54e0c609e080912cb620c3eda88d0e26",
    "default_coeff_base_cdf.h":
        "9f415dbf859f39c062c49c836eba6a1e9646c1f654ed998574761de942c42819",
    "default_coeff_base_lf_cdf.h":
        "45f66ff809ad075fc1b17f2e8366c254290e6e505deef9c69904a338802a2f55",
    "default_coeff_br_cdf.h":
        "e3f0ebd02b0f01086a7ca2129ac861078440450442ea53f1309a31a97993c7a0",
    "default_coeff_br_lf_cdf.h":
        "fee755c055bd50681616f80618fb39ae47468f99a284259aac118b938d56943a",
    "default_dc_sign_cdf.h":
        "17d4545c3d60dbab3e10a2ca3a26d852c224a1ccd8170ce3daf089a2eb6161fa",
    "default_coeff_base_bob_cdf.h":
        "d5dcce4c47da97d2eb4515c45608a2e3db617381eff472cdd7eee1e8a8e39678",
    "default_coeff_base_idtx_cdf.h":
        "1c2d642d0d072ef50dc38c61ee54f10dbf77318cc60f1d20b92862f65ef1d411",
    "default_coeff_br_idtx_cdf.h":
        "d777566c10f275dab2de0e006f107d444cf643add576f2ed4f2b8c7114839a18",
    "default_idtx_sign_cdf.h":
        "1a0a872bab3a04314c5b037949639787cff3a5de3bef98c0b62e07b8a15568c4",
    "default_v_txb_skip_cdf.h":
        "f368e4a2c9311eea7012c0da0a511091954a583b4522dea670097a75bed12dc6",
    "default_coeff_base_eob_uv_cdf.h":
        "ad62dae7998609ec5aa6984fb460745994c8b8b5f222a6491dc9f9ac68d65e44",
    "default_coeff_base_lf_eob_uv_cdf.h":
        "2b6cbd982adbdf70dda1141ad182e39abfebfe12df95fcabff4abca6e3c963eb",
    "default_coeff_base_uv_cdf.h":
        "7f493110886aa59ad6bacff5d62b20997d7651c96fa3604732aa81e55c57c82a",
    "default_coeff_base_lf_uv_cdf.h":
        "0bd07578490b05b84494e8026cec0595ae42179c0f3c5a81bf220a4023c8e0ef",
    "default_coeff_br_uv_cdf.h":
        "f1899efcba26b458c99cffae89c512593ce32a3de16cee49fa43a5b09e9cf1d5",
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
        ("eob_pt_16", "default_eob_pt_16_cdf.h",
         "av2_default_eob_multi16_cdfs", 5),
        ("eob_pt_32", "default_eob_pt_32_cdf.h",
         "av2_default_eob_multi32_cdfs", 6),
        ("eob_pt_64", "default_eob_pt_64_cdf.h",
         "av2_default_eob_multi64_cdfs", 7),
        ("eob_pt_1024", "default_eob_pt_1024_cdf.h",
         "av2_default_eob_multi1024_cdfs", 8),
        ("eob_extra", "default_eob_extra_cdf.h",
         "av2_default_eob_extra_cdfs", 2),
        ("coeff_base_eob", "default_coeff_base_eob_cdf.h",
         "av2_default_coeff_base_eob_multi_cdfs", 3),
        ("coeff_base_lf_eob", "default_coeff_base_lf_eob_cdf.h",
         "av2_default_coeff_base_lf_eob_multi_cdfs", 5),
        ("coeff_base", "default_coeff_base_cdf.h",
         "av2_default_coeff_base_multi_cdfs", 4),
        ("coeff_base_lf", "default_coeff_base_lf_cdf.h",
         "av2_default_coeff_base_lf_multi_cdfs", 6),
        ("coeff_br", "default_coeff_br_cdf.h",
         "av2_default_coeff_lps_multi_cdfs", 4),
        ("coeff_br_lf", "default_coeff_br_lf_cdf.h",
         "av2_default_coeff_lps_lf_multi_cdfs", 4),
        ("dc_sign", "default_dc_sign_cdf.h",
         "av2_default_dc_sign_cdfs", 2),
        ("coeff_base_bob", "default_coeff_base_bob_cdf.h",
         "av2_default_coeff_base_bob_multi_cdfs", 3),
        ("coeff_base_idtx", "default_coeff_base_idtx_cdf.h",
         "av2_default_coeff_base_multi_cdfs_idtx", 4),
        ("coeff_br_idtx", "default_coeff_br_idtx_cdf.h",
         "av2_default_coeff_lps_multi_cdfs_idtx", 4),
        ("idtx_sign", "default_idtx_sign_cdf.h",
         "av2_default_idtx_sign_cdfs", 2),
        ("v_txb_skip", "default_v_txb_skip_cdf.h",
         "av2_default_v_txb_skip_cdfs", 2),
        ("coeff_base_eob_uv", "default_coeff_base_eob_uv_cdf.h",
         "av2_default_coeff_base_eob_multi_uv_cdfs", 3),
        ("coeff_base_lf_eob_uv", "default_coeff_base_lf_eob_uv_cdf.h",
         "av2_default_coeff_base_lf_eob_multi_uv_cdfs", 5),
        ("coeff_base_uv", "default_coeff_base_uv_cdf.h",
         "av2_default_coeff_base_multi_uv_cdfs", 4),
        ("coeff_base_lf_uv", "default_coeff_base_lf_uv_cdf.h",
         "av2_default_coeff_base_lf_multi_uv_cdfs", 6),
        ("coeff_br_uv", "default_coeff_br_uv_cdf.h",
         "av2_default_coeff_lps_multi_uv_cdfs", 4),
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
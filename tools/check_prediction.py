#!/usr/bin/env python3
import argparse
import hashlib
import re
from pathlib import Path


DERIVATIVE_HASH = (
    "1820b55f70dac00ab8ece6346578459c69babfb31f4c924acad5d35037b17504"
)
D203_DERIVATIVE_INDEX = 67
D203_DERIVATIVE = 24
D45_DERIVATIVE_INDEX = 54
D45_DERIVATIVE = 44
D45_FILTER_PHASES = (22, 12, 2, 24)
D67_DERIVATIVE_INDEX = 65
D67_DERIVATIVE = 27
D67_FILTER_PHASES = (27, 8, 22, 3, 17, 30, 12, 25)
D67_MRL4_DERIVATIVE_INDEX = 62
D67_MRL4_DERIVATIVE = 31
D67_MRL4_FILTER_PHASES = (31, 14, 30, 13)
D67_MRL3_DERIVATIVE_INDEX = 58
D67_MRL3_DERIVATIVE = 38
D67_MRL3_FILTER_PHASES = (12, 31, 18, 5)
SMOOTH8_BLEND = (32, 16, 8, 4, 2, 1, 0, 0)


def initializer(source: str, name: str) -> str:
    match = re.search(
        rf"\b{name}\b[^=]*=\s*\{{(.*?)\n\}};?", source, re.DOTALL
    )
    if match is None:
        raise ValueError(f"initializer not found: {name}")
    return match.group(1)


def integers(source: str) -> list[int]:
    source = re.sub(r"//.*", "", source)
    return [int(value) for value in re.findall(r"-?\d+", source)]


def rows(values: list[int], width: int) -> list[tuple[int, ...]]:
    if len(values) % width:
        raise ValueError(f"{len(values)} values do not form {width}-word rows")
    return [tuple(values[index:index + width])
            for index in range(0, len(values), width)]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spec-derivative", type=Path,
        default=Path("docs/spec/av2-v1.0.0/dr_intra_derivative.h"))
    parser.add_argument(
        "--avm-common", type=Path,
        default=Path("vendor/avm/av2/common/av2_common_int.h"))
    parser.add_argument(
        "--avm-intra", type=Path,
        default=Path("vendor/avm/av2/common/reconintra.h"))
    parser.add_argument(
        "--avm-predictor", type=Path,
        default=Path("vendor/avm/avm_dsp/intrapred.c"))
    parser.add_argument(
        "--production", type=Path,
        default=Path("src/decoder/intra_prediction.c"))
    args = parser.parse_args()

    digest = hashlib.sha256(args.spec_derivative.read_bytes()).hexdigest()
    if digest != DERIVATIVE_HASH:
        raise ValueError(
            f"{args.spec_derivative}: sha256 {digest}, expected {DERIVATIVE_HASH}")
    specification = integers(args.spec_derivative.read_text().split("=", 1)[1])
    avm = integers(initializer(args.avm_common.read_text(),
                               "dr_intra_derivative"))
    if len(specification) != 90 or specification != avm:
        raise ValueError("normative and pinned AVM derivative tables differ")
    if specification[D203_DERIVATIVE_INDEX] != D203_DERIVATIVE:
        raise ValueError("D203 derivative is not 24")
    if specification[D45_DERIVATIVE_INDEX] != D45_DERIVATIVE:
        raise ValueError("D45 +3 derivative is not 44")
    if specification[D67_DERIVATIVE_INDEX] != D67_DERIVATIVE:
        raise ValueError("MRL-adjusted D67 derivative is not 27")
    if specification[D67_MRL4_DERIVATIVE_INDEX] != D67_MRL4_DERIVATIVE:
        raise ValueError("4x4 MRL-adjusted D67 derivative is not 31")
    if specification[D67_MRL3_DERIVATIVE_INDEX] != D67_MRL3_DERIVATIVE:
        raise ValueError("4x4 MRL-3 D67 derivative is not 38")

    avm_filters = rows(integers(initializer(
        args.avm_intra.read_text(), "av2_dr_interp_filter")), 4)
    production = args.production.read_text()
    production_filters = rows(integers(initializer(
        production, "dr_interp_filter")), 4)
    if production_filters != avm_filters:
        raise ValueError("production interpolation table differs from AVM")

    production_filters = rows(integers(initializer(
        production, "d45_filter")), 4)
    expected_filters = [avm_filters[phase] for phase in D45_FILTER_PHASES]
    if production_filters != expected_filters:
        raise ValueError(
            f"production D45 filters differ: {production_filters} != "
            f"{expected_filters}")

    production_filters = rows(integers(initializer(
        production, "d67_filter")), 4)
    expected_filters = [avm_filters[phase] for phase in D67_FILTER_PHASES]
    if production_filters != expected_filters:
        raise ValueError(
            f"production D67 filters differ: {production_filters} != "
            f"{expected_filters}")

    production_filters = rows(integers(initializer(
        production, "d67_mrl4_filter")), 4)
    expected_filters = [
        avm_filters[phase] for phase in D67_MRL4_FILTER_PHASES]
    if production_filters != expected_filters:
        raise ValueError(
            f"production 4x4 D67 filters differ: {production_filters} != "
            f"{expected_filters}")

    production_filters = rows(integers(initializer(
        production, "d67_mrl3_filter")), 4)
    expected_filters = [
        avm_filters[phase] for phase in D67_MRL3_FILTER_PHASES]
    if production_filters != expected_filters:
        raise ValueError(
            f"production 4x4 MRL-3 D67 filters differ: "
            f"{production_filters} != {expected_filters}")

    avm_predictor = args.avm_predictor.read_text()
    if "#define BLEND_WEIGHT_MAX 32" not in avm_predictor or not re.search(
            r"BLEND_WEIGHT_MAX\s*>>\s*\n?\s*AVMMIN\([^;]*"
            r"\(\(r\s*<<\s*1\)\s*>>\s*scale\)\)",
            avm_predictor, re.DOTALL):
        raise ValueError("pinned AVM smooth blend process differs")
    production_blend = tuple(integers(initializer(
        production, "smooth8_blend")))
    if production_blend != SMOOTH8_BLEND:
        raise ValueError(
            f"production 8-wide smooth blend differs: {production_blend} != "
            f"{SMOOTH8_BLEND}")
    if ("unsigned int shift = ((unsigned int)position << 1U) >> scale;"
            not in production or
            "return shift >= 6U ? 0U : (uint8_t)(64U >> shift);"
            not in production):
        raise ValueError("production geometric smooth blend process differs")

    print("D203 derivative: normative attachment and 90 AVM entries match")
    print("directional interpolation: all 32 production phases match pinned AVM")
    print("D45 +3 derivative: normative index 54 is 44")
    print("D45 interpolation: production phases 22,12,2,24 match pinned AVM")
    print("D67 MRL derivative: normative index 65 is 27")
    print("D67 interpolation: production phases 27,8,22,3,17,30,12,25 match pinned AVM")
    print("D67 4x4 MRL derivative: normative index 62 is 31")
    print("D67 4x4 interpolation: production phases 31,14,30,13 match pinned AVM")
    print("D67 4x4 MRL-3 derivative: normative index 58 is 38")
    print("D67 4x4 MRL-3 interpolation: production phases 12,31,18,5 match pinned AVM")
    print("smooth blend: production geometric process matches pinned AVM")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
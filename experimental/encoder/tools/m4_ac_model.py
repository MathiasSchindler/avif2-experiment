#!/usr/bin/env python3
"""Executable AV2 v1.0.0 8x8 DCT_DCT DC-plus-one-AC model."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[3]
ATTACHMENTS = ROOT / "docs/spec/av2-v1.0.0/attachments"

DCT8 = (
    (64, 64, 64, 64, 64, 64, 64, 64),
    (89, 75, 50, 18, -18, -50, -75, -89),
    (83, 35, -35, -83, -83, -35, 35, 83),
    (75, -18, -89, -50, 50, 89, 18, -75),
    (64, -64, -64, 64, 64, -64, -64, 64),
    (50, -89, 18, 75, -75, -18, 89, -50),
    (35, -83, 83, -35, -35, 83, -83, 35),
    (18, -50, 75, -89, 89, -75, 50, -18),
)

AC_QLOOKUP = (
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
)

ATTACHMENT_HASHES = {
    "dct_kernel8.h":
        "e89f5aac7056f3a8e55ee93a855520e3efb6392666a611563c17be3d9b86d560",
    "default_txb_skip_cdf.h":
        "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
    "default_eob_pt_64_cdf.h":
        "cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d",
    "default_coeff_base_lf_eob_cdf.h":
        "36dcaa7a6e355098c737a3187ec0038c54e0c609e080912cb620c3eda88d0e26",
    "default_coeff_base_lf_cdf.h":
        "45f66ff809ad075fc1b17f2e8366c254290e6e505deef9c69904a338802a2f55",
    "default_coeff_br_lf_cdf.h":
        "fee755c055bd50681616f80618fb39ae47468f99a284259aac118b938d56943a",
    "default_dc_sign_cdf.h":
        "17d4545c3d60dbab3e10a2ca3a26d852c224a1ccd8170ce3daf089a2eb6161fa",
}

SCAN_8X8_2D = (
    0, 8, 1, 16, 9, 2, 24, 17, 10, 3, 32, 25, 18, 11, 4, 40,
    33, 26, 19, 12, 5, 48, 41, 34, 27, 20, 13, 6, 56, 49, 42, 35,
    28, 21, 14, 7, 57, 50, 43, 36, 29, 22, 15, 58, 51, 44, 37,
    30, 23, 59, 52, 45, 38, 31, 60, 53, 46, 39, 61, 54, 47, 62,
    55, 63,
)
MIN_LEVEL_MAGNITUDE = 1
MAX_LEVEL_MAGNITUDE = 4


@dataclass(frozen=True)
class AcVector:
    qindex: int
    prediction: int
    source: tuple[tuple[int, ...], ...]
    analysis_dc: int
    analysis_ac10: int
    quantized_dc: int
    quantized_ac10: int
    dequantized_dc: int
    dequantized_ac10: int
    reconstruction: tuple[tuple[int, ...], ...]
    squared_error: int


def clip3(low: int, high: int, value: int) -> int:
    return min(high, max(low, value))


def round2(value: int, bits: int) -> int:
    return (value + (1 << (bits - 1))) >> bits if bits else value


def qlookup(qindex: int) -> int:
    if not 0 <= qindex <= 255:
        raise ValueError("qindex must be in 0..255")
    if qindex < 25:
        return AC_QLOOKUP[qindex]
    return AC_QLOOKUP[((qindex - 1) % 24) + 1] << ((qindex - 1) // 24)


def dequantize(level: int, qindex: int) -> int:
    magnitude = round2(abs(level) * qlookup(qindex), 3)
    return clip3(-32768, 32767, -magnitude if level < 0 else magnitude)


def verify_attachments() -> None:
    for filename, expected in ATTACHMENT_HASHES.items():
        digest = hashlib.sha256((ATTACHMENTS / filename).read_bytes()).hexdigest()
        if digest != expected:
            raise RuntimeError(f"{filename}: {digest} != {expected}")
    numbers = tuple(map(int, re.findall(
        r"-?\d+", (ATTACHMENTS / "dct_kernel8.h").read_text().split("=", 1)[1])))
    if numbers != tuple(value for row in DCT8 for value in row):
        raise RuntimeError("embedded DCT8 differs from the normative attachment")


def scan_8x8_2d() -> tuple[int, ...]:
    out: list[int] = []
    x = y = 0
    for _ in range(64):
        out.append(y * 8 + x)
        x += 1
        y -= 1
        if y < 0 or x >= 8:
            x += 1
            step = min(x, 7 - y)
            x -= step
            y += step
    return tuple(out)


def forward_analysis(samples: Sequence[Sequence[int]], prediction: int) -> list[list[int]]:
    if len(samples) != 8 or any(len(row) != 8 for row in samples):
        raise ValueError("source must be 8x8")
    residual = [[samples[y][x] - prediction for x in range(8)] for y in range(8)]
    return [[
        round2(sum(
            residual[y][x] * DCT8[v][y] * DCT8[u][x]
            for y in range(8) for x in range(8)
        ), 12)
        for u in range(8)] for v in range(8)]


def inverse_dct(dc: int, ac10: int) -> tuple[tuple[int, ...], ...]:
    coeff = [[0] * 8 for _ in range(8)]
    coeff[0][0] = dc
    coeff[1][0] = ac10
    intermediate = [[
        clip3(-32768, 32767, round2(
            sum(DCT8[j][x] * coeff[y][j] for j in range(8)), 7))
        for x in range(8)] for y in range(8)]
    return tuple(tuple(
        clip3(-256, 255, round2(
            sum(DCT8[i][y] * intermediate[i][x] for i in range(8)), 11))
        for x in range(8)) for y in range(8))


def reconstruct(dc_level: int, ac_level: int, qindex: int,
                prediction: int = 128) -> tuple[tuple[int, ...], ...]:
    residual = inverse_dct(
        dequantize(dc_level, qindex), dequantize(ac_level, qindex))
    return tuple(tuple(clip3(0, 255, prediction + value) for value in row)
                 for row in residual)


def choose_levels(samples: Sequence[Sequence[int]], qindex: int = 50,
                  prediction: int = 128) -> AcVector:
    source = tuple(tuple(row) for row in samples)
    analysis = forward_analysis(source, prediction)
    q = qlookup(qindex)
    best = None
    for dc_level in range(-MAX_LEVEL_MAGNITUDE, MAX_LEVEL_MAGNITUDE + 1):
        for ac_level in range(-MAX_LEVEL_MAGNITUDE, MAX_LEVEL_MAGNITUDE + 1):
            if (abs(dc_level) < MIN_LEVEL_MAGNITUDE or
                    abs(ac_level) < MIN_LEVEL_MAGNITUDE):
                continue
            decoded = reconstruct(dc_level, ac_level, qindex, prediction)
            error = sum((source[y][x] - decoded[y][x]) ** 2
                        for y in range(8) for x in range(8))
            key = (error, abs(dc_level) + abs(ac_level),
                   abs(ac_level), abs(dc_level), dc_level < 0, ac_level < 0)
            if best is None or key < best[0]:
                best = (key, dc_level, ac_level, decoded)
    if best is None:
        raise RuntimeError("no nonzero DC-plus-AC candidate")
    _, dc_level, ac_level, decoded = best
    return AcVector(
        qindex=qindex,
        prediction=prediction,
        source=source,
        analysis_dc=analysis[0][0],
        analysis_ac10=analysis[1][0],
        quantized_dc=dc_level,
        quantized_ac10=ac_level,
        dequantized_dc=dequantize(dc_level, qindex),
        dequantized_ac10=dequantize(ac_level, qindex),
        reconstruction=decoded,
        squared_error=sum((source[y][x] - decoded[y][x]) ** 2
                          for y in range(8) for x in range(8)),
    )


def vertical_gradient() -> tuple[tuple[int, ...], ...]:
    values = (128, 128, 128, 129, 129, 129, 130, 130)
    return tuple((value,) * 8 for value in values)


def _rows(filename: str, width: int) -> list[tuple[int, ...]]:
    source = (ATTACHMENTS / filename).read_text()
    pattern = r"\{\s*" + r"\s*,\s*".join([r"(-?\d+)"] * width) + r"\s*\}"
    return [tuple(map(int, row)) for row in re.findall(pattern, source)]


def coeff_q_context(base_qindex: int) -> int:
    return 0 if base_qindex <= 90 else 1 if base_qindex <= 140 else \
        2 if base_qindex <= 190 else 3


def _base_br(magnitude: int, eob: bool) -> tuple[int, int | None, int]:
    base = min(magnitude - 1, 5) if eob else min(magnitude, 6)
    level = base + 1 if eob else base
    br = min(magnitude - level, 3) if level > 4 else None
    level += br or 0
    return base, br, level


def _remainder(value: int) -> dict[str, object]:
    if value < 0:
        raise ValueError("negative remainder")
    m, k, cmax = 1, 2, 5
    quotient, rem = divmod(value, 1 << m)
    if quotient < cmax:
        return {"q_length_bits": "0" * quotient + "1",
                "golomb_length_bits": "", "coeff_rem_width": m,
                "coeff_rem": rem}
    length = k
    while value >= (cmax << m) + (1 << (length + 1)) - (1 << k):
        length += 1
    xbase = (cmax << m) + (1 << length) - (1 << k)
    return {"q_length_bits": "0" * cmax,
            "golomb_length_bits": "0" * (length - k) + "1",
            "coeff_rem_width": length, "coeff_rem": value - xbase}


def symbol_transcript(vector: AcVector) -> list[dict[str, object]]:
    verify_attachments()
    qctx = coeff_q_context(vector.qindex)
    dc_mag, ac_mag = abs(vector.quantized_dc), abs(vector.quantized_ac10)
    ac_base, ac_br, ac_coded = _base_br(ac_mag, True)
    dc_base, dc_br, dc_coded = _base_br(dc_mag, False)
    ac_eob_ctx = 1
    dc_base_ctx = min((min(ac_mag, 5) + 1) >> 1, 8)
    ac_br_ctx = 7
    dc_br_ctx = min((min(ac_mag, 7) + 1) >> 1, 6)

    skip = _rows("default_txb_skip_cdf.h", 3)
    eob = _rows("default_eob_pt_64_cdf.h", 8)
    base_eob = _rows("default_coeff_base_lf_eob_cdf.h", 6)
    base_lf = _rows("default_coeff_base_lf_cdf.h", 7)
    br_lf = _rows("default_coeff_br_lf_cdf.h", 5)
    dc_sign = _rows("default_dc_sign_cdf.h", 3)

    out: list[dict[str, object]] = [
        {"name": "all_zero", "symbol": 0, "context": 0,
         "cdf": skip[qctx * 100 + 10]},
        {"name": "eob_pt_64", "symbol": 1, "context": 0,
         "cdf": eob[qctx * 3]},
        {"name": "coeff_base_lf_eob", "position": 1, "symbol": ac_base,
         "context": ac_eob_ctx,
         "cdf": base_eob[(qctx * 5 + 1) * 4 + ac_eob_ctx]},
    ]
    if ac_br is not None:
        out.append({"name": "coeff_br_lf", "position": 1, "symbol": ac_br,
                    "context": ac_br_ctx,
                    "cdf": br_lf[qctx * 14 + ac_br_ctx]})
    out.append({"name": "coeff_base_lf", "position": 0, "symbol": dc_base,
                "context": dc_base_ctx,
                "cdf": base_lf[((qctx * 5 + 1) * 33 + dc_base_ctx) * 2]})
    if dc_br is not None:
        out.append({"name": "coeff_br_lf", "position": 0, "symbol": dc_br,
                    "context": dc_br_ctx,
                    "cdf": br_lf[qctx * 14 + dc_br_ctx]})
    out.append({"name": "sign_bit", "position": 1,
                "symbol": int(vector.quantized_ac10 < 0),
                "context": "equiprobable literal"})
    out.append({"name": "dc_sign", "position": 0,
                "symbol": int(vector.quantized_dc < 0), "context": 0,
                "cdf": dc_sign[qctx * 6]})
    for position, magnitude, coded in (
            (1, ac_mag, ac_coded), (0, dc_mag, dc_coded)):
        if coded >= 8:
            out.append({"name": "coeff_remainder", "position": position,
                        "value": magnitude - coded,
                        **_remainder(magnitude - coded)})
    out.append({"name": "br_summary", "position": 1,
                "context": ac_br_ctx, "symbol": ac_br, "emitted": ac_br is not None})
    out.append({"name": "br_summary", "position": 0,
                "context": dc_br_ctx, "symbol": dc_br, "emitted": dc_br is not None})
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qindex", type=int, default=50)
    parser.add_argument("--transcript", action="store_true")
    args = parser.parse_args()
    verify_attachments()
    if scan_8x8_2d() != SCAN_8X8_2D:
        raise RuntimeError("derived scan differs from the locked 8x8 scan")
    vector = choose_levels(vertical_gradient(), args.qindex)
    result = asdict(vector)
    if args.transcript:
        result["transcript"] = symbol_transcript(vector)
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

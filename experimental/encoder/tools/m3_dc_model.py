#!/usr/bin/env python3
"""Executable AV2 v1.0.0 grayscale DC transform/quantization model."""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from typing import Iterable, Sequence


AC_QLOOKUP = (
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
)

# Rows are cumulative Q15 thresholds followed by parameter index and count.
# Dimensions: q-context, then transform size where applicable.
CDF_ROWS = {
    "all_zero": {
        4: (
            (25759, 31, 0), (30296, 5, 0),
            (28021, 3, 0), (21443, 16, 0),
        ),
        8: (
            (24799, 56, 0), (31530, 30, 0),
            (30300, 0, 0), (27522, 0, 0),
        ),
    },
    "eob": {
        4: (
            (1946, 3059, 6834, 15123, 6, 0),
            (1839, 2995, 6796, 15126, 30, 0),
            (3946, 6406, 13325, 26004, 30, 0),
            (5029, 12046, 26017, 32453, 31, 0),
        ),
        8: (
            (1848, 2357, 4392, 7629, 13328, 21530, 32, 0),
            (939, 1381, 3142, 6536, 13617, 21799, 31, 0),
            (2024, 3239, 6969, 13073, 21617, 27643, 31, 0),
            (3765, 6333, 13340, 20831, 28830, 32289, 31, 0),
        ),
    },
    "coeff_base_lf_eob": {
        4: (
            (27486, 31140, 31779, 32064, 7, 0),
            (28595, 31926, 32420, 32582, 91, 0),
            (29573, 32066, 32477, 32625, 76, 0),
            (30609, 32413, 32637, 32716, 5, 0),
        ),
        8: (
            (27197, 31209, 31698, 31944, 32, 0),
            (28550, 32010, 32465, 32593, 1, 0),
            (29324, 32291, 32617, 32714, 0, 0),
            (31289, 32570, 32714, 32740, 1, 0),
        ),
    },
    "coeff_br_lf": (
        (7943, 14193, 20775, 32, 0),
        (11992, 17977, 23847, 37, 0),
        (13319, 21082, 26725, 37, 0),
        (13965, 20979, 26518, 62, 0),
    ),
    "dc_sign": (
        (
            (15831, 93, 0), (13632, 75, 0), (19041, 75, 0),
        ),
        (
            (16540, 93, 0), (14804, 93, 0), (18312, 93, 0),
        ),
        (
            (17575, 75, 0), (14224, 78, 0), (19801, 93, 0),
        ),
        (
            (20108, 1, 0), (14675, 0, 0), (20547, 93, 0),
        ),
    ),
}


@dataclass(frozen=True)
class DcVector:
    size: int
    qindex: int
    prediction: int
    source_mean_num: int
    source_mean_den: int
    forward_dc: int
    quantized_dc: int
    dequantized_dc: int
    residual: int
    reconstruction: int
    squared_error: int


def clip3(low: int, high: int, value: int) -> int:
    return min(high, max(low, value))


def round2(value: int, bits: int) -> int:
    if bits == 0:
        return value
    return (value + (1 << (bits - 1))) >> bits


def qlookup(qindex: int) -> int:
    if qindex < 0:
        raise ValueError("qindex must be non-negative")
    if qindex < 25:
        return AC_QLOOKUP[qindex]
    return AC_QLOOKUP[((qindex - 1) % 24) + 1] << ((qindex - 1) // 24)


def get_q(qindex: int, delta: int = 0, max_q: int = 255) -> int:
    if qindex == 0 and delta <= 0:
        return AC_QLOOKUP[0]
    return qlookup(clip3(1, max_q, qindex + delta))


def coeff_q_context(base_qindex: int) -> int:
    if base_qindex <= 90:
        return 0
    if base_qindex <= 140:
        return 1
    if base_qindex <= 190:
        return 2
    return 3


def dequantize_dc(level: int, qindex: int, delta: int = 0) -> int:
    magnitude = round2(abs(level) * get_q(qindex, delta), 3)
    value = -magnitude if level < 0 else magnitude
    return clip3(-(1 << 15), (1 << 15) - 1, value)


def inverse_dct_dc(dequant: int, size: int) -> int:
    if size not in (4, 8):
        raise ValueError("DC model supports only 4x4 and 8x8")
    row = clip3(-(1 << 15), (1 << 15) - 1, round2(64 * dequant, 7))
    col_shift = 10 if size == 4 else 11
    return clip3(-(1 << 8), (1 << 8) - 1, round2(64 * row, col_shift))


def forward_dc(samples: Sequence[int], prediction: int, size: int) -> int:
    if len(samples) != size * size or size not in (4, 8):
        raise ValueError("sample count must match a 4x4 or 8x8 block")
    residual_sum = sum(sample - prediction for sample in samples)
    return residual_sum * (2 if size == 4 else 1)


def reconstruct_dc(level: int, qindex: int, prediction: int, size: int,
                   delta: int = 0) -> tuple[int, int, int]:
    dequant = dequantize_dc(level, qindex, delta)
    residual = inverse_dct_dc(dequant, size)
    return dequant, residual, clip3(0, 255, prediction + residual)


def choose_dc_level(samples: Sequence[int], prediction: int, size: int,
                    qindex: int, delta: int = 0) -> DcVector:
    target = forward_dc(samples, prediction, size)
    q = get_q(qindex, delta)
    max_level = ((1 << 15) * 8 + q - 1) // q + 1
    candidates = [0]
    candidates.extend(range(1, max_level + 1))
    candidates.extend(range(-1, -max_level - 1, -1))
    best = None
    for level in candidates:
        dequant, residual, reconstruction = reconstruct_dc(
            level, qindex, prediction, size, delta)
        error = sum((sample - reconstruction) ** 2 for sample in samples)
        key = (error, abs(level), level < 0)
        if best is None or key < best[0]:
            best = (key, level, dequant, residual, reconstruction)
    assert best is not None
    _, level, dequant, residual, reconstruction = best
    return DcVector(
        size=size,
        qindex=qindex,
        prediction=prediction,
        source_mean_num=sum(samples),
        source_mean_den=len(samples),
        forward_dc=target,
        quantized_dc=level,
        dequantized_dc=dequant,
        residual=residual,
        reconstruction=reconstruction,
        squared_error=sum((sample - reconstruction) ** 2
                          for sample in samples),
    )


def dc_prediction(above: Sequence[int] | None,
                  left: Sequence[int] | None, size: int) -> int:
    edges: list[int] = []
    if left is not None:
        if len(left) < size:
            raise ValueError("left edge is shorter than the block")
        edges.extend(left[:size])
    if above is not None:
        if len(above) < size:
            raise ValueError("above edge is shorter than the block")
        edges.extend(above[:size])
    if not edges:
        return 128
    # For 4x4 and 8x8, normative approx_divide has a power-of-two divisor.
    return clip3(0, 255, round2(sum(edges), len(edges).bit_length() - 1))


def all_zero_context(block_equals_transform: bool,
                     above_levels: Iterable[int] = (),
                     left_levels: Iterable[int] = ()) -> int:
    if block_equals_transform:
        return 0
    above = tuple(above_levels)
    left_values = tuple(left_levels)
    top = 0
    for value in above:
        top |= value
    left = 0
    for value in left_values:
        left |= value
    top = min(4, top)
    left = min(4, left)
    return (top + left + 3) >> 1


def dc_sign_context(above_categories: Iterable[int] = (),
                    left_categories: Iterable[int] = ()) -> int:
    score = 0
    for category in (*tuple(above_categories), *tuple(left_categories)):
        if category == 1:
            score -= 1
        elif category == 2:
            score += 1
    return 1 if score < 0 else 2 if score > 0 else 0


def _remainder_literals(value: int) -> dict[str, object]:
    if value < 0:
        raise ValueError("coefficient remainder cannot be negative")
    if value < 10:
        quotient, remainder = divmod(value, 2)
        return {
            "q_length_bits": "0" * quotient + "1",
            "golomb_length_bits": "",
            "coeff_rem_width": 1,
            "coeff_rem": remainder,
        }
    length = 2
    while value >= 6 + (1 << (length + 1)):
        length += 1
    return {
        "q_length_bits": "00000",
        "golomb_length_bits": "0" * (length - 2) + "1",
        "coeff_rem_width": length,
        "coeff_rem": value - 6 - (1 << length),
    }


def symbol_transcript(level: int, size: int, base_qindex: int,
                      all_zero_ctx: int = 0,
                      sign_ctx: int = 0) -> list[dict[str, object]]:
    if size not in (4, 8):
        raise ValueError("size must be 4 or 8")
    if all_zero_ctx != 0:
        raise ValueError("the embedded fixed-block CDF subset has skip ctx 0")
    if sign_ctx not in (0, 1, 2):
        raise ValueError("DC sign context must be 0, 1, or 2")
    qctx = coeff_q_context(base_qindex)
    transcript: list[dict[str, object]] = [{
        "name": "all_zero",
        "symbol": int(level == 0),
        "context": all_zero_ctx,
        "cdf": CDF_ROWS["all_zero"][size][qctx],
    }]
    if level == 0:
        return transcript
    magnitude = abs(level)
    transcript.append({
        "name": "eob_pt_16" if size == 4 else "eob_pt_64",
        "symbol": 0,
        "context": 0,
        "cdf": CDF_ROWS["eob"][size][qctx],
    })
    transcript.append({
        "name": "coeff_base_lf_eob",
        "symbol": min(magnitude - 1, 4),
        "context": 0,
        "cdf": CDF_ROWS["coeff_base_lf_eob"][size][qctx],
    })
    if magnitude >= 5:
        transcript.append({
            "name": "coeff_br_lf",
            "symbol": min(magnitude - 5, 3),
            "context": 0,
            "cdf": CDF_ROWS["coeff_br_lf"][qctx],
        })
    transcript.append({
        "name": "dc_sign",
        "symbol": int(level < 0),
        "context": sign_ctx,
        "cdf": CDF_ROWS["dc_sign"][qctx][sign_ctx],
    })
    if magnitude >= 8:
        transcript.append({
            "name": "coeff_remainder",
            "value": magnitude - 8,
            **_remainder_literals(magnitude - 8),
        })
    return transcript


def lossless_wht_dc(level: int) -> list[list[int]]:
    def wht(values: list[int], shift: int) -> list[int]:
        a, c, d, b = (value >> shift for value in values)
        a += c
        d -= b
        e = (a - d) >> 1
        b = e - b
        c = e - c
        a -= b
        d += c
        return [a, b, c, d]

    dequant = dequantize_dc(level, 0)
    rows = [wht([dequant, 0, 0, 0], 3)] + [[0] * 4 for _ in range(3)]
    return [wht([rows[i][j] for i in range(4)], 0) for j in range(4)]


def lossless_idtx_residual(level: int) -> int:
    return dequantize_dc(level, 0) >> 3


def vectors() -> list[DcVector]:
    cases: list[tuple[int, int, int, list[int]]] = []
    for size in (4, 8):
        for qindex in (1, 91, 141, 191, 255):
            for value in (0, 120, 127, 128, 129, 136, 255):
                cases.append((size, qindex, 128, [value] * (size * size)))
        ramp = list(range(size * size))
        cases.append((size, 141, 128, ramp))
        cases.append((size, 191, 250, [255] * (size * size)))
        cases.append((size, 191, 5, [0] * (size * size)))
    return [choose_dc_level(samples, prediction, size, qindex)
            for size, qindex, prediction, samples in cases]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vectors", action="store_true",
                        help="emit locked constant/ramp/clipping vectors")
    parser.add_argument("--transcript", nargs=3, metavar=("LEVEL", "SIZE", "Q"),
                        type=int, help="emit one DC coefficient transcript")
    args = parser.parse_args()
    if args.transcript:
        print(json.dumps(symbol_transcript(*args.transcript), indent=2))
    else:
        print(json.dumps([asdict(vector) for vector in vectors()], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

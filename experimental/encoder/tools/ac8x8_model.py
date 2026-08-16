#!/usr/bin/env python3
"""Independent scalar AV2 8x8 DCT_DCT coefficient and RD model."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Sequence


ROOT = Path(__file__).resolve().parents[3]
ATTACHMENTS = ROOT / "docs/spec/av2-v1.0.0/attachments"
VECTORS = ROOT / "experimental/encoder/tests/ac8x8_vectors.json"
RD_REPORT = ROOT / "experimental/encoder/metrics/ac8x8-rd.json"

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
SCAN = (
    0, 8, 1, 16, 9, 2, 24, 17, 10, 3, 32, 25, 18, 11, 4, 40,
    33, 26, 19, 12, 5, 48, 41, 34, 27, 20, 13, 6, 56, 49, 42, 35,
    28, 21, 14, 7, 57, 50, 43, 36, 29, 22, 15, 58, 51, 44, 37,
    30, 23, 59, 52, 45, 38, 31, 60, 53, 46, 39, 61, 54, 47, 62,
    55, 63,
)
GROUP_START = (0, 1, 2, 3, 5, 9, 17, 33, 65)
OFFSET_BITS = (0, 0, 0, 1, 2, 3, 4, 5, 6)
AC_QLOOKUP = (
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
)
HASHES = {
    "dct_kernel8.h": "e89f5aac7056f3a8e55ee93a855520e3efb6392666a611563c17be3d9b86d560",
    "para_adjustment_list.h": "0488e7e0a8d16a89f53ba8cf1ec8e449a9a3e74aa34ba2fa49e287159336ef80",
    "default_txb_skip_cdf.h": "0a2189ceaf2dc653d20a21bb6dad16b307c8137a1eb0fabfded3f0b36fa502c0",
    "default_eob_pt_64_cdf.h": "cfaf9793bdae974a2233dd36d5c233a8a56dea4dc06ce24d5dc2725d3d7b8a4d",
    "default_eob_extra_cdf.h": "0c0e719e415956b36d25cf326f7914aac890477791bb6d61edf987786914dba1",
    "default_coeff_base_eob_cdf.h": "77e0545f4a068d7cc194be884aff763af5a51af73fbb6dbbff97588dfd31f0a2",
    "default_coeff_base_lf_eob_cdf.h": "36dcaa7a6e355098c737a3187ec0038c54e0c609e080912cb620c3eda88d0e26",
    "default_coeff_base_cdf.h": "9f415dbf859f39c062c49c836eba6a1e9646c1f654ed998574761de942c42819",
    "default_coeff_base_lf_cdf.h": "45f66ff809ad075fc1b17f2e8366c254290e6e505deef9c69904a338802a2f55",
    "default_coeff_br_cdf.h": "e3f0ebd02b0f01086a7ca2129ac861078440450442ea53f1309a31a97993c7a0",
    "default_coeff_br_lf_cdf.h": "fee755c055bd50681616f80618fb39ae47468f99a284259aac118b938d56943a",
    "default_dc_sign_cdf.h": "17d4545c3d60dbab3e10a2ca3a26d852c224a1ccd8170ce3daf089a2eb6161fa",
}
TABLES = {
    "skip": ("default_txb_skip_cdf.h", 2, 400),
    "eob_pt": ("default_eob_pt_64_cdf.h", 7, 12),
    "eob_extra": ("default_eob_extra_cdf.h", 2, 4),
    "eob": ("default_coeff_base_eob_cdf.h", 3, 80),
    "eob_lf": ("default_coeff_base_lf_eob_cdf.h", 5, 80),
    "base": ("default_coeff_base_cdf.h", 4, 800),
    "base_lf": ("default_coeff_base_lf_cdf.h", 6, 1320),
    "br": ("default_coeff_br_cdf.h", 4, 28),
    "br_lf": ("default_coeff_br_lf_cdf.h", 4, 56),
    "dc_sign": ("default_dc_sign_cdf.h", 2, 48),
}


def clip(low: int, high: int, value: int) -> int:
    return min(high, max(low, value))


def round2(value: int, bits: int) -> int:
    return (value + (1 << (bits - 1))) >> bits if bits else value


def qlookup(qindex: int) -> int:
    if not 0 <= qindex <= 255:
        raise ValueError("qindex must be in 0..255")
    if qindex < 25:
        return AC_QLOOKUP[qindex]
    return AC_QLOOKUP[((qindex - 1) % 24) + 1] << ((qindex - 1) // 24)


def qcontext(qindex: int) -> int:
    return 0 if qindex <= 90 else 1 if qindex <= 140 else 2 if qindex <= 190 else 3


def dequantize(level: int, qindex: int) -> int:
    magnitude = clip(0, 32767, round2(abs(level) * qlookup(qindex), 3))
    return -magnitude if level < 0 else magnitude


def verify_attachments() -> None:
    for name, expected in HASHES.items():
        actual = hashlib.sha256((ATTACHMENTS / name).read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(f"{name}: {actual} != {expected}")
    values = tuple(map(int, re.findall(
        r"-?\d+", (ATTACHMENTS / "dct_kernel8.h").read_text().split("=", 1)[1]
    )))
    if values != tuple(value for row in DCT8 for value in row):
        raise RuntimeError("embedded DCT8 differs from normative attachment")


def derive_scan() -> tuple[int, ...]:
    result = []
    for diagonal in range(15):
        row = min(diagonal, 7)
        last = max(0, diagonal - 7)
        while row >= last:
            result.append(row * 8 + diagonal - row)
            row -= 1
    return tuple(result)


def forward(samples: Sequence[Sequence[int]], prediction: int = 128) -> tuple[int, ...]:
    if len(samples) != 8 or any(len(row) != 8 for row in samples):
        raise ValueError("samples must be 8x8")
    residual = [[int(samples[y][x]) - prediction for x in range(8)] for y in range(8)]
    return tuple(
        round2(sum(
            residual[y][x] * DCT8[v][y] * DCT8[u][x]
            for y in range(8) for x in range(8)
        ), 12)
        for v in range(8) for u in range(8)
    )


def inverse(coefficients: Sequence[int]) -> tuple[tuple[int, ...], ...]:
    if len(coefficients) != 64:
        raise ValueError("coefficients must have 64 entries")
    rows = [[
        clip(-32768, 32767, round2(sum(
            DCT8[j][x] * int(coefficients[y * 8 + j]) for j in range(8)
        ), 7))
        for x in range(8)] for y in range(8)]
    return tuple(tuple(
        clip(-256, 255, round2(sum(DCT8[i][y] * rows[i][x] for i in range(8)), 11))
        for x in range(8)) for y in range(8))


def reconstruct(levels: Sequence[int], qindex: int,
                prediction: int = 128) -> tuple[tuple[int, ...], ...]:
    residual = inverse(tuple(dequantize(level, qindex) for level in levels))
    return tuple(tuple(clip(0, 255, prediction + value) for value in row)
                 for row in residual)


def _rows(filename: str, symbols: int, expected_rows: int) -> list[list[int]]:
    text = (ATTACHMENTS / filename).read_text()
    pattern = r"\{\s*" + r"\s*,\s*".join([r"(\d+)"] * (symbols + 1)) + r"\s*\}"
    rows = [list(map(int, row)) for row in re.findall(pattern, text)]
    if len(rows) != expected_rows:
        raise RuntimeError(f"{filename}: {len(rows)} rows != {expected_rows}")
    return rows


def load_tables() -> dict[str, list[list[int]]]:
    verify_attachments()
    return {name: _rows(*spec) for name, spec in TABLES.items()}


def materialize(row: Sequence[int], symbols: int) -> list[int]:
    adjustments = (0, -1, -2, 1, 1)
    parameter, count = row[-2:]
    base = 2 if symbols < 4 else 3
    return [
        *(32768 - value for value in row[:symbols - 1]), 0, count,
        base + adjustments[parameter // 25],
        base + 1 + adjustments[(parameter // 5) % 5],
        base + 2 + adjustments[parameter % 5],
    ]


class RangeWriter:
    def __init__(self) -> None:
        self.output = bytearray()
        self.symbols_out = 0
        self.low = 0
        self.range = 32768
        self.count = -9
        self.pending_byte = -1
        self.pending_ff = 0

    def tell(self) -> int:
        return self.symbols_out * 8 + self.count + 10

    def _carry(self, value: int) -> None:
        self.symbols_out += 1
        if value == 0xFF:
            self.pending_ff += 1
            return
        carry = value >> 8
        if self.pending_byte >= 0:
            self.output.append((self.pending_byte + carry) & 0xFF)
        self.output.extend(bytes([(0xFF + carry) & 0xFF]) * self.pending_ff)
        self.pending_ff = 0
        self.pending_byte = value & 0xFF

    def _normalize(self, low: int, width: int) -> None:
        shift = 15 - (width.bit_length() - 1)
        count = self.count
        available = count + shift
        if available >= 0:
            count += 16
            mask = (1 << count) - 1
            if available >= 8:
                self._carry(low >> count)
                low &= mask
                count -= 8
                mask >>= 8
            self._carry(low >> count)
            available = count + shift - 24
            low &= mask
        self.low, self.range, self.count = low << shift, width << shift, available

    @staticmethod
    def _scale(probability: int, width: int, symbol: int, count: int) -> int:
        reduced = ((probability >> 7) << 4) + 16 * (count - symbol - 1) // count
        return (((width >> 8) * reduced) >> 7) << 3

    def symbol(self, symbol: int, cdf: list[int], count: int) -> None:
        upper = self.range if symbol == 0 else self._scale(
            cdf[symbol - 1], self.range, symbol - 1, count)
        lower = self._scale(cdf[symbol], self.range, symbol, count)
        self._normalize(self.low + self.range - upper, upper - lower)
        interval = 2 if cdf[count] > 31 else 1 if cdf[count] > 15 else 0
        rate = 2 + cdf[count + 1 + interval]
        target = 32768
        for index in range(count - 1):
            if index == symbol:
                target = 0
            cdf[index] += (target - cdf[index]) >> rate
        cdf[count] += int(cdf[count] < 32)

    def bit(self, value: int) -> None:
        width = self.range >> 1
        self._normalize(self.low + (self.range - width if value else 0), width)

    def literal(self, value: int, bits: int) -> None:
        for shift in reversed(range(bits)):
            self.bit((value >> shift) & 1)

    def finish(self) -> bytes:
        trailing_position = self.tell() - 1
        trailing_byte, trailing_offset = divmod(trailing_position, 8)
        low, width, count = self.low, self.range, self.count
        bits, mask = 9, 0x7FFF
        end = (low + mask) & ~mask
        while (end | mask) >= low + width:
            bits += 1
            mask >>= 1
            end = (low + mask) & ~mask
        bits += count
        if bits > 0:
            remainder_mask = (1 << (count + 16)) - 1
            while True:
                self._carry(end >> (count + 16))
                end &= remainder_mask
                bits -= 8
                count -= 8
                remainder_mask >>= 8
                if bits <= 0:
                    break
        if self.pending_byte >= 0 or self.pending_ff:
            self._carry(0)
        while len(self.output) <= trailing_byte:
            self.output.append(0)
        self.output[trailing_byte] &= (0xFF << (7 - trailing_offset)) & 0xFF
        self.output[trailing_byte] |= 1 << (7 - trailing_offset)
        return bytes(self.output)


class CdfState:
    def __init__(self, tables: dict[str, list[list[int]]]) -> None:
        self.tables = tables
        self.cdfs: dict[tuple[str, int], list[int]] = {}

    def get(self, name: str, row: int) -> tuple[list[int], int]:
        symbols = TABLES[name][1]
        key = (name, row)
        if key not in self.cdfs:
            self.cdfs[key] = materialize(self.tables[name][row], symbols)
        return self.cdfs[key], symbols


def level_at(levels: Sequence[int], row: int, column: int) -> int:
    return 0 if row < 0 or column < 0 or row >= 8 or column >= 8 else abs(levels[row * 8 + column])


def is_lf(position: int) -> bool:
    return position // 8 + position % 8 < 4


def lf_context(levels: Sequence[int], position: int) -> int:
    row, column = divmod(position, 8)
    magnitude = (
        min(level_at(levels, row, column + 1), 5)
        + min(level_at(levels, row + 1, column), 5)
        + min(level_at(levels, row + 1, column + 1), 5)
        + min(level_at(levels, row, column + 2), 5)
        + min(level_at(levels, row + 2, column), 5)
    )
    context = (magnitude + 1) >> 1
    if position == 0:
        return min(context, 8)
    if row + column < 2:
        return 9 + min(context, 6)
    return 16 + min(context, 4)


def base_context(levels: Sequence[int], position: int) -> int:
    row, column = divmod(position, 8)
    magnitude = (
        min(level_at(levels, row, column + 1), 3)
        + min(level_at(levels, row + 1, column), 3)
        + min(level_at(levels, row + 1, column + 1), 3)
        + min(level_at(levels, row, column + 2), 3)
        + min(level_at(levels, row + 2, column), 3)
    )
    context = min((magnitude + 1) >> 1, 4)
    diagonal = row + column
    return context if diagonal < 6 else 5 + context if diagonal < 8 else 10 + context


def br_context(levels: Sequence[int], position: int, low: bool) -> int:
    row, column = divmod(position, 8)
    magnitude = (
        min(level_at(levels, row, column + 1), 5)
        + min(level_at(levels, row + 1, column), 5)
        + min(level_at(levels, row + 1, column + 1), 5)
    )
    context = min((magnitude + 1) >> 1, 6)
    return context if not low or position == 0 else context + 7


def eob_context(scan_index: int) -> int:
    return 0 if scan_index == 0 else 1 if scan_index <= 8 else 2 if scan_index <= 16 else 3


def _write_symbol(writer: RangeWriter, state: CdfState, events: list[dict[str, object]],
                  name: str, row: int, symbol: int, **fields: object) -> None:
    cdf, symbols = state.get(name, row)
    before = writer.tell()
    initial = tuple(cdf)
    writer.symbol(symbol, cdf, symbols)
    events.append({"name": name, "row": row, "symbol": symbol,
                   "bits_before": before, "bits_after": writer.tell(),
                   "cdf_before": initial, **fields})


def _write_high(writer: RangeWriter, value: int, average: int,
                events: list[dict[str, object]], position: int) -> int:
    rice = 1
    while rice < 6 and average >= (4 << (rice - 1)):
        rice += 1
    maximum = min(rice + 4, 6)
    quotient = value >> rice
    before = writer.tell()
    if quotient < maximum:
        for _ in range(quotient):
            writer.bit(0)
        writer.bit(1)
        writer.literal(value & ((1 << rice) - 1), rice)
    else:
        for _ in range(maximum):
            writer.bit(0)
        length = rice + 1
        while value >= (maximum << rice) + (1 << (length + 1)) - (1 << (rice + 1)):
            length += 1
        extension = length - rice - 1
        for _ in range(extension):
            writer.bit(0)
        writer.bit(1)
        base = (maximum << rice) + (1 << length) - (1 << (rice + 1))
        writer.literal(value - base, length)
    events.append({"name": "remainder", "position": position, "value": value,
                   "rice": rice, "bits_before": before, "bits_after": writer.tell()})
    return (average + value) >> 1


@dataclass(frozen=True)
class CodingResult:
    eob: int
    nonzero: int
    tell_bits: int
    finished_bytes: int
    hex: str
    events: tuple[dict[str, object], ...]


def encode_coefficients(levels: Sequence[int], qindex: int,
                        skip_context: int = 0, dc_sign_context: int = 0) -> CodingResult:
    if len(levels) != 64 or any(abs(int(level)) > 32767 for level in levels):
        raise ValueError("levels must contain 64 signed 15-bit values")
    tables, writer, events = load_tables(), RangeWriter(), []
    state = CdfState(tables)
    qctx = qcontext(qindex)
    last = max((index for index, position in enumerate(SCAN)
                if levels[position] != 0), default=-1)
    eob = last + 1
    _write_symbol(writer, state, events, "skip",
                  ((qctx * 2) * 5 + 1) * 10 + skip_context,
                  int(eob == 0), context=skip_context)
    if eob == 0:
        tell = writer.tell()
        encoded = writer.finish()
        return CodingResult(0, 0, tell, len(encoded), encoded.hex(), tuple(events))

    point = max(index for index, start in enumerate(GROUP_START) if start <= eob)
    _write_symbol(writer, state, events, "eob_pt", qctx * 3, point - 1,
                  eob=eob, eob_point=point)
    offset_bits = OFFSET_BITS[point]
    extra = eob - GROUP_START[point]
    if offset_bits:
        _write_symbol(writer, state, events, "eob_extra", qctx,
                      extra >> (offset_bits - 1), eob=eob)
        if offset_bits > 1:
            before = writer.tell()
            writer.literal(extra & ((1 << (offset_bits - 1)) - 1), offset_bits - 1)
            events.append({"name": "eob_extra_literal", "value": extra,
                           "bits_before": before, "bits_after": writer.tell()})

    base_levels = [0] * 64
    for scan_index in reversed(range(eob)):
        position = SCAN[scan_index]
        magnitude = abs(int(levels[position]))
        low = is_lf(position)
        if scan_index == eob - 1:
            context = eob_context(scan_index)
            if low:
                symbol = min(magnitude - 1, 4)
                row = (qctx * 5 + 1) * 4 + context
                _write_symbol(writer, state, events, "eob_lf", row, symbol,
                              position=position, context=context)
                coded = symbol + 1
                if coded > 4:
                    brctx = br_context(base_levels, position, True)
                    br = min(magnitude - coded, 3)
                    _write_symbol(writer, state, events, "br_lf", qctx * 14 + brctx,
                                  br, position=position, context=brctx)
                    coded += br
            else:
                symbol = min(magnitude - 1, 2)
                row = (qctx * 5 + 1) * 4 + context
                _write_symbol(writer, state, events, "eob", row, symbol,
                              position=position, context=context)
                coded = symbol + 1
                if coded > 2:
                    br = min(magnitude - coded, 3)
                    _write_symbol(writer, state, events, "br", qctx * 7, br,
                                  position=position, context=0)
                    coded += br
        else:
            context = lf_context(base_levels, position) if low else base_context(base_levels, position)
            if low:
                symbol = min(magnitude, 5)
                row = ((qctx * 5 + 1) * 33 + context) * 2
                _write_symbol(writer, state, events, "base_lf", row, symbol,
                              position=position, context=context)
                coded = symbol
                if symbol > 4:
                    brctx = br_context(base_levels, position, True)
                    br = min(magnitude - coded, 3)
                    _write_symbol(writer, state, events, "br_lf", qctx * 14 + brctx,
                                  br, position=position, context=brctx)
                    coded += br
            else:
                symbol = min(magnitude, 3)
                row = ((qctx * 5 + 1) * 20 + context) * 2
                _write_symbol(writer, state, events, "base", row, symbol,
                              position=position, context=context)
                coded = symbol
                if symbol > 2:
                    brctx = br_context(base_levels, position, False)
                    br = min(magnitude - coded, 3)
                    _write_symbol(writer, state, events, "br", qctx * 7 + brctx,
                                  br, position=position, context=brctx)
                    coded += br
        base_levels[position] = coded

    high_average = 0
    for scan_index in reversed(range(eob)):
        position = SCAN[scan_index]
        magnitude = abs(int(levels[position]))
        if magnitude == 0:
            continue
        before = writer.tell()
        if position == 0:
            _write_symbol(writer, state, events, "dc_sign",
                          qctx * 12 + dc_sign_context,
                          int(levels[position] < 0), position=position,
                          context=dc_sign_context)
        else:
            writer.bit(int(levels[position] < 0))
            events.append({"name": "sign_bit", "position": position,
                           "symbol": int(levels[position] < 0),
                           "bits_before": before, "bits_after": writer.tell()})
        threshold = 8 if is_lf(position) else 6
        if base_levels[position] >= threshold:
            high_average = _write_high(
                writer, magnitude - base_levels[position], high_average,
                events, position)
    tell = writer.tell()
    encoded = writer.finish()
    return CodingResult(eob, sum(level != 0 for level in levels), tell,
                        len(encoded), encoded.hex(), tuple(events))


@dataclass(frozen=True)
class Candidate:
    levels: tuple[int, ...]
    sse: int
    rate_bits: int
    rd_cost: float
    reconstruction: tuple[tuple[int, ...], ...]


def evaluate(samples: Sequence[Sequence[int]], levels: Sequence[int], qindex: int,
             prediction: int, lambda_rd: float) -> Candidate:
    decoded = reconstruct(levels, qindex, prediction)
    sse = sum((int(samples[y][x]) - decoded[y][x]) ** 2
              for y in range(8) for x in range(8))
    rate = encode_coefficients(levels, qindex).tell_bits
    return Candidate(tuple(levels), sse, rate, sse + lambda_rd * rate, decoded)


def search(samples: Sequence[Sequence[int]], qindex: int, prediction: int = 128,
           max_nonzero: int = 8, max_positions: int = 16,
           level_limit: int = 63, lambda_rd: float | None = None,
           allowed_positions: Sequence[int] | None = None) -> Candidate:
    if lambda_rd is None:
        lambda_rd = max(0.25, (qlookup(qindex) / 64.0) ** 2)
    analysis = forward(samples, prediction)
    surface = range(64) if allowed_positions is None else allowed_positions
    ordered = sorted(surface, key=lambda p: (-abs(analysis[p]), SCAN.index(p)))
    positions = ordered[:max_positions]
    levels = [0] * 64
    best = evaluate(samples, levels, qindex, prediction, lambda_rd)
    for position in positions:
        ideal = clip(-level_limit, level_limit, round(analysis[position] * 8 / qlookup(qindex)))
        choices = {0, ideal}
        choices.update(clip(-level_limit, level_limit, ideal + delta)
                       for delta in (-2, -1, 1, 2))
        local = best
        for value in sorted(choices):
            trial = list(best.levels)
            trial[position] = value
            if sum(level != 0 for level in trial) > max_nonzero:
                continue
            candidate = evaluate(samples, trial, qindex, prediction, lambda_rd)
            key = (candidate.rd_cost, candidate.sse, candidate.rate_bits,
                   sum(abs(level) for level in candidate.levels), candidate.levels)
            local_key = (local.rd_cost, local.sse, local.rate_bits,
                         sum(abs(level) for level in local.levels), local.levels)
            if key < local_key:
                local = candidate
        best = local
    return best


def patterns() -> dict[str, tuple[tuple[int, ...], ...]]:
    photo = (
        (52, 55, 61, 66, 70, 61, 64, 73),
        (63, 59, 55, 90, 109, 85, 69, 72),
        (62, 59, 68, 113, 144, 104, 66, 73),
        (63, 58, 71, 122, 154, 106, 70, 69),
        (67, 61, 68, 104, 126, 88, 68, 70),
        (79, 65, 60, 70, 77, 68, 58, 75),
        (85, 71, 64, 59, 55, 61, 65, 83),
        (87, 79, 69, 68, 65, 76, 78, 94),
    )
    noise = tuple(tuple(clip(0, 255, 128 + (((x * 37 + y * 53 + 17) % 97) - 48))
                        for x in range(8)) for y in range(8))
    return {
        "horizontal-edge": tuple((64,) * 8 if y < 4 else (192,) * 8 for y in range(8)),
        "vertical-edge": tuple(tuple(64 if x < 4 else 192 for x in range(8)) for _ in range(8)),
        "checker": tuple(tuple(48 if (x + y) & 1 else 208 for x in range(8)) for y in range(8)),
        "detail": tuple(tuple(clip(0, 255, 24 + x * 19 + y * 11 + ((x * y) % 5) * 9)
                              for x in range(8)) for y in range(8)),
        "noise": noise,
        "photographic": photo,
    }


def _summary(candidate: Candidate) -> dict[str, object]:
    nonzero = [{"position": index, "scan": SCAN.index(index), "level": level}
               for index, level in enumerate(candidate.levels) if level]
    return {
        "sse": candidate.sse,
        "rate_bits": candidate.rate_bits,
        "rd_cost": round(candidate.rd_cost, 6),
        "nonzero": nonzero,
        "reconstruction_sha256": hashlib.sha256(bytes(
            value for row in candidate.reconstruction for value in row)).hexdigest(),
    }


def build_report() -> dict[str, object]:
    verify_attachments()
    cases = []
    for name, samples in patterns().items():
        for qindex in (50, 100, 150, 210):
            lam = max(0.25, (qlookup(qindex) / 64.0) ** 2)
            general = search(samples, qindex, lambda_rd=lam)
            dc = search(samples, qindex, max_nonzero=1, max_positions=1,
                        lambda_rd=lam, allowed_positions=(0,))
            m4 = search(samples, qindex, max_nonzero=2, max_positions=2,
                        lambda_rd=lam, allowed_positions=(0, 8))
            cases.append({
                "name": name, "qindex": qindex, "lambda": round(lam, 6),
                "source_sha256": hashlib.sha256(bytes(
                    value for row in samples for value in row)).hexdigest(),
                "dc_only": _summary(dc), "m4_surface": _summary(m4),
                "general": _summary(general),
                "general_vs_dc_sse_percent": round(
                    100 * (dc.sse - general.sse) / max(1, dc.sse), 3),
                "general_vs_m4_sse_percent": round(
                    100 * (m4.sse - general.sse) / max(1, m4.sse), 3),
            })
    return {
        "schema": "av2-encoder-ac8x8-rd-v1",
        "surface": {
            "transform": "8x8 DCT_DCT", "scan_positions": [0, 63],
            "raster_positions": [0, 63], "level_magnitudes": [1, 32767],
            "search_level_magnitudes": [1, 63], "max_search_nonzero": 8,
            "parity_hiding": False, "tcq": False,
        },
        "cases": cases,
    }


def build_vectors() -> dict[str, object]:
    vectors = []
    surfaces = {
        "all-zero": [0] * 64,
        "dc": [7] + [0] * 63,
        "early-ac": [3 if index == 0 else -9 if index == 8 else 0 for index in range(64)],
        "high-frequency": [11 if index == 63 else 0 for index in range(64)],
        "base-br-remainder": [
            -31 if index == 0 else 8 if index == 1 else -32767 if index == 55 else 0
            for index in range(64)
        ],
    }
    for qindex in (50, 100, 150, 210):
        for name, levels in surfaces.items():
            coding = encode_coefficients(levels, qindex)
            reconstruction = reconstruct(levels, qindex)
            vectors.append({
                "name": name, "qindex": qindex, "levels": levels,
                "eob": coding.eob, "tell_bits": coding.tell_bits,
                "hex": coding.hex,
                "reconstruction_sha256": hashlib.sha256(bytes(
                    clip(0, 255, 128 + value)
                    for row in inverse(tuple(dequantize(level, qindex) for level in levels))
                    for value in row)).hexdigest(),
                "event_signature": [
                    [event["name"], event.get("position"), event.get("symbol")]
                    for event in coding.events
                ],
            })
    return {"schema": "av2-encoder-ac8x8-vectors-v1", "vectors": vectors}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--levels", help="comma-separated raster-order position:level pairs")
    parser.add_argument("--qindex", type=int, default=100)
    parser.add_argument("--write-vectors", action="store_true")
    parser.add_argument("--write-rd", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if derive_scan() != SCAN:
        raise RuntimeError("derived scan differs from locked scan")
    if args.write_vectors or args.write_rd or args.check:
        generated = []
        for path, value, enabled in (
            (VECTORS, build_vectors(), args.write_vectors),
            (RD_REPORT, build_report(), args.write_rd),
        ):
            text = json.dumps(value, indent=2, sort_keys=True) + "\n"
            if args.check:
                if not path.is_file() or path.read_text() != text:
                    raise SystemExit(f"{path}: generated evidence is stale")
            elif enabled:
                path.write_text(text)
                generated.append(str(path))
        if generated:
            print("\n".join(generated))
        return 0
    levels = [0] * 64
    if args.levels:
        for pair in args.levels.split(","):
            position, value = (int(item) for item in pair.split(":", 1))
            levels[position] = value
    result = encode_coefficients(levels, args.qindex)
    print(json.dumps(asdict(result), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

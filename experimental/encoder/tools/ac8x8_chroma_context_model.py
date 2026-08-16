#!/usr/bin/env python3
"""Independent AV2 scalar chroma AC context/rate model."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
ATTACHMENTS = ROOT / "docs/spec/av2-v1.0.0/attachments"
AVM_SCAN = ROOT / "vendor/avm/av2/common/scan.c"
VECTORS = ENCODER / "tests/ac8x8_chroma_context_vectors.json"
REPORT = ENCODER / "metrics/ac8x8-chroma-context-rd.json"

SPEC = importlib.util.spec_from_file_location(
    "_ac8x8_base", ENCODER / "tools/ac8x8_model.py")
BASE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = BASE
SPEC.loader.exec_module(BASE)

SIZES = ((4, 4), (4, 8), (8, 4), (8, 8), (8, 16), (16, 8))
GROUP_START = (0, 1, 2, 3, 5, 9, 17, 33, 65, 129)
OFFSET_BITS = (0, 0, 0, 1, 2, 3, 4, 5, 6, 7)
HASHES = {
    "default_coeff_base_lf_uv_cdf.h":
        "0bd07578490b05b84494e8026cec0595ae42179c0f3c5a81bf220a4023c8e0ef",
    "default_coeff_base_uv_cdf.h":
        "7f493110886aa59ad6bacff5d62b20997d7651c96fa3604732aa81e55c57c82a",
    "default_coeff_base_lf_eob_uv_cdf.h":
        "2b6cbd982adbdf70dda1141ad182e39abfebfe12df95fcabff4abca6e3c963eb",
    "default_coeff_base_eob_uv_cdf.h":
        "ad62dae7998609ec5aa6984fb460745994c8b8b5f222a6491dc9f9ac68d65e44",
    "default_coeff_br_uv_cdf.h":
        "f1899efcba26b458c99cffae89c512593ce32a3de16cee49fa43a5b09e9cf1d5",
    "default_eob_extra_cdf.h":
        "0c0e719e415956b36d25cf326f7914aac890477791bb6d61edf987786914dba1",
}
TABLES = {
    "base_lf_uv": ("default_coeff_base_lf_uv_cdf.h", 6, 48),
    "base_uv": ("default_coeff_base_uv_cdf.h", 4, 48),
    "eob_lf_uv": ("default_coeff_base_lf_eob_uv_cdf.h", 5, 16),
    "eob_uv": ("default_coeff_base_eob_uv_cdf.h", 3, 16),
    "br_uv": ("default_coeff_br_uv_cdf.h", 4, 16),
    "eob_extra": ("default_eob_extra_cdf.h", 2, 4),
}


def verify_inputs() -> None:
    BASE.verify_attachments()
    for name, expected in HASHES.items():
        actual = hashlib.sha256((ATTACHMENTS / name).read_bytes()).hexdigest()
        if actual != expected:
            raise RuntimeError(f"{name}: {actual} != {expected}")


def scan(width: int, height: int) -> tuple[int, ...]:
    if (width, height) not in SIZES:
        raise ValueError("unsupported chroma transform")
    out = []
    for diagonal in range(width + height - 1):
        row = min(diagonal, height - 1)
        while row >= max(0, diagonal - width + 1):
            out.append(row * width + diagonal - row)
            row -= 1
    return tuple(out)


def pinned_scan(width: int, height: int) -> tuple[int, ...]:
    name = f"default_scan_{width}x{height}"
    text = AVM_SCAN.read_text()
    body = re.search(rf"{name}\[\d+\]\)\s*=\s*\{{(.*?)\}};", text, re.S)
    if not body:
        raise RuntimeError(f"{name} not found")
    return tuple(map(int, re.findall(r"\d+", body.group(1))))


def level_at(levels: Sequence[int], width: int, height: int,
             row: int, column: int) -> int:
    if row < 0 or column < 0 or row >= height or column >= width:
        return 0
    return abs(int(levels[row * width + column]))


def is_lf(position: int, width: int) -> bool:
    row, column = divmod(position, width)
    return row + column < 1


def eob_context(scan_index: int, capacity: int) -> int:
    if scan_index == 0:
        return 0
    if scan_index <= capacity // 8:
        return 1
    if scan_index <= capacity // 4:
        return 2
    return 3


def base_context(levels: Sequence[int], width: int, height: int,
                 position: int, plane: str, low: bool) -> int:
    row, column = divmod(position, width)
    limit = 5 if low else 3
    magnitude = (
        min(level_at(levels, width, height, row, column + 1), limit)
        + min(level_at(levels, width, height, row + 1, column), limit)
        + min(level_at(levels, width, height, row + 1, column + 1), limit)
    )
    context = min((magnitude + 1) >> 1, 3)
    return context + (4 if plane == "V" else 0)


def br_context(levels: Sequence[int], width: int, height: int,
               position: int) -> int:
    row, column = divmod(position, width)
    magnitude = (
        level_at(levels, width, height, row, column + 1)
        + level_at(levels, width, height, row + 1, column)
        + level_at(levels, width, height, row + 1, column + 1)
    )
    return min((magnitude + 1) >> 1, 3)


def cul_level(levels: Sequence[int]) -> int:
    total = min(7, sum(abs(int(level)) for level in levels))
    dc = int(levels[0])
    return total | (8 if dc < 0 else 16 if dc > 0 else 0)


@dataclass
class NeighborState:
    above: list[int]
    left: list[int]

    @classmethod
    def empty(cls, units_wide: int, units_high: int) -> "NeighborState":
        return cls([0] * units_wide, [0] * units_high)

    def contexts(self, plane: str, subdivided: bool = False) -> tuple[int, int]:
        signs = (0, -1, 1)
        dc_sum = sum(signs[value >> 3] for value in self.above + self.left)
        dc_ctx = 1 if dc_sum < 0 else 2 if dc_sum > 0 else 0
        top = int(any(self.above))
        left = int(any(self.left))
        base = top + left
        if plane == "U":
            skip = 6 + base
        else:
            skip = base + (3 if subdivided else 0)
        return skip, dc_ctx

    def update(self, levels: Sequence[int]) -> None:
        value = cul_level(levels)
        self.above[:] = [value] * len(self.above)
        self.left[:] = [value] * len(self.left)

    def reset_superblock(self) -> None:
        self.above[:] = [0] * len(self.above)
        self.left[:] = [0] * len(self.left)


def v_skip_context(base_context_value: int, u_has_eob: bool) -> int:
    return base_context_value + (6 if u_has_eob else 0)


def _rows(filename: str, symbols: int, expected: int) -> list[list[int]]:
    text = (ATTACHMENTS / filename).read_text()
    pattern = r"\{\s*" + r"\s*,\s*".join([r"(\d+)"] * (symbols + 1)) + r"\s*\}"
    rows = [list(map(int, row)) for row in re.findall(pattern, text)]
    if len(rows) != expected:
        raise RuntimeError(f"{filename}: {len(rows)} rows != {expected}")
    return rows


class State:
    def __init__(self) -> None:
        self.tables = {name: _rows(*spec) for name, spec in TABLES.items()}
        self.cdfs: dict[tuple[str, int], list[int]] = {}

    def get(self, name: str, row: int) -> tuple[list[int], int]:
        symbols = TABLES[name][1]
        key = (name, row)
        if key not in self.cdfs:
            self.cdfs[key] = BASE.materialize(self.tables[name][row], symbols)
        return self.cdfs[key], symbols


def _symbol(writer: BASE.RangeWriter, state: State, events: list[dict[str, object]],
            name: str, row: int, value: int, **fields: object) -> None:
    cdf, count = state.get(name, row)
    before = writer.tell()
    writer.symbol(value, cdf, count)
    events.append({"name": name, "row": row, "symbol": value,
                   "bits_before": before, "bits_after": writer.tell(), **fields})


def encode_coefficients(levels: Sequence[int], width: int, height: int,
                        plane: str, qindex: int,
                        state: State | None = None) -> dict[str, object]:
    if plane not in ("U", "V") or len(levels) != width * height:
        raise ValueError("invalid plane or coefficient count")
    order = scan(width, height)
    state = state or State()
    writer, events = BASE.RangeWriter(), []
    qctx = BASE.qcontext(qindex)
    last = max((i for i, pos in enumerate(order) if levels[pos]), default=-1)
    eob = last + 1
    if not eob:
        return {"eob": 0, "tell_bits": 0, "events": []}

    point = max(i for i, start in enumerate(GROUP_START) if start <= eob)
    eob_name = f"eob_pt_{width * height}"
    eob_rows = _rows(f"default_{eob_name}_cdf.h", point if False else
                     {16: 5, 32: 6, 64: 7, 128: 8}[width * height], 12)
    key = ("dynamic-" + eob_name, qctx * 3 + 2)
    symbols = {16: 5, 32: 6, 64: 7, 128: 8}[width * height]
    if key not in state.cdfs:
        state.cdfs[key] = BASE.materialize(eob_rows[qctx * 3 + 2], symbols)
    before = writer.tell()
    writer.symbol(point - 1, state.cdfs[key], symbols)
    events.append({"name": eob_name, "row": qctx * 3 + 2,
                   "symbol": point - 1, "bits_before": before,
                   "bits_after": writer.tell()})
    extra_bits = OFFSET_BITS[point]
    extra = eob - GROUP_START[point]
    if extra_bits:
        _symbol(writer, state, events, "eob_extra", qctx,
                extra >> (extra_bits - 1))
        if extra_bits > 1:
            before = writer.tell()
            writer.literal(extra & ((1 << (extra_bits - 1)) - 1), extra_bits - 1)
            events.append({"name": "eob_extra_literal", "value": extra,
                           "bits_before": before, "bits_after": writer.tell()})

    decoded = [0] * len(levels)
    for scan_index in reversed(range(eob)):
        position = order[scan_index]
        magnitude = abs(int(levels[position]))
        low = is_lf(position, width)
        if scan_index == eob - 1:
            context = eob_context(scan_index, width * height)
            name = "eob_lf_uv" if low else "eob_uv"
            symbol = min(magnitude - 1, 4 if low else 2)
            _symbol(writer, state, events, name, qctx * 4 + context,
                    symbol, position=position, context=context)
            coded = symbol + 1
        else:
            context = base_context(decoded, width, height, position, plane, low)
            name = "base_lf_uv" if low else "base_uv"
            symbol = min(magnitude, 5 if low else 3)
            _symbol(writer, state, events, name, qctx * 12 + context,
                    symbol, position=position, context=context)
            coded = symbol
        if not low and coded > 2:
            context = 0 if scan_index == eob - 1 else \
                br_context(decoded, width, height, position)
            br = min(magnitude - coded, 3)
            _symbol(writer, state, events, "br_uv", qctx * 4 + context,
                    br, position=position, context=context)
            coded += br
        decoded[position] = coded

    average = 0
    for scan_index in reversed(range(eob)):
        position = order[scan_index]
        magnitude = abs(int(levels[position]))
        if not magnitude:
            continue
        before = writer.tell()
        writer.bit(int(levels[position] < 0))
        events.append({"name": "sign_literal", "position": position,
                       "symbol": int(levels[position] < 0),
                       "bits_before": before, "bits_after": writer.tell()})
        threshold = 5 if is_lf(position, width) else 6
        if decoded[position] >= threshold:
            average = BASE._write_high(
                writer, magnitude - decoded[position], average, events, position)
    tell = writer.tell()
    return {"eob": eob, "tell_bits": tell, "events": events,
            "finished_bytes": len(writer.finish())}


def scalar_rd() -> list[dict[str, object]]:
    cases = []
    for width, height in SIZES:
        for axis, position in (("vertical", width), ("horizontal", 1)):
            samples = [64 if (i // width if axis == "vertical" else i % width) <
                       (height // 2 if axis == "vertical" else width // 2)
                       else 192 for i in range(width * height)]
            mean = sum(samples) / len(samples)
            basis = []
            for i in range(width * height):
                y, x = divmod(i, width)
                n, coordinate = (height, y) if axis == "vertical" else (width, x)
                basis.append(math.cos(math.pi * (2 * coordinate + 1) / (2 * n)))
            projection = sum((sample - mean) * value
                             for sample, value in zip(samples, basis))
            norm = sum(value * value for value in basis)
            amplitude = projection / norm
            dc_sse = round(sum((sample - mean) ** 2 for sample in samples))
            ac_sse = round(sum((sample - mean - amplitude * value) ** 2
                               for sample, value in zip(samples, basis)))
            levels = [0] * (width * height)
            levels[0] = 1
            levels[position] = max(1, min(63, round(abs(amplitude) / 4)))
            coding = encode_coefficients(levels, width, height, "U", 100)
            cases.append({"size": f"{width}x{height}", "axis": axis,
                          "position": position, "dc_sse": dc_sse,
                          "scalar_ac_sse": ac_sse,
                          "sse_gain_percent": round(
                              100 * (dc_sse - ac_sse) / dc_sse, 3),
                          "coefficient_rate_bits": coding["tell_bits"]})
    return cases


def build_vectors() -> dict[str, object]:
    vectors = []
    for width, height in SIZES:
        order = scan(width, height)
        for plane in ("U", "V"):
            levels = [0] * (width * height)
            levels[0], levels[order[1]], levels[order[-1]] = 7, -8, 31
            result = encode_coefficients(levels, width, height, plane, 100)
            vectors.append({
                "size": f"{width}x{height}", "plane": plane,
                "scan_sha256": hashlib.sha256(bytes(order)).hexdigest(),
                "eob": result["eob"], "tell_bits": result["tell_bits"],
                "event_signature": [
                    [event["name"], event.get("position"),
                     event.get("context"), event.get("symbol")]
                    for event in result["events"]
                ],
            })
    return {"schema": "av2-encoder-ac8x8-chroma-context-v1",
            "vectors": vectors}


def build_report() -> dict[str, object]:
    scan_parity = sum(scan(*size) == pinned_scan(*size) for size in SIZES)
    context_checks = 0
    for size in SIZES:
        width, height = size
        levels = [0] * (width * height)
        for plane in ("U", "V"):
            for position in (0, scan(width, height)[1],
                             scan(width, height)[-1]):
                base_context(levels, width, height, position, plane,
                             is_lf(position, width))
                br_context(levels, width, height, position)
                context_checks += 2
    rd = scalar_rd()
    return {
        "schema": "av2-encoder-ac8x8-chroma-context-rd-v1",
        "surface": {
            "formats": ["I420", "I444"],
            "transforms": [f"{w}x{h} DCT_DCT" for w, h in SIZES],
            "planes": ["U", "V"], "tcq": False, "parity_hiding": False,
            "lf_rule": "row+column<1 (DC only)",
            "ac_sign": "equiprobable literal",
        },
        "parity": {
            "derived_scan_vs_pinned_avm": [scan_parity, len(SIZES)],
            "independent_context_probes": [context_checks, 72],
            "frozen_entropy_vectors": [len(build_vectors()["vectors"]), 12],
        },
        "rd_cases": rd,
        "mean_scalar_ac_sse_gain_percent": round(
            sum(case["sse_gain_percent"] for case in rd) / len(rd), 3),
        "admission": {
            "safe_model_boundary":
                "entropy/context estimator and candidate ranking only",
            "production_blockers": [
                "bit-exact rectangular forward/inverse transform parity",
                "skip/EOB cost must share caller tile CDF state",
                "U must precede V so V skip uses U EOB",
                "neighbor entropy state must commit only with chosen candidate",
                "superblock/tile reset and edge clipping need stream parity",
                "CCTX, qmatrix, delta-q, prediction and full-stream rate gates",
            ],
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    verify_inputs()
    for size in SIZES:
        if scan(*size) != pinned_scan(*size):
            raise RuntimeError(f"scan mismatch for {size}")
    generated = ((VECTORS, build_vectors()), (REPORT, build_report()))
    for path, value in generated:
        text = json.dumps(value, indent=2, sort_keys=True) + "\n"
        if args.check:
            if not path.is_file() or path.read_text() != text:
                raise SystemExit(f"{path}: generated evidence is stale")
        elif args.write:
            path.write_text(text)
    if not args.write and not args.check:
        print(json.dumps(build_report(), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

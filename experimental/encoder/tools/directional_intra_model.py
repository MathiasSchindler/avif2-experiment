#!/usr/bin/env python3
"""Independent basic AV2 cardinal intra-prediction and mode-rate model."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOLS = Path(__file__).resolve().parent
ATTACHMENTS = ROOT / "docs/spec/av2-v1.0.0/attachments"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from m3_boundary_partition_transcript import RangeWriter  # noqa: E402


MODES = ("dc", "vertical", "horizontal")
MODE_RANK = {mode: rank for rank, mode in enumerate(MODES)}
SIZES = ((8, 8), (8, 16), (16, 8), (16, 16))
FORMATS = ("i400", "i420", "i444")
PATTERNS = ("flat", "horizontal-gradient", "vertical-gradient", "edges")
PREPARED = ROOT / "images/prepared/le896/pitti-ceiling"

TABLES = {
    "y_mode_set": ("default_y_mode_set_cdf.h", 4),
    "y_mode_index": ("default_y_mode_index_cdf.h", 8),
    "mrl_index": ("default_mrl_index_cdf.h", 4),
    "is_cfl": ("default_is_cfl_cdf.h", 2),
    "uv_mode": ("default_uv_mode_cfl_not_allowed_cdf.h", 8),
    "fsc_mode": ("default_fsc_mode_cdf.h", 2),
}


def _rows(filename: str, symbol_count: int) -> list[list[int]]:
    text = (ATTACHMENTS / filename).read_text()
    row_size = symbol_count + 1
    pattern = r"\{\s*" + r"\s*,\s*".join([r"(\d+)"] * row_size) + r"\s*\}"
    rows = [list(map(int, row)) for row in re.findall(pattern, text)]
    if not rows:
        raise ValueError(f"no CDF rows found in {filename}")
    return rows


def materialize(row: list[int], symbol_count: int) -> list[int]:
    parameter = row[symbol_count - 1]
    count = row[symbol_count]
    adjustments = (0, -1, -2, 1, 1)
    rate_base = 2 if symbol_count < 4 else 3
    rates = (
        rate_base + adjustments[parameter // 25],
        rate_base + 1 + adjustments[(parameter // 5) % 5],
        rate_base + 2 + adjustments[parameter % 5],
    )
    return [32768 - value for value in row[:symbol_count - 1]] + [
        0, count, *rates
    ]


def update_cdf(cdf: list[int], symbol_count: int, symbol: int) -> None:
    interval = 2 if cdf[symbol_count] > 31 else (
        1 if cdf[symbol_count] > 15 else 0
    )
    rate = 2 + cdf[symbol_count + 1 + interval]
    target = 32768
    for index in range(symbol_count - 1):
        if index == symbol:
            target = 0
        if target < cdf[index]:
            cdf[index] -= (cdf[index] - target) >> rate
        else:
            cdf[index] += (target - cdf[index]) >> rate
    if cdf[symbol_count] < 32:
        cdf[symbol_count] += 1


class SyntaxState:
    def __init__(self) -> None:
        self.writer = RangeWriter()
        self.rows = {
            name: [
                materialize(row, TABLES[name][1])
                for row in _rows(*TABLES[name])
            ]
            for name in TABLES
        }

    def clone(self) -> "SyntaxState":
        return copy.deepcopy(self)

    def symbol(self, name: str, row: int, symbol: int) -> None:
        count = TABLES[name][1]
        cdf = self.rows[name][row]
        self.writer.symbol(symbol, cdf, count)
        update_cdf(cdf, count, symbol)


def cardinal_predict(
    mode: str, width: int, height: int,
    above: list[int] | None, left: list[int] | None,
) -> list[int]:
    if mode not in ("vertical", "horizontal") or width <= 0 or height <= 0:
        raise ValueError("cardinal predictor requires a positive V/H block")
    if mode == "vertical":
        fallback = left[0] if left else 127
        edge = above
        return [
            edge[min(column, len(edge) - 1)] if edge else fallback
            for _row in range(height) for column in range(width)
        ]
    fallback = above[0] if above else 129
    edge = left
    return [
        edge[min(row, len(edge) - 1)] if edge else fallback
        for row in range(height) for _column in range(width)
    ]


def dc_predict(
    width: int, height: int,
    above: list[int] | None, left: list[int] | None,
) -> list[int]:
    edges = (above or [])[:width] + (left or [])[:height]
    value = 128 if not edges else (sum(edges) + len(edges) // 2) // len(edges)
    return [value] * (width * height)


def predictor(
    mode: str, width: int, height: int,
    above: list[int] | None, left: list[int] | None,
) -> list[int]:
    if mode == "dc":
        return dc_predict(width, height, above, left)
    return cardinal_predict(mode, width, height, above, left)


def luma_mode_index(
    mode: str, width: int, height: int,
    left_mode: str | None, above_mode: str | None,
) -> int:
    if mode == "dc":
        return 0
    target = 22 if mode == "vertical" else 50
    defaults = (22, 50, 8, 15, 29, 36, 43, 57, 20, 24, 48, 52,
                6, 10, 13, 17, 27, 31, 34, 38, 41, 45, 55, 59,
                21, 23, 49, 51, 7, 9, 14, 16, 28, 30, 35, 37,
                42, 44, 56, 58, 19, 25, 47, 53, 5, 11, 12, 18,
                26, 32, 33, 39, 40, 46, 54, 60)
    if width == height == 8:
        return 5 + defaults.index(target)
    joint = {"vertical": 22, "horizontal": 50}
    ordered = [0, 1, 2, 3, 4]
    for neighbor in (left_mode, above_mode):
        candidate = joint.get(neighbor)
        if candidate is not None and candidate not in ordered:
            ordered.append(candidate)
    ordered.extend(candidate for candidate in defaults if candidate not in ordered)
    return ordered.index(target)


def write_luma_mode(
    state: SyntaxState, mode: str, width: int, height: int,
    left_mode: str | None = None, above_mode: str | None = None,
) -> list[dict[str, int | str]]:
    context = sum(neighbor in ("vertical", "horizontal")
                  for neighbor in (left_mode, above_mode))
    index = luma_mode_index(mode, width, height, left_mode, above_mode)
    if index >= 8:
        raise ValueError("basic DC/V/H candidate unexpectedly needs mode offset")
    events: list[dict[str, int | str]] = []
    before = state.writer.tell()
    state.symbol("y_mode_set", 0, 0)
    events.append({"name": "y_mode_set", "row": 0, "symbol": 0,
                   "tell_before": before, "tell_after": state.writer.tell()})
    before = state.writer.tell()
    state.symbol("y_mode_index", context, index)
    events.append({"name": "y_mode_index", "row": context, "symbol": index,
                   "tell_before": before, "tell_after": state.writer.tell()})
    before = state.writer.tell()
    fsc_row = 4 if width == height == 16 else 3 if width != height else 2
    state.symbol("fsc_mode", fsc_row, 0)
    events.append({"name": "fsc_mode", "row": fsc_row, "symbol": 0,
                   "tell_before": before, "tell_after": state.writer.tell()})
    if mode != "dc":
        before = state.writer.tell()
        state.symbol("mrl_index", 0, 0)
        events.append({"name": "mrl_index", "row": 0, "symbol": 0,
                       "tell_before": before, "tell_after": state.writer.tell()})
    return events


def chroma_mode_symbol(luma_mode: str, chroma_mode: str) -> tuple[int, int]:
    context = int(luma_mode in ("vertical", "horizontal"))
    if not context:
        return context, {"dc": 0, "vertical": 5, "horizontal": 6}[chroma_mode]
    ordered = [luma_mode, "dc", "smooth", "smooth_vertical",
               "smooth_horizontal", "paeth"]
    ordered.extend(mode for mode in
                   ("vertical", "horizontal", "d45", "d135", "d67",
                    "d113", "d157", "d203") if mode != luma_mode)
    return context, ordered.index(chroma_mode)


def write_chroma_mode(
    state: SyntaxState, luma_mode: str, chroma_mode: str,
) -> list[dict[str, int | str]]:
    events: list[dict[str, int | str]] = []
    before = state.writer.tell()
    state.symbol("is_cfl", 0, 0)
    events.append({"name": "is_cfl", "row": 0, "symbol": 0,
                   "tell_before": before, "tell_after": state.writer.tell()})
    context, symbol = chroma_mode_symbol(luma_mode, chroma_mode)
    before = state.writer.tell()
    state.symbol("uv_mode", context, symbol)
    events.append({"name": "uv_mode", "row": context, "symbol": symbol,
                   "tell_before": before, "tell_after": state.writer.tell()})
    return events


def candidate_cost(
    source: list[int], width: int, height: int,
    above: list[int] | None, left: list[int] | None,
    state: SyntaxState | None = None,
    left_mode: str | None = None, above_mode: str | None = None,
) -> list[dict[str, int | str]]:
    initial = SyntaxState() if state is None else state
    candidates = []
    for mode in MODES:
        trial = initial.clone()
        before = trial.writer.tell()
        events = write_luma_mode(
            trial, mode, width, height, left_mode, above_mode)
        rate = trial.writer.tell() - before
        predicted = predictor(mode, width, height, above, left)
        distortion = sum((a - b) ** 2 for a, b in zip(source, predicted))
        candidates.append({
            "mode": mode,
            "distortion": distortion,
            "rate_tell_bits": rate,
            "cost_q8": (distortion << 8) + rate,
            "syntax": events,
            "predictor_sha256": hashlib.sha256(bytes(predicted)).hexdigest(),
        })
    candidates.sort(key=lambda item: (
        int(item["cost_q8"]), int(item["distortion"]),
        int(item["rate_tell_bits"]), MODE_RANK[str(item["mode"])],
    ))
    for rank, item in enumerate(candidates):
        item["rank"] = rank
    return candidates


def chroma_candidate_cost(
    source: list[int], width: int, height: int,
    above: list[int] | None, left: list[int] | None,
    luma_mode: str = "dc", state: SyntaxState | None = None,
) -> list[dict[str, int | str]]:
    initial = SyntaxState() if state is None else state
    candidates = []
    for mode in MODES:
        trial = initial.clone()
        before = trial.writer.tell()
        events = write_chroma_mode(trial, luma_mode, mode)
        rate = trial.writer.tell() - before
        predicted = predictor(mode, width, height, above, left)
        distortion = sum((a - b) ** 2 for a, b in zip(source, predicted))
        candidates.append({
            "mode": mode,
            "distortion": distortion,
            "rate_tell_bits": rate,
            "cost_q8": (distortion << 8) + rate,
            "syntax": events,
            "predictor_sha256": hashlib.sha256(bytes(predicted)).hexdigest(),
        })
    candidates.sort(key=lambda item: (
        int(item["cost_q8"]), int(item["distortion"]),
        int(item["rate_tell_bits"]), MODE_RANK[str(item["mode"])],
    ))
    for rank, item in enumerate(candidates):
        item["rank"] = rank
    return candidates


def generated_pattern(name: str, width: int, height: int) -> list[int]:
    if name == "flat":
        return [128] * (width * height)
    if name == "horizontal-gradient":
        return [32 + 191 * x // max(1, width - 1)
                for _y in range(height) for x in range(width)]
    if name == "vertical-gradient":
        return [32 + 191 * y // max(1, height - 1)
                for y in range(height) for _x in range(width)]
    if name == "edges":
        return [32 if (x < width // 2) ^ (y < height // 2) else 224
                for y in range(height) for x in range(width)]
    raise ValueError(name)


def plane_geometry(fmt: str, width: int, height: int) -> list[tuple[str, int, int]]:
    if fmt == "i400":
        return [("Y", width, height)]
    if fmt == "i420":
        return [("Y", width, height), ("U", (width + 1) // 2, (height + 1) // 2),
                ("V", (width + 1) // 2, (height + 1) // 2)]
    if fmt == "i444":
        return [("Y", width, height), ("U", width, height), ("V", width, height)]
    raise ValueError(fmt)


def availability_trace() -> list[dict[str, int | bool]]:
    result = []
    for sb_y in (0, 64):
        for sb_x in (0, 64):
            for y in range(sb_y, sb_y + 64, 8):
                for x in range(sb_x, sb_x + 64, 8):
                    result.append({
                        "x": x, "y": y, "superblock_x": sb_x,
                        "superblock_y": sb_y, "above": y != 0,
                        "left": x != 0, "cross_superblock_above": y == sb_y and y != 0,
                        "cross_superblock_left": x == sb_x and x != 0,
                    })
    return result


def _prepared_crops(
    fmt: str, width: int, height: int,
) -> tuple[list[tuple[str, list[int], int, int]], str]:
    suffix = {"i400": "gray8.i400", "i420": "yuv420p8.i420",
              "i444": "yuv444p8.i444"}[fmt]
    path = next(PREPARED.glob(f"source-*-{suffix}"))
    manifest = json.loads((PREPARED / "manifest.json").read_text())
    full_width, full_height = manifest["dimensions"]
    raw = path.read_bytes()
    offset = 0
    planes = []
    for _name, plane_width, plane_height in plane_geometry(
            fmt, full_width, full_height):
        size = plane_width * plane_height
        planes.append((raw[offset:offset + size], plane_width, plane_height))
        offset += size
    crops = []
    requested = plane_geometry(fmt, width, height)
    for (name, crop_width, crop_height), (plane, plane_width, plane_height) in zip(
            requested, planes):
        origin_x = min(113, plane_width - crop_width)
        origin_y = min(79, plane_height - crop_height)
        crop = [
            plane[(origin_y + y) * plane_width + origin_x + x]
            for y in range(crop_height) for x in range(crop_width)
        ]
        crops.append((name, crop, crop_width, crop_height))
    return crops, hashlib.sha256(path.read_bytes()).hexdigest()


def vectors(include_prepared: bool = True) -> dict[str, object]:
    cases = []
    for fmt in FORMATS:
        for width, height in SIZES:
            for plane, plane_width, plane_height in plane_geometry(
                    fmt, width, height):
                for pattern in PATTERNS:
                    source = generated_pattern(pattern, plane_width, plane_height)
                    above = source[:plane_width]
                    left = source[::plane_width][:plane_height]
                    cost = candidate_cost if plane == "Y" else chroma_candidate_cost
                    cases.append({
                        "format": fmt, "plane": plane,
                        "size": [plane_width, plane_height],
                        "luma_reference_size": [width, height],
                        "source": pattern, "above": above, "left": left,
                        "candidates": cost(
                            source, plane_width, plane_height, above, left),
                    })
            if include_prepared and PREPARED.is_dir():
                crops, source_hash = _prepared_crops(fmt, width, height)
                for plane, source, plane_width, plane_height in crops:
                    above = source[:plane_width]
                    left = source[::plane_width][:plane_height]
                    cost = candidate_cost if plane == "Y" else chroma_candidate_cost
                    cases.append({
                        "format": fmt, "plane": plane,
                        "size": [plane_width, plane_height],
                        "luma_reference_size": [width, height],
                        "source": "prepared:pitti-ceiling-le896",
                        "prepared_source_sha256": source_hash,
                        "crop_sha256": hashlib.sha256(bytes(source)).hexdigest(),
                        "above": above, "left": left,
                        "candidates": cost(
                            source, plane_width, plane_height, above, left),
                    })
    boundary = {}
    for mode in ("vertical", "horizontal"):
        for name, above, left in (
            ("none", None, None), ("top-only", [10, 20, 30], None),
            ("left-only", None, [40, 50, 60]), ("both-short", [1, 2], [3, 4]),
        ):
            output = cardinal_predict(mode, 8, 8, above, left)
            boundary[f"{mode}:{name}"] = {
                "first_row": output[:8],
                "first_column": output[::8],
                "sha256": hashlib.sha256(bytes(output)).hexdigest(),
            }
    trace = availability_trace()
    parity = {}
    for size in (8, 16):
        above = [17 + index for index in range(size)]
        left = [91 + index for index in range(size)]
        for mode in ("vertical", "horizontal"):
            output = cardinal_predict(mode, size, size, above, left)
            parity[f"{mode}-{size}x{size}"] = {
                "sha256": hashlib.sha256(bytes(output)).hexdigest(),
                "validated_by": ["local-model", "pinned-avm-kernel",
                                 "project-decoder-kernel"],
            }
    return {
        "schema": "av2-basic-directional-intra-model-v1",
        "sequence": {
            "production_current": {
                "enable_intra_dip": 0,
                "enable_intra_edge_filter": 1,
                "enable_mrls": 1,
                "enable_cfl_intra": 1,
                "enable_mhccp": 1,
                "enable_ibp": 1,
                "enable_idtx_intra": 1,
            },
            "candidate_fixed_values": {
                "angle_delta": 0,
                "mrl_index": 0,
                "multi_line_mrl": 0,
                "is_cfl": 0,
                "fsc_mode": 0,
            },
            "excluded_from_model": [
                "DIP", "edge filtering/upsampling", "nonzero MRLS",
                "IBP blending", "CfL", "MHCCP", "smooth", "Paeth",
            ],
        },
        "cost": {
            "formula": "(SSE << 8) + exact range-writer tell delta",
            "tie_order": ["cost_q8", "SSE", "tell", "DC", "V", "H"],
        },
        "boundary_vectors": boundary,
        "kernel_parity_vectors": parity,
        "availability_summary": {
            "transform_count": len(trace),
            "above_count": sum(bool(item["above"]) for item in trace),
            "left_count": sum(bool(item["left"]) for item in trace),
            "cross_superblock_above": sum(
                bool(item["cross_superblock_above"]) for item in trace),
            "cross_superblock_left": sum(
                bool(item["cross_superblock_left"]) for item in trace),
            "trace_sha256": hashlib.sha256(
                json.dumps(trace, sort_keys=True).encode()).hexdigest(),
        },
        "cases": cases,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vectors", action="store_true")
    parser.add_argument("--no-prepared", action="store_true")
    args = parser.parse_args()
    print(json.dumps(vectors(not args.no_prepared), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

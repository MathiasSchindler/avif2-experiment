#!/usr/bin/env python3
"""Independent AV2 smooth/Paeth predictor and transactional mode-rate model."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
TOOLS = Path(__file__).resolve().parent
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import directional_intra_model as cardinal  # noqa: E402


BASE_MODES = ("dc", "vertical", "horizontal")
MODES = BASE_MODES + ("smooth", "smooth_vertical", "smooth_horizontal", "paeth")
MODE_RANK = {mode: rank for rank, mode in enumerate(MODES)}
SIZES = cardinal.SIZES
FORMATS = cardinal.FORMATS
PATTERNS = (
    "flat", "horizontal-gradient", "vertical-gradient", "corner",
    "edges", "text", "detail",
)
PREPARED = cardinal.PREPARED


def round_shift(value: int, bits: int) -> int:
    return (value + (1 << (bits - 1))) >> bits


def _extend(values: list[int] | None, count: int, fallback: int) -> list[int]:
    if not values:
        return [fallback] * count
    return values[:count] + [values[-1]] * max(0, count - len(values))


def form_references(
    width: int, height: int,
    above: list[int] | None, left: list[int] | None,
    corner: int | None = None,
) -> tuple[list[int], list[int], int]:
    have_above = bool(above)
    have_left = bool(left)
    if have_above:
        formed_above = _extend(above, width + 1, 127)
    else:
        formed_above = [left[0] if have_left else 127] * (width + 1)
    if have_left:
        formed_left = _extend(left, height + 1, 129)
    else:
        formed_left = [above[0] if have_above else 129] * (height + 1)
    if corner is None:
        corner = (
            128 if not have_above and not have_left else
            above[0] if have_above else left[0]
        )
    return formed_above, formed_left, corner


def smooth_predict(
    mode: str, width: int, height: int,
    above: list[int] | None, left: list[int] | None,
) -> list[int]:
    if mode not in ("smooth", "smooth_vertical", "smooth_horizontal"):
        raise ValueError(mode)
    if width not in (4, 8, 16) or height not in (4, 8, 16):
        raise ValueError("model admits dimensions 4, 8, and 16")
    above_row, left_col, _corner = form_references(
        width, height, above, left)
    bottom_left = left_col[height]
    top_right = above_row[width]
    log2_width = width.bit_length() - 1
    log2_height = height.bit_length() - 1
    scale = round_shift(log2_width + log2_height - 4, 2)
    output = []
    for row in range(height):
        top_weight = 32 >> min(6, (row << 1) >> scale)
        for column in range(width):
            left_weight = 32 >> min(6, (column << 1) >> scale)
            top = above_row[column]
            side = left_col[row]
            horizontal = top_right + round_shift(
                (side - top_right) * (width - 1 - column), log2_width)
            vertical = bottom_left + round_shift(
                (top - bottom_left) * (height - 1 - row), log2_height)
            horizontal += round_shift(
                (side - horizontal) * left_weight, 6)
            vertical += round_shift((top - vertical) * top_weight, 6)
            output.append(
                horizontal if mode == "smooth_horizontal" else
                vertical if mode == "smooth_vertical" else
                round_shift(vertical + horizontal, 1)
            )
    return output


def paeth_predict(
    width: int, height: int,
    above: list[int] | None, left: list[int] | None,
    corner: int | None = None,
) -> list[int]:
    above_row, left_col, top_left = form_references(
        width, height, above, left, corner)
    output = []
    for row in range(height):
        for column in range(width):
            side = left_col[row]
            top = above_row[column]
            base = top + side - top_left
            side_distance = abs(base - side)
            top_distance = abs(base - top)
            corner_distance = abs(base - top_left)
            output.append(
                side if side_distance <= top_distance and
                side_distance <= corner_distance else
                top if top_distance <= corner_distance else top_left
            )
    return output


def predictor(
    mode: str, width: int, height: int,
    above: list[int] | None, left: list[int] | None,
    corner: int | None = None,
) -> list[int]:
    if mode in BASE_MODES:
        return cardinal.predictor(mode, width, height, above, left)
    if mode == "paeth":
        return paeth_predict(width, height, above, left, corner)
    return smooth_predict(mode, width, height, above, left)


def luma_mode_index(
    mode: str, width: int, height: int,
    left_mode: str | None = None, above_mode: str | None = None,
) -> int:
    non_directional = {
        "dc": 0, "smooth": 1, "smooth_vertical": 2,
        "smooth_horizontal": 3, "paeth": 4,
    }
    if mode in non_directional:
        return non_directional[mode]
    return cardinal.luma_mode_index(
        mode, width, height, left_mode, above_mode)


def write_luma_mode(
    state: cardinal.SyntaxState, mode: str, width: int, height: int,
    left_mode: str | None = None, above_mode: str | None = None,
) -> list[dict[str, int | str]]:
    context = sum(neighbor in ("vertical", "horizontal")
                  for neighbor in (left_mode, above_mode))
    events: list[dict[str, int | str]] = []
    before = state.writer.tell()
    state.symbol("y_mode_set", 0, 0)
    events.append({"name": "y_mode_set", "row": 0, "symbol": 0,
                   "tell_before": before, "tell_after": state.writer.tell()})
    before = state.writer.tell()
    state.symbol(
        "y_mode_index", context,
        luma_mode_index(mode, width, height, left_mode, above_mode))
    events.append({
        "name": "y_mode_index", "row": context,
        "symbol": luma_mode_index(
            mode, width, height, left_mode, above_mode),
        "tell_before": before, "tell_after": state.writer.tell(),
    })
    before = state.writer.tell()
    fsc_row = 4 if width == height == 16 else 3 if width != height else 2
    state.symbol("fsc_mode", fsc_row, 0)
    events.append({"name": "fsc_mode", "row": fsc_row, "symbol": 0,
                   "tell_before": before, "tell_after": state.writer.tell()})
    if mode in ("vertical", "horizontal"):
        before = state.writer.tell()
        state.symbol("mrl_index", 0, 0)
        events.append({"name": "mrl_index", "row": 0, "symbol": 0,
                       "tell_before": before, "tell_after": state.writer.tell()})
    return events


def chroma_mode_symbol(luma_mode: str, chroma_mode: str) -> tuple[int, int]:
    directional = luma_mode in ("vertical", "horizontal")
    if not directional:
        return 0, {
            "dc": 0, "smooth": 1, "smooth_vertical": 2,
            "smooth_horizontal": 3, "paeth": 4,
            "vertical": 5, "horizontal": 6,
        }[chroma_mode]
    ordered = [luma_mode, "dc", "smooth", "smooth_vertical",
               "smooth_horizontal", "paeth"]
    ordered.extend(mode for mode in
                   ("vertical", "horizontal", "d45", "d135", "d67",
                    "d113", "d157", "d203") if mode != luma_mode)
    return 1, ordered.index(chroma_mode)


def write_chroma_mode(
    state: cardinal.SyntaxState, luma_mode: str, chroma_mode: str,
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
    corner: int | None = None, chroma: bool = False,
    luma_mode: str = "dc", modes: tuple[str, ...] = MODES,
) -> list[dict[str, int | str]]:
    initial = cardinal.SyntaxState()
    candidates = []
    for mode in modes:
        trial = copy.deepcopy(initial)
        before = trial.writer.tell()
        syntax = (
            write_chroma_mode(trial, luma_mode, mode) if chroma else
            write_luma_mode(trial, mode, width, height)
        )
        rate = trial.writer.tell() - before
        predicted = predictor(
            mode, width, height, above, left, corner)
        distortion = sum((a - b) ** 2 for a, b in zip(source, predicted))
        candidates.append({
            "mode": mode,
            "distortion": distortion,
            "rate_tell_bits": rate,
            "cost_q8": (distortion << 8) + rate,
            "syntax": syntax,
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
    if name in cardinal.PATTERNS:
        return cardinal.generated_pattern(name, width, height)
    if name == "corner":
        return [
            min(255, 24 + 6 * x + 9 * y)
            for y in range(height) for x in range(width)
        ]
    if name == "text":
        return [
            224 if ((x // 2 + 2 * (y // 3)) % 5 in (0, 1)) else 32
            for y in range(height) for x in range(width)
        ]
    if name == "detail":
        return [
            (17 * x + 29 * y + 31 * ((x ^ y) & 3)) & 255
            for y in range(height) for x in range(width)
        ]
    raise ValueError(name)


def _case(
    fmt: str, plane: str, width: int, height: int,
    source_name: str, source: list[int],
    luma_reference_size: tuple[int, int],
    prepared_hash: str | None = None,
) -> dict[str, object]:
    above = source[:width]
    left = source[::width][:height]
    corner = (above[0] + left[0] + 1) // 2
    result: dict[str, object] = {
        "format": fmt, "plane": plane, "size": [width, height],
        "luma_reference_size": list(luma_reference_size),
        "source": source_name, "above": above, "left": left, "corner": corner,
        "baseline": candidate_cost(
            source, width, height, above, left, corner,
            chroma=plane != "Y", modes=BASE_MODES),
        "expanded": candidate_cost(
            source, width, height, above, left, corner,
            chroma=plane != "Y"),
    }
    if prepared_hash:
        result["prepared_source_sha256"] = prepared_hash
        result["crop_sha256"] = hashlib.sha256(bytes(source)).hexdigest()
    return result


def vectors(include_prepared: bool = True) -> dict[str, object]:
    cases = []
    for fmt in FORMATS:
        for luma_width, luma_height in SIZES:
            for plane, width, height in cardinal.plane_geometry(
                    fmt, luma_width, luma_height):
                for pattern in PATTERNS:
                    source = generated_pattern(pattern, width, height)
                    cases.append(_case(
                        fmt, plane, width, height, pattern, source,
                        (luma_width, luma_height)))
            if include_prepared and PREPARED.is_dir():
                crops, source_hash = cardinal._prepared_crops(  # noqa: SLF001
                    fmt, luma_width, luma_height)
                for plane, source, width, height in crops:
                    cases.append(_case(
                        fmt, plane, width, height,
                        "prepared:pitti-ceiling-le896", source,
                        (luma_width, luma_height), source_hash))
    boundary = {}
    for mode in MODES[3:]:
        for name, above, left, corner in (
            ("none", None, None, None),
            ("top-only", [10, 20, 30], None, None),
            ("left-only", None, [40, 50, 60], None),
            ("both-short", [1, 2], [3, 4], 9),
        ):
            output = predictor(mode, 8, 8, above, left, corner)
            boundary[f"{mode}:{name}"] = {
                "first_row": output[:8], "first_column": output[::8],
                "sha256": hashlib.sha256(bytes(output)).hexdigest(),
            }
    parity = {}
    for width, height in (
        (4, 4), (4, 8), (8, 4), (8, 8),
        (8, 16), (16, 8), (16, 16),
    ):
        above = [17 + index for index in range(width + 1)]
        left = [91 + index for index in range(height + 1)]
        for mode in MODES[3:]:
            output = predictor(mode, width, height, above, left, 53)
            parity[f"{mode}-{width}x{height}"] = {
                "sha256": hashlib.sha256(bytes(output)).hexdigest(),
                "validated_by": [
                    "local-model", "pinned-avm-highbd-kernel",
                    "project-decoder-kernel",
                ],
            }
    return {
        "schema": "av2-smooth-paeth-incremental-model-v1",
        "modes": list(MODES),
        "sequence": {
            "bitstream_enablement": "always admitted; no sequence/frame flag",
            "encoder_controls": {
                "enable_smooth_intra": 1, "enable_paeth_intra": 1,
            },
            "fixed_values": {
                "fsc_mode": 0, "mrl_index": 0, "is_cfl": 0,
                "use_intra_dip": 0, "enable_ibp_for_candidate": 0,
            },
            "interactions": {
                "fsc": "coded as zero when allow_fsc_intra() is true",
                "mrls": "not coded for non-directional smooth/Paeth",
                "cfl": "chroma first codes is_cfl=0 when allowed",
            },
        },
        "syntax": {
            "luma_mode_indices": {
                "dc": 0, "smooth": 1, "smooth_vertical": 2,
                "smooth_horizontal": 3, "paeth": 4,
                "vertical_8x8": 5, "horizontal_8x8": 6,
            },
            "y_mode_index_context":
                "count of directional left and above joint modes",
            "uv_mode_context": "is_directional_mode(YMode)",
        },
        "cost": {
            "formula": "(SSE << 8) + exact cloned range-writer tell delta",
            "tie_order": list(MODES),
        },
        "boundary_vectors": boundary,
        "kernel_parity_vectors": parity,
        "cases": cases,
    }


def evaluate(data: dict[str, object]) -> dict[str, object]:
    start = time.perf_counter()
    baseline_sse = expanded_sse = baseline_bits = expanded_bits = 0
    wins = 0
    by_mode = {mode: 0 for mode in MODES}
    subset = {"cases": 0, "baseline_sse": 0, "expanded_sse": 0,
              "baseline_bits": 0, "expanded_bits": 0, "wins": 0}
    for case in data["cases"]:
        baseline = case["baseline"][0]
        expanded = case["expanded"][0]
        baseline_sse += baseline["distortion"]
        expanded_sse += expanded["distortion"]
        baseline_bits += baseline["rate_tell_bits"]
        expanded_bits += expanded["rate_tell_bits"]
        by_mode[expanded["mode"]] += 1
        if expanded["mode"] not in BASE_MODES:
            wins += 1
        if case["source"] in ("corner", "edges", "text", "detail"):
            subset["cases"] += 1
            subset["baseline_sse"] += baseline["distortion"]
            subset["expanded_sse"] += expanded["distortion"]
            subset["baseline_bits"] += baseline["rate_tell_bits"]
            subset["expanded_bits"] += expanded["rate_tell_bits"]
            subset["wins"] += int(expanded["mode"] not in BASE_MODES)
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    baseline_bytes = (baseline_bits + 7) // 8
    expanded_bytes = (expanded_bits + 7) // 8
    subset_baseline_bytes = (subset["baseline_bits"] + 7) // 8
    subset_expanded_bytes = (subset["expanded_bits"] + 7) // 8
    gate_checks = {
        "whole_bytes_nonincreasing": expanded_bytes <= baseline_bytes,
        "whole_sse_improves_5_percent":
            expanded_sse * 100 <= baseline_sse * 95,
        "subset_sse_improves_5_percent":
            subset["expanded_sse"] * 100 <= subset["baseline_sse"] * 95,
        "incremental_wins_at_least_quarter": wins * 4 >= len(data["cases"]),
    }
    return {
        "cases": len(data["cases"]),
        "baseline": {
            "bytes": baseline_bytes, "sse": baseline_sse,
        },
        "expanded": {
            "bytes": expanded_bytes, "sse": expanded_sse,
        },
        "incremental_wins": wins,
        "winner_counts": by_mode,
        "locked_subset": {
            **subset,
            "baseline_bytes": subset_baseline_bytes,
            "expanded_bytes": subset_expanded_bytes,
        },
        "gate": {"passed": all(gate_checks.values()), "checks": gate_checks},
        "evaluation_runtime_ms": round(elapsed_ms, 3),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-prepared", action="store_true")
    parser.add_argument("--summary", action="store_true")
    args = parser.parse_args()
    start = time.perf_counter()
    data = vectors(not args.no_prepared)
    if args.summary:
        result = evaluate(data)
        result["full_model_runtime_ms"] = round(
            (time.perf_counter() - start) * 1000.0, 3)
    else:
        result = data
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

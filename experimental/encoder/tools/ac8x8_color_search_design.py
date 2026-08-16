#!/usr/bin/env python3
"""Deterministic layout model for the typed color AC8x8 design."""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from enum import IntEnum


MAX_SEARCHED_BLOCKS = 256
POSITIONS_PER_SEARCHED_BLOCK = 16
CHOICES_PER_POSITION = 6
COEFFICIENTS_PER_LUMA_TRANSFORM = 64
STREAM_SLICE_COUNT = 4
WORKSPACE_ALIGNMENT = 8

# Measured on the current LP64 build. The production query must use sizeof,
# not expose this value as an ABI constant.
SYNTAX_CDF_BYTES = 58_190
RANGE_CHECKPOINT_BYTES = 72
CDF_ROLLBACK_JOURNAL_BYTES = 8_192
TRANSFORM_SCRATCH_BYTES = 2_048


class PixelFormat(IntEnum):
    I420 = 0
    I400 = 1
    I444 = 2


class Search(IntEnum):
    OFF = 0
    BOUNDED = 1


@dataclass(frozen=True)
class Layout:
    format: str
    width: int
    height: int
    alignment: int
    size: int
    stream_capacity: int
    plane_width: tuple[int, int, int]
    plane_height: tuple[int, int, int]
    plane_size: tuple[int, int, int]
    coded_plane_size: tuple[int, int, int]
    transform_count: tuple[int, int, int]
    coded_reconstruction_offset: int
    baseline_reconstruction_offset: int
    luma_coefficients_offset: int
    chroma_dc_levels_offset: int
    entropy_state_offset: int
    stream_staging_offset: int
    coefficient_bytes: int
    entropy_state_bytes: int
    searched_blocks: int
    candidate_trials: int


def ceil_div(value: int, divisor: int) -> int:
    return (value + divisor - 1) // divisor


def align(value: int, alignment: int = WORKSPACE_ALIGNMENT) -> int:
    return ceil_div(value, alignment) * alignment


def checked_dimension(width: int, height: int) -> None:
    if width < 1 or height < 1 or width > 1440 or height > 1440:
        raise ValueError("dimensions must be in 1..1440")


def plane_geometry(
        pixel_format: PixelFormat, width: int, height: int
) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    checked_dimension(width, height)
    if pixel_format == PixelFormat.I400:
        return (width, 0, 0), (height, 0, 0)
    if pixel_format == PixelFormat.I420:
        return (
            width, ceil_div(width, 2), ceil_div(width, 2)
        ), (
            height, ceil_div(height, 2), ceil_div(height, 2)
        )
    if pixel_format == PixelFormat.I444:
        return (width, width, width), (height, height, height)
    raise ValueError("unsupported pixel format")


def query_layout(
        pixel_format: PixelFormat, width: int, height: int
) -> Layout:
    plane_width, plane_height = plane_geometry(pixel_format, width, height)
    plane_size = tuple(
        plane_width[index] * plane_height[index] for index in range(3))
    coded_width = tuple(
        0 if plane_width[index] == 0 else align(plane_width[index], 8)
        for index in range(3))
    coded_height = tuple(
        0 if plane_height[index] == 0 else align(plane_height[index], 8)
        for index in range(3))
    coded_plane_size = tuple(
        coded_width[index] * coded_height[index] for index in range(3))
    y_transforms = ceil_div(width, 8) * ceil_div(height, 8)
    uv_transforms = (
        0 if pixel_format == PixelFormat.I400
        else ceil_div(width, 16) * ceil_div(height, 16)
    )
    transform_count = (y_transforms, uv_transforms, uv_transforms)
    stream_capacity = y_transforms * 64 + 256

    mi_columns = 2 * ceil_div(width, 8)
    # Y partition/mode/coeff plus U/V magnitude/sign above and left state.
    neighbor_bytes = align(7 * (mi_columns + 16))
    entropy_state_bytes = (
        align(SYNTAX_CDF_BYTES) + RANGE_CHECKPOINT_BYTES +
        CDF_ROLLBACK_JOURNAL_BYTES + TRANSFORM_SCRATCH_BYTES +
        neighbor_bytes
    )

    coded_reconstruction_offset = 0
    baseline_reconstruction_offset = align(sum(coded_plane_size))
    luma_coefficients_offset = align(
        baseline_reconstruction_offset + sum(plane_size))
    luma_coefficient_bytes = y_transforms * 64 * 2
    chroma_dc_levels_offset = align(
        luma_coefficients_offset + luma_coefficient_bytes)
    chroma_level_bytes = 2 * uv_transforms * 2
    entropy_state_offset = align(
        chroma_dc_levels_offset + chroma_level_bytes)
    stream_staging_offset = align(
        entropy_state_offset + entropy_state_bytes)
    size = align(
        stream_staging_offset + STREAM_SLICE_COUNT * stream_capacity)

    searched_blocks = min(y_transforms, MAX_SEARCHED_BLOCKS)
    candidate_trials = (
        searched_blocks * POSITIONS_PER_SEARCHED_BLOCK *
        CHOICES_PER_POSITION +
        (y_transforms - searched_blocks) * CHOICES_PER_POSITION
    )
    return Layout(
        format=pixel_format.name,
        width=width,
        height=height,
        alignment=WORKSPACE_ALIGNMENT,
        size=size,
        stream_capacity=stream_capacity,
        plane_width=plane_width,
        plane_height=plane_height,
        plane_size=plane_size,
        coded_plane_size=coded_plane_size,
        transform_count=transform_count,
        coded_reconstruction_offset=coded_reconstruction_offset,
        baseline_reconstruction_offset=baseline_reconstruction_offset,
        luma_coefficients_offset=luma_coefficients_offset,
        chroma_dc_levels_offset=chroma_dc_levels_offset,
        entropy_state_offset=entropy_state_offset,
        stream_staging_offset=stream_staging_offset,
        coefficient_bytes=luma_coefficient_bytes + chroma_level_bytes,
        entropy_state_bytes=entropy_state_bytes,
        searched_blocks=searched_blocks,
        candidate_trials=candidate_trials,
    )


def build_report(width: int = 896, height: int = 896) -> dict:
    layouts = {
        pixel_format.name: asdict(query_layout(pixel_format, width, height))
        for pixel_format in (
            PixelFormat.I400, PixelFormat.I420, PixelFormat.I444)
    }
    return {
        "schema": 1,
        "policy": {
            "search_values": {"off": 0, "bounded": 1},
            "max_searched_blocks": MAX_SEARCHED_BLOCKS,
            "positions_per_searched_block": POSITIONS_PER_SEARCHED_BLOCK,
            "choices_per_position": CHOICES_PER_POSITION,
            "stream_slice_count": STREAM_SLICE_COUNT,
            "uv_coefficients_per_transform": 1,
        },
        "layouts": layouts,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--width", type=int, default=896)
    parser.add_argument("--height", type=int, default=896)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    report = build_report(arguments.width, arguments.height)
    if arguments.check:
        assert report["layouts"]["I400"]["size"] == 6_493_736
        assert report["layouts"]["I420"]["size"] == 7_309_096
        assert report["layouts"]["I444"]["size"] == 9_717_544
        assert report["layouts"]["I400"]["candidate_trials"] == 98_304
        return 0
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

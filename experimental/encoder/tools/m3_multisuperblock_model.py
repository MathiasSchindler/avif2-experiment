#!/usr/bin/env python3
"""Independent AV2 64x64-superblock traversal model for M3."""

from __future__ import annotations

import argparse
import json


def _forced_axis(
    mi_rows: int, mi_columns: int, row: int, column: int,
    width: int, height: int,
) -> int:
    has_rows = row + height // 2 < mi_rows
    has_columns = column + width // 2 < mi_columns
    if has_rows and has_columns:
        return 0
    if width == height:
        return 2 if has_rows and not has_columns else 1
    if height > width:
        if not has_rows:
            return 1
        if not has_columns and width >= 4 and column + width // 4 >= mi_columns:
            return 1
    else:
        if not has_columns:
            return 2
        if not has_rows and height >= 4 and row + height // 4 >= mi_rows:
            return 2
    return 0


def _visit(
    mi_rows: int, mi_columns: int, row: int, column: int,
    width: int, height: int, transforms: list[tuple[int, int]],
) -> None:
    if row >= mi_rows or column >= mi_columns:
        return
    axis = _forced_axis(
        mi_rows, mi_columns, row, column, width, height
    )
    if axis == 0 and (width > 4 or height > 4):
        axis = 1 if height >= width else 2
    if axis == 1:
        _visit(
            mi_rows, mi_columns, row, column,
            width, height // 2, transforms,
        )
        _visit(
            mi_rows, mi_columns, row + height // 2, column,
            width, height // 2, transforms,
        )
        return
    if axis == 2:
        _visit(
            mi_rows, mi_columns, row, column,
            width // 2, height, transforms,
        )
        _visit(
            mi_rows, mi_columns, row, column + width // 2,
            width // 2, height, transforms,
        )
        return
    for y in range(row * 4, (row + height) * 4, 8):
        for x in range(column * 4, (column + width) * 4, 8):
            transforms.append((x, y))


def model(width: int, height: int) -> dict[str, object]:
    if width < 1 or height < 1:
        raise ValueError("dimensions must be positive")
    mi_columns = ((width + 7) // 8) * 2
    mi_rows = ((height + 7) // 8) * 2
    roots: list[dict[str, int]] = []
    transforms: list[tuple[int, int]] = []
    for row in range(0, mi_rows, 16):
        for column in range(0, mi_columns, 16):
            roots.append({
                "row": row, "column": column,
                "left_context_epoch": row // 16,
            })
            _visit(
                mi_rows, mi_columns, row, column, 16, 16, transforms
            )
    expected = {
        (x, y)
        for y in range(0, ((height + 7) // 8) * 8, 8)
        for x in range(0, ((width + 7) // 8) * 8, 8)
    }
    return {
        "width": width,
        "height": height,
        "mi_columns": mi_columns,
        "mi_rows": mi_rows,
        "superblock_order": roots,
        "transform_order": transforms,
        "transform_exact_cover": set(transforms) == expected,
        "transform_count": len(transforms),
        "above_context_span": mi_columns,
        "left_context_span": 16,
        "cdf_epoch_count": 1,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    arguments = parser.parse_args()
    print(json.dumps(model(arguments.width, arguments.height), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

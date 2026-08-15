#!/usr/bin/env python3
"""Independent geometry model for bounded M3 chroma partition leaves."""

from dataclasses import dataclass


@dataclass(frozen=True)
class Leaf:
    row: int
    column: int
    width: int
    height: int


def forced_axis(rows: int, columns: int, leaf: Leaf) -> int:
    has_rows = leaf.row + leaf.height // 2 < rows
    has_columns = leaf.column + leaf.width // 2 < columns
    if has_rows and has_columns:
        return 0
    if leaf.width == leaf.height:
        return 2 if has_rows and not has_columns else 1
    if leaf.height > leaf.width:
        if not has_rows:
            return 1
        if (not has_columns and leaf.width >= 4 and
                leaf.column + leaf.width // 4 >= columns):
            return 1
    else:
        if not has_columns:
            return 2
        if (not has_rows and leaf.height >= 4 and
                leaf.row + leaf.height // 4 >= rows):
            return 2
    return 0


def partition_leaves(width: int, height: int) -> tuple[Leaf, ...]:
    rows = ((height + 7) // 8) * 2
    columns = ((width + 7) // 8) * 2
    leaves: list[Leaf] = []

    def visit(leaf: Leaf) -> None:
        if leaf.row >= rows or leaf.column >= columns:
            return
        axis = forced_axis(rows, columns, leaf)
        if not axis and leaf.width <= 4 and leaf.height <= 4:
            leaves.append(leaf)
            return
        if not axis:
            axis = 1 if leaf.height >= leaf.width else 2
        if axis == 1:
            half = leaf.height // 2
            visit(Leaf(leaf.row, leaf.column, leaf.width, half))
            visit(Leaf(leaf.row + half, leaf.column, leaf.width, half))
        else:
            half = leaf.width // 2
            visit(Leaf(leaf.row, leaf.column, half, leaf.height))
            visit(Leaf(leaf.row, leaf.column + half, half, leaf.height))

    for row in range(0, rows, 16):
        for column in range(0, columns, 16):
            visit(Leaf(row, column, 16, 16))
    return tuple(leaves)


def transform_geometry(leaf: Leaf, format_id: int) -> tuple[int, int, int, int]:
    divisor = 2 if format_id == 0 else 1
    width = leaf.width * 4 // divisor
    height = leaf.height * 4 // divisor
    context = 0 if max(width, height) == 4 else (
        1 if max(width, height) == 8 else 2)
    eob_class = {16: 0, 32: 1, 64: 2, 128: 3, 256: 4}[width * height]
    return width, height, context, eob_class


def admitted_pair_count(max_dimension: int = 896) -> int:
    return max_dimension * max_dimension

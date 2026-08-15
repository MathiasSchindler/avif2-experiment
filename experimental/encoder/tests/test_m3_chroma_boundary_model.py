import sys
from pathlib import Path
from unittest import TestCase

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))

from m3_chroma_boundary_model import (  # noqa: E402
    admitted_pair_count,
    partition_leaves,
    transform_geometry,
)


class M3ChromaBoundaryModelTests(TestCase):
    def test_full_admission_count(self) -> None:
        self.assertEqual(admitted_pair_count(), 802816)

    def test_half_grid_leaf_shapes(self) -> None:
        expected = {
            (8, 9): {(2, 4)},
            (17, 16): {(4, 4), (2, 4)},
            (24, 24): {(4, 4), (2, 4), (4, 2), (2, 2)},
            (64, 65): {(4, 4), (4, 2)},
        }
        for dimensions, shapes in expected.items():
            leaves = partition_leaves(*dimensions)
            self.assertEqual({(leaf.width, leaf.height) for leaf in leaves},
                             shapes)
            self.assertEqual(
                len(leaves),
                ((dimensions[0] + 15) // 16) *
                ((dimensions[1] + 15) // 16))

    def test_rectangular_transform_contexts(self) -> None:
        leaves = partition_leaves(24, 24)
        self.assertEqual(
            {transform_geometry(leaf, 0) for leaf in leaves},
            {(8, 8, 1, 2), (4, 8, 1, 1),
             (8, 4, 1, 1), (4, 4, 0, 0)})
        self.assertEqual(
            {transform_geometry(leaf, 2) for leaf in leaves},
            {(16, 16, 2, 4), (8, 16, 2, 3),
             (16, 8, 2, 3), (8, 8, 1, 2)})

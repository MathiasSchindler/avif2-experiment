#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENCODER / "tools"))

from m2_transcript_matrix import CASES, build_matrix


class M2TranscriptMatrixTests(unittest.TestCase):
    def test_frozen_matrix(self) -> None:
        actual = build_matrix()
        expected = __import__("json").loads(
            (ENCODER / "fixtures/m2-transcript-matrix.json").read_text())
        self.assertEqual(actual, expected)

    def test_required_paths_are_distinct(self) -> None:
        operations = {kind for case in CASES.values()
                      for kind, _, _ in case}
        self.assertTrue(
            {"symbol", "base", "br", "sign", "bypass",
             "literal", "remainder"} <= operations)
        self.assertEqual(
            {argument for case in CASES.values()
             for kind, _, argument in case
             if kind in {"symbol", "base", "br"}},
            {2, 3, 5, 8},
        )

    def test_exhaustive_matrix_covers_every_alphabet_and_boundary(self) -> None:
        matrix = build_matrix()["exhaustive_cases"]
        self.assertEqual(
            {(case["alphabet"], case["initial_count"]) for case in matrix},
            {
                (alphabet, count)
                for alphabet in range(2, 17)
                for count in (0, 15, 16, 31, 32)
            },
        )
        for case in matrix:
            alphabet = case["alphabet"]
            self.assertEqual(
                case["decoded"],
                list(range(alphabet)) + list(reversed(range(alphabet))),
            )


if __name__ == "__main__":
    unittest.main()

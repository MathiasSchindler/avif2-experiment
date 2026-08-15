#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENCODER / "tools"))

from generate_cdf_subset import (
    TABLES,
    parse_table,
    render,
    verify_parameter_adjustments,
)


class CdfSubsetTests(unittest.TestCase):
    def test_hashes_counts_and_ranges(self) -> None:
        spec = ENCODER.parents[1] / "docs/spec/av2-v1.0.0/attachments"
        self.assertEqual(sum(table.rows for table in TABLES), 2512)
        verify_parameter_adjustments(spec)
        for table in TABLES:
            self.assertEqual(len(parse_table(spec, table)), table.rows)

    def test_tracked_include_is_current(self) -> None:
        spec = ENCODER.parents[1] / "docs/spec/av2-v1.0.0/attachments"
        self.assertEqual(
            (ENCODER / "src/normative_cdfs.inc").read_text(),
            render(spec))


if __name__ == "__main__":
    unittest.main()

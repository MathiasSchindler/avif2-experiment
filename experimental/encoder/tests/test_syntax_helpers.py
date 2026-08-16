#!/usr/bin/env python3

import subprocess
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENCODER / "tools"))

from m1_syntax import SymbolDecoder
from syntax_reference import RawReader, read_coeff_remainder, read_range_ns


UVLC_VALUES = (
    0, 1, 2, 3, 14, 15, 16, 65534, 0x7FFFFFFE, 0xFFFFFFFE,
)
SVLC_VALUES = (
    -2147483647, -32768, -2, -1, 0, 1, 2, 32767, 2147483647,
)
PREDICTIONS = (0, 1, 3, 4, 15, 64)
REMAINDERS = (0, 1, 9, 10, 17, 18, 33, 34, 255, 65535, 0xFFFFFFFF)
EOB_BASE = [26000, 19000, 12000, 6000, 0]
NON_EOB_BASE = [24000, 15000, 7000, 0]
BR = [24000, 15000, 7000, 0]


class SyntaxHelperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.binary = ENCODER / "build/syntax-helpers-vectors"
        cls.binary.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "cc", "-Iinclude", "-std=c11", "-Wall", "-Wextra",
                "-Wpedantic", "-Werror", "-O2", "-fno-builtin",
                "-o", str(cls.binary), "tests/syntax_helpers_vectors.c",
                "src/memory.c", "src/writer.c", "src/range.c", "src/cdf.c",
                "src/syntax_cdf.c", "src/syntax.c", "src/directional.c",
            ],
            cwd=ENCODER, check=True,
        )
        output = subprocess.check_output([str(cls.binary)], text=True)
        cls.vectors = {
            name: bytes.fromhex(payload)
            for name, payload in (
                line.split() for line in output.splitlines()
            )
        }

    def test_raw_helpers_against_independent_decoder(self) -> None:
        reader = RawReader(self.vectors["RAW"])
        self.assertEqual([reader.uvlc() for _ in UVLC_VALUES], list(UVLC_VALUES))
        self.assertEqual([reader.svlc() for _ in SVLC_VALUES], list(SVLC_VALUES))
        self.assertEqual(
            [reader.signed_literal(3) for _ in range(16)],
            list(range(-8, 8)),
        )
        for count in range(1, 66):
            self.assertEqual(
                [reader.ns(count) for _ in range(count)],
                list(range(count)),
            )
        for count in range(1, 65):
            self.assertEqual(
                [reader.subexp(count, 3) for _ in range(count)],
                list(range(count)),
            )
            for reference in range(count):
                self.assertEqual(
                    [
                        reader.unsigned_subexp_with_ref(count, reference, 3)
                        for _ in range(count)
                    ],
                    list(range(count)),
                )
        self.assertEqual(
            [
                reader.signed_subexp_with_ref(-8, 9, 3, 3)
                for _ in range(17)
            ],
            list(range(-8, 9)),
        )
        reader.trailing_bits()
        self.assertEqual(reader.position, len(reader.bits))

    def test_range_helpers_against_independent_decoder(self) -> None:
        decoder = SymbolDecoder(self.vectors["RANGE"])
        signed = []
        for _ in range(16):
            value = decoder.read_literal(4)
            signed.append(value - 16 if value & 8 else value)
        self.assertEqual(signed, list(range(-8, 8)))
        for count in range(1, 34):
            self.assertEqual(
                [read_range_ns(decoder, count) for _ in range(count)],
                list(range(count)),
            )
        for prediction in PREDICTIONS:
            self.assertEqual(
                [
                    read_coeff_remainder(decoder, prediction)
                    for _ in REMAINDERS
                ],
                list(REMAINDERS),
            )
        for magnitude in range(1, 21):
            base = decoder.read_symbol(EOB_BASE, symbol_count=5)
            level = base + 1
            if level > 4:
                level += decoder.read_symbol(BR, symbol_count=4)
            if level >= 8:
                level += read_coeff_remainder(decoder, magnitude // 2)
            self.assertEqual(level, magnitude)
        for magnitude in range(21):
            base = decoder.read_symbol(NON_EOB_BASE, symbol_count=4)
            level = base
            if level > 2:
                level += decoder.read_symbol(BR, symbol_count=4)
            if level >= 6:
                level += read_coeff_remainder(decoder, magnitude // 2)
            self.assertEqual(level, magnitude)
        decoder.check_exit()


if __name__ == "__main__":
    unittest.main()

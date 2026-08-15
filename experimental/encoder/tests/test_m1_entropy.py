#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENCODER / "tools"))

from m1_entropy import (
    M1EntropyModel,
    RECONSTRUCTED_ENCODER_TRANSCRIPT,
    probability_increments,
    verify_attachments,
)
from m1_syntax import SymbolDecoder, parse_annexb


class M1EntropyTests(unittest.TestCase):
    def test_normative_attachment_hashes(self) -> None:
        verify_attachments()

    def test_exact_normative_prefix_matches_reconstructed_stream(self) -> None:
        model = M1EntropyModel(bytes.fromhex("8d80"))
        decisions = model.decode_prefix()
        self.assertEqual(
            [(item.name, item.value) for item in decisions],
            [
                ("do_split", 0),
                ("delta_q_abs", 1),
                ("delta_q_sign_bit", 1),
            ],
        )
        self.assertEqual(decisions[0].context, "TileDoSplitCdf[0][0]")
        self.assertEqual(decisions[0].initial_cdf, (28084, 3, 0))
        self.assertEqual(
            decisions[1].initial_cdf,
            (16594, 23325, 26424, 28225, 29358, 30099, 30613, 56, 0),
        )
        self.assertTrue(all(item.adaptation_enabled
                            for item in decisions if item.initial_cdf))

    def test_probability_increment_rows_match_normative_formula(self) -> None:
        for symbol_count, row in enumerate(probability_increments(), 2):
            self.assertEqual(
                row[:symbol_count],
                tuple(
                    16 * (symbol_count - symbol - 1) // symbol_count
                    for symbol in range(symbol_count)
                ),
            )

    def test_reconstructed_encoder_transcript_is_locked(self) -> None:
        self.assertEqual(
            RECONSTRUCTED_ENCODER_TRANSCRIPT,
            (
                ("do_split", 0),
                ("delta_q_abs", 1),
                ("delta_q_sign_bit", 1),
                ("y_mode_set", 0),
                ("y_mode_index", 0),
                ("fsc_mode", 0),
                ("all_zero", 1),
            ),
        )
        M1EntropyModel(bytes.fromhex("8d80")).require_reconstructed_prefix()

    def test_c_range_writer_mixed_vector_roundtrip(self) -> None:
        decoder = SymbolDecoder(bytes.fromhex("2b798240"))
        self.assertEqual(
            [
                decoder.read_symbol([24576, 8192, 0], symbol_count=3),
                decoder.read_literal(5),
                decoder.read_symbol([16384, 0], symbol_count=2),
                decoder.read_symbol(
                    [30000, 22000, 14000, 6000, 0], symbol_count=5),
                decoder.read_bool(),
                decoder.read_symbol([24576, 8192, 0], symbol_count=3),
                decoder.read_literal(10),
                decoder.read_symbol([16384, 0], symbol_count=2),
            ],
            [0, 21, 1, 4, 0, 2, 0x2D3, 0],
        )
        decoder.check_exit()

    def test_c_range_writer_adaptive_vector_roundtrip(self) -> None:
        decoder = SymbolDecoder(bytes.fromhex("387980"))
        cdf = [24576, 8192, 0, 0, 0, 1, 2]
        self.assertEqual(
            [
                decoder.read_symbol(
                    cdf, symbol_count=3, update_cdf=True,
                    update_rates=(2, 3, 4))
                for _ in range(8)
            ],
            [0, 2, 1, 1, 0, 2, 2, 1],
        )
        self.assertEqual(cdf, [27398, 13030, 0, 8, 0, 1, 2])
        decoder.check_exit()

    def test_avm_fixture_uses_a_different_configuration(self) -> None:
        fixture = (
            ENCODER / "fixtures/avm-v1.0.0-monochrome/constant-128-8x8.obu"
        ).read_bytes()
        units = parse_annexb(fixture)
        self.assertEqual(fixture.hex(), "01080a04820488bf385eea08110510e2000010")
        self.assertEqual(units[2][1], bytes.fromhex("e2000010"))
        self.assertNotEqual(units[1][1], bytes.fromhex("820488bf187e6f1022"))

        model = M1EntropyModel(bytes.fromhex("10"))
        do_split = model.symbol(
            "do_split", "TileDoSplitCdf[0][0]",
            (28084, 3, 0),
        )
        self.assertEqual(do_split, 0)
        # The fixture's lossless frame header has no delta-Q decision. Applying
        # the reconstructed QP-255 transcript would consume unrelated bits.
        self.assertEqual(len(model.decisions), 1)


if __name__ == "__main__":
    unittest.main()

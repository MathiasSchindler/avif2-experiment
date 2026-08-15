#!/usr/bin/env python3

import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ENCODER = Path(__file__).resolve().parents[1]
ROOT = ENCODER.parents[1]
sys.path.insert(0, str(ENCODER / "tools"))
sys.path.insert(0, str(ENCODER / "tests"))

from m1_syntax import (
    BitReader,
    BitWriter,
    SymbolDecoder,
    SyntaxError,
    build_m1_stream,
    decode_frame_header_8x8,
    decode_sequence_header_8x8,
    encode_frame_header_8x8,
    encode_sequence_header_8x8,
    forced_partition_path_8x8,
    leb128_decode,
    leb128_encode,
    parse_annexb,
)
from validator_support import configured_path, require_validator


class DescriptorTests(unittest.TestCase):
    def test_uvlc_exact_vectors_and_roundtrip(self) -> None:
        expected = {0: "1", 1: "010", 2: "011", 3: "00100", 14: "0001111"}
        for value, bits in expected.items():
            writer = BitWriter()
            writer.uvlc("x", value)
            self.assertEqual("".join(map(str, writer.bits)), bits)
            reader = BitReader(bytes([int(bits.ljust(8, "0"), 2)]))
            self.assertEqual(reader.uvlc("x"), value)
        with self.assertRaises(SyntaxError):
            BitReader(b"\0\0\0\0").uvlc("x")

    def test_ns_five_exact_vectors(self) -> None:
        self.assertEqual(
            [
                self._ns_bits(value, 5)
                for value in range(5)
            ],
            ["00", "01", "10", "110", "111"],
        )

    @staticmethod
    def _ns_bits(value: int, n: int) -> str:
        writer = BitWriter()
        writer.ns("x", value, n)
        return "".join(map(str, writer.bits))

    def test_canonical_leb128(self) -> None:
        for value in (0, 1, 127, 128, 16383, 16384, (1 << 32) - 1):
            encoded = leb128_encode(value)
            self.assertEqual(leb128_decode(encoded), (value, len(encoded)))
        with self.assertRaisesRegex(SyntaxError, "non-canonical"):
            leb128_decode(b"\x81\x00")


class M1RawSyntaxTests(unittest.TestCase):
    TILE = bytes.fromhex("8d80")

    def test_sequence_header_exact_and_all_fields_checked(self) -> None:
        payload, fields = encode_sequence_header_8x8()
        self.assertEqual(payload.hex(), "820488bf187e6f1022")
        self.assertEqual(decode_sequence_header_8x8(payload), fields)
        self.assertEqual(fields[-2].name, "trailing_one_bit")
        for bit in range(len(payload) * 8):
            changed = bytearray(payload)
            changed[bit >> 3] ^= 1 << (7 - (bit & 7))
            with self.assertRaises(SyntaxError):
                decode_sequence_header_8x8(bytes(changed))

    def test_frame_header_exact_and_tile_split(self) -> None:
        header, fields = encode_frame_header_8x8()
        self.assertEqual(header.hex(), "e3fe6c00")
        decoded, tile = decode_frame_header_8x8(header + self.TILE)
        self.assertEqual(decoded, fields)
        self.assertEqual(tile, self.TILE)
        for bit in range(32):
            changed = bytearray(header + self.TILE)
            changed[bit >> 3] ^= 1 << (7 - (bit & 7))
            with self.assertRaises(SyntaxError):
                decode_frame_header_8x8(bytes(changed))

    def test_obu_lengths_include_header_and_payload(self) -> None:
        stream = build_m1_stream(self.TILE)
        self.assertEqual(stream.hex(), "01080a04820488bf187e6f10220710e3fe6c008d80")
        units = parse_annexb(stream)
        self.assertEqual([kind for kind, _ in units], [2, 1, 4])
        self.assertEqual([len(payload) for _, payload in units], [0, 9, 6])

    def test_forced_partition_path(self) -> None:
        self.assertEqual(
            forced_partition_path_8x8(),
            (
                "64x64:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
                "32x32:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
                "16x16:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
                "8x8:PARTITION_NONE(do_split=0)",
            ),
        )

    def test_symbol_decoder_initialization_and_exit_rule(self) -> None:
        decoder = SymbolDecoder(self.TILE)
        self.assertEqual(decoder.bit_pos, 15)
        self.assertEqual(decoder.range, 1 << 15)
        self.assertEqual(decoder.max_bits, 1)
        self.assertEqual(decoder.value, 0x393F)
        synthetic = SymbolDecoder(bytes.fromhex("8000"))
        synthetic.max_bits = 1
        synthetic.check_exit()
        synthetic.data = bytes.fromhex("c000")
        with self.assertRaisesRegex(SyntaxError, "padding"):
            synthetic.check_exit()

    def test_black_box_decoders_accept_rebuilt_stream(self) -> None:
        avmdec = require_validator(
            self,
            configured_path("AVMDEC", ROOT / "build/reference/avm/avmdec"),
            "pinned avmdec",
        )
        project = require_validator(
            self,
            configured_path(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder",
            ),
            "project decoder",
        )
        work = ENCODER / "build/m1-syntax-test"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        obu = work / "flat-8.obu"
        avm_raw = work / "avm.raw"
        project_raw = work / "project.i400"
        obu.write_bytes(build_m1_stream(self.TILE))
        subprocess.run(
            [
                str(avmdec), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_raw), str(obu),
            ],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            [str(project), str(obu), "-o", str(project_raw)],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        expected = bytes([128]) * 64
        self.assertEqual(avm_raw.read_bytes(), expected)
        self.assertEqual(project_raw.read_bytes(), expected)
        shutil.rmtree(work)


if __name__ == "__main__":
    unittest.main()

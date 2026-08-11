import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from obu_carriage import (  # noqa: E402
    CarriageError,
    encode_uleb128,
    hash_i420,
    parse_obus,
    split_carriage,
)


def make_obu(obu_type: int, payload: bytes = b"") -> bytes:
    body = bytes([obu_type << 2]) + payload
    return encode_uleb128(len(body)) + body


class CarriageTests(unittest.TestCase):
    def test_parse_extension_header(self) -> None:
        body = bytes([0x80 | (4 << 2) | 2, (3 << 5) | 17, 0xAA])
        obu = parse_obus(encode_uleb128(len(body)) + body)[0]
        self.assertEqual(obu.obu_type, 4)
        self.assertEqual(obu.temporal_id, 2)
        self.assertEqual(obu.mlayer_id, 3)
        self.assertEqual(obu.xlayer_id, 17)
        self.assertEqual(obu.header_size, 2)

    def test_split_moves_sequence_header_and_removes_delimiter(self) -> None:
        sequence = make_obu(1, b"sequence")
        closed_loop_key = make_obu(4, b"picture")
        original = make_obu(2) + sequence + closed_loop_key
        config, payload, reconstructed, ledger = split_carriage(original)
        self.assertEqual(config, sequence)
        self.assertEqual(payload, closed_loop_key)
        self.assertEqual(reconstructed, sequence + closed_loop_key)
        self.assertEqual([obu.obu_type for obu in ledger], [2, 1, 4])

    def test_rejects_noncanonical_uleb128(self) -> None:
        with self.assertRaisesRegex(CarriageError, "non-canonical"):
            parse_obus(b"\x81\x00\x04")

    def test_rejects_truncated_obu(self) -> None:
        with self.assertRaisesRegex(CarriageError, "past input"):
            parse_obus(b"\x02\x04")

    def test_rejects_duplicate_sequence_header(self) -> None:
        data = make_obu(1) + make_obu(1) + make_obu(4)
        with self.assertRaisesRegex(CarriageError, "found 2"):
            split_carriage(data)

    def test_rejects_padding(self) -> None:
        data = make_obu(1) + make_obu(25) + make_obu(4)
        with self.assertRaisesRegex(CarriageError, r"OBU 1.*Padding OBUs"):
            split_carriage(data)

    def test_rejects_sequence_header_after_picture(self) -> None:
        data = make_obu(4) + make_obu(1)
        with self.assertRaisesRegex(CarriageError, "must precede"):
            split_carriage(data)

    def test_rejects_non_closed_picture(self) -> None:
        data = make_obu(1) + make_obu(5)
        with self.assertRaisesRegex(CarriageError, "must begin with"):
            split_carriage(data)

    def test_hash_i420_odd_dimensions(self) -> None:
        result = hash_i420(bytes(range(27)), 3, 5)
        self.assertEqual(result["bytes"], 27)
        self.assertEqual(result["chroma_width"], 2)
        self.assertEqual(result["chroma_height"], 3)


if __name__ == "__main__":
    unittest.main()
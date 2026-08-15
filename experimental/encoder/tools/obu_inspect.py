#!/usr/bin/env python3
"""Inspect AV2 length-delimited OBU boundaries and payload bits."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


OBU_NAMES = {
    1: "sequence_header",
    2: "temporal_delimiter",
    3: "multi_frame_header",
    4: "closed_loop_key",
    5: "open_loop_key",
    6: "leading_tile_group",
    7: "regular_tile_group",
    8: "metadata_short",
    9: "metadata_group",
    10: "switch",
    11: "leading_sef",
    12: "regular_sef",
    13: "leading_tip",
    14: "regular_tip",
    15: "buffer_removal_timing",
    16: "layer_configuration_record",
    17: "atlas_segment",
    18: "operating_point_set",
    19: "bridge_frame",
    20: "multi_stream_decoder_operation",
    21: "ras_frame",
    22: "quantization_matrix",
    23: "film_grain_model",
    24: "content_interpretation",
    25: "padding",
}


class ObuParseError(ValueError):
    pass


@dataclass(frozen=True)
class Obu:
    index: int
    length_offset: int
    length_bytes: int
    obu_offset: int
    obu_bytes: int
    header_bytes: int
    obu_type: int
    temporal_id: int
    mlayer_id: int
    xlayer_id: int
    payload: bytes
    encoded: bytes

    def record(self, dump_bits: bool = False) -> dict[str, object]:
        result: dict[str, object] = {
            "index": self.index,
            "length_offset": self.length_offset,
            "length_bytes": self.length_bytes,
            "obu_offset": self.obu_offset,
            "obu_bytes": self.obu_bytes,
            "header_bytes": self.header_bytes,
            "payload_offset": self.obu_offset + self.header_bytes,
            "payload_bytes": len(self.payload),
            "type": self.obu_type,
            "type_name": OBU_NAMES.get(self.obu_type, f"reserved_{self.obu_type}"),
            "temporal_id": self.temporal_id,
            "mlayer_id": self.mlayer_id,
            "xlayer_id": self.xlayer_id,
            "sha256": hashlib.sha256(self.encoded).hexdigest(),
        }
        if dump_bits:
            result["payload_bits"] = " ".join(f"{byte:08b}" for byte in self.payload)
        return result


def decode_uleb128(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for index in range(8):
        position = offset + index
        if position >= len(data):
            raise ObuParseError(f"truncated ULEB128 at byte {offset}")
        byte = data[position]
        value |= (byte & 0x7F) << (index * 7)
        if byte < 0x80:
            if index and value < 1 << (index * 7):
                raise ObuParseError(f"non-canonical ULEB128 at byte {offset}")
            return value, index + 1
    raise ObuParseError(f"unterminated ULEB128 at byte {offset}")


def parse_obus(data: bytes) -> list[Obu]:
    result: list[Obu] = []
    offset = 0
    while offset < len(data):
        obu_bytes, length_bytes = decode_uleb128(data, offset)
        if obu_bytes == 0:
            raise ObuParseError(f"zero-sized OBU at byte {offset}")
        obu_offset = offset + length_bytes
        end = obu_offset + obu_bytes
        if end > len(data):
            raise ObuParseError(f"OBU at byte {offset} extends past input")
        first = data[obu_offset]
        extension = first >> 7
        header_bytes = 2 if extension else 1
        if obu_bytes < header_bytes:
            raise ObuParseError(f"truncated OBU header at byte {obu_offset}")
        second = data[obu_offset + 1] if extension else 0
        result.append(
            Obu(
                index=len(result),
                length_offset=offset,
                length_bytes=length_bytes,
                obu_offset=obu_offset,
                obu_bytes=obu_bytes,
                header_bytes=header_bytes,
                obu_type=(first >> 2) & 0x1F,
                temporal_id=first & 0x03,
                mlayer_id=(second >> 5) & 0x07,
                xlayer_id=second & 0x1F,
                payload=data[obu_offset + header_bytes : end],
                encoded=data[offset:end],
            )
        )
        offset = end
    if not result:
        raise ObuParseError("input contains no OBUs")
    return result


def inspect(path: Path, dump_bits: bool = False) -> dict[str, object]:
    data = path.read_bytes()
    return {
        "path": str(path),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "obus": [obu.record(dump_bits) for obu in parse_obus(data)],
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--bits", action="store_true", help="include MSB-first payload bits")
    args = parser.parse_args(argv)
    try:
        print(json.dumps(inspect(args.input, args.bits), indent=2, sort_keys=True))
    except (OSError, ObuParseError) as error:
        parser.exit(2, f"error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

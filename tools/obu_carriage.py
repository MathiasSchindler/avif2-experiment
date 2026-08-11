#!/usr/bin/env python3
"""Strict AV2 length-delimited OBU utilities for carriage experiments."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


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


class CarriageError(ValueError):
    pass


@dataclass(frozen=True)
class Obu:
    index: int
    prefix_offset: int
    prefix_size: int
    data_offset: int
    size: int
    header_size: int
    obu_type: int
    temporal_id: int
    mlayer_id: int
    xlayer_id: int
    encoded: bytes

    @property
    def name(self) -> str:
        return OBU_NAMES.get(self.obu_type, f"reserved_{self.obu_type}")

    def ledger_entry(self) -> dict[str, int | str]:
        return {
            "index": self.index,
            "offset": self.prefix_offset,
            "length_field_bytes": self.prefix_size,
            "obu_offset": self.data_offset,
            "obu_size": self.size,
            "header_bytes": self.header_size,
            "payload_offset": self.data_offset + self.header_size,
            "payload_bytes": self.size - self.header_size,
            "type": self.obu_type,
            "type_name": self.name,
            "temporal_id": self.temporal_id,
            "mlayer_id": self.mlayer_id,
            "xlayer_id": self.xlayer_id,
            "sha256": hashlib.sha256(self.encoded).hexdigest(),
        }


def encode_uleb128(value: int) -> bytes:
    if value < 0 or value >= 1 << 56:
        raise CarriageError("ULEB128 value is outside the supported 8-byte range")
    encoded = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            encoded.append(byte | 0x80)
        else:
            encoded.append(byte)
            return bytes(encoded)


def decode_uleb128(data: bytes, offset: int) -> tuple[int, int]:
    value = 0
    for index in range(8):
        position = offset + index
        if position >= len(data):
            raise CarriageError(f"truncated ULEB128 at byte {offset}")
        byte = data[position]
        value |= (byte & 0x7F) << (7 * index)
        if byte < 0x80:
            width = index + 1
            if encode_uleb128(value) != data[offset : offset + width]:
                raise CarriageError(f"non-canonical ULEB128 at byte {offset}")
            return value, width
    raise CarriageError(f"unterminated ULEB128 at byte {offset}")


def parse_obus(data: bytes) -> list[Obu]:
    obus: list[Obu] = []
    offset = 0
    while offset < len(data):
        size, prefix_size = decode_uleb128(data, offset)
        if size == 0:
            raise CarriageError(f"zero-sized OBU at byte {offset}")
        data_offset = offset + prefix_size
        end = data_offset + size
        if end > len(data):
            raise CarriageError(
                f"OBU at byte {offset} extends {end - len(data)} bytes past input"
            )

        first = data[data_offset]
        extension = (first >> 7) & 1
        header_size = 2 if extension else 1
        if size < header_size:
            raise CarriageError(f"truncated OBU header at byte {data_offset}")

        obu_type = (first >> 2) & 31
        temporal_id = first & 3
        if extension:
            second = data[data_offset + 1]
            mlayer_id = (second >> 5) & 7
            xlayer_id = second & 31
        else:
            mlayer_id = 0
            xlayer_id = 0

        encoded = data[offset:end]
        obus.append(
            Obu(
                index=len(obus),
                prefix_offset=offset,
                prefix_size=prefix_size,
                data_offset=data_offset,
                size=size,
                header_size=header_size,
                obu_type=obu_type,
                temporal_id=temporal_id,
                mlayer_id=mlayer_id,
                xlayer_id=xlayer_id,
                encoded=encoded,
            )
        )
        offset = end

    if not obus:
        raise CarriageError("input contains no OBUs")
    return obus


def join_obus(obus: Iterable[Obu]) -> bytes:
    return b"".join(obu.encoded for obu in obus)


def reject_obu(obu: Obu, rule: str) -> None:
    raise CarriageError(
        f"OBU {obu.index} at byte {obu.prefix_offset} ({obu.name}): {rule}"
    )


def split_carriage(data: bytes) -> tuple[bytes, bytes, bytes, list[Obu]]:
    obus = parse_obus(data)
    sequence_headers = [obu for obu in obus if obu.obu_type == 1]
    if len(sequence_headers) != 1:
        raise CarriageError(
            f"expected exactly one Sequence Header OBU, found {len(sequence_headers)}"
        )

    sequence_header = sequence_headers[0]
    for obu in obus:
        if obu.obu_type == 25:
            reject_obu(obu, "Padding OBUs are forbidden in v0")
        if obu.obu_type == 16:
            reject_obu(obu, "Layer Configuration Record OBUs are unsupported in v0")

    payload_obus = [obu for obu in obus if obu.obu_type not in (1, 2)]
    if not payload_obus:
        raise CarriageError("normalization produced an empty item payload")
    first_coded = payload_obus[0]
    if sequence_header.index > first_coded.index:
        reject_obu(sequence_header, "Sequence Header must precede coded picture data")
    if first_coded.obu_type != 4:
        reject_obu(first_coded, "item payload must begin with a Closed Loop Key OBU")
    if len(payload_obus) != 1:
        reject_obu(payload_obus[1], "initial v0 profile permits one coded OBU")

    config = sequence_header.encoded
    payload = join_obus(payload_obus)
    reconstructed = config + payload
    parse_obus(reconstructed)
    return config, payload, reconstructed, obus


def hash_i420(data: bytes, width: int, height: int) -> dict[str, object]:
    if width <= 0 or height <= 0:
        raise CarriageError("I420 dimensions must be positive")
    chroma_width = (width + 1) // 2
    chroma_height = (height + 1) // 2
    y_size = width * height
    chroma_size = chroma_width * chroma_height
    expected_size = y_size + 2 * chroma_size
    if len(data) != expected_size:
        raise CarriageError(
            f"I420 size mismatch: expected {expected_size} bytes, got {len(data)}"
        )
    y = data[:y_size]
    u = data[y_size : y_size + chroma_size]
    v = data[y_size + chroma_size :]
    return {
        "format": "I420",
        "width": width,
        "height": height,
        "chroma_width": chroma_width,
        "chroma_height": chroma_height,
        "bytes": len(data),
        "file_sha256": hashlib.sha256(data).hexdigest(),
        "planes": {
            "y": hashlib.sha256(y).hexdigest(),
            "u": hashlib.sha256(u).hexdigest(),
            "v": hashlib.sha256(v).hexdigest(),
        },
    }


def write_json(path: Path | None, value: object) -> None:
    rendered = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if path is None:
        print(rendered, end="")
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(rendered, encoding="ascii")


def command_inspect(args: argparse.Namespace) -> None:
    obus = parse_obus(args.input.read_bytes())
    write_json(args.json, [obu.ledger_entry() for obu in obus])


def command_split(args: argparse.Namespace) -> None:
    config, payload, reconstructed, original_obus = split_carriage(
        args.input.read_bytes()
    )
    for path, content in (
        (args.config, config),
        (args.payload, payload),
        (args.reconstructed, reconstructed),
    ):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    write_json(
        args.ledger,
        {
            "original": [obu.ledger_entry() for obu in original_obus],
            "config_obus": [
                obu.ledger_entry() for obu in parse_obus(config)
            ],
            "item_payload": [
                obu.ledger_entry() for obu in parse_obus(payload)
            ],
            "reconstructed": [
                obu.ledger_entry() for obu in parse_obus(reconstructed)
            ],
        },
    )


def command_hash_i420(args: argparse.Namespace) -> None:
    write_json(args.json, hash_i420(args.input.read_bytes(), args.width, args.height))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("input", type=Path)
    inspect_parser.add_argument("--json", type=Path)
    inspect_parser.set_defaults(handler=command_inspect)

    split_parser = subparsers.add_parser("split")
    split_parser.add_argument("input", type=Path)
    split_parser.add_argument("--config", type=Path, required=True)
    split_parser.add_argument("--payload", type=Path, required=True)
    split_parser.add_argument("--reconstructed", type=Path, required=True)
    split_parser.add_argument("--ledger", type=Path, required=True)
    split_parser.set_defaults(handler=command_split)

    hash_parser = subparsers.add_parser("hash-i420")
    hash_parser.add_argument("input", type=Path)
    hash_parser.add_argument("--width", type=int, required=True)
    hash_parser.add_argument("--height", type=int, required=True)
    hash_parser.add_argument("--json", type=Path)
    hash_parser.set_defaults(handler=command_hash_i420)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        args.handler(args)
    except (CarriageError, OSError) as error:
        raise SystemExit(f"error: {error}") from error
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
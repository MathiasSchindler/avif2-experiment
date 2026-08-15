#!/usr/bin/env python3
"""Executable AV2 M1 raw-bit model derived from AV2 v1.0.0."""

from __future__ import annotations

from dataclasses import dataclass


class SyntaxError(ValueError):
    pass


@dataclass(frozen=True)
class Field:
    name: str
    value: int
    start: int
    width: int
    descriptor: str


class BitWriter:
    def __init__(self) -> None:
        self.bits: list[int] = []
        self.fields: list[Field] = []

    def f(self, name: str, value: int, width: int) -> None:
        if width < 0 or value < 0 or value >= 1 << width:
            raise SyntaxError(f"{name}: {value} does not fit f({width})")
        start = len(self.bits)
        self.bits.extend((value >> shift) & 1 for shift in range(width - 1, -1, -1))
        self.fields.append(Field(name, value, start, width, f"f({width})"))

    def uvlc(self, name: str, value: int) -> None:
        if not 0 <= value <= (1 << 32) - 2:
            raise SyntaxError(f"{name}: invalid uvlc value")
        code_num = value + 1
        leading = code_num.bit_length() - 1
        start = len(self.bits)
        self.bits.extend([0] * leading)
        self.bits.extend((code_num >> shift) & 1 for shift in range(leading, -1, -1))
        self.fields.append(Field(name, value, start, 2 * leading + 1, "uvlc()"))

    def ns(self, name: str, value: int, n: int) -> None:
        if n <= 0 or not 0 <= value < n:
            raise SyntaxError(f"{name}: {value} outside ns({n})")
        width = (n - 1).bit_length()
        cutoff = (1 << width) - n
        start = len(self.bits)
        if value < cutoff:
            self.f(name, value, width - 1)
        else:
            encoded = value + cutoff
            self.f(name, encoded >> 1, width - 1)
            self.f(f"{name}.extra_bit", encoded & 1, 1)
        self.fields.append(Field(name, value, start, len(self.bits) - start, f"ns({n})"))

    def align_zero(self) -> None:
        while len(self.bits) & 7:
            self.f("zero_bit", 0, 1)

    def bytes(self) -> bytes:
        if len(self.bits) & 7:
            raise SyntaxError("bit writer is not byte aligned")
        return bytes(
            sum(self.bits[offset + bit] << (7 - bit) for bit in range(8))
            for offset in range(0, len(self.bits), 8)
        )


class BitReader:
    def __init__(self, data: bytes) -> None:
        self.bits = "".join(f"{byte:08b}" for byte in data)
        self.pos = 0
        self.fields: list[Field] = []

    def f(self, name: str, width: int, expected: int | None = None) -> int:
        if self.pos + width > len(self.bits):
            raise SyntaxError(f"{name}: truncated f({width})")
        start = self.pos
        value = int(self.bits[start : start + width] or "0", 2)
        self.pos += width
        if expected is not None and value != expected:
            raise SyntaxError(f"{name}: expected {expected}, got {value}")
        self.fields.append(Field(name, value, start, width, f"f({width})"))
        return value

    def uvlc(self, name: str, expected: int | None = None) -> int:
        start = self.pos
        leading = 0
        while True:
            done = self.f(f"{name}.done", 1)
            if done:
                break
            leading += 1
            if leading >= 32:
                raise SyntaxError(f"{name}: forbidden 32-zero uvlc prefix")
        suffix = self.f(f"{name}.value", leading) if leading else 0
        value = suffix + (1 << leading) - 1
        if expected is not None and value != expected:
            raise SyntaxError(f"{name}: expected {expected}, got {value}")
        self.fields.append(Field(name, value, start, self.pos - start, "uvlc()"))
        return value

    def ns(self, name: str, n: int, expected: int | None = None) -> int:
        if n <= 0:
            raise SyntaxError("ns domain must be positive")
        start = self.pos
        width = (n - 1).bit_length()
        cutoff = (1 << width) - n
        prefix = self.f(f"{name}.prefix", width - 1)
        value = prefix if prefix < cutoff else (prefix << 1) - cutoff + self.f(
            f"{name}.extra_bit", 1
        )
        if value >= n or (expected is not None and value != expected):
            raise SyntaxError(f"{name}: invalid ns({n}) value {value}")
        self.fields.append(Field(name, value, start, self.pos - start, f"ns({n})"))
        return value

    def trailing_bits(self) -> None:
        self.f("trailing_one_bit", 1, 1)
        while self.pos < len(self.bits):
            self.f("trailing_zero_bit", 1, 0)


def leb128_encode(value: int) -> bytes:
    if value < 0:
        raise SyntaxError("negative LEB128 value")
    result = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        result.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(result)


def leb128_decode(data: bytes, offset: int = 0) -> tuple[int, int]:
    value = 0
    for index in range(8):
        if offset + index >= len(data):
            raise SyntaxError("truncated LEB128")
        byte = data[offset + index]
        value |= (byte & 0x7F) << (7 * index)
        if byte < 0x80:
            if leb128_encode(value) != data[offset : offset + index + 1]:
                raise SyntaxError("non-canonical LEB128")
            return value, offset + index + 1
    raise SyntaxError("oversized LEB128")


def obu_header(obu_type: int, tlayer_id: int = 0) -> bytes:
    if not 0 <= obu_type < 32 or not 0 <= tlayer_id < 4:
        raise SyntaxError("invalid OBU header value")
    return bytes([(obu_type << 2) | tlayer_id])


def annexb_obu(obu_type: int, payload: bytes = b"") -> bytes:
    unit = obu_header(obu_type) + payload
    return leb128_encode(len(unit)) + unit


def parse_annexb(data: bytes) -> list[tuple[int, bytes]]:
    result = []
    offset = 0
    while offset < len(data):
        size, offset = leb128_decode(data, offset)
        end = offset + size
        if size < 1 or end > len(data):
            raise SyntaxError("invalid OBU unit size")
        header = data[offset]
        if header & 0x80:
            raise SyntaxError("OBU extension flag is unsupported by the M1 profile")
        result.append(((header >> 2) & 31, data[offset + 1 : end]))
        offset = end
    return result


def encode_sequence_header_8x8() -> tuple[bytes, list[Field]]:
    w = BitWriter()
    w.uvlc("seq_header_id", 0)
    w.f("seq_profile_idc", 0, 5)
    w.f("single_picture_header_flag", 1, 1)
    w.f("seq_level_idx", 0, 5)
    w.uvlc("chroma_format_idc", 1)
    w.uvlc("bit_depth_idc", 1)
    w.f("frame_width_bits_minus_1", 2, 4)
    w.f("frame_height_bits_minus_1", 2, 4)
    w.f("max_frame_width_minus_1", 7, 3)
    w.f("max_frame_height_minus_1", 7, 3)
    for name, value in (
        ("seq_cropping_window_present_flag", 0),
        ("use_256x256_superblock", 0),
        ("use_128x128_superblock", 0),
        ("enable_ext_partitions", 1),
        ("enable_uneven_4way_partitions", 1),
        ("reduce_pb_aspect_ratio", 0),
        ("enable_ext_seg", 0),
        ("seq_seg_info_present_flag", 0),
        ("enable_dip", 0),
        ("enable_intra_edge_filter", 1),
        ("enable_mrls", 1),
        ("enable_cfl_intra", 1),
        ("enable_mhccp", 1),
        ("enable_ibp", 1),
        ("enable_refmvbank", 1),
        ("disable_drl_reorder", 0),
        ("constrain_drl_reorder", 0),
    ):
        w.f(name, value, 1)
    w.ns("seq_max_bvp_drl_bits_minus_1", 2, 3)
    for name, value in (
        ("allow_frame_max_bvp_drl_bits", 0),
        ("enable_bawp", 1),
        ("enable_fsc", 1),
        ("enable_intra_ist", 1),
        ("enable_inter_ist", 1),
        ("reduced_tx_part_set", 0),
        ("enable_tcq", 0),
        ("enable_parity_hiding", 0),
        ("equal_ac_dc_q", 1),
        ("disable_loopfilters_across_tiles", 0),
        ("enable_cdef", 0),
        ("enable_gdf", 0),
        ("enable_restoration", 0),
        ("enable_ccso", 0),
    ):
        w.f(name, value, 1)
    w.f("df_par_bits_minus_2", 1, 2)
    w.f("seq_tile_info_present_flag", 0, 1)
    w.f("film_grain_params_present", 0, 1)
    w.f("obu_extension_flag", 0, 1)
    w.f("trailing_one_bit", 1, 1)
    w.align_zero()
    return w.bytes(), w.fields


def decode_sequence_header_8x8(payload: bytes) -> list[Field]:
    expected, _ = encode_sequence_header_8x8()
    if payload != expected:
        raise SyntaxError("sequence header differs from the exact M1 profile")
    # Re-encoding is the decoder/checker: every branch-dependent field above is
    # represented separately and the equality check includes trailing padding.
    return encode_sequence_header_8x8()[1]


def encode_frame_header_8x8() -> tuple[bytes, list[Field]]:
    w = BitWriter()
    w.uvlc("cur_mfh_id", 0)
    w.uvlc("seq_header_id_in_frame_header", 0)
    for name, value in (
        ("allow_screen_content_tools", 1),
        ("force_integer_mv", 0),
        ("allow_intrabc", 0),
        ("disable_cdf_update", 0),
        ("uniform_tile_spacing_flag", 1),
    ):
        w.f(name, value, 1)
    w.f("base_q_idx", 255, 8)
    w.f("segmentation_enabled", 0, 1)
    w.f("using_qmatrix", 0, 1)
    w.f("delta_q_present", 1, 1)
    w.f("delta_q_res", 2, 2)
    w.f("apply_deblocking_filter[0]", 1, 1)
    w.f("apply_deblocking_filter[1]", 1, 1)
    w.f("df_delta_q_present[0]", 0, 1)
    w.f("df_delta_q_present[1]", 0, 1)
    w.f("tx_mode_select", 0, 1)
    w.f("reduced_tx_set", 0, 2)
    w.align_zero()
    return w.bytes(), w.fields


def decode_frame_header_8x8(payload: bytes) -> tuple[list[Field], bytes]:
    header, fields = encode_frame_header_8x8()
    if not payload.startswith(header):
        raise SyntaxError("frame header differs from the exact M1 profile")
    tile = payload[len(header) :]
    if not tile:
        raise SyntaxError("missing tile arithmetic payload")
    return fields, tile


def build_m1_stream(tile_payload: bytes) -> bytes:
    sequence, _ = encode_sequence_header_8x8()
    frame_header, _ = encode_frame_header_8x8()
    return (
        annexb_obu(2)
        + annexb_obu(1, sequence)
        + annexb_obu(4, frame_header + tile_payload)
    )


def forced_partition_path_8x8() -> tuple[str, ...]:
    return (
        "64x64:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
        "32x32:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
        "16x16:PARTITION_SPLIT(implied-at-right-and-bottom-boundary)",
        "8x8:PARTITION_NONE(do_split=0)",
    )


@dataclass
class SymbolDecoder:
    data: bytes
    bit_pos: int = 0

    def __post_init__(self) -> None:
        num_bits = min(len(self.data) * 8, 15)
        buf = self._read(num_bits)
        self.value = ((1 << 15) - 1) ^ (buf << (15 - num_bits))
        self.range = 1 << 15
        self.max_bits = len(self.data) * 8 - 15

    def _read(self, count: int) -> int:
        value = 0
        for _ in range(count):
            value <<= 1
            if self.bit_pos < len(self.data) * 8:
                byte = self.data[self.bit_pos >> 3]
                value |= (byte >> (7 - (self.bit_pos & 7))) & 1
            self.bit_pos += 1
        return value

    def read_bool(self) -> int:
        cur = self.range >> 1
        symbol = int(self.value < cur)
        if symbol == 0:
            self.value -= cur
        num_bits = max(0, min(1, self.max_bits))
        new_data = self._read(num_bits)
        self.value = (self.value << 1) | (new_data ^ 1)
        self.max_bits -= 1
        return symbol

    def read_literal(self, bit_count: int) -> int:
        if not 0 <= bit_count <= 32:
            raise SyntaxError("invalid literal width")
        value = 0
        for _ in range(bit_count):
            value = (value << 1) | self.read_bool()
        return value

    def read_symbol(
        self,
        cdf: list[int],
        prob_inc: list[int] | None = None,
        *,
        symbol_count: int | None = None,
        update_cdf: bool = False,
        update_rates: tuple[int, int, int] | None = None,
    ) -> int:
        official_layout = symbol_count is None
        n = len(cdf) - 1 if official_layout else symbol_count
        increments = [16 * (n - symbol - 1) // n for symbol in range(n)]
        if (
            n < 2
            or len(cdf) < n + int(update_cdf)
            or (not official_layout and cdf[n - 1] != 0)
            or (prob_inc is not None and prob_inc[:n] != increments)
            or (update_cdf and (
                len(cdf) <= n
                or update_rates is None
                or len(update_rates) != 3
            ))
        ):
            raise SyntaxError("invalid CDF or probability-increment convention")
        cur = self.range
        symbol = -1
        while True:
            symbol += 1
            prev = cur
            frequency = 0 if symbol == n - 1 else (
                (1 << 15) - cdf[symbol]
                if official_layout else cdf[symbol]
            )
            pp = ((frequency >> 7) << 4) + increments[symbol]
            cur = ((((self.range >> 8) * pp) >> 7) << 3)
            if self.value >= cur:
                break
        new_range = prev - cur
        new_value = self.value - cur
        bits = 15 - (new_range.bit_length() - 1)
        self.range = new_range << bits
        num_bits = max(0, min(bits, self.max_bits))
        new_data = self._read(num_bits)
        padded = new_data << (bits - num_bits)
        self.value = (new_value << bits) | (padded ^ ((1 << bits) - 1))
        self.max_bits -= bits
        if update_cdf:
            interval = 2 if cdf[n] > 31 else 1 if cdf[n] > 15 else 0
            rate = update_rates[interval]
            if not 0 <= rate <= 15:
                raise SyntaxError("invalid CDF adaptation rate")
            if official_layout:
                for index in range(n - 1):
                    if index < symbol:
                        cdf[index] -= cdf[index] >> rate
                    else:
                        cdf[index] += ((1 << 15) - cdf[index]) >> rate
            else:
                target = 1 << 15
                for index in range(n - 1):
                    if index == symbol:
                        target = 0
                    if target < cdf[index]:
                        cdf[index] -= (cdf[index] - target) >> rate
                    else:
                        cdf[index] += (target - cdf[index]) >> rate
            cdf[n] += int(cdf[n] < 32)
        return symbol

    def check_exit(self) -> None:
        if self.max_bits < -14:
            raise SyntaxError("SymbolMaxBits is below -14")
        trailing = self.bit_pos - min(15, self.max_bits + 15)
        padding_end = self.bit_pos + max(0, self.max_bits)
        bits = "".join(f"{byte:08b}" for byte in self.data)
        if trailing < 0 or padding_end > len(bits) or bits[trailing] != "1":
            raise SyntaxError("missing symbol-decoder trailing one bit")
        if "1" in bits[trailing + 1 : padding_end]:
            raise SyntaxError("nonzero symbol-decoder trailing padding")

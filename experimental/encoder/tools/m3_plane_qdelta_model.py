#!/usr/bin/env python3
"""Isolated AV2 v1.0.0 frame chroma quantizer-delta model."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Sequence

from m1_syntax import BitWriter, annexb_obu


AC_QLOOKUP = (
    64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
    57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,
)
FRAME_DELTA_MIN = -64
FRAME_DELTA_MAX = 63
SEQUENCE_BASE_DELTA_MIN = -23
SEQUENCE_BASE_DELTA_MAX = 8


@dataclass(frozen=True)
class QuantizerDeltaConfig:
    separate_uv_delta_q: bool = False
    equal_ac_dc_q: bool = True
    base_y_dc_delta_q: int = 0
    y_dc_delta_q_enabled: bool = False
    base_uv_dc_delta_q: int = 0
    uv_dc_delta_q_enabled: bool = False
    base_uv_ac_delta_q: int = 0
    uv_ac_delta_q_enabled: bool = True

    def __post_init__(self) -> None:
        for value in (
            self.base_y_dc_delta_q,
            self.base_uv_dc_delta_q,
            self.base_uv_ac_delta_q,
        ):
            if not SEQUENCE_BASE_DELTA_MIN <= value <= SEQUENCE_BASE_DELTA_MAX:
                raise ValueError("sequence base delta must be in -23..8")
        if self.equal_ac_dc_q:
            if self.y_dc_delta_q_enabled:
                raise ValueError("equal AC/DC forbids Y DC frame deltas")
            if self.uv_dc_delta_q_enabled:
                raise ValueError("equal AC/DC forbids UV DC frame deltas")
            if self.base_y_dc_delta_q != 0:
                raise ValueError("equal AC/DC requires zero base Y DC delta")
            if self.base_uv_dc_delta_q != self.base_uv_ac_delta_q:
                raise ValueError("equal AC/DC shares the UV base delta")


@dataclass(frozen=True)
class FrameQuantizerDeltas:
    y_dc: int = 0
    u_dc: int = 0
    u_ac: int = 0
    v_dc: int = 0
    v_ac: int = 0

    def __post_init__(self) -> None:
        for value in (self.y_dc, self.u_dc, self.u_ac, self.v_dc, self.v_ac):
            if not FRAME_DELTA_MIN <= value <= FRAME_DELTA_MAX:
                raise ValueError("frame delta must be in -64..63")


@dataclass(frozen=True)
class Candidate:
    u_delta: int
    v_delta: int
    levels: tuple[int, int]
    reconstruction: tuple[int, int]
    weighted_chroma_sse: int
    stream_bytes: int
    header_bits: int


def clip3(low: int, high: int, value: int) -> int:
    return min(high, max(low, value))


def round2(value: int, bits: int) -> int:
    return (value + (1 << (bits - 1))) >> bits if bits else value


def qlookup(qindex: int) -> int:
    if not 0 <= qindex <= 255:
        raise ValueError("8-bit qindex must be in 0..255")
    if qindex < 25:
        return AC_QLOOKUP[qindex]
    return AC_QLOOKUP[((qindex - 1) % 24) + 1] << ((qindex - 1) // 24)


def dequant_step(base_qindex: int, frame_delta: int,
                 sequence_base_delta: int = 0) -> int:
    total_delta = frame_delta + sequence_base_delta
    if base_qindex == 0 and total_delta <= 0:
        return AC_QLOOKUP[0]
    return qlookup(clip3(1, 255, base_qindex + total_delta))


def coeff_q_context(base_qindex: int) -> int:
    return 0 if base_qindex <= 90 else (
        1 if base_qindex <= 140 else 2 if base_qindex <= 190 else 3)


def delta_code_bits(value: int) -> str:
    if not FRAME_DELTA_MIN <= value <= FRAME_DELTA_MAX:
        raise ValueError("frame delta must be in -64..63")
    if value == 0:
        return "0"
    return "1" + f"{value & 0x7f:07b}"


def frame_delta_payload_bits(config: QuantizerDeltaConfig,
                             deltas: FrameQuantizerDeltas) -> str:
    if not config.y_dc_delta_q_enabled and deltas.y_dc != 0:
        raise ValueError("disabled Y DC delta must be zero")
    if config.equal_ac_dc_q:
        if deltas.u_dc != deltas.u_ac or deltas.v_dc != deltas.v_ac:
            raise ValueError("equal AC/DC requires matching plane deltas")
    elif not config.uv_dc_delta_q_enabled and (
        deltas.u_dc != 0 or deltas.v_dc != 0
    ):
        raise ValueError("disabled UV DC deltas must be zero")
    if not config.uv_ac_delta_q_enabled and (
        deltas.u_ac != 0 or deltas.v_ac != 0
    ):
        raise ValueError("disabled UV AC deltas must be zero")
    bits = ""
    if config.y_dc_delta_q_enabled:
        bits += delta_code_bits(deltas.y_dc)
    if config.uv_dc_delta_q_enabled or config.uv_ac_delta_q_enabled:
        different = (
            deltas.u_dc != deltas.v_dc or deltas.u_ac != deltas.v_ac
        )
        if config.separate_uv_delta_q:
            bits += "1" if different else "0"
        elif different:
            raise ValueError("V deltas must equal U when separate UV is off")
        if not config.equal_ac_dc_q and config.uv_dc_delta_q_enabled:
            bits += delta_code_bits(deltas.u_dc)
        if config.uv_ac_delta_q_enabled:
            bits += delta_code_bits(deltas.u_ac)
        if different:
            if not config.equal_ac_dc_q and config.uv_dc_delta_q_enabled:
                bits += delta_code_bits(deltas.v_dc)
            if config.uv_ac_delta_q_enabled:
                bits += delta_code_bits(deltas.v_ac)
    return bits


def _dimension_bits(value: int) -> int:
    if not 1 <= value <= 16384:
        raise ValueError("dimension must be in 1..16384")
    return (value - 1).bit_length() or 1


def encode_sequence_header(width: int, height: int, format_id: int,
                           config: QuantizerDeltaConfig) -> bytes:
    if format_id not in (0, 2):
        raise ValueError("qdelta model admits only I420 and I444")
    width_bits = _dimension_bits(width)
    height_bits = _dimension_bits(height)
    w = BitWriter()
    w.uvlc("seq_header_id", 0)
    w.f("seq_profile_idc", 4 if format_id == 2 else 0, 5)
    w.f("single_picture_header_flag", 1, 1)
    w.f("seq_level_idx", 1 if width * height > 552960 else 0, 5)
    w.uvlc("chroma_format_idc", format_id)
    w.uvlc("bit_depth_idc", 1)
    w.f("frame_width_bits_minus_1", width_bits - 1, 4)
    w.f("frame_height_bits_minus_1", height_bits - 1, 4)
    w.f("max_frame_width_minus_1", width - 1, width_bits)
    w.f("max_frame_height_minus_1", height - 1, height_bits)
    for name, value in (
        ("seq_cropping_window_present_flag", 0),
        ("use_256x256_superblock", 0),
        ("use_128x128_superblock", 0),
        ("enable_sdp", 1),
        ("enable_ext_partitions", 1),
        ("enable_uneven_4way_partitions", 1),
        ("reduce_pb_aspect_ratio", 0),
        ("enable_ext_seg", 0),
        ("seq_seg_info_present_flag", 0),
        ("enable_dip", 0),
        ("enable_intra_edge_filter", 1),
        ("enable_mrls", 1),
        ("enable_cfl_intra", 1),
    ):
        w.f(name, value, 1)
    w.f("cfl_downsample_filter_index", 0, 2)
    for name, value in (
        ("enable_mhccp", 1),
        ("enable_ibp", 0),
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
        ("enable_chroma_dctonly", 1),
        ("reduced_tx_part_set", 0),
        ("enable_cctx", 0),
        ("enable_tcq", 0),
        ("enable_parity_hiding", 0),
    ):
        w.f(name, value, 1)
    w.f("separate_uv_delta_q", int(config.separate_uv_delta_q), 1)
    w.f("equal_ac_dc_q", int(config.equal_ac_dc_q), 1)
    if not config.equal_ac_dc_q:
        w.f("base_y_dc_delta_q", config.base_y_dc_delta_q + 23, 5)
        w.f("y_dc_delta_q_enabled", int(config.y_dc_delta_q_enabled), 1)
        w.f("base_uv_dc_delta_q", config.base_uv_dc_delta_q + 23, 5)
        w.f("uv_dc_delta_q_enabled", int(config.uv_dc_delta_q_enabled), 1)
    w.f("base_uv_ac_delta_q", config.base_uv_ac_delta_q + 23, 5)
    w.f("uv_ac_delta_q_enabled", int(config.uv_ac_delta_q_enabled), 1)
    for name, value in (
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
    return w.bytes()


def encode_frame_header(width: int, height: int, qindex: int,
                        config: QuantizerDeltaConfig,
                        deltas: FrameQuantizerDeltas) -> tuple[bytes, int]:
    if not 1 <= qindex <= 255:
        raise ValueError("admitted qindex must be in 1..255")
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
    if width > 64:
        w.f("tile_columns_log2", 0, 1)
    if height > 64:
        w.f("tile_rows_log2", 0, 1)
    w.f("base_q_idx", qindex, 8)
    payload = frame_delta_payload_bits(config, deltas)
    for index, bit in enumerate(payload):
        w.f(f"plane_delta_bit[{index}]", int(bit), 1)
    for name, value in (
        ("segmentation_enabled", 0),
        ("using_qmatrix", 0),
        ("delta_q_present", 0),
        ("apply_deblocking_filter[0]", 0),
        ("apply_deblocking_filter[1]", 0),
        ("tx_mode_select", 1),
    ):
        w.f(name, value, 1)
    w.f("reduced_tx_set", 0, 2)
    unaligned_bits = len(w.bits)
    w.align_zero()
    return w.bytes(), unaligned_bits


def build_stream(width: int, height: int, format_id: int, qindex: int,
                 config: QuantizerDeltaConfig, deltas: FrameQuantizerDeltas,
                 tile_payload: bytes) -> tuple[bytes, int]:
    sequence = encode_sequence_header(width, height, format_id, config)
    frame, header_bits = encode_frame_header(
        width, height, qindex, config, deltas)
    return (
        annexb_obu(2)
        + annexb_obu(1, sequence)
        + annexb_obu(4, frame + tile_payload),
        header_bits,
    )


def reconstruct_dc(level: int, qindex: int, delta: int, size: int,
                   prediction: int = 128) -> int:
    if size not in (4, 8):
        raise ValueError("bounded chroma model supports 4x4 and 8x8")
    magnitude = round2(abs(level) * dequant_step(qindex, delta), 3)
    dequantized = -magnitude if level < 0 else magnitude
    row = clip3(-(1 << 15), (1 << 15) - 1, round2(64 * dequantized, 7))
    residual = round2(64 * row, 10 if size == 4 else 11)
    return clip3(0, 255, prediction + residual)


def choose_dc_level(samples: Sequence[int], qindex: int, delta: int,
                    size: int, prediction: int = 128) -> tuple[int, int, int]:
    if len(samples) != size * size:
        raise ValueError("sample count does not match transform")
    step = dequant_step(qindex, delta)
    maximum = ((1 << 15) * 8 + step - 1) // step + 1
    best = None
    for level in (0, *range(1, maximum + 1), *range(-1, -maximum - 1, -1)):
        value = reconstruct_dc(level, qindex, delta, size, prediction)
        error = sum((sample - value) ** 2 for sample in samples)
        key = (error, abs(level), level < 0)
        if best is None or key < best[0]:
            best = (key, level, value, error)
    assert best is not None
    return best[1], best[2], best[3]


def candidate_delta_values() -> tuple[int, ...]:
    return (0, -16, -8, 8, 16)


def candidate_pairs(u_samples: Sequence[int], v_samples: Sequence[int],
                    qindex: int, size: int) -> tuple[tuple[int, int], ...]:
    values = candidate_delta_values()
    shared = [(value, value) for value in values]
    u_best = min(
        values,
        key=lambda value: (
            choose_dc_level(u_samples, qindex, value, size)[2],
            abs(value), value,
        ),
    )
    v_best = min(
        values,
        key=lambda value: (
            choose_dc_level(v_samples, qindex, value, size)[2],
            abs(value), value,
        ),
    )
    return tuple(dict.fromkeys((*shared, (u_best, v_best))))


def select_candidate(candidates: Iterable[Candidate],
                     baseline_bytes: int,
                     maximum_added_bytes: int = 2) -> Candidate:
    admitted = [
        candidate for candidate in candidates
        if candidate.stream_bytes <= baseline_bytes + maximum_added_bytes
    ]
    if not admitted:
        raise ValueError("candidate set has no rate-admitted member")
    return min(
        admitted,
        key=lambda candidate: (
            candidate.weighted_chroma_sse,
            candidate.stream_bytes,
            abs(candidate.u_delta) + abs(candidate.v_delta),
            candidate.u_delta != candidate.v_delta,
            candidate.u_delta,
            candidate.v_delta,
        ),
    )

#include "frame.h"

static void frame_fail(Av2hBitReader *reader, Av2hStatus status) {
    if (reader->status == AV2H_OK) reader->status = status;
}

static uint32_t read_ref_quasi_uniform(Av2hBitReader *reader,
                                       uint32_t symbol_count,
                                       uint32_t reference) {
    uint32_t value;
    if (symbol_count <= 1U || reference >= symbol_count) {
        frame_fail(reader, AV2H_INVALID_ARGUMENT);
        return 0U;
    }
    if (av2h_bit_reader_read(reader, 1U) == 0U) return reference;
    value = av2h_bit_reader_quasi_uniform(reader, symbol_count - 1U);
    if (reader->status != AV2H_OK) return 0U;
    return value + (value >= reference ? 1U : 0U);
}

static int16_t read_delta_q(Av2hBitReader *reader) {
    if (av2h_bit_reader_read(reader, 1U) == 0U) return 0;
    return (int16_t)av2h_bit_reader_inverse_signed(reader, 6U);
}

static unsigned int ceil_log2_u32(uint32_t value) {
    unsigned int bits = 0U;
    uint32_t probe = value > 0U ? value - 1U : 0U;
    while (probe != 0U) {
        ++bits;
        probe >>= 1U;
    }
    return bits;
}

static void derive_gdf_block_geometry(
    const Av2hSequenceState *sequence,
    Av2hFrameState *frame) {
    uint32_t block_size = sequence->superblock_size;
    if (sequence->gdf_unit_matches_superblock_size == 0U &&
        block_size < 128U) {
        block_size = 128U;
    }
    frame->gdf_block_size = (uint16_t)block_size;
    frame->gdf_block_columns = (uint16_t)(
        ((uint32_t)frame->width + block_size - 1U) / block_size);
    frame->gdf_block_rows = (uint16_t)(
        ((uint32_t)frame->height + block_size - 1U) / block_size);
    frame->gdf_block_count =
        (uint32_t)frame->gdf_block_columns * frame->gdf_block_rows;
}

static int read_one_tile(Av2hBitReader *reader,
                         const Av2hSequenceState *sequence,
                         Av2hFrameState *frame) {
    uint32_t sb_columns;
    uint32_t sb_rows;
    unsigned int max_log2_columns;
    unsigned int max_log2_rows;
    if (sequence->superblock_size == 0U) {
        frame_fail(reader, AV2H_INVALID_DATA);
        return 0;
    }
    sb_columns = ((uint32_t)frame->width + sequence->superblock_size - 1U) /
                 sequence->superblock_size;
    sb_rows = ((uint32_t)frame->height + sequence->superblock_size - 1U) /
              sequence->superblock_size;
    max_log2_columns = ceil_log2_u32(sb_columns);
    max_log2_rows = ceil_log2_u32(sb_rows);
    frame->uniform_tile_spacing =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (frame->uniform_tile_spacing == 0U) {
        frame_fail(reader, AV2H_UNSUPPORTED);
        return 0;
    }
    if (max_log2_columns != 0U &&
        av2h_bit_reader_read(reader, 1U) != 0U) {
        frame_fail(reader, AV2H_UNSUPPORTED);
        return 0;
    }
    if (max_log2_rows != 0U && av2h_bit_reader_read(reader, 1U) != 0U) {
        frame_fail(reader, AV2H_UNSUPPORTED);
        return 0;
    }
    return reader->status == AV2H_OK;
}

static void read_quantization(Av2hBitReader *reader,
                              const Av2hSequenceState *sequence,
                              Av2hFrameState *frame) {
    uint8_t different_uv = 0U;
    unsigned int q_index_bits =
        sequence->info.bit_depth_idc == 1U ? 8U : 9U;
    frame->base_q_index =
        (Av2hQIndex)av2h_bit_reader_read(reader, q_index_bits);
    if (sequence->y_dc_delta_q_enabled != 0U) {
        frame->y_dc_delta_q = read_delta_q(reader);
    }
    if (!av2h_sequence_is_monochrome(sequence) &&
        (sequence->uv_dc_delta_q_enabled != 0U ||
         sequence->uv_ac_delta_q_enabled != 0U)) {
        if (sequence->separate_uv_delta_q != 0U) {
            different_uv = (uint8_t)av2h_bit_reader_read(reader, 1U);
        }
        if (sequence->equal_ac_dc_q == 0U &&
            sequence->uv_dc_delta_q_enabled != 0U) {
            frame->u_dc_delta_q = read_delta_q(reader);
        }
        if (sequence->uv_ac_delta_q_enabled != 0U) {
            frame->u_ac_delta_q = read_delta_q(reader);
        }
        if (sequence->equal_ac_dc_q != 0U) {
            frame->u_dc_delta_q = frame->u_ac_delta_q;
        }
        if (different_uv != 0U) {
            if (sequence->equal_ac_dc_q == 0U &&
                sequence->uv_dc_delta_q_enabled != 0U) {
                frame->v_dc_delta_q = read_delta_q(reader);
            }
            if (sequence->uv_ac_delta_q_enabled != 0U) {
                frame->v_ac_delta_q = read_delta_q(reader);
            }
            if (sequence->equal_ac_dc_q != 0U) {
                frame->v_dc_delta_q = frame->v_ac_delta_q;
            }
        } else {
            frame->v_dc_delta_q = frame->u_dc_delta_q;
            frame->v_ac_delta_q = frame->u_ac_delta_q;
        }
    }
}

static void read_deblock(Av2hBitReader *reader,
                         const Av2hSequenceState *sequence,
                         Av2hFrameState *frame) {
    unsigned int index;
    unsigned int parameter_bits =
        (unsigned int)sequence->deblock_parameter_bits_minus_2 + 2U;
    int16_t offset = (int16_t)((uint16_t)1U << (parameter_bits - 1U));
    frame->deblock_enabled[0] =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    frame->deblock_enabled[1] =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (av2h_frame_plane_count(frame) > 1U &&
        (frame->deblock_enabled[0] != 0U ||
         frame->deblock_enabled[1] != 0U)) {
        frame->deblock_enabled[2] =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
        frame->deblock_enabled[3] =
            (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
        for (index = 0U;
            index < (av2h_frame_plane_count(frame) == 1U ? 2U : 4U);
            ++index) {
        if (frame->deblock_enabled[index] != 0U) {
            if (av2h_bit_reader_read(reader, 1U) != 0U) {
                frame->deblock_delta[index] =
                    (int16_t)av2h_bit_reader_read(reader, parameter_bits) -
                    offset;
            } else if (index == 1U) {
                frame->deblock_delta[index] = frame->deblock_delta[0];
            }
        }
    }
}

static void read_cdef_strength(Av2hBitReader *reader, uint8_t *strength) {
    if (av2h_bit_reader_read(reader, 1U) != 0U) {
        *strength = (uint8_t)av2h_bit_reader_read(reader, 2U);
    } else {
        *strength = (uint8_t)av2h_bit_reader_read(reader, 6U);
    }
}

static void read_cdef(Av2hBitReader *reader, Av2hFrameState *frame) {
    unsigned int index;
    frame->cdef_damping =
        (uint8_t)(av2h_bit_reader_read(reader, 2U) + 3U);
    frame->cdef_strength_count =
        (uint8_t)(av2h_bit_reader_read(reader, 3U) + 1U);
    frame->cdef_on_skip_transform =
        (uint8_t)av2h_bit_reader_read(reader, 1U);
    for (index = 0U; index < frame->cdef_strength_count; ++index) {
        read_cdef_strength(reader, &frame->cdef_y_strength[index]);
        if (av2h_frame_plane_count(frame) > 1U) {
            read_cdef_strength(reader, &frame->cdef_uv_strength[index]);
        }
    }
}

static uint16_t restoration_unit_count(uint16_t dimension,
                                       uint16_t unit_size) {
    uint32_t count = ((uint32_t)dimension + unit_size / 2U) / unit_size;
    return (uint16_t)(count != 0U ? count : 1U);
}

static void read_restoration(Av2hBitReader *reader,
                             const Av2hSequenceState *sequence,
                             Av2hFrameState *frame) {
    unsigned int plane;
    uint8_t uses_luma = 0U;
    uint8_t uses_chroma = 0U;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint8_t index_to_tool[4] = { AV2H_RESTORE_NONE, 0U, 0U, 0U };
        uint8_t mask = sequence->lr_tools_disable_mask[plane != 0U];
        uint32_t tool_count = 1U;
        uint32_t tool_index;
        unsigned int tool;
        for (tool = AV2H_RESTORE_PC_WIENER;
             tool < AV2H_RESTORE_SWITCHABLE; ++tool) {
            if ((mask & (1U << tool)) == 0U) {
                index_to_tool[tool_count++] = (uint8_t)tool;
            }
        }
        if (tool_count > 2U) {
            index_to_tool[tool_count++] = AV2H_RESTORE_SWITCHABLE;
        }
        tool_index = av2h_bit_reader_quasi_uniform(reader, tool_count);
        if (reader->status != AV2H_OK || tool_index >= tool_count) return;
        frame->restoration_type[plane] = index_to_tool[tool_index];
        if (frame->restoration_type[plane] != AV2H_RESTORE_NONE) {
            if (plane == 0U) uses_luma = 1U;
            else uses_chroma = 1U;
        }
        if (frame->restoration_type[plane] != AV2H_RESTORE_NONE &&
            (plane != 0U || frame->restoration_type[plane] !=
                 AV2H_RESTORE_PC_WIENER)) {
            frame_fail(reader, AV2H_UNSUPPORTED);
            return;
        }
    }
    frame->restoration_unit_size[0] = 64U;
    frame->restoration_unit_size[1] = 32U;
    frame->restoration_unit_size[2] = 32U;
    if (uses_luma != 0U) {
        unsigned int shift;
        if (av2h_bit_reader_read(reader, 1U) != 0U) {
            shift = 1U;
        } else if (sequence->superblock_size == 256U) {
            shift = 0U;
        } else if (av2h_bit_reader_read(reader, 1U) != 0U) {
            shift = 0U;
        } else if (sequence->superblock_size == 128U) {
            shift = 2U;
        } else {
            shift = av2h_bit_reader_read(reader, 1U) != 0U ? 2U : 3U;
        }
        frame->restoration_unit_size[0] = (uint16_t)(512U >> shift);
    }
    if (uses_chroma != 0U) {
        frame_fail(reader, AV2H_UNSUPPORTED);
        return;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint16_t width = (uint16_t)av2h_frame_plane_dimension(
            frame, plane, frame->width);
        uint16_t height = (uint16_t)av2h_frame_plane_dimension(
            frame, plane, frame->height);
        frame->restoration_unit_columns[plane] = restoration_unit_count(
            width, frame->restoration_unit_size[plane]);
        frame->restoration_unit_rows[plane] = restoration_unit_count(
            height, frame->restoration_unit_size[plane]);
        frame->restoration_unit_count[plane] =
            (uint32_t)frame->restoration_unit_columns[plane] *
            frame->restoration_unit_rows[plane];
    }
}

static int8_t ccso_offset_from_index(unsigned int index,
                                     unsigned int scale_index) {
    static const int8_t values[8] = { 0, 1, -1, 3, -3, 7, -7, -10 };
    return (int8_t)(values[index] * (int8_t)(scale_index + 1U));
}

static uint16_t read_ccso_offsets(Av2hBitReader *reader,
                                  Av2hCcsoPlane *plane,
                                  uint32_t edge_intervals,
                                  uint32_t band_count) {
    uint32_t first;
    uint32_t second;
    uint32_t band;
    uint16_t count = 0U;
    for (first = 0U; first < edge_intervals; ++first) {
        for (second = 0U; second < edge_intervals; ++second) {
            for (band = 0U; band < band_count; ++band) {
                unsigned int unary;
                for (unary = 0U; unary < 7U; ++unary) {
                    if (av2h_bit_reader_read(reader, 1U) == 0U) break;
                }
                plane->filter_offset[(band << 4U) + (first << 2U) + second] =
                    ccso_offset_from_index(unary, plane->scale_index);
                ++count;
                if (reader->status != AV2H_OK) return count;
            }
        }
    }
    return count;
}

static void read_ccso(Av2hBitReader *reader, Av2hFrameState *frame) {
    unsigned int plane_index;
    for (plane_index = 0U; plane_index < frame->plane_count;
         ++plane_index) {
        Av2hCcsoPlane *plane = &frame->ccso[plane_index];
        uint32_t edge_intervals;
        uint32_t band_count;
        plane->enabled = (uint8_t)av2h_bit_reader_read(reader, 1U);
        if (plane->enabled == 0U) continue;
        plane->band_only = (uint8_t)av2h_bit_reader_read(reader, 1U);
        plane->scale_index =
            (uint8_t)av2h_bit_reader_read(reader, 2U);
        if (plane->band_only != 0U) {
            plane->max_band_log2 =
                (uint8_t)av2h_bit_reader_read(reader, 3U);
            if (plane->max_band_log2 > 6U) {
                frame_fail(reader, AV2H_INVALID_DATA);
                return;
            }
            edge_intervals = 1U;
        } else {
            plane->quant_index =
                (uint8_t)av2h_bit_reader_read(reader, 2U);
            plane->extended_filter_support =
                (uint8_t)av2h_bit_reader_read(reader, 3U);
            if (plane->extended_filter_support == 7U) {
                frame_fail(reader, AV2H_INVALID_DATA);
                return;
            }
            if (!(plane->scale_index == 0U && plane->quant_index == 3U)) {
                plane->edge_classifier =
                    (uint8_t)av2h_bit_reader_read(reader, 1U);
            }
            plane->max_band_log2 =
                (uint8_t)av2h_bit_reader_read(reader, 2U);
            edge_intervals = plane->edge_classifier != 0U ? 2U : 3U;
        }
        band_count = (uint32_t)1U << plane->max_band_log2;
        plane->offset_count =
            read_ccso_offsets(reader, plane, edge_intervals, band_count);
        if (reader->status != AV2H_OK) return;
    }
}

Av2hStatus av2h_frame_header_closed_key_parse(
    Av2hSpan payload,
    const Av2hSequenceState *sequence,
    Av2hFrameState *frame) {
    Av2hBitReader reader;
    uint32_t multi_frame_header_id;
    uint32_t sequence_header_id;
    if (sequence == 0 || frame == 0 ||
        (payload.data == 0 && payload.size != 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(frame, 0U, sizeof(*frame));
    frame->plane_count = (uint8_t)av2h_sequence_plane_count(sequence);
    frame->chroma_format_idc = sequence->info.chroma_format_idc;
    frame->ccso_unit_size =
        sequence->ccso_unit_matches_superblock_size ?
            sequence->superblock_size : 256U;
    if (payload.size == 0U) return AV2H_TRUNCATED;
    if (sequence->info.still_picture == 0U ||
        (sequence->info.bit_depth_idc != 1U &&
         (sequence->info.bit_depth_idc != 0U ||
          !((sequence->info.seq_profile_idc == 0U &&
             sequence->info.chroma_format_idc == 0U) ||
            (sequence->info.seq_profile_idc == 4U &&
             sequence->info.chroma_format_idc == 2U)))) ||
        sequence->info.chroma_format_idc > 2U ||
        sequence->seq_seg_info_present_flag != 0U ||
        sequence->seq_tile_info_present_flag != 0U) {
        return AV2H_UNSUPPORTED;
    }
    av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
    frame->first_tile_group =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->first_tile_group == 0U) return AV2H_UNSUPPORTED;
    multi_frame_header_id = av2h_bit_reader_uvlc(&reader);
    sequence_header_id = av2h_bit_reader_uvlc(&reader);
    if (reader.status != AV2H_OK) return reader.status;
    if (multi_frame_header_id != 0U ||
        sequence_header_id != sequence->sequence_header_id) {
        return AV2H_UNSUPPORTED;
    }
    frame->width = (uint16_t)(sequence->info.max_frame_width_minus_1 + 1U);
    frame->height = (uint16_t)(sequence->info.max_frame_height_minus_1 + 1U);
    frame->allow_screen_content_tools =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->allow_screen_content_tools != 0U) {
        frame->force_integer_mv =
            (uint8_t)av2h_bit_reader_read(&reader, 1U);
    }
    frame->allow_intrabc = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->allow_intrabc != 0U) {
        frame->allow_global_intrabc =
            (uint8_t)av2h_bit_reader_read(&reader, 1U);
        frame->allow_local_intrabc = frame->allow_global_intrabc != 0U
            ? (uint8_t)av2h_bit_reader_read(&reader, 1U) : 1U;
        frame->max_bvp_drl_bits = sequence->default_max_bvp_drl_bits;
        if (sequence->allow_frame_max_bvp_drl_bits != 0U) {
            frame->max_bvp_drl_bits = (uint8_t)
                (read_ref_quasi_uniform(
                     &reader, 3U,
                     (uint32_t)sequence->default_max_bvp_drl_bits - 1U) +
                 1U);
        }
    }
    frame->disable_cdf_update =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (!read_one_tile(&reader, sequence, frame)) return reader.status;
    read_quantization(&reader, sequence, frame);
    frame->segmentation_enabled =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->segmentation_enabled != 0U) return AV2H_UNSUPPORTED;
    frame->using_qmatrix =
        (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->using_qmatrix != 0U) {
        frame->qm_y = (uint8_t)av2h_bit_reader_read(&reader, 4U);
        if (frame->plane_count > 1U) {
            uint8_t qm_uv_same_as_y =
                (uint8_t)av2h_bit_reader_read(&reader, 1U);
            if (qm_uv_same_as_y != 0U) {
                frame->qm_u = frame->qm_y;
                frame->qm_v = frame->qm_y;
            } else {
                frame->qm_u = (uint8_t)av2h_bit_reader_read(&reader, 4U);
                frame->qm_v = sequence->separate_uv_delta_q != 0U ?
                    (uint8_t)av2h_bit_reader_read(&reader, 4U) : frame->qm_u;
            }
        }
    }
    if (frame->base_q_index != 0U) {
        frame->delta_q_present =
            (uint8_t)av2h_bit_reader_read(&reader, 1U);
        if (frame->delta_q_present != 0U) {
            frame->delta_q_res = (uint8_t)(
                1U << av2h_bit_reader_read(&reader, 2U));
        }
    }
    if (reader.status != AV2H_OK) return reader.status;
    if (frame->base_q_index == 0U) {
        frame->tx_mode = 1U;
        frame->reduced_tx_set =
            (uint8_t)av2h_bit_reader_read(&reader, 2U);
        if (reader.status != AV2H_OK) return reader.status;
        frame->uncompressed_header_bits = reader.bit_position - 1U;
        return AV2H_OK;
    }
    read_deblock(&reader, sequence, frame);
    if (sequence->enable_gdf != 0U) {
        derive_gdf_block_geometry(sequence, frame);
        frame->gdf_mode = 1U;
        if (frame->gdf_block_count > 1U) {
            frame->gdf_mode = (uint8_t)(
                frame->gdf_mode + av2h_bit_reader_read(&reader, 1U));
        }
        frame->gdf_qp_index =
            (uint8_t)av2h_bit_reader_read(&reader, 2U);
        frame->gdf_scale_index =
            (uint8_t)av2h_bit_reader_read(&reader, 2U);
    }
    if (sequence->enable_cdef != 0U) read_cdef(&reader, frame);
    if (sequence->enable_restoration != 0U) {
        read_restoration(&reader, sequence, frame);
    }
    if (sequence->enable_ccso != 0U) read_ccso(&reader, frame);
    frame->tx_mode = av2h_bit_reader_read(&reader, 1U) != 0U ? 2U : 1U;
    frame->reduced_tx_set =
        (uint8_t)av2h_bit_reader_read(&reader, 2U);
    if (reader.status != AV2H_OK) return reader.status;
    frame->uncompressed_header_bits = reader.bit_position - 1U;
    return AV2H_OK;
}
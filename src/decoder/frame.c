#include "frame.h"

#include "coeff_level.h"
#include "error.h"
#include "internal/leaf_modules.h"
#include "internal/wiener_nonsep.h"
#include "shared/bit_math.h"

#include <stddef.h>

static void frame_fail(Av2hBitReader *reader, Av2hStatus status) {
    if (reader->status == AV2H_OK)
        reader->status = status;
}

static uint32_t read_ref_quasi_uniform(Av2hBitReader *reader, uint32_t symbol_count,
                                       uint32_t reference) {
    uint32_t value;
    if (symbol_count <= 1U || reference >= symbol_count) {
        frame_fail(reader, AV2H_INVALID_ARGUMENT);
        return 0U;
    }
    if (av2h_bit_reader_read(reader, 1U) == 0U)
        return reference;
    value = av2h_bit_reader_quasi_uniform(reader, symbol_count - 1U);
    if (reader->status != AV2H_OK)
        return 0U;
    return value + (value >= reference ? 1U : 0U);
}

static int16_t read_delta_q(Av2hBitReader *reader) {
    if (av2h_bit_reader_read(reader, 1U) == 0U)
        return 0;
    return (int16_t)av2h_bit_reader_inverse_signed(reader, 6U);
}

static int read_segmentation_features(Av2hBitReader *reader, size_t segment_count,
                                      uint8_t feature_mask[AV2H_SEGMENT_COUNT],
                                      int16_t alt_q[AV2H_SEGMENT_COUNT], uint8_t *last_active_id) {
    size_t segment;
    for (segment = 0U; segment < segment_count; ++segment) {
        unsigned int feature;
        for (feature = 0U; feature < 3U; ++feature) {
            uint8_t enabled = (uint8_t)av2h_bit_reader_read(reader, 1U);
            if (enabled == 0U)
                continue;
            feature_mask[segment] |= (uint8_t)(1U << feature);
            if (feature != 0U)
                return 0;
            alt_q[segment] = (int16_t)av2h_bit_reader_inverse_signed(reader, 9U);
            if (alt_q[segment] < -351 || alt_q[segment] > 351)
                return 0;
            *last_active_id = (uint8_t)segment;
        }
    }
    return reader->status == AV2H_OK;
}

static uint32_t tile_width_scale(const Av2hSequenceState *sequence) {
    unsigned int level = sequence->info.seq_level_idx;
    if (level > 21U)
        return 0U;
    if (sequence->info.seq_tier != 0U) {
        if (level >= 18U)
            return 16U;
        if (level >= 14U)
            return 8U;
        return 4U;
    }
    return level >= 18U ? 8U : 4U;
}

static uint32_t tile_area_scale(const Av2hSequenceState *sequence) {
    unsigned int level = sequence->info.seq_level_idx;
    if (level > 21U)
        return 0U;
    if (sequence->info.seq_tier != 0U) {
        if (level >= 18U)
            return 32U;
        if (level >= 14U)
            return 16U;
        return 4U;
    }
    if (level >= 18U)
        return 16U;
    if (level >= 14U)
        return 8U;
    return 4U;
}

static void calculate_uniform_tile_starts(uint32_t mi_dimension, uint32_t superblock_mi,
                                          unsigned int log2_count, uint16_t *starts,
                                          uint8_t *count) {
    uint32_t full_superblocks = mi_dimension / superblock_mi;
    uint32_t superblocks = (mi_dimension + superblock_mi - 1U) / superblock_mi;
    uint32_t base_size = full_superblocks >> log2_count;
    uint32_t extras = full_superblocks - (base_size << log2_count);
    uint32_t start = 0U;
    uint32_t index = 0U;
    if (base_size == 0U)
        extras += superblocks - full_superblocks;
    while (start < superblocks && index < (1U << log2_count)) {
        starts[index++] = (uint16_t)start;
        start += base_size + (extras != 0U);
        if (extras != 0U)
            --extras;
    }
    starts[index] = (uint16_t)superblocks;
    *count = (uint8_t)index;
}

static void derive_gdf_block_geometry(const Av2hSequenceState *sequence, Av2hFrameState *frame) {
    uint32_t block_size = sequence->superblock_size;
    if (sequence->gdf_unit_matches_superblock_size == 0U && block_size < 128U) {
        block_size = 128U;
    }
    frame->gdf_block_size = (uint16_t)block_size;
    frame->gdf_block_columns = (uint16_t)(((uint32_t)frame->width + block_size - 1U) / block_size);
    frame->gdf_block_rows = (uint16_t)(((uint32_t)frame->height + block_size - 1U) / block_size);
    frame->gdf_block_count = (uint32_t)frame->gdf_block_columns * frame->gdf_block_rows;
}

int av2h_frame_tile_layout_read(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                                Av2hFrameState *frame) {
    uint32_t superblock_mi;
    uint32_t mi_columns;
    uint32_t mi_rows;
    uint32_t sb_columns;
    uint32_t sb_rows;
    uint32_t max_width_sb;
    uint32_t max_area_sb;
    uint32_t width_scale;
    uint32_t area_scale;
    unsigned int min_log2_columns;
    unsigned int min_log2_tiles;
    unsigned int min_log2_rows;
    unsigned int max_log2_columns;
    unsigned int max_log2_rows;
    uint32_t remaining;
    uint32_t start;
    uint32_t widest;
    size_t index;
    uint8_t reuse = 0U;
    if (sequence->superblock_size == 0U) {
        frame_fail(reader, AV2H_INVALID_DATA);
        return 0;
    }
    superblock_mi = sequence->superblock_size / 4U;
    mi_columns = (uint32_t)av2h_frame_mi_dimension(frame->width);
    mi_rows = (uint32_t)av2h_frame_mi_dimension(frame->height);
    sb_columns = (mi_columns + superblock_mi - 1U) / superblock_mi;
    sb_rows = (mi_rows + superblock_mi - 1U) / superblock_mi;
    width_scale = tile_width_scale(sequence);
    area_scale = tile_area_scale(sequence);
    max_width_sb =
        width_scale == 0U ? sb_columns : (width_scale * 4096U) / sequence->superblock_size / 4U;
    max_area_sb = area_scale == 0U ? sb_columns * sb_rows
                                   : (area_scale * (4096U * 2304U)) / sequence->superblock_size /
                                         sequence->superblock_size / 4U;
    if (max_width_sb == 0U || max_area_sb == 0U) {
        frame_fail(reader, AV2H_UNSUPPORTED);
        return 0;
    }
    min_log2_columns = av2h_ceil_log2_ratio_u32(max_width_sb, sb_columns);
    min_log2_tiles = av2h_ceil_log2_ratio_u32(max_area_sb, sb_columns * sb_rows);
    if (min_log2_tiles < min_log2_columns) {
        min_log2_tiles = min_log2_columns;
    }
    max_log2_columns =
        av2h_ceil_log2_u32(sb_columns < AV2H_MAX_TILE_COLUMNS ? sb_columns : AV2H_MAX_TILE_COLUMNS);
    max_log2_rows = av2h_ceil_log2_u32(sb_rows < AV2H_MAX_TILE_ROWS ? sb_rows : AV2H_MAX_TILE_ROWS);
    if (sequence->seq_tile_info_present_flag != 0U) {
        uint8_t eligible;
        if (sequence->uniform_tile_spacing == 0U) {
            eligible = sequence->tile_sb_columns == sb_columns && sequence->tile_sb_rows == sb_rows;
        } else {
            uint32_t columns = 1U << sequence->tile_column_log2;
            uint32_t rows = 1U << sequence->tile_row_log2;
            uint32_t tile_width = (sb_columns + columns - 1U) >> sequence->tile_column_log2;
            uint32_t tile_height = (sb_rows + rows - 1U) >> sequence->tile_row_log2;
            eligible = tile_width != 0U && tile_height != 0U &&
                       sb_columns > (columns - 1U) * tile_width &&
                       sb_rows > (rows - 1U) * tile_height;
        }
        if (eligible != 0U) {
            reuse = sequence->allow_tile_info_change != 0U
                        ? (uint8_t)av2h_bit_reader_read(reader, 1U)
                        : 1U;
        }
    }
    if (reuse != 0U) {
        frame->uniform_tile_spacing = sequence->uniform_tile_spacing;
        frame->tile_column_log2 = sequence->tile_column_log2;
        frame->tile_row_log2 = sequence->tile_row_log2;
        if (frame->uniform_tile_spacing != 0U) {
            calculate_uniform_tile_starts(mi_columns, superblock_mi, frame->tile_column_log2,
                                          frame->tile_column_start_sb, &frame->tile_column_count);
            calculate_uniform_tile_starts(mi_rows, superblock_mi, frame->tile_row_log2,
                                          frame->tile_row_start_sb, &frame->tile_row_count);
        } else {
            frame->tile_column_count = sequence->tile_column_count;
            frame->tile_row_count = sequence->tile_row_count;
            for (index = 0U; index <= frame->tile_column_count; ++index) {
                frame->tile_column_start_sb[index] = sequence->tile_column_start_sb[index];
            }
            for (index = 0U; index <= frame->tile_row_count; ++index) {
                frame->tile_row_start_sb[index] = sequence->tile_row_start_sb[index];
            }
        }
        goto tile_layout_complete;
    }
    frame->uniform_tile_spacing = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (frame->uniform_tile_spacing != 0U) {
        frame->tile_column_log2 = (uint8_t)min_log2_columns;
        while (frame->tile_column_log2 < max_log2_columns &&
               av2h_bit_reader_read(reader, 1U) != 0U) {
            ++frame->tile_column_log2;
        }
        min_log2_rows = min_log2_tiles > frame->tile_column_log2
                            ? min_log2_tiles - frame->tile_column_log2
                            : 0U;
        frame->tile_row_log2 = (uint8_t)min_log2_rows;
        while (frame->tile_row_log2 < max_log2_rows && av2h_bit_reader_read(reader, 1U) != 0U) {
            ++frame->tile_row_log2;
        }
        calculate_uniform_tile_starts(mi_columns, superblock_mi, frame->tile_column_log2,
                                      frame->tile_column_start_sb, &frame->tile_column_count);
        calculate_uniform_tile_starts(mi_rows, superblock_mi, frame->tile_row_log2,
                                      frame->tile_row_start_sb, &frame->tile_row_count);
    } else {
        remaining = sb_columns;
        start = 0U;
        index = 0U;
        widest = 1U;
        while (remaining != 0U && index < AV2H_MAX_TILE_COLUMNS) {
            uint32_t maximum = remaining < max_width_sb ? remaining : max_width_sb;
            uint32_t size = av2h_bit_reader_quasi_uniform(reader, maximum) + 1U;
            frame->tile_column_start_sb[index++] = (uint16_t)start;
            start += size;
            remaining -= size;
            if (size > widest)
                widest = size;
        }
        if (remaining != 0U) {
            frame_fail(reader, AV2H_LIMIT_EXCEEDED);
            return 0;
        }
        frame->tile_column_start_sb[index] = (uint16_t)start;
        frame->tile_column_count = (uint8_t)index;
        frame->tile_column_log2 = (uint8_t)av2h_ceil_log2_u32((uint32_t)index);
        max_area_sb = sb_columns * sb_rows;
        if (min_log2_tiles != 0U)
            max_area_sb >>= min_log2_tiles + 1U;
        remaining = sb_rows;
        start = 0U;
        index = 0U;
        max_width_sb = max_area_sb / widest;
        if (max_width_sb == 0U)
            max_width_sb = 1U;
        while (remaining != 0U && index < AV2H_MAX_TILE_ROWS) {
            uint32_t maximum = remaining < max_width_sb ? remaining : max_width_sb;
            uint32_t size = av2h_bit_reader_quasi_uniform(reader, maximum) + 1U;
            frame->tile_row_start_sb[index++] = (uint16_t)start;
            start += size;
            remaining -= size;
        }
        if (remaining != 0U) {
            frame_fail(reader, AV2H_LIMIT_EXCEEDED);
            return 0;
        }
        frame->tile_row_start_sb[index] = (uint16_t)start;
        frame->tile_row_count = (uint8_t)index;
        frame->tile_row_log2 = (uint8_t)av2h_ceil_log2_u32((uint32_t)index);
    }
tile_layout_complete:
    if (sequence->enable_ccso != 0U && sequence->ccso_unit_matches_superblock_size == 0U &&
        ((uint32_t)frame->tile_column_count * frame->tile_row_count > 1U)) {
        uint32_t odd_by_two = 0U;
        uint32_t odd_by_four = 0U;
        for (index = 0U; index + 1U < frame->tile_column_count; ++index) {
            uint32_t size =
                frame->tile_column_start_sb[index + 1U] - frame->tile_column_start_sb[index];
            odd_by_two += size & 1U;
            odd_by_four += size & 3U;
        }
        for (index = 0U; index + 1U < frame->tile_row_count; ++index) {
            uint32_t size = frame->tile_row_start_sb[index + 1U] - frame->tile_row_start_sb[index];
            odd_by_two += size & 1U;
            odd_by_four += size & 3U;
        }
        if (sequence->superblock_size >= 256U || odd_by_two != 0U) {
            frame->ccso_unit_size = sequence->superblock_size;
        } else if (sequence->superblock_size >= 128U || odd_by_four != 0U) {
            frame->ccso_unit_size = sequence->superblock_size * 2U;
        } else {
            frame->ccso_unit_size = sequence->superblock_size * 4U;
        }
        if (frame->ccso_unit_size > 256U)
            frame->ccso_unit_size = 256U;
    }
    if ((uint32_t)frame->tile_column_count * frame->tile_row_count > 1U) {
        frame->context_update_tile_id = 0U;
        frame->tile_size_bytes = (uint8_t)(av2h_bit_reader_read(reader, 2U) + 1U);
    }
    return reader->status == AV2H_OK;
}

static void read_quantization(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                              Av2hFrameState *frame) {
    uint8_t different_uv = 0U;
    unsigned int q_index_bits = sequence->info.bit_depth_idc == 1U ? 8U : 9U;
    frame->base_q_index = (Av2hQIndex)av2h_bit_reader_read(reader, q_index_bits);
    if (sequence->y_dc_delta_q_enabled != 0U) {
        frame->y_dc_delta_q = read_delta_q(reader);
    }
    if (!av2h_sequence_is_monochrome(sequence) &&
        (sequence->uv_dc_delta_q_enabled != 0U || sequence->uv_ac_delta_q_enabled != 0U)) {
        if (sequence->separate_uv_delta_q != 0U) {
            different_uv = (uint8_t)av2h_bit_reader_read(reader, 1U);
        }
        if (sequence->equal_ac_dc_q == 0U && sequence->uv_dc_delta_q_enabled != 0U) {
            frame->u_dc_delta_q = read_delta_q(reader);
        }
        if (sequence->uv_ac_delta_q_enabled != 0U) {
            frame->u_ac_delta_q = read_delta_q(reader);
        }
        if (sequence->equal_ac_dc_q != 0U) {
            frame->u_dc_delta_q = frame->u_ac_delta_q;
        }
        if (different_uv != 0U) {
            if (sequence->equal_ac_dc_q == 0U && sequence->uv_dc_delta_q_enabled != 0U) {
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

static void read_deblock(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                         Av2hFrameState *frame) {
    unsigned int index;
    unsigned int parameter_bits = (unsigned int)sequence->deblock_parameter_bits_minus_2 + 2U;
    int16_t offset = (int16_t)((uint16_t)1U << (parameter_bits - 1U));
    frame->deblock_enabled[0] = (uint8_t)av2h_bit_reader_read(reader, 1U);
    frame->deblock_enabled[1] = (uint8_t)av2h_bit_reader_read(reader, 1U);
    if (av2h_frame_plane_count(frame) > 1U &&
        (frame->deblock_enabled[0] != 0U || frame->deblock_enabled[1] != 0U)) {
        frame->deblock_enabled[2] = (uint8_t)av2h_bit_reader_read(reader, 1U);
        frame->deblock_enabled[3] = (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    for (index = 0U; index < (av2h_frame_plane_count(frame) == 1U ? 2U : 4U); ++index) {
        if (frame->deblock_enabled[index] != 0U) {
            if (av2h_bit_reader_read(reader, 1U) != 0U) {
                frame->deblock_delta[index] =
                    (int16_t)av2h_bit_reader_read(reader, parameter_bits) - offset;
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
    frame->cdef_damping = (uint8_t)(av2h_bit_reader_read(reader, 2U) + 3U);
    frame->cdef_strength_count = (uint8_t)(av2h_bit_reader_read(reader, 3U) + 1U);
    frame->cdef_on_skip_transform = (uint8_t)av2h_bit_reader_read(reader, 1U);
    for (index = 0U; index < frame->cdef_strength_count; ++index) {
        read_cdef_strength(reader, &frame->cdef_y_strength[index]);
        if (av2h_frame_plane_count(frame) > 1U) {
            read_cdef_strength(reader, &frame->cdef_uv_strength[index]);
        }
    }
}

static uint16_t restoration_unit_count(uint16_t dimension, uint16_t unit_size) {
    uint32_t count = ((uint32_t)dimension + unit_size / 2U) / unit_size;
    return (uint16_t)(count != 0U ? count : 1U);
}

static uint32_t inverse_recenter_nonnegative(uint32_t reference, uint32_t value) {
    if (value > 2U * reference)
        return value;
    if ((value & 1U) == 0U)
        return reference + value / 2U;
    return reference - (value + 1U) / 2U;
}

static uint32_t inverse_recenter_finite(uint32_t count, uint32_t reference, uint32_t value) {
    if (2U * reference <= count) {
        return inverse_recenter_nonnegative(reference, value);
    }
    return count - 1U - inverse_recenter_nonnegative(count - 1U - reference, value);
}

static uint32_t read_subexp(Av2hBitReader *reader, uint32_t count, unsigned int parameter) {
    unsigned int iteration = 0U;
    uint32_t consumed = 0U;
    for (;;) {
        unsigned int bits = iteration != 0U ? parameter + iteration - 1U : parameter;
        uint32_t group_size = 1U << bits;
        if (count <= consumed + 3U * group_size) {
            return consumed + av2h_bit_reader_quasi_uniform(reader, count - consumed);
        }
        if (av2h_bit_reader_read(reader, 1U) == 0U) {
            return consumed + av2h_bit_reader_read(reader, bits);
        }
        ++iteration;
        consumed += group_size;
    }
}

static uint32_t read_ref_subexp(Av2hBitReader *reader, uint32_t count, unsigned int parameter,
                                uint32_t reference) {
    return inverse_recenter_finite(count, reference, read_subexp(reader, count, parameter));
}

static unsigned int wienerns_most_probable_group(unsigned int class_index,
                                                 const uint32_t *group_counts) {
    uint32_t first = class_index + 1U;
    if (first > 2U || group_counts[1] > 2U) {
        return first > group_counts[1] ? 0U : 1U;
    }
    if (first >= group_counts[1] && first >= group_counts[2])
        return 0U;
    if (group_counts[1] >= group_counts[2])
        return 1U;
    return 2U;
}

static unsigned int wienerns_index_to_group(uint32_t match_index, const uint32_t *group_counts) {
    if (match_index < group_counts[0])
        return 0U;
    if (match_index < group_counts[0] + group_counts[1])
        return 1U;
    return 2U;
}

static unsigned int wienerns_predict_group(unsigned int class_index, const uint32_t *match_indices,
                                           const uint32_t *group_counts, unsigned int *only) {
    unsigned int prediction;
    if (class_index == 0U) {
        prediction = wienerns_most_probable_group(0U, group_counts);
    } else {
        uint32_t running[3] = {0U, 0U, 0U};
        unsigned int index;
        for (index = 0U; index < class_index; ++index) {
            ++running[wienerns_index_to_group(match_indices[index], group_counts)];
        }
        if (running[0] >= running[1] && running[0] >= running[2]) {
            prediction = 0U;
        } else if (running[1] >= running[2]) {
            prediction = 1U;
        } else {
            prediction = 2U;
        }
    }
    *only =
        group_counts[(prediction + 1U) % 3U] == 0U && group_counts[(prediction + 2U) % 3U] == 0U;
    return prediction;
}

static uint32_t wienerns_group_base(unsigned int group, const uint32_t *group_counts) {
    uint32_t base = 0U;
    unsigned int index;
    for (index = 0U; index < group; ++index)
        base += group_counts[index];
    return base;
}

static void read_wienerns_match_indices(Av2hBitReader *reader, unsigned int class_count,
                                        const uint32_t *group_counts, uint32_t *match_indices) {
    static const unsigned int convert_larger[3] = {2U, 2U, 1U};
    static const unsigned int convert_smaller[3] = {1U, 0U, 0U};
    unsigned int class_index;
    for (class_index = 0U; class_index < class_count; ++class_index) {
        unsigned int only;
        unsigned int prediction =
            wienerns_predict_group(class_index, match_indices, group_counts, &only);
        unsigned int group = prediction;
        uint32_t base;
        uint32_t count;
        if (only == 0U && av2h_bit_reader_read(reader, 1U) != 0U) {
            unsigned int index;
            unsigned int zero_group = 3U;
            for (index = 0U; index < 3U; ++index) {
                if (index == prediction)
                    continue;
                if (group_counts[index] == 0U) {
                    zero_group = index;
                    break;
                }
            }
            if (zero_group != 3U) {
                group = 3U - (prediction + zero_group);
            } else {
                group = av2h_bit_reader_read(reader, 1U) != 0U ? convert_larger[prediction]
                                                               : convert_smaller[prediction];
            }
        }
        base = wienerns_group_base(group, group_counts);
        count = group == 0U ? class_index + 1U : group_counts[group];
        match_indices[class_index] = base;
        if (count > 1U) {
            match_indices[class_index] += read_ref_subexp(reader, count, 4U, count / 2U);
        }
        if (reader->status != AV2H_OK)
            return;
    }
}

static void read_wienerns_frame_filter(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                                       Av2hFrameState *frame, unsigned int plane) {
    int16_t validation_filters[AV2H_WIENER_NONSEP_MAX_CLASSES][AV2H_WIENER_NONSEP_MAX_TAPS];
    int16_t (*filters)[AV2H_WIENER_NONSEP_MAX_TAPS] = frame->restoration_frame_filter != NULL
                                                          ? frame->restoration_frame_filter[plane]
                                                          : validation_filters;
    const Av2hWienerNonsepParameters *parameters = av2h_wienerns_parameters(plane);
    unsigned int class_count = frame->restoration_num_filter_classes[plane];
    unsigned int no_pc_wiener = (sequence->lr_tools_disable_mask[0] >> AV2H_RESTORE_PC_WIENER) & 1U;
    uint32_t group_counts[3];
    uint32_t match_indices[AV2H_WIENER_NONSEP_MAX_CLASSES];
    uint8_t exact_match[AV2H_WIENER_NONSEP_MAX_CLASSES];
    unsigned int class_index;
    if (frame->restoration_frame_filter == NULL)
        av2h_memory_fill(validation_filters, 0U, sizeof(validation_filters));
    if (class_count == 0U || class_count > AV2H_WIENER_NONSEP_MAX_CLASSES ||
        (plane != 0U && class_count != 1U)) {
        frame_fail(reader, AV2H_INVALID_DATA);
        return;
    }
    group_counts[0] = class_count;
    group_counts[1] = 0U;
    group_counts[2] = plane == 0U && no_pc_wiener == 0U ? 64U - class_count : 0U;
    read_wienerns_match_indices(reader, class_count, group_counts, match_indices);
    if (reader->status != AV2H_OK)
        return;
    for (class_index = 0U; class_index < class_count; ++class_index) {
        exact_match[class_index] = (uint8_t)av2h_bit_reader_read(reader, 1U);
    }
    if (reader->status != AV2H_OK)
        return;
    for (class_index = 0U; class_index < class_count; ++class_index) {
        int16_t predictor[AV2H_WIENER_NONSEP_MAX_TAPS];
        int16_t *taps = filters[class_index];
        unsigned int subset = 0U;
        unsigned int symmetric = 1U;
        unsigned int index;
        Av2hStatus status = av2h_wienerns_frame_predictor(
            plane, match_indices[class_index], class_index, class_count,
            (const int16_t (*)[AV2H_WIENER_NONSEP_MAX_TAPS])filters, predictor);
        if (status != AV2H_OK) {
            frame_fail(reader, status);
            return;
        }
        if (exact_match[class_index] != 0U) {
            av2h_memory_copy(taps, predictor, sizeof(predictor));
            continue;
        }
        av2h_memory_fill(taps, 0U, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(taps[0]));
        while (subset + 1U < parameters->subset_count && av2h_bit_reader_read(reader, 1U) != 0U) {
            ++subset;
        }
        if (parameters->symmetric_first < parameters->coefficient_count &&
            (parameters->subset_mask[subset] >> parameters->symmetric_first) != 0U) {
            symmetric = av2h_bit_reader_read(reader, 1U);
        }
        for (index = 0U; index < parameters->coefficient_count; ++index) {
            uint32_t count;
            uint32_t reference;
            if ((parameters->subset_mask[subset] & (1U << index)) == 0U) {
                continue;
            }
            count = 1U << parameters->bits[index];
            reference = (uint32_t)(predictor[index] - parameters->minimum[index]);
            taps[index] = (int16_t)((int32_t)read_ref_subexp(
                                        reader, count, parameters->bits[index] - 3U, reference) +
                                    parameters->minimum[index]);
            if (reader->status != AV2H_OK)
                return;
            if (symmetric != 0U && index >= parameters->symmetric_first &&
                index + 1U < parameters->coefficient_count) {
                taps[index + 1U] = taps[index];
                ++index;
            }
        }
    }
}

static void read_restoration(Av2hBitReader *reader, const Av2hSequenceState *sequence,
                             Av2hFrameState *frame, Av2hFrameSyntaxReferences *references) {
    unsigned int plane;
    uint8_t uses_luma = 0U;
    uint8_t uses_chroma = 0U;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint8_t index_to_tool[4] = {AV2H_RESTORE_NONE, 0U, 0U, 0U};
        uint8_t mask = sequence->lr_tools_disable_mask[plane != 0U];
        uint32_t tool_count = 1U;
        uint32_t tool_index;
        unsigned int tool;
        for (tool = AV2H_RESTORE_PC_WIENER; tool < AV2H_RESTORE_SWITCHABLE; ++tool) {
            if ((mask & (1U << tool)) == 0U) {
                index_to_tool[tool_count++] = (uint8_t)tool;
            }
        }
        if (tool_count > 2U) {
            index_to_tool[tool_count++] = AV2H_RESTORE_SWITCHABLE;
        }
        tool_index = av2h_bit_reader_quasi_uniform(reader, tool_count);
        if (reader->status != AV2H_OK || tool_index >= tool_count)
            return;
        frame->restoration_type[plane] = index_to_tool[tool_index];
        if (frame->restoration_type[plane] == AV2H_RESTORE_WIENER_NONSEP ||
            frame->restoration_type[plane] == AV2H_RESTORE_SWITCHABLE) {
            frame->restoration_frame_filters_on[plane] = (uint8_t)av2h_bit_reader_read(reader, 1U);
            frame->restoration_num_filter_classes[plane] =
                frame->restoration_frame_filters_on[plane] != 0U && plane == 0U
                    ? (uint8_t[]){1U, 2U, 3U, 4U, 6U, 8U, 12U, 16U}[av2h_bit_reader_read(reader,
                                                                                         3U)]
                    : 1U;
        }
        if (frame->restoration_type[plane] != AV2H_RESTORE_NONE) {
            if (plane == 0U)
                uses_luma = 1U;
            else
                uses_chroma = 1U;
        }
        if (frame->restoration_type[plane] != AV2H_RESTORE_NONE &&
            (plane != 0U || (frame->restoration_type[plane] != AV2H_RESTORE_PC_WIENER &&
                             frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE)) &&
            frame->restoration_type[plane] != AV2H_RESTORE_WIENER_NONSEP) {
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
        frame->restoration_unit_size[1] =
            (uint16_t)((frame->chroma_format_idc == 2U ? 512U : 256U) >> shift);
        frame->restoration_unit_size[2] = frame->restoration_unit_size[1];
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint16_t width = (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->width);
        uint16_t height = (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->height);
        frame->restoration_unit_columns[plane] =
            restoration_unit_count(width, frame->restoration_unit_size[plane]);
        frame->restoration_unit_rows[plane] =
            restoration_unit_count(height, frame->restoration_unit_size[plane]);
        frame->restoration_unit_count[plane] =
            (uint32_t)frame->restoration_unit_columns[plane] * frame->restoration_unit_rows[plane];
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->restoration_frame_filters_on[plane] == 0U)
            continue;
        if (references != NULL)
            references->restoration_bit_positions[plane] = reader->bit_position;
        read_wienerns_frame_filter(reader, sequence, frame, plane);
        if (reader->status != AV2H_OK)
            return;
    }
}

static int8_t ccso_offset_from_index(unsigned int index, unsigned int scale_index) {
    static const int8_t values[8] = {0, 1, -1, 3, -3, 7, -7, -10};
    return (int8_t)(values[index] * (int8_t)(scale_index + 1U));
}

static uint16_t read_ccso_offsets(Av2hBitReader *reader, Av2hCcsoPlane *plane,
                                  uint32_t edge_intervals, uint32_t band_count) {
    uint32_t first;
    uint32_t second;
    uint32_t band;
    uint16_t count = 0U;
    for (first = 0U; first < edge_intervals; ++first) {
        for (second = 0U; second < edge_intervals; ++second) {
            for (band = 0U; band < band_count; ++band) {
                unsigned int unary;
                for (unary = 0U; unary < 7U; ++unary) {
                    if (av2h_bit_reader_read(reader, 1U) == 0U)
                        break;
                }
                if (plane->filter_offset != NULL)
                    plane->filter_offset[(band << 4U) + (first << 2U) + second] =
                        ccso_offset_from_index(unary, plane->scale_index);
                ++count;
                if (reader->status != AV2H_OK)
                    return count;
            }
        }
    }
    return count;
}

static void read_ccso(Av2hBitReader *reader, Av2hFrameState *frame,
                      Av2hFrameSyntaxReferences *references) {
    unsigned int plane_index;
    for (plane_index = 0U; plane_index < frame->plane_count; ++plane_index) {
        Av2hCcsoPlane *plane = &frame->ccso[plane_index];
        uint32_t edge_intervals;
        uint32_t band_count;
        plane->enabled = (uint8_t)av2h_bit_reader_read(reader, 1U);
        if (plane->enabled == 0U)
            continue;
        plane->band_only = (uint8_t)av2h_bit_reader_read(reader, 1U);
        plane->scale_index = (uint8_t)av2h_bit_reader_read(reader, 2U);
        if (plane->band_only != 0U) {
            plane->max_band_log2 = (uint8_t)av2h_bit_reader_read(reader, 3U);
            if (plane->max_band_log2 > 6U) {
                frame_fail(reader, AV2H_INVALID_DATA);
                return;
            }
            edge_intervals = 1U;
        } else {
            plane->quant_index = (uint8_t)av2h_bit_reader_read(reader, 2U);
            plane->extended_filter_support = (uint8_t)av2h_bit_reader_read(reader, 3U);
            if (plane->extended_filter_support == 7U) {
                frame_fail(reader, AV2H_INVALID_DATA);
                return;
            }
            if (!(plane->scale_index == 0U && plane->quant_index == 3U)) {
                plane->edge_classifier = (uint8_t)av2h_bit_reader_read(reader, 1U);
            }
            plane->max_band_log2 = (uint8_t)av2h_bit_reader_read(reader, 2U);
            edge_intervals = plane->edge_classifier != 0U ? 2U : 3U;
        }
        band_count = (uint32_t)1U << plane->max_band_log2;
        if (references != NULL)
            references->ccso_bit_positions[plane_index] = reader->bit_position;
        plane->offset_count = read_ccso_offsets(reader, plane, edge_intervals, band_count);
        if (reader->status != AV2H_OK)
            return;
    }
}

static Av2hStatus frame_diagnostic_fail(Av2hError *error, Av2hStatus status,
                                        const Av2hBitReader *reader, uint8_t syntax,
                                        uint32_t parsed_value) {
    if (error != NULL) {
        (void)av2h_error_set_core(error, status, reader != NULL ? reader->bit_position / 8U : 0U,
                                  AV2H_CONTEXT_FRAME);
        error->obu_type = AV2H_OBU_CLOSED_LOOP_KEY;
        error->diagnostic_stage = AV2H_DIAGNOSTIC_STAGE_FRAME_HEADER;
        error->diagnostic_reason = status == AV2H_TRUNCATED      ? AV2H_DIAGNOSTIC_REASON_TRUNCATED
                                   : status == AV2H_INVALID_DATA ? AV2H_DIAGNOSTIC_REASON_MALFORMED
                                   : status == AV2H_INVALID_ARGUMENT
                                       ? AV2H_DIAGNOSTIC_REASON_ARGUMENT
                                       : AV2H_DIAGNOSTIC_REASON_UNSUPPORTED_TUPLE;
        error->syntax_stage = syntax;
        error->plane = 0U;
        error->row = 0U;
        error->column = 0U;
        error->entropy_tell = reader != NULL ? reader->bit_position : 0U;
        error->parsed_value = parsed_value;
    }
    return status;
}

Av2hStatus av2h_frame_header_closed_key_parse(Av2hSpan payload, const Av2hSequenceState *sequence,
                                              Av2hFrameState *frame,
                                              Av2hFrameSyntaxStorage *storage,
                                              Av2hFrameSyntaxReferences *references,
                                              Av2hError *error) {
    Av2hBitReader reader;
    uint32_t multi_frame_header_id;
    uint32_t sequence_header_id;
    if (sequence == NULL || frame == NULL || (payload.data == NULL && payload.size != 0U)) {
        return frame_diagnostic_fail(error, AV2H_INVALID_ARGUMENT, NULL,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE, 0U);
    }
    av2h_memory_fill(frame, 0U, sizeof(*frame));
    if (references != NULL)
        av2h_memory_fill(references, 0U, sizeof(*references));
    if (storage != NULL)
        av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_frame_syntax_storage_bind(frame, storage);
    frame->plane_count = (uint8_t)av2h_sequence_plane_count(sequence);
    frame->chroma_format_idc = sequence->info.chroma_format_idc;
    frame->ccso_unit_size =
        sequence->ccso_unit_matches_superblock_size ? sequence->superblock_size : 256U;
    if (payload.size == 0U) {
        return frame_diagnostic_fail(error, AV2H_TRUNCATED, NULL,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE, 0U);
    }
    if (sequence->info.still_picture == 0U ||
        (sequence->info.bit_depth_idc != 1U &&
         (sequence->info.bit_depth_idc != 0U ||
          !((sequence->info.seq_profile_idc == 0U && sequence->info.chroma_format_idc == 0U) ||
            (sequence->info.seq_profile_idc == 4U && sequence->info.chroma_format_idc == 2U)))) ||
        sequence->info.chroma_format_idc > 2U) {
        return frame_diagnostic_fail(
            error, AV2H_UNSUPPORTED, NULL, AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE,
            ((uint32_t)sequence->info.bit_depth_idc << 8U) | sequence->info.chroma_format_idc);
    }
    av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
    frame->first_tile_group = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->first_tile_group == 0U) {
        return frame_diagnostic_fail(error, AV2H_UNSUPPORTED, &reader,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_FIRST_TILE_GROUP, 0U);
    }
    multi_frame_header_id = av2h_bit_reader_uvlc(&reader);
    sequence_header_id = av2h_bit_reader_uvlc(&reader);
    if (reader.status != AV2H_OK)
        return reader.status;
    if (multi_frame_header_id != 0U || sequence_header_id != sequence->sequence_header_id) {
        return frame_diagnostic_fail(error, AV2H_UNSUPPORTED, &reader,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_HEADER_IDS,
                                     (multi_frame_header_id << 8U) | sequence_header_id);
    }
    frame->width = (uint16_t)(sequence->info.max_frame_width_minus_1 + 1U);
    frame->height = (uint16_t)(sequence->info.max_frame_height_minus_1 + 1U);
    frame->allow_screen_content_tools = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->allow_screen_content_tools != 0U) {
        frame->force_integer_mv = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    }
    frame->allow_intrabc = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->allow_intrabc != 0U) {
        frame->allow_global_intrabc = (uint8_t)av2h_bit_reader_read(&reader, 1U);
        frame->allow_local_intrabc =
            frame->allow_global_intrabc != 0U ? (uint8_t)av2h_bit_reader_read(&reader, 1U) : 1U;
        frame->max_bvp_drl_bits = sequence->default_max_bvp_drl_bits;
        if (sequence->allow_frame_max_bvp_drl_bits != 0U) {
            frame->max_bvp_drl_bits =
                (uint8_t)(read_ref_quasi_uniform(
                              &reader, 3U, (uint32_t)sequence->default_max_bvp_drl_bits - 1U) +
                          1U);
        }
    }
    frame->disable_cdf_update = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (!av2h_frame_tile_layout_read(&reader, sequence, frame)) {
        return frame_diagnostic_fail(error, reader.status, &reader,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_TILE_LAYOUT, 0U);
    }
    read_quantization(&reader, sequence, frame);
    frame->segmentation_enabled = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->segmentation_enabled != 0U) {
        uint8_t reuse = 0U;
        if (sequence->seq_seg_info_present_flag != 0U) {
            reuse = sequence->seg_allow_info_change != 0U
                        ? (uint8_t)av2h_bit_reader_read(&reader, 1U)
                        : 1U;
        }
        if (reuse != 0U) {
            size_t segment;
            size_t segment_count = av2h_sequence_segment_count(sequence);
            for (segment = 0U; segment < segment_count; ++segment) {
                frame->seg_feature_mask[segment] = sequence->seg_feature_mask[segment];
                frame->seg_alt_q[segment] = sequence->seg_alt_q[segment];
            }
            frame->seg_last_active_id = sequence->seg_last_active_id;
        } else if (!read_segmentation_features(&reader, av2h_sequence_segment_count(sequence),
                                               frame->seg_feature_mask, frame->seg_alt_q,
                                               &frame->seg_last_active_id)) {
            return frame_diagnostic_fail(
                error, reader.status != AV2H_OK ? reader.status : AV2H_UNSUPPORTED, &reader,
                AV2H_DIAGNOSTIC_SYNTAX_FRAME_SEGMENTATION, 0U);
        }
    }
    frame->using_qmatrix = (uint8_t)av2h_bit_reader_read(&reader, 1U);
    if (frame->using_qmatrix != 0U) {
        size_t matrix;
        frame->picture_qm_count = frame->segmentation_enabled != 0U
                                      ? (uint8_t)(av2h_bit_reader_read(&reader, 2U) + 1U)
                                      : 1U;
        frame->qm_index_bits = (uint8_t)av2h_ceil_log2_u32(frame->picture_qm_count);
        for (matrix = 0U; matrix < frame->picture_qm_count; ++matrix) {
            frame->qm_y[matrix] = (uint8_t)av2h_bit_reader_read(&reader, 4U);
            if (frame->plane_count > 1U) {
                uint8_t qm_uv_same_as_y = (uint8_t)av2h_bit_reader_read(&reader, 1U);
                if (qm_uv_same_as_y != 0U) {
                    frame->qm_u[matrix] = frame->qm_y[matrix];
                    frame->qm_v[matrix] = frame->qm_y[matrix];
                } else {
                    frame->qm_u[matrix] = (uint8_t)av2h_bit_reader_read(&reader, 4U);
                    frame->qm_v[matrix] = sequence->separate_uv_delta_q != 0U
                                              ? (uint8_t)av2h_bit_reader_read(&reader, 4U)
                                              : frame->qm_u[matrix];
                }
            }
        }
    }
    if (frame->base_q_index != 0U) {
        frame->delta_q_present = (uint8_t)av2h_bit_reader_read(&reader, 1U);
        if (frame->delta_q_present != 0U) {
            frame->delta_q_res = (uint8_t)(1U << av2h_bit_reader_read(&reader, 2U));
        }
    }
    if (reader.status != AV2H_OK)
        return reader.status;
    if (frame->using_qmatrix != 0U && frame->qm_index_bits != 0U) {
        size_t segment;
        size_t segment_count = av2h_sequence_segment_count(sequence);
        for (segment = 0U; segment < segment_count; ++segment) {
            Av2hQIndex q_index =
                av2h_leaf_segment_q_index(sequence, frame, frame->base_q_index, (uint8_t)segment);
            if (av2h_quantizer_is_lossless(sequence, frame, q_index))
                continue;
            frame->segment_qm_index[segment] =
                (uint8_t)av2h_bit_reader_read(&reader, frame->qm_index_bits);
            if (frame->segment_qm_index[segment] >= frame->picture_qm_count) {
                return frame_diagnostic_fail(error, AV2H_INVALID_DATA, &reader,
                                             AV2H_DIAGNOSTIC_SYNTAX_FRAME_QUANTIZATION,
                                             frame->segment_qm_index[segment]);
            }
        }
    }
    if (reader.status != AV2H_OK)
        return reader.status;
    if (sequence->enable_parity_hiding != 0U) {
        size_t segment;
        size_t segment_count = av2h_sequence_segment_count(sequence);
        int coded_lossless = 1;
        for (segment = 0U; segment < segment_count; ++segment) {
            Av2hQIndex q_index =
                av2h_leaf_segment_q_index(sequence, frame, frame->base_q_index, (uint8_t)segment);
            if (!av2h_quantizer_is_lossless(sequence, frame, q_index)) {
                coded_lossless = 0;
                break;
            }
        }
        if (!coded_lossless) {
            frame->allow_parity_hiding = (uint8_t)av2h_bit_reader_read(&reader, 1U);
        }
    }
    if (reader.status != AV2H_OK)
        return reader.status;
    if (frame->base_q_index == 0U) {
        frame->tx_mode = 1U;
        frame->reduced_tx_set = (uint8_t)av2h_bit_reader_read(&reader, 2U);
        if (reader.status != AV2H_OK)
            return reader.status;
        frame->uncompressed_header_bits = reader.bit_position - 1U;
        return AV2H_OK;
    }
    read_deblock(&reader, sequence, frame);
    if (sequence->enable_gdf != 0U) {
        derive_gdf_block_geometry(sequence, frame);
        frame->gdf_mode = 1U;
        if (frame->gdf_block_count > 1U) {
            frame->gdf_mode = (uint8_t)(frame->gdf_mode + av2h_bit_reader_read(&reader, 1U));
        }
        frame->gdf_qp_index = (uint8_t)av2h_bit_reader_read(&reader, 2U);
        frame->gdf_scale_index = (uint8_t)av2h_bit_reader_read(&reader, 2U);
    }
    if (sequence->enable_cdef != 0U)
        read_cdef(&reader, frame);
    if (sequence->enable_restoration != 0U) {
        read_restoration(&reader, sequence, frame, references);
    }
    if (sequence->enable_ccso != 0U)
        read_ccso(&reader, frame, references);
    frame->tx_mode = av2h_bit_reader_read(&reader, 1U) != 0U ? 2U : 1U;
    frame->reduced_tx_set = (uint8_t)av2h_bit_reader_read(&reader, 2U);
    if (reader.status != AV2H_OK)
        return reader.status;
    frame->uncompressed_header_bits = reader.bit_position - 1U;
    return AV2H_OK;
}

Av2hStatus av2h_frame_syntax_materialize(Av2hSpan payload, const Av2hSequenceState *sequence,
                                         Av2hFrameState *frame,
                                         const Av2hFrameSyntaxReferences *references,
                                         Av2hFrameSyntaxStorage *storage, Av2hError *error) {
    Av2hBitReader reader;
    unsigned int plane;
    if (payload.data == NULL || sequence == NULL || frame == NULL || references == NULL ||
        storage == NULL || frame->plane_count == 0U ||
        frame->plane_count > AV2H_FRAME_PLANE_COUNT) {
        return frame_diagnostic_fail(error, AV2H_INVALID_ARGUMENT, NULL,
                                     AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE, 0U);
    }
    av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_frame_syntax_storage_bind(frame, storage);
    for (plane = 0U; plane < frame->plane_count; ++plane) {
        if (frame->restoration_frame_filters_on[plane] != 0U) {
            av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
            (void)av2h_bit_reader_skip(&reader, references->restoration_bit_positions[plane]);
            read_wienerns_frame_filter(&reader, sequence, frame, plane);
            if (reader.status != AV2H_OK)
                return frame_diagnostic_fail(error, reader.status, &reader,
                                             AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE, 0U);
        }
        if (frame->ccso[plane].enabled != 0U) {
            Av2hCcsoPlane *parameters = &frame->ccso[plane];
            uint32_t edge_intervals = parameters->band_only != 0U         ? 1U
                                      : parameters->edge_classifier != 0U ? 2U
                                                                          : 3U;
            av2h_bit_reader_init(&reader, payload.data, payload.size, 0U);
            (void)av2h_bit_reader_skip(&reader, references->ccso_bit_positions[plane]);
            (void)read_ccso_offsets(&reader, parameters, edge_intervals,
                                    (uint32_t)1U << parameters->max_band_log2);
            if (reader.status != AV2H_OK)
                return frame_diagnostic_fail(error, reader.status, &reader,
                                             AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE, 0U);
        }
    }
    return AV2H_OK;
}

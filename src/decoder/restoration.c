#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"
#include "decoder/internal/wiener_nonsep.h"

#define RESTORATION_PROC_UNIT_SIZE 64
#define NUM_PC_WIENER_TAPS_LUMA 13
#define AV2H_PC_WIENER_FILTER_COUNT 64U
#define AV2H_PC_WIENER_CLASS_COUNT 256U

#include "pc_wiener_data.inc"

static const int32_t pc_wiener_feature_normalizers[4] = {
    0, 3739, 3273, 3074
};

static const int32_t pc_wiener_mode_weights[4][3] = {
    { -527, 15325, 321 },
    { 26436, -17705, 17905 },
    { 366, -147, -194 },
    { 202, -267, -179 }
};

static const int32_t pc_wiener_mode_offsets[4] = {
    -547, -21565, -573, -680
};

static const int32_t pc_wiener_lut_weights[4] = {
    512, 64, 8, 1
};

static unsigned int pc_wiener_filter_set_index(unsigned int qindex) {
    if (qindex < 130U) return 0U;
    if (qindex < 190U) return 1U;
    if (qindex < 220U) return 2U;
    return 3U;
}

static const int16_t *pc_wiener_filter(unsigned int set_index,
                                       unsigned int filter_index) {
    return av2h_pc_wiener_filters +
        ((size_t)(set_index & 3U) * AV2H_PC_WIENER_FILTER_COUNT +
         (filter_index & 63U)) * NUM_PC_WIENER_TAPS_LUMA;
}

static const uint8_t *pc_wiener_filter_selector(unsigned int set_index) {
    return av2h_pc_wiener_sub_classify +
        (size_t)(set_index & 3U) * AV2H_PC_WIENER_CLASS_COUNT;
}

const Av2hWienerNonsepParameters *av2h_wienerns_parameters(
    unsigned int plane) {
    static const Av2hWienerNonsepParameters luma = {
        (uint8_t)AV2H_WIENER_NONSEP_LUMA_TAPS, 4U, 16U,
        { 6U, 6U, 5U, 5U, 5U, 5U, 4U, 4U, 4U,
          4U, 4U, 4U, 4U, 4U, 4U, 4U, 0U, 0U },
        { -24, -24, -14, -14, -16, -16, -8, -8, -8,
          -8, -8, -8, -8, -8, -8, -8, 0, 0 },
        { 0x0000003fU, 0x00000fc3U, 0x00000fffU, 0x0000ffffU }
    };
    static const Av2hWienerNonsepParameters chroma = {
        (uint8_t)AV2H_WIENER_NONSEP_CHROMA_TAPS, 3U, 6U,
        { 6U, 6U, 5U, 5U, 5U, 5U, 5U, 5U, 5U,
          5U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U },
        { -24, -24, -14, -14, -16, -16, -16, -16, -16,
          -16, -8, -8, -8, -8, -8, -8, -8, -8 },
        { 0x0000003fU, 0x000003ffU, 0x0003ffffU, 0x0003ffffU }
    };
    return plane == 0U ? &luma : &chroma;
}

Av2hStatus av2h_wienerns_frame_predictor(
    unsigned int plane, uint32_t match_index, unsigned int class_index,
    unsigned int class_count,
    const int16_t decoded[AV2H_WIENER_NONSEP_MAX_CLASSES]
                         [AV2H_WIENER_NONSEP_MAX_TAPS],
    int16_t taps[AV2H_WIENER_NONSEP_MAX_TAPS]) {
    static const uint8_t shuffled_index[64] = {
        16U, 7U, 58U, 21U, 12U, 61U, 26U, 38U, 18U, 30U, 50U,
        45U, 23U, 49U, 43U, 62U, 42U, 54U, 27U, 36U, 17U, 44U,
        32U, 34U, 4U, 24U, 52U, 31U, 37U, 11U, 33U, 19U, 35U,
        6U, 22U, 53U, 63U, 25U, 41U, 47U, 1U, 59U, 0U, 28U,
        40U, 55U, 48U, 8U, 5U, 51U, 9U, 46U, 56U, 60U, 15U,
        2U, 13U, 14U, 57U, 29U, 3U, 20U, 39U, 10U
    };
    const Av2hWienerNonsepParameters *parameters =
        av2h_wienerns_parameters(plane);
    const int16_t *source;
    unsigned int index;
    unsigned int count;
    if (taps == 0 || decoded == 0 || class_count == 0U ||
        class_count > AV2H_WIENER_NONSEP_MAX_CLASSES ||
        class_index >= class_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(taps, 0U,
                     AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(*taps));
    if (match_index == 0U) return AV2H_OK;
    if (match_index < class_count) {
        if (match_index > class_index) return AV2H_INVALID_DATA;
        av2h_memory_copy(taps, decoded[match_index - 1U],
                         AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(*taps));
        return AV2H_OK;
    }
    if (plane != 0U) return AV2H_INVALID_DATA;
    if (match_index - class_count >= 64U - class_count) {
        return AV2H_INVALID_DATA;
    }
    source = pc_wiener_filter(
        0U, shuffled_index[match_index - class_count]);
    count = NUM_PC_WIENER_TAPS_LUMA - 1;
    if (count > parameters->coefficient_count) {
        count = parameters->coefficient_count;
    }
    for (index = 0U; index < count; ++index) {
        int value = source[index];
        int maximum = parameters->minimum[index] +
            (1 << parameters->bits[index]) - 1;
        if (value < parameters->minimum[index]) {
            value = parameters->minimum[index];
        }
        if (value > maximum) value = maximum;
        taps[index] = (int16_t)value;
    }
    return AV2H_OK;
}

const uint8_t *av2h_wienerns_class_map(unsigned int class_count,
                                       unsigned int set_index) {
    unsigned int class_index;
    switch (class_count) {
        case 1U: class_index = 0U; break;
        case 2U: class_index = 1U; break;
        case 3U: class_index = 2U; break;
        case 4U: class_index = 3U; break;
        case 6U: class_index = 4U; break;
        case 8U: class_index = 5U; break;
        case 12U: class_index = 6U; break;
        case 16U: class_index = 7U; break;
        default: class_index = 0U; break;
    }
    return av2h_pc_wiener_sub_classify2 +
        ((size_t)(set_index & 3U) * 8U + class_index) *
            AV2H_PC_WIENER_CLASS_COUNT;
}

#define AV2H_PC_WIENER_BLOCK_SIZE 4
#define AV2H_PC_WIENER_FEATURE_LEAD 1
#define AV2H_PC_WIENER_FEATURE_LAG 4
#define AV2H_PC_WIENER_UNIT_OFFSET 8

typedef struct {
    const Av2hFilterPlane *source;
    const Av2hFilterPlane *deblocked;
    int stripe_start;
    int stripe_end;
} Av2hPcWienerSource;

static int clamp_coordinate(int value, int limit) {
    if (value < 0) return 0;
    if (value >= limit) return limit - 1;
    return value;
}

static uint8_t pc_wiener_source_sample(
    const Av2hPcWienerSource *context, int row, int column) {
    int source_row = row;
    int source_column = clamp_coordinate(
        column, context->source->width);
    if (row < context->stripe_start) {
        if (context->stripe_start == 0) {
            source_row = 0;
        } else {
            source_row = row < context->stripe_start - 1 ?
                context->stripe_start - 2 : context->stripe_start - 1;
            source_row = clamp_coordinate(
                source_row, context->deblocked->height);
            return context->deblocked->data[
                (size_t)source_row * context->deblocked->stride_samples +
                (size_t)source_column];
        }
    } else if (row >= context->stripe_end) {
        if (context->stripe_end >= context->source->height) {
            source_row = context->source->height - 1;
        } else {
            source_row = row == context->stripe_end ?
                context->stripe_end : context->stripe_end + 1;
            source_row = clamp_coordinate(
                source_row, context->deblocked->height);
            return context->deblocked->data[
                (size_t)source_row * context->deblocked->stride_samples +
                (size_t)source_column];
        }
    }
    source_row = clamp_coordinate(source_row, context->source->height);
    return context->source->data[
        (size_t)source_row * context->source->stride_samples +
        (size_t)source_column];
}

static int32_t pc_wiener_quantizer(unsigned int qindex) {
    static const uint16_t lookup[25] = {
        64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
        57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78
    };
    if (qindex < 25U) return lookup[qindex];
    return (int32_t)lookup[((qindex - 1U) % 24U) + 1U] <<
        ((qindex - 1U) / 24U);
}

static int pc_wiener_q_adjustment(
    unsigned int qindex, unsigned int tskip, unsigned int feature) {
    const int qstep = pc_wiener_quantizer(qindex);
    const int tskip_shifted = (int)tskip << 5;
    const int product = round_shift_symmetric(
        (int)tskip * qstep, 8U);
    int value = pc_wiener_mode_weights[feature][0] * tskip_shifted +
        pc_wiener_mode_weights[feature][1] * qstep +
        pc_wiener_mode_weights[feature][2] * product;
    value = round_shift_symmetric(value, 13U);
    return 255 * (value + pc_wiener_mode_offsets[feature]);
}

static unsigned int pc_wiener_tx_skip_sum(
    const Av2hRestorationControlMap *controls,
    int stripe_start, int stripe_height,
    int chunk_start, int chunk_width,
    int block_row, int block_column) {
    unsigned int sum = 0U;
    int row;
    for (row = -AV2H_PC_WIENER_FEATURE_LEAD;
         row <= AV2H_PC_WIENER_FEATURE_LAG; ++row) {
        int local_row = clamp_coordinate(
            block_row + row, stripe_height);
        int column;
        for (column = -AV2H_PC_WIENER_FEATURE_LEAD;
             column <= AV2H_PC_WIENER_FEATURE_LAG; ++column) {
            int local_column = clamp_coordinate(
                block_column + column, chunk_width);
            size_t map_row = (size_t)(stripe_start + local_row) / 4U;
            size_t map_column = (size_t)(chunk_start + local_column) / 4U;
            sum += controls->tx_skip[
                map_row * controls->tx_skip_stride + map_column];
        }
    }
    return sum * (256U / 36U);
}

static uint8_t pc_wiener_classify(
    const Av2hPcWienerSource *context,
    const Av2hRestorationControlMap *controls,
    unsigned int qindex, int chunk_start, int chunk_width,
    int block_row, int block_column) {
    int32_t feature[4] = { 0, 0, 0, 0 };
    unsigned int tskip = pc_wiener_tx_skip_sum(
        controls, context->stripe_start,
        context->stripe_end - context->stripe_start,
        chunk_start, chunk_width, block_row, block_column);
    int row;
    int lut_index = 0;
    for (row = -AV2H_PC_WIENER_FEATURE_LEAD;
         row <= AV2H_PC_WIENER_FEATURE_LAG; ++row) {
        int sample_row = context->stripe_start + block_row + row;
        int column;
        for (column = -AV2H_PC_WIENER_FEATURE_LEAD;
             column <= AV2H_PC_WIENER_FEATURE_LAG; ++column) {
            int sample_column = chunk_start + block_column + column;
            int center = 2 * pc_wiener_source_sample(
                context, sample_row, sample_column);
            feature[1] += absolute_value(
                (int)pc_wiener_source_sample(
                    context, sample_row - 1, sample_column) +
                pc_wiener_source_sample(
                    context, sample_row + 1, sample_column) - center);
            feature[2] += absolute_value(
                (int)pc_wiener_source_sample(
                    context, sample_row - 1, sample_column + 1) +
                pc_wiener_source_sample(
                    context, sample_row + 1, sample_column - 1) - center);
            feature[3] += absolute_value(
                (int)pc_wiener_source_sample(
                    context, sample_row - 1, sample_column - 1) +
                pc_wiener_source_sample(
                    context, sample_row + 1, sample_column + 1) - center);
        }
    }
    for (row = 0; row < 4; ++row) {
        int value = round_shift_symmetric(
            feature[row] * pc_wiener_feature_normalizers[row] +
                pc_wiener_q_adjustment(qindex, tskip, (unsigned int)row),
            14U);
        value = av2h_sample_clamp(value, 0, 255) >> 5U;
        lut_index += pc_wiener_lut_weights[row] * value;
    }
    return av2h_pc_wiener_lut_to_class[lut_index];
}

static void pc_wiener_filter_block(
    Av2hFilterPlane *destination,
    const Av2hPcWienerSource *context,
    int block_y, int block_x, int block_height, int block_width,
    const int16_t *filter) {
    static const int8_t tap_row[12] = {
        1, 0, 2, 0, 1, -1, 2, 2, 1, 1, 3, 0
    };
    static const int8_t tap_column[12] = {
        0, 1, 0, 2, 1, 1, 1, -1, 2, -2, 0, 3
    };
    int row;
    for (row = 0; row < block_height; ++row) {
        int column;
        for (column = 0; column < block_width; ++column) {
            int y = block_y + row;
            int x = block_x + column;
            int32_t value = (filter[12] + 128) *
                pc_wiener_source_sample(context, y, x);
            unsigned int tap;
            for (tap = 0U; tap < 12U; ++tap) {
                value += filter[tap] * (
                    (int)pc_wiener_source_sample(
                        context, y + tap_row[tap], x + tap_column[tap]) +
                    pc_wiener_source_sample(
                        context, y - tap_row[tap], x - tap_column[tap]));
            }
            destination->data[
                (size_t)y * destination->stride_samples + (size_t)x] =
                av2h_sample_clip(round_shift_symmetric(value, 7U));
        }
    }
}

static int pc_wiener_unit_row_start(
    const Av2hRestorationControlMap *controls, size_t row) {
    if (row == 0U) return 0;
    return (int)(row * controls->unit_size[0]) -
        AV2H_PC_WIENER_UNIT_OFFSET;
}

Av2hStatus av2h_pc_wiener_filter_luma(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *source,
    const Av2hFilterPlane *deblocked,
    const Av2hFrameState *frame,
    const Av2hRestorationControlMap *controls) {
    size_t unit_row;
    const int16_t *filters;
    const uint8_t *selector;
    unsigned int filter_set;
    if (destination == 0 || source == 0 || deblocked == 0 || frame == 0 ||
        controls == 0 || destination->data == 0 || source->data == 0 ||
        deblocked->data == 0 || controls->unit_flags[0] == 0 ||
        controls->tx_skip == 0 || controls->unit_size[0] == 0U ||
        controls->unit_columns[0] == 0U || controls->unit_rows[0] == 0U ||
        controls->tx_skip_stride < ((size_t)source->width + 3U) / 4U ||
        destination->width != source->width ||
        destination->height != source->height ||
        deblocked->width != source->width ||
        deblocked->height != source->height ||
        destination->stride_samples < destination->width ||
        source->stride_samples < source->width ||
        deblocked->stride_samples < deblocked->width ||
        (frame->restoration_type[0] != AV2H_RESTORE_PC_WIENER &&
         frame->restoration_type[0] != AV2H_RESTORE_SWITCHABLE)) {
        return AV2H_INVALID_ARGUMENT;
    }
    filter_set = pc_wiener_filter_set_index(frame->base_q_index);
    filters = pc_wiener_filter(filter_set, 0U);
    selector = pc_wiener_filter_selector(filter_set);
    filter_copy_plane(destination, source);
    for (unit_row = 0U; unit_row < controls->unit_rows[0]; ++unit_row) {
        int unit_y0 = pc_wiener_unit_row_start(controls, unit_row);
        int unit_y1 = unit_row + 1U == controls->unit_rows[0] ?
            source->height : pc_wiener_unit_row_start(controls, unit_row + 1U);
        size_t unit_column;
        for (unit_column = 0U; unit_column < controls->unit_columns[0];
             ++unit_column) {
            int unit_x0 = (int)(unit_column * controls->unit_size[0]);
            int unit_x1 = unit_column + 1U == controls->unit_columns[0] ?
                source->width : unit_x0 + controls->unit_size[0];
            int stripe_start;
            if (controls->unit_flags[0][
                    unit_row * controls->unit_columns[0] + unit_column] !=
                AV2H_RESTORE_PC_WIENER) {
                continue;
            }
            for (stripe_start = unit_y0; stripe_start < unit_y1;) {
                int relative_stripe =
                    (stripe_start + AV2H_PC_WIENER_UNIT_OFFSET) /
                    RESTORATION_PROC_UNIT_SIZE;
                int stripe_height = relative_stripe == 0 ?
                    RESTORATION_PROC_UNIT_SIZE -
                        AV2H_PC_WIENER_UNIT_OFFSET :
                    RESTORATION_PROC_UNIT_SIZE;
                Av2hPcWienerSource context;
                int chunk_start;
                if (stripe_height > unit_y1 - stripe_start) {
                    stripe_height = unit_y1 - stripe_start;
                }
                context = (Av2hPcWienerSource){
                    source, deblocked, stripe_start,
                    stripe_start + stripe_height
                };
                for (chunk_start = unit_x0; chunk_start < unit_x1;
                     chunk_start += RESTORATION_PROC_UNIT_SIZE) {
                    int chunk_width = unit_x1 - chunk_start;
                    int block_row;
                    if (chunk_width > RESTORATION_PROC_UNIT_SIZE) {
                        chunk_width = RESTORATION_PROC_UNIT_SIZE;
                    }
                    for (block_row = 0; block_row < stripe_height;
                         block_row += AV2H_PC_WIENER_BLOCK_SIZE) {
                        int block_column;
                        for (block_column = 0; block_column < chunk_width;
                             block_column += AV2H_PC_WIENER_BLOCK_SIZE) {
                            uint8_t class_index = pc_wiener_classify(
                                &context, controls, frame->base_q_index,
                                chunk_start, chunk_width,
                                block_row, block_column);
                            int block_height = stripe_height - block_row;
                            int block_width = chunk_width - block_column;
                            if (block_height > AV2H_PC_WIENER_BLOCK_SIZE) {
                                block_height = AV2H_PC_WIENER_BLOCK_SIZE;
                            }
                            if (block_width > AV2H_PC_WIENER_BLOCK_SIZE) {
                                block_width = AV2H_PC_WIENER_BLOCK_SIZE;
                            }
                            pc_wiener_filter_block(
                                destination, &context,
                                stripe_start + block_row,
                                chunk_start + block_column,
                                block_height, block_width,
                                filters +
                                    (size_t)selector[class_index] *
                                        NUM_PC_WIENER_TAPS_LUMA);
                        }
                    }
                }
                stripe_start += stripe_height;
            }
        }
    }
    return AV2H_OK;
}

static uint8_t wiener_nonsep_luma_sample(
    const Av2hPcWienerSource *luma, uint8_t filter_index,
    unsigned int chroma_shift, int row, int column) {
    int chroma_height =
        (luma->source->height + ((int)1 << chroma_shift) - 1) >> chroma_shift;
    int chroma_width =
        (luma->source->width + ((int)1 << chroma_shift) - 1) >> chroma_shift;
    row = clamp_coordinate(row, chroma_height);
    column = clamp_coordinate(column, chroma_width);
    if (chroma_shift == 0U) {
        return pc_wiener_source_sample(luma, row, column);
    }
    if (filter_index == 1U) {
        return (uint8_t)(((unsigned int)pc_wiener_source_sample(
                              luma, row * 2, column * 2) +
                          pc_wiener_source_sample(
                              luma, row * 2 + 1, column * 2)) >> 1U);
    }
    if (filter_index == 2U) {
        return pc_wiener_source_sample(luma, row * 2, column * 2);
    }
    return (uint8_t)(((unsigned int)pc_wiener_source_sample(
                          luma, row * 2, column * 2) +
                      pc_wiener_source_sample(
                          luma, row * 2, column * 2 + 1) +
                      pc_wiener_source_sample(
                          luma, row * 2 + 1, column * 2) +
                      pc_wiener_source_sample(
                          luma, row * 2 + 1, column * 2 + 1)) >> 2U);
}

static void wiener_nonsep_filter_luma_region(
    Av2hFilterPlane *destination, const Av2hPcWienerSource *source,
    int y0, int y1, int x0, int x1, const int16_t *taps) {
    static const int8_t tap_row[16] = {
        1, 0, 2, 0, 1, -1, 2, 2, 1, 1, 3, 0, 4, 0, 3, 3
    };
    static const int8_t tap_column[16] = {
        0, 1, 0, 2, 1, 1, 1, -1, 2, -2, 0, 3, 0, 4, 3, -3
    };
    int row;
    for (row = y0; row < y1; ++row) {
        int column;
        for (column = x0; column < x1; ++column) {
            int center = pc_wiener_source_sample(source, row, column);
            int32_t value = center << 7U;
            unsigned int tap;
            for (tap = 0U; tap < 16U; ++tap) {
                value += taps[tap] * (
                    (int)pc_wiener_source_sample(
                        source, row + tap_row[tap],
                        column + tap_column[tap]) +
                    pc_wiener_source_sample(
                        source, row - tap_row[tap],
                        column - tap_column[tap]) - 2 * center);
            }
            destination->data[(size_t)row * destination->stride_samples +
                              (size_t)column] =
                av2h_sample_clip(round_shift_symmetric(value, 7U));
        }
    }
}

static void wiener_nonsep_filter_chroma_region(
    Av2hFilterPlane *destination, const Av2hPcWienerSource *source,
    const Av2hPcWienerSource *luma, uint8_t luma_filter_index,
    unsigned int chroma_shift, int y0, int y1, int x0, int x1,
    const int16_t *taps) {
    static const int8_t uv_row[6] = { 1, 0, 1, -1, 2, 0 };
    static const int8_t uv_column[6] = { 0, 1, 1, 1, 0, 2 };
    static const int8_t luma_row[12] = {
        1, -1, 0, 0, 1, -1, -1, 1, 2, -2, 0, 0
    };
    static const int8_t luma_column[12] = {
        0, 0, 1, -1, 1, -1, 1, -1, 0, 0, 2, -2
    };
    int row;
    for (row = y0; row < y1; ++row) {
        int column;
        for (column = x0; column < x1; ++column) {
            int center = pc_wiener_source_sample(source, row, column);
            int luma_center = wiener_nonsep_luma_sample(
                luma, luma_filter_index, chroma_shift, row, column);
            int32_t value = center << 7U;
            unsigned int tap;
            for (tap = 0U; tap < 6U; ++tap) {
                value += taps[tap] * (
                    (int)pc_wiener_source_sample(
                        source, row + uv_row[tap],
                        column + uv_column[tap]) +
                    pc_wiener_source_sample(
                        source, row - uv_row[tap],
                        column - uv_column[tap]) - 2 * center);
            }
            for (tap = 0U; tap < 12U; ++tap) {
                value += taps[tap + 6U] * (
                    (int)wiener_nonsep_luma_sample(
                        luma, luma_filter_index, chroma_shift,
                        row + luma_row[tap],
                        column + luma_column[tap]) - luma_center);
            }
            destination->data[(size_t)row * destination->stride_samples +
                              (size_t)column] =
                av2h_sample_clip(round_shift_symmetric(value, 7U));
        }
    }
}

Av2hStatus av2h_wiener_nonsep_filter_frame(
    Av2hFilterPlanes *destination, const Av2hFilterPlanes *source,
    const Av2hFilterPlanes *deblocked, const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hRestorationControlMap *controls) {
    unsigned int plane;
    if (destination == 0 || source == 0 || deblocked == 0 || sequence == 0 ||
        frame == 0 || controls == 0 || frame->chroma_format_idc > 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *input = &source->plane[plane];
        const Av2hFilterPlane *deblocked_plane = &deblocked->plane[plane];
        Av2hFilterPlane *output = &destination->plane[plane];
        size_t unit_row;
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        int stripe_size = RESTORATION_PROC_UNIT_SIZE >> shift;
        int unit_offset = AV2H_PC_WIENER_UNIT_OFFSET >> shift;
        unsigned int class_count =
            frame->restoration_num_filter_classes[plane];
        const uint8_t *class_map = 0;
        if (frame->restoration_type[plane] != AV2H_RESTORE_WIENER_NONSEP &&
            frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE) {
            continue;
        }
        if (input->data == 0 || deblocked_plane->data == 0 ||
            output->data == 0 ||
            input->width != output->width ||
            input->height != output->height ||
            input->width != deblocked_plane->width ||
            input->height != deblocked_plane->height) {
            return AV2H_UNSUPPORTED;
        }
        if (frame->restoration_type[plane] != AV2H_RESTORE_SWITCHABLE) {
            filter_copy_plane(output, input);
        }
        if (class_count == 0U ||
            class_count > AV2H_WIENER_NONSEP_MAX_CLASSES ||
            controls->unit_flags[plane] == 0 ||
            controls->unit_taps[plane] == 0 ||
            controls->unit_columns[plane] == 0U ||
            controls->unit_rows[plane] == 0U ||
            controls->unit_size[plane] == 0U) {
            return AV2H_UNSUPPORTED;
        }
        if (class_count > 1U) {
            if (plane != 0U ||
                frame->restoration_frame_filters_on[plane] == 0U ||
                controls->tx_skip == 0 ||
                controls->tx_skip_stride < ((size_t)input->width + 3U) / 4U) {
                return AV2H_UNSUPPORTED;
            }
            class_map = av2h_wienerns_class_map(
                class_count,
                pc_wiener_filter_set_index(frame->base_q_index));
        }
        for (unit_row = 0U; unit_row < controls->unit_rows[plane];
             ++unit_row) {
            int unit_y0 = unit_row == 0U ? 0 :
                (int)(unit_row * controls->unit_size[plane]) - unit_offset;
            int unit_y1 = unit_row + 1U == controls->unit_rows[plane] ?
                input->height :
                (int)((unit_row + 1U) * controls->unit_size[plane]) -
                    unit_offset;
            size_t unit_column;
            for (unit_column = 0U;
                 unit_column < controls->unit_columns[plane];
                 ++unit_column) {
                size_t unit_index =
                    unit_row * controls->unit_columns[plane] + unit_column;
                int unit_x0 = (int)(
                    unit_column * controls->unit_size[plane]);
                int unit_x1 =
                    unit_column + 1U == controls->unit_columns[plane] ?
                    input->width : unit_x0 + controls->unit_size[plane];
                int stripe_start;
                const int16_t *taps = controls->unit_taps[plane] +
                    unit_index * AV2H_WIENER_NONSEP_MAX_TAPS;
                if (controls->unit_flags[plane][unit_index] !=
                    AV2H_RESTORE_WIENER_NONSEP) continue;
                for (stripe_start = unit_y0; stripe_start < unit_y1;) {
                    int relative_stripe =
                        (stripe_start + unit_offset) / stripe_size;
                    int stripe_height = stripe_size -
                        (relative_stripe == 0 ? unit_offset : 0);
                    Av2hPcWienerSource context;
                    if (stripe_height > unit_y1 - stripe_start) {
                        stripe_height = unit_y1 - stripe_start;
                    }
                    context = (Av2hPcWienerSource){
                        input, deblocked_plane, stripe_start,
                        stripe_start + stripe_height
                    };
                    if (class_count > 1U) {
                        int chunk_start;
                        for (chunk_start = unit_x0; chunk_start < unit_x1;
                             chunk_start += RESTORATION_PROC_UNIT_SIZE) {
                            int chunk_width = unit_x1 - chunk_start;
                            int block_row;
                            if (chunk_width > RESTORATION_PROC_UNIT_SIZE) {
                                chunk_width = RESTORATION_PROC_UNIT_SIZE;
                            }
                            for (block_row = 0; block_row < stripe_height;
                                 block_row += AV2H_PC_WIENER_BLOCK_SIZE) {
                                int block_column;
                                for (block_column = 0;
                                     block_column < chunk_width;
                                     block_column +=
                                         AV2H_PC_WIENER_BLOCK_SIZE) {
                                    uint8_t full_class = pc_wiener_classify(
                                        &context, controls,
                                        frame->base_q_index, chunk_start,
                                        chunk_width, block_row, block_column);
                                    unsigned int sub_class =
                                        class_map[full_class];
                                    int block_y = stripe_start + block_row;
                                    int block_x = chunk_start + block_column;
                                    int block_y1 =
                                        block_y + AV2H_PC_WIENER_BLOCK_SIZE;
                                    int block_x1 =
                                        block_x + AV2H_PC_WIENER_BLOCK_SIZE;
                                    if (sub_class >= class_count) {
                                        return AV2H_INVALID_DATA;
                                    }
                                    if (block_y1 >
                                        stripe_start + stripe_height) {
                                        block_y1 =
                                            stripe_start + stripe_height;
                                    }
                                    if (block_x1 > chunk_start + chunk_width) {
                                        block_x1 = chunk_start + chunk_width;
                                    }
                                    wiener_nonsep_filter_luma_region(
                                        output, &context, block_y, block_y1,
                                        block_x, block_x1,
                                        frame->restoration_frame_filter[
                                            plane][sub_class]);
                                }
                            }
                        }
                    } else if (plane == 0U) {
                        wiener_nonsep_filter_luma_region(
                            output, &context, stripe_start,
                            stripe_start + stripe_height, unit_x0, unit_x1,
                            taps);
                    } else {
                        Av2hPcWienerSource luma = {
                            &source->plane[0], &deblocked->plane[0],
                            stripe_start << shift,
                            (stripe_start + stripe_height) << shift
                        };
                        wiener_nonsep_filter_chroma_region(
                            output, &context, &luma,
                            sequence->cfl_downsample_filter_index,
                            shift,
                            stripe_start, stripe_start + stripe_height,
                            unit_x0, unit_x1, taps);
                    }
                    stripe_start += stripe_height;
                }
            }
        }
    }
    return AV2H_OK;
}
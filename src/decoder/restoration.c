#include "decoder/filter.h"
#include "decoder/internal/filter_common.h"

#define RESTORATION_PROC_UNIT_SIZE 64
#define NUM_PC_WIENER_TAPS_LUMA 13
#define NUM_FILTER_CLASSES_BITS 3
#define AVM_INLINE inline
#define assert(condition) ((void)0)
#include "../../vendor/avm/av2/common/pc_wiener_filters.h"
#undef assert
#undef AVM_INLINE

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
    int value = mode_weights[feature][0] * tskip_shifted +
        mode_weights[feature][1] * qstep +
        mode_weights[feature][2] * product;
    value = round_shift_symmetric(value, 13U);
    return 255 * (value + mode_offsets[feature]);
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
            feature[row] * feature_normalizers_luma[row] +
                pc_wiener_q_adjustment(qindex, tskip, (unsigned int)row),
            PC_WIENER_PREC_FEATURE);
        value = av2h_sample_clamp(value, 0, 255) >> pc_wiener_threshold_shift;
        lut_index += pc_wiener_thresholds[row] * value;
    }
    return pc_wiener_lut_to_class_index[lut_index];
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
    return (int)(row * controls->unit_size) - AV2H_PC_WIENER_UNIT_OFFSET;
}

Av2hStatus av2h_pc_wiener_filter_luma(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *source,
    const Av2hFilterPlane *deblocked,
    const Av2hFrameState *frame,
    const Av2hRestorationControlMap *controls) {
    size_t unit_row;
    const int16_t (*filters)[NUM_PC_WIENER_TAPS_LUMA];
    const uint8_t *selector;
    if (destination == 0 || source == 0 || deblocked == 0 || frame == 0 ||
        controls == 0 || destination->data == 0 || source->data == 0 ||
        deblocked->data == 0 || controls->unit_flags == 0 ||
        controls->tx_skip == 0 || controls->unit_size == 0U ||
        controls->unit_columns == 0U || controls->unit_rows == 0U ||
        controls->tx_skip_stride < ((size_t)source->width + 3U) / 4U ||
        destination->width != source->width ||
        destination->height != source->height ||
        deblocked->width != source->width ||
        deblocked->height != source->height ||
        destination->stride_samples < destination->width ||
        source->stride_samples < source->width ||
        deblocked->stride_samples < deblocked->width ||
        frame->restoration_type[0] != AV2H_RESTORE_PC_WIENER) {
        return AV2H_INVALID_ARGUMENT;
    }
    filters = get_filter_set(get_filter_set_index(frame->base_q_index, 0));
    selector = get_filter_selector(
        get_filter_set_index(frame->base_q_index, 0));
    filter_copy_plane(destination, source);
    for (unit_row = 0U; unit_row < controls->unit_rows; ++unit_row) {
        int unit_y0 = pc_wiener_unit_row_start(controls, unit_row);
        int unit_y1 = unit_row + 1U == controls->unit_rows ?
            source->height : pc_wiener_unit_row_start(controls, unit_row + 1U);
        size_t unit_column;
        for (unit_column = 0U; unit_column < controls->unit_columns;
             ++unit_column) {
            int unit_x0 = (int)(unit_column * controls->unit_size);
            int unit_x1 = unit_column + 1U == controls->unit_columns ?
                source->width : unit_x0 + controls->unit_size;
            int stripe_start;
            if (controls->unit_flags[
                    unit_row * controls->unit_columns + unit_column] == 0U) {
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
                                filters[selector[class_index]]);
                        }
                    }
                }
                stripe_start += stripe_height;
            }
        }
    }
    return AV2H_OK;
}
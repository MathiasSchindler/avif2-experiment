#include "decoder/filter.h"

#include "decoder/internal/filter_common.h"
#include "decoder/internal/filter_modules.h"
#include "decoder/internal/wiener_nonsep.h"

#include <stddef.h>

#define RESTORATION_PROC_UNIT_SIZE 64
#define NUM_PC_WIENER_TAPS_LUMA 13
#define AV2H_PC_WIENER_FILTER_COUNT 64U
#define AV2H_PC_WIENER_CLASS_COUNT 256U

#include "pc_wiener_data.inc"

#define AV2H_PC_WIENER_BLOCK_SIZE 4
#define AV2H_PC_WIENER_FEATURE_LEAD 1
#define AV2H_PC_WIENER_FEATURE_LAG 4
#define AV2H_PC_WIENER_UNIT_OFFSET 8

static const int32_t pc_wiener_feature_normalizers[4] = {0, 3739, 3273, 3074};

static const int32_t pc_wiener_mode_weights[4][3] = {
    {-527, 15325, 321}, {26436, -17705, 17905}, {366, -147, -194}, {202, -267, -179}};

static const int32_t pc_wiener_mode_offsets[4] = {-547, -21565, -573, -680};

static const int32_t pc_wiener_lut_weights[4] = {512, 64, 8, 1};

static unsigned int pc_wiener_filter_set_index(unsigned int qindex) {
    if (qindex < 130U)
        return 0U;
    if (qindex < 190U)
        return 1U;
    if (qindex < 220U)
        return 2U;
    return 3U;
}

static const int16_t *pc_wiener_filter(unsigned int set_index, unsigned int filter_index) {
    return av2h_pc_wiener_filters +
           ((size_t)(set_index & 3U) * AV2H_PC_WIENER_FILTER_COUNT + (filter_index & 63U)) *
               NUM_PC_WIENER_TAPS_LUMA;
}

static const uint8_t *pc_wiener_filter_selector(unsigned int set_index) {
    return av2h_pc_wiener_sub_classify + (size_t)(set_index & 3U) * AV2H_PC_WIENER_CLASS_COUNT;
}

const Av2hWienerNonsepParameters *av2h_wienerns_parameters(unsigned int plane) {
    static const Av2hWienerNonsepParameters luma = {
        (uint8_t)AV2H_WIENER_NONSEP_LUMA_TAPS,
        4U,
        16U,
        {6U, 6U, 5U, 5U, 5U, 5U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 0U, 0U},
        {-24, -24, -14, -14, -16, -16, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, 0, 0},
        {0x0000003fU, 0x00000fc3U, 0x00000fffU, 0x0000ffffU}};
    static const Av2hWienerNonsepParameters chroma = {
        (uint8_t)AV2H_WIENER_NONSEP_CHROMA_TAPS,
        3U,
        6U,
        {6U, 6U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U},
        {-24, -24, -14, -14, -16, -16, -16, -16, -16, -16, -8, -8, -8, -8, -8, -8, -8, -8},
        {0x0000003fU, 0x000003ffU, 0x0003ffffU, 0x0003ffffU}};
    return plane == 0U ? &luma : &chroma;
}

Av2hStatus av2h_wienerns_frame_predictor(
    unsigned int plane, uint32_t match_index, unsigned int class_index, unsigned int class_count,
    const int16_t decoded[AV2H_WIENER_NONSEP_MAX_CLASSES][AV2H_WIENER_NONSEP_MAX_TAPS],
    int16_t taps[AV2H_WIENER_NONSEP_MAX_TAPS]) {
    static const uint8_t shuffled_index[64] = {
        16U, 7U,  58U, 21U, 12U, 61U, 26U, 38U, 18U, 30U, 50U, 45U, 23U, 49U, 43U, 62U,
        42U, 54U, 27U, 36U, 17U, 44U, 32U, 34U, 4U,  24U, 52U, 31U, 37U, 11U, 33U, 19U,
        35U, 6U,  22U, 53U, 63U, 25U, 41U, 47U, 1U,  59U, 0U,  28U, 40U, 55U, 48U, 8U,
        5U,  51U, 9U,  46U, 56U, 60U, 15U, 2U,  13U, 14U, 57U, 29U, 3U,  20U, 39U, 10U};
    const Av2hWienerNonsepParameters *parameters = av2h_wienerns_parameters(plane);
    const int16_t *source;
    unsigned int index;
    unsigned int count;
    if (taps == NULL || decoded == NULL || class_count == 0U ||
        class_count > AV2H_WIENER_NONSEP_MAX_CLASSES || class_index >= class_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(taps, 0U, AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(*taps));
    if (match_index == 0U)
        return AV2H_OK;
    if (match_index < class_count) {
        if (match_index > class_index)
            return AV2H_INVALID_DATA;
        av2h_memory_copy(taps, decoded[match_index - 1U],
                         AV2H_WIENER_NONSEP_MAX_TAPS * sizeof(*taps));
        return AV2H_OK;
    }
    if (plane != 0U)
        return AV2H_INVALID_DATA;
    if (match_index - class_count >= 64U - class_count) {
        return AV2H_INVALID_DATA;
    }
    source = pc_wiener_filter(0U, shuffled_index[match_index - class_count]);
    count = NUM_PC_WIENER_TAPS_LUMA - 1;
    if (count > parameters->coefficient_count) {
        count = parameters->coefficient_count;
    }
    for (index = 0U; index < count; ++index) {
        int value = source[index];
        int maximum = parameters->minimum[index] + (1 << parameters->bits[index]) - 1;
        if (value < parameters->minimum[index]) {
            value = parameters->minimum[index];
        }
        if (value > maximum)
            value = maximum;
        taps[index] = (int16_t)value;
    }
    return AV2H_OK;
}

const uint8_t *av2h_wienerns_class_map(unsigned int class_count, unsigned int set_index) {
    unsigned int class_index;
    switch (class_count) {
    case 1U:
        class_index = 0U;
        break;
    case 2U:
        class_index = 1U;
        break;
    case 3U:
        class_index = 2U;
        break;
    case 4U:
        class_index = 3U;
        break;
    case 6U:
        class_index = 4U;
        break;
    case 8U:
        class_index = 5U;
        break;
    case 12U:
        class_index = 6U;
        break;
    case 16U:
        class_index = 7U;
        break;
    default:
        class_index = 0U;
        break;
    }
    return av2h_pc_wiener_sub_classify2 +
           ((size_t)(set_index & 3U) * 8U + class_index) * AV2H_PC_WIENER_CLASS_COUNT;
}

static int32_t pc_wiener_quantizer(unsigned int qindex) {
    static const uint16_t lookup[25] = {64, 40, 41, 43, 44, 45, 47, 48, 49, 51, 52, 54, 55,
                                        57, 59, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78};
    unsigned int shift;
    if (qindex < 25U)
        return lookup[qindex];
    shift = (qindex - 1U) / 24U;
    if (shift > 20U)
        shift = 20U;
    return (int32_t)((uint32_t)lookup[((qindex - 1U) % 24U) + 1U] << shift);
}

static int64_t pc_wiener_q_adjustment(unsigned int qindex, unsigned int tskip, unsigned int feature,
                                      int depth_shift) {
    const int64_t qstep = av2h_sample_round_pow2_signed(pc_wiener_quantizer(qindex), depth_shift);
    const int64_t tskip_shifted = (int64_t)tskip << 5U;
    const int64_t product = av2h_sample_round_pow2_signed((int64_t)tskip * qstep, 8);
    int64_t value = (int64_t)pc_wiener_mode_weights[feature][0] * tskip_shifted +
                    (int64_t)pc_wiener_mode_weights[feature][1] * qstep +
                    (int64_t)pc_wiener_mode_weights[feature][2] * product;
    value = av2h_sample_round_pow2_signed(value, 13);
    return 255 * (value + pc_wiener_mode_offsets[feature]);
}

static unsigned int pc_wiener_tx_skip_sum(const Av2hRestorationControlMap *controls,
                                          int stripe_start, int stripe_height, int chunk_start,
                                          int chunk_width, int block_row, int block_column) {
    unsigned int sum = 0U;
    int row;
    for (row = -AV2H_PC_WIENER_FEATURE_LEAD; row <= AV2H_PC_WIENER_FEATURE_LAG; ++row) {
        int local_row = av2h_sample_clamp(block_row + row, 0, stripe_height - 1);
        int column;
        for (column = -AV2H_PC_WIENER_FEATURE_LEAD; column <= AV2H_PC_WIENER_FEATURE_LAG;
             ++column) {
            int local_column = av2h_sample_clamp(block_column + column, 0, chunk_width - 1);
            size_t map_row = (size_t)(stripe_start + local_row) / 4U;
            size_t map_column = (size_t)(chunk_start + local_column) / 4U;
            sum += controls->tx_skip[map_row * controls->tx_skip_stride + map_column];
        }
    }
    return sum * (256U / 36U);
}

static int pc_wiener_unit_row_start(const Av2hRestorationControlMap *controls, size_t row) {
    if (row == 0U)
        return 0;
    return (int)(row * controls->unit_size[0]) - AV2H_PC_WIENER_UNIT_OFFSET;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/restoration_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/restoration_kernel.inc"
#include "internal/sample_kernel_end.inc"

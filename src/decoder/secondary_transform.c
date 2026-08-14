#include "reconstruct.h"
#include "internal/coeff_modules.h"
#include "internal/sample_math.h"
#include "internal/transform_modules.h"
#include "ist_data.inc"
#include "ist_type1_data.inc"
#include "ist_type2_data.inc"
#include "ist_set6_data.inc"
#include "ist_set2_data.inc"
#include "ist_set4_type3_data.inc"
#include "ist_set5_type1_data.inc"
#include "ist_set9_type1_data.inc"
#include "ist_adst_data.inc"
#include "ist_reached_gaps_data.inc"
#include "ist_palace_data.inc"

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    return av2h_sample_clamp(value, minimum, maximum);
}

static int32_t round_shift_signed(int64_t value, uint8_t shift) {
    int64_t magnitude = value < 0 ? -value : value;
    int32_t rounded = (int32_t)((magnitude +
        ((int64_t)1 << (shift - 1U))) >> shift);
    return value < 0 ? -rounded : rounded;
}

static uint16_t ist_scan_position(
    uint16_t scan_index, uint16_t width, uint16_t height) {
    unsigned int diagonal;
    unsigned int remaining = scan_index;
    for (diagonal = 0U; diagonal < (unsigned int)width + height - 1U;
         ++diagonal) {
        int row = (int)(diagonal < height ? diagonal : height - 1U);
        int last_row = diagonal >= width ? (int)(diagonal - width + 1U) : 0;
        unsigned int diagonal_count = (unsigned int)(row - last_row + 1);
        if (remaining < diagonal_count) {
            row -= (int)remaining;
            return (uint16_t)((unsigned int)row * width + diagonal -
                              (unsigned int)row);
        }
        remaining -= diagonal_count;
    }
    return AV2H_FIRST_COEFF_CAPACITY;
}

static uint16_t ist_output_position(
    uint16_t scan_index, uint16_t stride, uint16_t support_size,
    uint8_t transpose) {
    unsigned int diagonal;
    unsigned int remaining = scan_index;
    for (diagonal = 0U; diagonal < 2U * support_size - 1U; ++diagonal) {
        unsigned int first_row = diagonal >= support_size ?
            diagonal - support_size + 1U : 0U;
        unsigned int last_row = diagonal < support_size ?
            diagonal : support_size - 1U;
        unsigned int diagonal_count = last_row - first_row + 1U;
        unsigned int row;
        unsigned int column;
        if (remaining >= diagonal_count) {
            remaining -= diagonal_count;
            continue;
        }
        if (((diagonal & 1U) != 0U) != (transpose != 0U)) {
            row = first_row + remaining;
        } else {
            row = last_row - remaining;
        }
        column = diagonal - row;
        return (uint16_t)(row * stride + column);
    }
    return AV2H_FIRST_COEFF_CAPACITY;
}

static Av2hStatus inverse_secondary_transform_4x4(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]) {
    const int16_t (*kernel)[16U];
    uint8_t transpose_output = (uint8_t)(
        mode->mode == AV2H_INTRA_HORIZONTAL ||
        mode->mode == AV2H_INTRA_D157 ||
        mode->mode == AV2H_INTRA_D67 ||
        mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL);
    int32_t input[8U];
    uint16_t index;
    if (transform->secondary_set == 0U &&
        transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set0_type1_kernel;
    } else if (transform->secondary_set == 0U &&
        transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set0_type2_kernel;
    } else if (transform->secondary_set == 0U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set0_type3_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set1_type1_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set1_type2_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set1_type3_kernel;
    } else if (transform->secondary_set == 2U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set2_type1_kernel;
    } else if (transform->secondary_set == 2U &&
        transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set2_type2_kernel;
    } else if (transform->secondary_set == 2U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set2_type3_kernel;
    } else if (transform->secondary_set == 3U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set3_type1_kernel;
    } else if (transform->secondary_set == 3U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set3_type2_kernel;
    } else if (transform->secondary_set == 3U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set3_type3_kernel;
    } else if (transform->secondary_set == 4U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set4_type1_kernel;
    } else if (transform->secondary_set == 4U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set4_type2_kernel;
    } else if (transform->secondary_set == 4U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set4_type3_kernel;
    } else if (transform->secondary_set == 5U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set5_type1_kernel;
    } else if (transform->secondary_set == 5U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set5_type2_kernel;
    } else if (transform->secondary_set == 5U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set5_type3_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set6_type1_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set6_type2_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set6_type3_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set7_type1_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set7_type2_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set7_type3_kernel;
    } else if (transform->secondary_set == 8U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set8_type1_kernel;
    } else if (transform->secondary_set == 8U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set8_type2_kernel;
    } else if (transform->secondary_set == 8U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set8_type3_kernel;
    } else if (transform->secondary_set == 9U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set9_type1_kernel;
    } else if (transform->secondary_set == 9U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set9_type2_kernel;
    } else if (transform->secondary_set == 9U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set9_type3_kernel;
    } else if (transform->secondary_set == 10U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set10_type1_kernel;
    } else if (transform->secondary_set == 10U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set10_type2_kernel;
    } else if (transform->secondary_set == 10U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set10_type3_kernel;
    } else if (transform->secondary_set == 11U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set11_type1_kernel;
    } else if (transform->secondary_set == 11U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set11_type2_kernel;
    } else if (transform->secondary_set == 11U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set11_type3_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set12_type1_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set12_type2_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set12_type3_kernel;
    } else if (transform->secondary_set == 13U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set13_type1_kernel;
    } else if (transform->secondary_set == 13U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set13_type2_kernel;
    } else if (transform->secondary_set == 13U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set13_type3_kernel;
    } else {
        return AV2H_UNSUPPORTED;
    }
    for (index = 0U; index < 8U; ++index) {
        uint16_t position = ist_scan_position(
            index, coefficients->width, coefficients->height);
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        input[index] = dequantized[position];
        dequantized[position] = 0;
    }
    for (index = 0U; index < 16U; ++index) {
        uint16_t row;
        uint16_t position = ist_output_position(
            index, coefficients->width, 4U, transpose_output);
        int64_t sum = 0;
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        for (row = 0U; row < 8U; ++row) {
            sum += (int64_t)input[row] * kernel[row][index];
        }
        dequantized[position] =
            clamp(round_shift_signed(sum, 7U), -32768, 32767);
    }
    return AV2H_OK;
}

Av2hStatus av2h_inverse_secondary_transform(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]) {
    Av2hFirstLeafMode mapped_mode;
    const Av2hTxDescriptor *descriptor;
    int32_t input[32U];
    const int16_t (*kernel)[48U];
    const uint8_t *scan_map = av2h_ist_8x8_set1_type3_scan_map;
    const uint16_t *output_positions = 0;
    uint8_t transpose_output;
    uint16_t input_count;
    uint16_t index;
    uint8_t input_vertical;
    uint8_t input_column_scan;
    Av2hStatus status = av2h_tx_descriptor_get(
        prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    mapped_mode = *mode;
    mapped_mode.mode = av2h_transform_mode(
        mode->mode, mode->angle_delta, mode->mrl_index, descriptor);
    mode = &mapped_mode;
    if (transform->secondary_symbol == 0U) return AV2H_OK;
    if (coefficients->width < 8U || coefficients->height < 8U) {
        return inverse_secondary_transform_4x4(
            mode, transform, coefficients, dequantized);
    }
    if (mode->mode == AV2H_INTRA_DC &&
        prefix->tx_size == AV2H_TX_4X4 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 8U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx4_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx4_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set8_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC &&
        prefix->tx_size == AV2H_TX_8X4 &&
        transform->secondary_symbol == 2U &&
        transform->secondary_set == 12U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx8_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx8_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set12_type2_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        prefix->tx_size == AV2H_TX_8X4 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 6U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx8_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx8_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set6_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D135 &&
        prefix->tx_size == AV2H_TX_4X8 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 3U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx4_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx4_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set3_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    transpose_output = (uint8_t)(
        mode->mode == AV2H_INTRA_HORIZONTAL ||
        mode->mode == AV2H_INTRA_D157 ||
        mode->mode == AV2H_INTRA_D67 ||
        mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL);
    input_vertical = (uint8_t)(
        transform->primary_type == AV2H_TX_V_DCT ||
        transform->primary_type == AV2H_TX_V_ADST ||
        transform->primary_type == AV2H_TX_V_FLIPADST);
    input_column_scan = (uint8_t)(
        transform->primary_type == AV2H_TX_H_DCT ||
        transform->primary_type == AV2H_TX_H_ADST ||
        transform->primary_type == AV2H_TX_H_FLIPADST);
    input_count = (uint16_t)(
        prefix->tx_size == AV2H_TX_8X8 ||
        transform->primary_type != AV2H_TX_DCT_DCT ? 20U : 32U);
    /* Only these geometries use output positions beyond the set/type map. */
    if (!((mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_8X32 &&
           transform->secondary_symbol == 2U &&
           transform->secondary_set == 0U) ||
          (mode->mode == AV2H_INTRA_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_32X64 &&
           transform->secondary_symbol == 2U &&
           transform->secondary_set == 0U) ||
          (mode->mode == AV2H_INTRA_DC &&
           prefix->tx_size == AV2H_TX_8X16 &&
           transform->secondary_set == 6U) ||
          (mode->mode == AV2H_INTRA_SMOOTH &&
           prefix->tx_size == AV2H_TX_8X16 &&
           transform->secondary_symbol == 3U &&
           transform->secondary_set == 6U) ||
          (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_64X64 &&
           transform->secondary_symbol == 1U &&
           transform->secondary_set == 0U) ||
          (mode->mode == AV2H_INTRA_D135 &&
           prefix->tx_size == AV2H_TX_16X16 &&
           transform->secondary_symbol == 2U &&
           transform->secondary_set == 3U))) {
        goto generic_dispatch;
    }
    goto specialized_dispatch;
generic_dispatch:
    if (transform->secondary_symbol == 1U &&
        transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
        transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
             transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type2_kernel;
         scan_map = av2h_ist_8x8_set3_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type2_kernel;
        scan_map = av2h_ist_8x8_set2_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type3_kernel;
        scan_map = av2h_ist_8x8_set7_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type1_kernel;
        scan_map = av2h_ist_8x8_set7_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type2_kernel;
        scan_map = av2h_ist_8x8_set7_type2_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type2_kernel;
        scan_map = av2h_ist_8x8_set8_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type3_kernel;
        scan_map = av2h_ist_8x8_set8_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type1_kernel;
        scan_map = av2h_ist_8x8_set8_type1_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type1_kernel;
        scan_map = av2h_ist_8x8_set9_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type2_kernel;
        scan_map = av2h_ist_8x8_set9_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type3_kernel;
        scan_map = av2h_ist_8x8_set9_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type2_kernel;
        scan_map = av2h_ist_8x8_set10_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type1_kernel;
        scan_map = av2h_ist_8x8_set10_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else {
        return AV2H_UNSUPPORTED;
    }
    goto apply_transform;
specialized_dispatch:
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
        prefix->tx_size == AV2H_TX_8X32 &&
        transform->secondary_symbol == 2U &&
        transform->secondary_set == 0U &&
        coefficients->width == 8U && coefficients->height == 32U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        output_positions =
            av2h_ist_8x32_set0_type2_transpose_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        output_positions = av2h_ist_32x64_set0_type2_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_SMOOTH) &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               (prefix->tx_size == AV2H_TX_8X16 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type2_kernel;
        output_positions = av2h_ist_16x16_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_32X16 ||
                prefix->tx_size == AV2H_TX_64X8) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               (prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D135) &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               (mode->angle_delta == 2 ||
                (mode->angle_delta == 0 && mode->mrl_index == 1U)) &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               mode->angle_delta == 2 &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D113 &&
        prefix->tx_size == AV2H_TX_64X32 &&
        transform->secondary_symbol == 3U &&
        transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL) &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X32) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               mode->angle_delta == -3 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_DC) &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               (prefix->tx_size == AV2H_TX_32X8 ||
                prefix->tx_size == AV2H_TX_64X8) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               mode->angle_delta == 0 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X8) &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45) &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_VERTICAL) &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if ((mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D135) &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_VERTICAL) &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               (prefix->tx_size == AV2H_TX_64X16 ||
                prefix->tx_size == AV2H_TX_64X64) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        output_positions =
            av2h_ist_64x64_set0_type1_transpose_positions;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL) &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
               (prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_64X16) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
             transform->secondary_set == 10U) {
         kernel = av2h_ist_8x8_set10_type3_kernel;
         scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else {
        goto generic_dispatch;
    }
apply_transform:
    for (index = 0U; index < input_count; ++index) {
        uint16_t position = av2h_coeff_scan_position(
            index, coefficients->width, coefficients->height,
            input_vertical, input_column_scan);
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        input[index] = dequantized[position];
        dequantized[position] = 0;
    }
    if (output_positions != 0) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
    }
    for (index = 0U; index < 48U; ++index) {
        uint16_t row;
        int64_t sum = 0;
        uint16_t position;
        for (row = 0U; row < input_count; ++row) {
            sum += (int64_t)input[row] * kernel[row][index];
        }
        if (output_positions != 0) {
            position = output_positions[index];
        } else {
            position = ist_output_position(
                scan_map[index], coefficients->width, 8U,
                transpose_output);
        }
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        dequantized[position] =
            clamp(round_shift_signed(sum, 7U), -32768, 32767);
    }
    return AV2H_OK;
}


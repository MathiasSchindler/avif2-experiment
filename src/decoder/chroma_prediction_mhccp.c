#include "internal/chroma_prediction.h"

#include "internal/decode_diagnostics.h"
#include "internal/sample_math.h"
#include "internal/sample_ops.h"
#include "shared/bit_math.h"

#include <stddef.h>

#define AV2H_MHCCP_DECIM_BITS 16
#define AV2H_MHCCP_DIV_PREC_BITS 14
#define AV2H_MHCCP_DIV_SLOT_BITS 3

static uint16_t mhccp_luma_filter2_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y);

static uint16_t mhccp_luma_q3(const Av2hReconstructionPlane *luma_plane, uint16_t chroma_x,
                              uint16_t chroma_y);
static uint16_t mhccp_luma_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane, uint16_t chroma_x,
                                  uint16_t chroma_y);
static uint16_t mhccp_luma_filter2_q3(const Av2hReconstructionPlane *luma_plane, uint16_t chroma_x,
                                      uint16_t chroma_y);
static uint16_t mhccp_luma_filter2_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                          uint16_t chroma_x, uint16_t chroma_y);
static uint16_t mhccp_luma_filter2_top_clamped_q3(const Av2hReconstructionPlane *luma_plane,
                                                  uint16_t chroma_x, uint16_t chroma_y);
static uint16_t mhccp_luma_filter2_top_clamped_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                                      uint16_t chroma_x, uint16_t chroma_y);
static uint16_t mhccp_luma_filter2_sb_top_q3(const Av2hReconstructionPlane *luma_plane,
                                             uint16_t chroma_x, uint16_t block_y);
static uint16_t mhccp_luma_filter2_sb_top_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                                 uint16_t chroma_x, uint16_t block_y);
static uint16_t mhccp_luma_filter1_q3(const Av2hReconstructionPlane *luma_plane, uint16_t chroma_x,
                                      uint16_t chroma_y);
static uint16_t mhccp_luma_filter1_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                          uint16_t chroma_x, uint16_t chroma_y);
static uint16_t mhccp_luma_filter1_sb_top_q3(const Av2hReconstructionPlane *luma_plane,
                                             uint16_t chroma_x, uint16_t block_y);
static uint16_t mhccp_luma_filter1_sb_top_q3_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                                 uint16_t chroma_x, uint16_t block_y);
Av2hStatus av2h_mhccp_predict_observed(Av2hDecodeDiagnostics *diagnostics,
                                       const Av2hChromaLeafMode *mode,
                                       const Av2hReconstructionPlane *luma_plane,
                                       const Av2hReconstructionPlane *chroma_plane,
                                       uint16_t block_x, uint16_t block_y, uint16_t width,
                                       uint16_t height, uint8_t *predictor,
                                       size_t predictor_stride_samples);
Av2hStatus av2h_mhccp_predict_observed_u16(const Av2hChromaLeafMode *mode,
                                           const Av2hReconstructionPlaneU16 *luma_plane,
                                           const Av2hReconstructionPlaneU16 *chroma_plane,
                                           uint16_t block_x, uint16_t block_y, uint16_t width,
                                           uint16_t height, uint16_t *predictor,
                                           size_t predictor_stride_samples,
                                           const Av2hSampleDomain *domain);

static int32_t mhccp_mul_fixed(int32_t left, int32_t right, int shift) {
    uint32_t abs_left = (uint32_t)(left < 0 ? -left : left);
    uint32_t abs_right = (uint32_t)(right < 0 ? -right : right);
    int left_bits = (int)av2h_floor_log2_u32(abs_left) + 1;
    int right_bits = (int)av2h_floor_log2_u32(abs_right) + 1;
    int drop = left_bits + right_bits - 29;
    int left_drop;
    int right_drop;
    int adjusted_shift;
    int32_t product;
    uint32_t bias;
    if (drop < 0) drop = 0;
    left_drop = drop >> 1;
    right_drop = drop - left_drop;
    adjusted_shift = shift - left_drop - right_drop;
    product = (left_drop != 0 ? left >> left_drop : left) *
              (right_drop != 0 ? right >> right_drop : right);
    if (adjusted_shift <= 0) return product;
    if (adjusted_shift > 29) return 0;
    bias = (uint32_t)1U << (adjusted_shift - 1);
    return product >= 0 ?
        (int32_t)(((uint32_t)product + bias) >> adjusted_shift) :
        -(int32_t)(((uint32_t)(-product) + bias) >> adjusted_shift);
}

static void mhccp_division_scale(uint32_t denominator, int32_t *scale,
                                  int *shift) {
    static const int32_t square_weight[8U] = {
        214, 153, 113, 86, 67, 53, 43, 35
    };
    static const int32_t offset[8U] = {
        4822, 5952, 6624, 6792, 6408, 5424, 3792, 1466
    };
    static const int32_t bias[8U] = {
        12784, 12054, 11670, 11583, 11764, 12195, 12870, 13782
    };
    int delta;
    int32_t normalized;
    int32_t fraction;
    int32_t centered;
    int32_t index;
    *shift = (int)av2h_floor_log2_u32(denominator);
    delta = *shift - AV2H_MHCCP_DIV_PREC_BITS;
    if (delta >= 0) {
        int32_t rounding = delta > 0 ?
            (int32_t)((uint32_t)1U << (delta - 1)) : 0;
        normalized = ((int32_t)denominator + rounding) >> delta;
    } else {
        normalized = (int32_t)(denominator << -delta);
    }
    if (normalized < 1) normalized = 1;
    if (normalized > (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1) {
        normalized = (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1;
    }
    fraction = normalized & ((1 << AV2H_MHCCP_DIV_PREC_BITS) - 1);
    index = fraction >>
        (AV2H_MHCCP_DIV_PREC_BITS - AV2H_MHCCP_DIV_SLOT_BITS);
    centered = fraction - offset[index];
    *scale = ((square_weight[index] *
               ((centered * centered) >> AV2H_MHCCP_DIV_PREC_BITS)) >> 8) -
             (centered >> 1) + bias[index];
    *scale <<= AV2H_MHCCP_DECIM_BITS - AV2H_MHCCP_DIV_PREC_BITS;
}

static void mhccp_solve(int32_t matrix[3U][3U], int32_t target[3U],
                        int32_t result[3U], uint8_t bit_depth) {
    int32_t work[3U][4U] = { { 0 } };
    int row;
    for (row = 0; row < 3; ++row) {
        int column;
        for (column = 0; column < 3; ++column) {
            work[row][column] = column >= row ?
                matrix[row][column] : matrix[column][row];
        }
        work[row][row] += 2 << (bit_depth - 8U);
        work[row][3] = target[row];
    }
    for (row = 0; row < 3; ++row) {
        uint32_t diagonal = (uint32_t)(work[row][row] < 0 ?
            -work[row][row] : work[row][row]);
        int32_t scale;
        int shift;
        int lower_row;
        if (diagonal < 1U) diagonal = 1U;
        mhccp_division_scale(diagonal, &scale, &shift);
        for (lower_row = row + 1; lower_row < 4; ++lower_row) {
            work[row][lower_row] = mhccp_mul_fixed(
                work[row][lower_row], scale, shift);
        }
        for (lower_row = row + 1; lower_row < 3; ++lower_row) {
            int32_t factor = work[lower_row][row];
            int column;
            for (column = row + 1; column < 4; ++column) {
                work[lower_row][column] -= mhccp_mul_fixed(
                    factor, work[row][column], AV2H_MHCCP_DECIM_BITS);
            }
        }
    }
    result[2] = work[2][3];
    for (row = 1; row >= 0; --row) {
        int column;
        result[row] = work[row][3];
        for (column = row + 1; column < 3; ++column) {
            result[row] -= mhccp_mul_fixed(
                work[row][column], result[column],
                AV2H_MHCCP_DECIM_BITS);
        }
    }
}

static void mhccp_solve_samples(int64_t matrix[3U][3U], int64_t target[3U], uint16_t count,
                                const Av2hSampleDomain *domain, int32_t parameters[3U]) {
    int32_t normalized_matrix[3U][3U] = {{0}};
    int32_t normalized_target[3U];
    int shift = (int)(12U - domain->bit_depth) - (int)av2h_ceil_log2_u32(count);
    unsigned int row;
    /* 12-bit products can exceed int32 before reference-count normalization. */
    for (row = 0U; row < 3U; ++row) {
        unsigned int column;
        for (column = row; column < 3U; ++column) {
            normalized_matrix[row][column] =
                shift > 0 ? (int32_t)(matrix[row][column] << shift)
                          : av2h_sample_floor_shift(matrix[row][column], (unsigned int)-shift);
        }
        normalized_target[row] = shift > 0
                                     ? (int32_t)(target[row] << shift)
                                     : av2h_sample_floor_shift(target[row], (unsigned int)-shift);
    }
    mhccp_solve(normalized_matrix, normalized_target, parameters, domain->bit_depth);
}

static uint16_t mhccp_luma_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y,
    uint8_t full_resolution) {
    if (full_resolution) {
        return (uint16_t)(luma_plane->data[
            (size_t)chroma_y * luma_plane->stride_samples + chroma_x] << 3U);
    }
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    if (filter_index == 0U) {
        return mhccp_luma_q3(luma_plane, chroma_x, chroma_y);
    }
    if (filter_index == 1U) {
        return (uint16_t)(
            3U * luma_plane->data[
                center_y * luma_plane->stride_samples + center_x] +
            luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
            3U * luma_plane->data[
                bottom_y * luma_plane->stride_samples + center_x] +
            luma_plane->data[bottom_y * luma_plane->stride_samples + right_x]);
    }
    return (uint16_t)(
        5U * luma_plane->data[
            center_y * luma_plane->stride_samples + center_x] +
        luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
        luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}

static uint16_t mhccp_luma_filter0_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(4U * (
        luma_plane->data[center_y * luma_plane->stride_samples + center_x] +
        luma_plane->data[center_y * luma_plane->stride_samples + right_x]));
}

static uint16_t mhccp_luma_sb_top_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y,
    uint8_t full_resolution) {
    if (full_resolution) {
        return (uint16_t)(luma_plane->data[
            ((size_t)block_y - 1U) * luma_plane->stride_samples + chroma_x] << 3U);
    }
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    uint16_t center = luma_plane->data[
        center_y * luma_plane->stride_samples + center_x];
    uint16_t right = luma_plane->data[
        center_y * luma_plane->stride_samples + right_x];
    if (filter_index == 0U) return (uint16_t)(4U * (center + right));
    if (filter_index == 1U) return (uint16_t)(2U * (3U * center + right));
    return (uint16_t)(7U * center + right);
}

static uint16_t mhccp_luma_sb_top_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y,
    uint8_t full_resolution) {
    if (full_resolution) {
        return (uint16_t)(luma_plane->data[
            ((size_t)block_y - 1U) * luma_plane->stride_samples + chroma_x] << 3U);
    }
    return filter_index == 0U ?
        mhccp_luma_filter0_sb_top_q3(luma_plane, chroma_x, block_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_sb_top_q3(luma_plane, chroma_x, block_y) :
        mhccp_luma_filter2_sb_top_q3(luma_plane, chroma_x, block_y);
}

static uint16_t mhccp_luma_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y,
    uint8_t full_resolution) {
    if (full_resolution) {
        return (uint16_t)(luma_plane->data[
            (size_t)chroma_y * luma_plane->stride_samples + chroma_x] << 3U);
    }
    return filter_index == 0U ?
        mhccp_luma_q3(luma_plane, chroma_x, chroma_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_q3(luma_plane, chroma_x, chroma_y) :
        mhccp_luma_filter2_q3(luma_plane, chroma_x, chroma_y);
}

const Av2hPredictionDiagnostics *av2h_prediction_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    return diagnostics != NULL ? &diagnostics->prediction : NULL;
#else
    (void)diagnostics;
    return NULL;
#endif
}

void av2h_prediction_diagnostics_set_mhccp_reason(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value) {
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (diagnostics != NULL) diagnostics->prediction.mhccp_reason = value;
#else
    (void)diagnostics;
    (void)value;
#endif
}
int av2h_chroma_geometry_supported(uint16_t width, uint16_t height);

static Av2hStatus mhccp_predict_direction01(
    Av2hDecodeDiagnostics *diagnostics,
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index,
    uint8_t direction,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t superblock_size,
    uint8_t full_resolution,
    uint8_t *predictor,
    size_t predictor_stride_samples) {
    int32_t matrix[3U][3U] = { { 0 } };
    int32_t target[3U] = { 0 };
    int32_t parameters[3U];
    uint16_t sample;
    uint16_t row;
    uint16_t column;
    uint16_t count = 0U;
    uint16_t top_count;
    uint16_t left_count;
    uint16_t top_start_x;
    uint16_t top_chroma_width;
    uint16_t top_luma_width;
    uint16_t prediction_width;
    uint16_t prediction_height;
    int32_t top_left_center = 0;
    uint8_t have_top;
    uint8_t have_left;
    uint8_t is_top_sb_boundary;
    unsigned int scale = full_resolution ? 1U : 2U;
    int matrix_shift;
    uint32_t count_ceiling;
    av2h_prediction_diagnostics_set_mhccp_reason(
        diagnostics, 0U);
    have_top = (uint8_t)(block_y >= scale);
    have_left = (uint8_t)(block_x >= scale);
    is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
        ((uint32_t)block_y * scale) % superblock_size == 0U);
    if (filter_index > 2U || direction > 2U ||
        width == 0U || height == 0U ||
                block_x >= chroma_plane->reference_width ||
                block_y >= chroma_plane->height ||
        (have_left &&
                 (block_x - 1U >= chroma_plane->reference_width ||
                    block_x * scale - scale >= luma_plane->reference_width))) {
                av2h_prediction_diagnostics_set_mhccp_reason(
                    diagnostics, 6U);
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if (!have_top && !have_left) {
        if (!av2h_chroma_plane_region_reconstructed(
                luma_plane, (uint16_t)(block_x * 2U),
                (uint16_t)(block_y * 2U),
                (uint16_t)(prediction_width * 2U),
                (uint16_t)(prediction_height * 2U))) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < prediction_height; ++row) {
            for (column = 0U; column < prediction_width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] =
                    av2h_sample_midpoint_8bit();
            }
        }
        return AV2H_OK;
    }
    top_count = have_top ? (uint16_t)(width * 2U) : 0U;
    if (!have_left && top_count >= 2U) top_count = (uint16_t)(top_count - 2U);
    if (top_count > (full_resolution ? 126U : 62U)) {
        top_count = full_resolution ? 126U : 62U;
    }
    left_count = have_left ? (uint16_t)(height * 2U - 1U) : 0U;
    if (have_top && left_count > (full_resolution ? 63U : 61U)) {
        left_count = full_resolution ? 63U : 61U;
    }
    if (left_count >= chroma_plane->height - block_y) {
        left_count = (uint16_t)(chroma_plane->height - block_y - 1U);
    }
    if (have_left) {
        uint16_t available_left = 0U;
         while (available_left < left_count &&
             av2h_plane_sample_reconstructed(
                   chroma_plane, (size_t)block_x - 1U,
                   (size_t)block_y + available_left)) {
            ++available_left;
        }
        if (available_left > height - 1U) {
            available_left = (uint16_t)(height - 1U +
                ((available_left - (height - 1U)) & ~1U));
        }
        left_count = available_left;
        have_left = (uint8_t)(available_left != 0U);
    }
    top_start_x = (uint16_t)(have_left ? block_x - 1U : block_x + 1U);
    top_chroma_width = 0U;
    top_luma_width = 0U;
    if (have_top) {
        uint16_t available_top = 0U;
        uint16_t effective_chroma_width = (uint16_t)(
            (chroma_plane->reference_width + (full_resolution ? 7U : 3U)) &
            ~(size_t)(full_resolution ? 7U : 3U));
        uint16_t effective_luma_width = (uint16_t)(
            (luma_plane->reference_width + 7U) & ~(size_t)7U);
        if (effective_chroma_width > chroma_plane->width) {
            effective_chroma_width = chroma_plane->width;
        }
        if (effective_luma_width > luma_plane->width) {
            effective_luma_width = luma_plane->width;
        }
        top_chroma_width = effective_chroma_width > top_start_x + 1U ?
            (uint16_t)(effective_chroma_width - top_start_x - 1U) : 0U;
        if (top_chroma_width > top_count) top_chroma_width = top_count;
        top_luma_width = effective_luma_width > top_start_x * scale ?
            (uint16_t)(effective_luma_width - top_start_x * scale) : 0U;
        if (top_luma_width > top_count * scale) {
            top_luma_width = (uint16_t)(top_count * scale);
        }
        if (top_count > top_chroma_width) top_count = top_chroma_width;
        if (top_count > (top_luma_width + scale - 1U) / scale) {
            top_count = (uint16_t)(
                (top_luma_width + scale - 1U) / scale);
        }
         while (available_top < top_count &&
             av2h_plane_sample_reconstructed(
                   chroma_plane, (size_t)top_start_x + available_top,
                   (size_t)block_y - 1U)) {
            ++available_top;
        }
        if (!have_left && available_top != 0U && available_top <= width) {
            --available_top;
        }
        if (available_top > width) {
            available_top = (uint16_t)(width +
                ((available_top - width) & ~1U));
        }
        top_count = available_top;
        top_chroma_width = available_top;
        top_luma_width = (uint16_t)(available_top * scale);
        have_top = (uint8_t)(available_top != 0U);
    }
    if (have_top &&
        (top_chroma_width == 0U || top_luma_width == 0U ||
         !av2h_chroma_plane_region_reconstructed(
             chroma_plane, top_start_x, (uint16_t)(block_y - 1U),
             top_chroma_width, 1U))) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 1U);
        return AV2H_UNSUPPORTED;
    }
    if (have_top && !av2h_chroma_plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * scale),
            (uint16_t)(block_y * scale - scale), top_luma_width,
            (uint16_t)scale)) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 2U);
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !av2h_chroma_plane_region_reconstructed(
            chroma_plane, (uint16_t)(block_x - 1U), block_y, 1U,
            left_count)) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 3U);
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !av2h_chroma_plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * scale - scale),
            (uint16_t)(block_y * scale), (uint16_t)scale,
            (uint16_t)(left_count * scale))) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 4U);
        return AV2H_UNSUPPORTED;
    }
    if (!av2h_chroma_plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * scale),
            (uint16_t)(block_y * scale),
            (uint16_t)(prediction_width * scale),
            (uint16_t)(prediction_height * scale))) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 5U);
        return AV2H_UNSUPPORTED;
    }
    if (direction == 1U && have_top &&
        !av2h_chroma_plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * scale),
            (uint16_t)(block_y * scale - 2U * scale),
            top_luma_width, (uint16_t)scale)) {
        av2h_prediction_diagnostics_set_mhccp_reason(
            diagnostics, 8U);
        return AV2H_UNSUPPORTED;
    }
    for (sample = 0U; sample < top_count; ++sample) {
        uint16_t luma_chroma_x = (uint16_t)(top_start_x + sample);
        uint16_t chroma_x = luma_chroma_x;
        uint16_t chroma_y = (uint16_t)(block_y - 1U);
        int32_t vector[3U];
        int first;
        int second;
        {
            int32_t center = (int32_t)((is_top_sb_boundary ?
                    mhccp_luma_sb_top_sample_q3(
                        luma_plane, filter_index, luma_chroma_x, block_y,
                        full_resolution) :
                    mhccp_luma_sample_q3(
                        luma_plane, filter_index, luma_chroma_x,
                        chroma_y, full_resolution)) >> 3U);
            if (sample == 0U) top_left_center = center;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (is_top_sb_boundary ? center :
                    (int32_t)((full_resolution ?
                        mhccp_luma_sample_q3(
                            luma_plane, filter_index, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U), full_resolution) :
                     filter_index == 0U ?
                        mhccp_luma_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                     filter_index == 1U ?
                        mhccp_luma_filter1_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                        mhccp_luma_filter2_top_clamped_q3(
                            luma_plane, luma_chroma_x,
                            (uint16_t)(chroma_y - 1U))) >> 3U)) :
                    (int32_t)((is_top_sb_boundary ?
                        (sample == 0U && have_left ?
                            mhccp_luma_sb_top_left_clamped_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U), block_y,
                                full_resolution) :
                            mhccp_luma_sb_top_sample_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U), block_y,
                                full_resolution)) :
                        sample == 0U && have_left ?
                            mhccp_luma_left_clamped_q3(
                                luma_plane, filter_index,
                                (uint16_t)(luma_chroma_x - 1U), chroma_y,
                                full_resolution) :
                            mhccp_luma_sample_q3(
                                luma_plane, filter_index,
                                luma_chroma_x == 0U ? 0U :
                                    (uint16_t)(luma_chroma_x - 1U),
                                chroma_y, full_resolution)) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    for (sample = have_top ? 0U : 1U; sample < left_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(block_x - 1U);
        uint16_t chroma_y = (uint16_t)(block_y + sample);
        int32_t vector[3U];
        int first;
        int second;
        {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index, chroma_x, chroma_y,
                full_resolution) >> 3U);
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (sample == 0U && have_top ? top_left_center :
                     (int32_t)(mhccp_luma_sample_q3(
                         luma_plane, filter_index, chroma_x,
                         (uint16_t)(chroma_y - 1U), full_resolution) >> 3U)) :
                    (int32_t)(mhccp_luma_left_clamped_q3(
                        luma_plane, filter_index,
                        (uint16_t)(chroma_x - 1U), chroma_y,
                        full_resolution) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    count_ceiling = count - 1U;
    matrix_shift = 0;
    while (count_ceiling != 0U) {
        ++matrix_shift;
        count_ceiling >>= 1U;
    }
    matrix_shift = 6 - matrix_shift;
    for (row = 0U; row < 3U; ++row) {
        for (column = row; column < 3U; ++column) {
            matrix[row][column] = matrix_shift > 0 ?
                matrix[row][column] << matrix_shift :
                matrix[row][column] >> -matrix_shift;
        }
        target[row] = matrix_shift > 0 ?
            target[row] << matrix_shift :
            target[row] >> -matrix_shift;
    }
    mhccp_solve(matrix, target, parameters, 8U);
    for (row = 0U; row < prediction_height; ++row) {
        for (column = 0U; column < prediction_width; ++column) {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index,
                (uint16_t)(block_x + column),
                (uint16_t)(block_y + row), full_resolution) >> 3U);
            int32_t vector[3U];
            int32_t sum = 0;
            int parameter;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (row == 0U && !have_top ? center :
                     row == 0U && is_top_sb_boundary ?
                        (int32_t)(mhccp_luma_sb_top_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column), block_y,
                            full_resolution) >> 3U) :
                        (int32_t)(mhccp_luma_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column),
                            (uint16_t)(block_y + row - 1U),
                            full_resolution) >> 3U)) :
                    (int32_t)(mhccp_luma_sample_q3(
                        luma_plane, filter_index,
                        column == 0U && !have_left ? block_x :
                            (uint16_t)(block_x + column - 1U),
                        (uint16_t)(block_y + row), full_resolution) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip(sum);
        }
    }
    return AV2H_OK;
}

static uint16_t mhccp_luma_sb_top_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint16_t chroma_x, uint16_t block_y) {
    size_t luma_x = (size_t)chroma_x * 2U;
    size_t luma_y = (size_t)block_y * 2U - 1U;
    size_t right_x = luma_x + 1U < luma_plane->width ? luma_x + 1U : luma_x;
    return (uint16_t)(4U * (
        luma_plane->data[luma_y * luma_plane->stride_samples + luma_x] +
        luma_plane->data[luma_y * luma_plane->stride_samples + right_x]));
}

uint16_t av2h_chroma_luma_sample_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    return filter_index == 0U ?
        mhccp_luma_q3_u16(luma_plane, chroma_x, chroma_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_q3_u16(luma_plane, chroma_x, chroma_y) :
        mhccp_luma_filter2_q3_u16(luma_plane, chroma_x, chroma_y);
}

uint16_t av2h_chroma_luma_sb_top_sample_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    return filter_index == 0U ?
        mhccp_luma_sb_top_q3_u16(luma_plane, chroma_x, block_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_sb_top_q3_u16(
            luma_plane, chroma_x, block_y) :
        mhccp_luma_filter2_sb_top_q3_u16(
            luma_plane, chroma_x, block_y);
}

uint16_t av2h_chroma_luma_left_clamped_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    if (filter_index == 0U) {
        return mhccp_luma_q3_u16(luma_plane, chroma_x, chroma_y);
    }
    if (filter_index == 1U) {
        return (uint16_t)(
            3U * luma_plane->data[
                center_y * luma_plane->stride_samples + center_x] +
            luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
            3U * luma_plane->data[
                bottom_y * luma_plane->stride_samples + center_x] +
            luma_plane->data[bottom_y * luma_plane->stride_samples + right_x]);
    }
    return (uint16_t)(
        5U * luma_plane->data[
            center_y * luma_plane->stride_samples + center_x] +
        luma_plane->data[center_y * luma_plane->stride_samples + right_x] +
        luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
}

uint16_t av2h_chroma_luma_sb_top_left_clamped_q3_u16(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    uint16_t center = luma_plane->data[
        center_y * luma_plane->stride_samples + center_x];
    uint16_t right = luma_plane->data[
        center_y * luma_plane->stride_samples + right_x];
    if (filter_index == 0U) return (uint16_t)(4U * (center + right));
    if (filter_index == 1U) return (uint16_t)(2U * (3U * center + right));
    return (uint16_t)(7U * center + right);
}

static uint16_t mhccp_luma_sample_q3_u16_444(
    const Av2hReconstructionPlaneU16 *luma_plane,
    uint16_t x,
    uint16_t y) {
    return (uint16_t)(luma_plane->data[
        (size_t)y * luma_plane->stride_samples + x] << 3U);
}

static Av2hStatus
mhccp_predict_u16_444(const Av2hChromaLeafMode *mode, const Av2hReconstructionPlaneU16 *luma_plane,
                      const Av2hReconstructionPlaneU16 *chroma_plane, uint16_t block_x,
                      uint16_t block_y, uint16_t width, uint16_t height, uint16_t *predictor,
                      size_t predictor_stride_samples, const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int64_t matrix[3U][3U] = {{0}};
    int64_t target[3U] = {0};
    int32_t parameters[3U];
    uint16_t prediction_width;
    uint16_t prediction_height;
    uint16_t top_count;
    uint16_t left_count;
    uint16_t top_start_x;
    uint16_t sample;
    uint16_t count = 0U;
    uint16_t row;
    uint16_t column;
    uint8_t have_top;
    uint8_t have_left;
    uint8_t is_top_sb_boundary;
    int32_t top_left_center = 0;
    if (mode == NULL || luma_plane == NULL || chroma_plane == NULL ||
        predictor == NULL || luma_plane->data == NULL ||
        chroma_plane->data == NULL || luma_plane->reconstructed == NULL ||
        mode->cfl_kind != AV2H_CFL_MULTI_PARAM ||
        mode->mhccp_direction > 2U || mode->chroma_format_idc != 2U ||
        !av2h_chroma_geometry_supported(width, height) ||
        predictor_stride_samples < width ||
        block_x >= chroma_plane->width || block_y >= chroma_plane->height ||
        luma_plane->stride_samples < luma_plane->width ||
        chroma_plane->stride_samples < chroma_plane->width) {
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if (!av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, block_x, block_y,
            prediction_width, prediction_height)) {
        return AV2H_UNSUPPORTED;
    }
    have_top = (uint8_t)(block_y != 0U);
    have_left = (uint8_t)(block_x != 0U);
    is_top_sb_boundary = (uint8_t)(mode->superblock_size != 0U &&
        block_y % mode->superblock_size == 0U);
    if (!have_top && !have_left) {
        for (row = 0U; row < prediction_height; ++row) {
            for (column = 0U; column < prediction_width; ++column) {
                predictor[(size_t)row * predictor_stride_samples + column] = domain->midpoint;
            }
        }
        return AV2H_OK;
    }
    top_count = have_top ? (uint16_t)(width * 2U) : 0U;
    if (!have_left && top_count >= 2U) top_count = (uint16_t)(top_count - 2U);
    if (top_count > 126U) top_count = 126U;
    left_count = have_left ? (uint16_t)(height * 2U - 1U) : 0U;
    if (have_top && left_count > 63U) left_count = 63U;
    if (left_count >= chroma_plane->height - block_y) {
        left_count = (uint16_t)(chroma_plane->height - block_y - 1U);
    }
    if (have_left) {
        uint16_t available_left = 0U;
        while (available_left < left_count &&
               av2h_chroma_plane_sample_reconstructed_u16(
                   chroma_plane, block_x - 1U,
                   (size_t)block_y + available_left)) {
            ++available_left;
        }
        if (available_left > height - 1U) {
            available_left = (uint16_t)(height - 1U +
                ((available_left - (height - 1U)) & ~1U));
        }
        left_count = available_left;
        have_left = (uint8_t)(left_count != 0U);
    }
    top_start_x = (uint16_t)(have_left ? block_x - 1U : block_x + 1U);
    if (have_top) {
        uint16_t available_top = 0U;
        uint16_t effective_chroma_width = (uint16_t)(
            (chroma_plane->reference_width + 7U) & ~(size_t)7U);
        uint16_t effective_luma_width = (uint16_t)(
            (luma_plane->reference_width + 7U) & ~(size_t)7U);
        uint16_t limit;
        if (effective_chroma_width > chroma_plane->width) {
            effective_chroma_width = chroma_plane->width;
        }
        if (effective_luma_width > luma_plane->width) {
            effective_luma_width = luma_plane->width;
        }
        limit = effective_chroma_width > top_start_x + 1U ?
            (uint16_t)(effective_chroma_width - top_start_x - 1U) : 0U;
        if (effective_luma_width <= top_start_x) {
            limit = 0U;
        } else if (limit > effective_luma_width - top_start_x) {
            limit = (uint16_t)(effective_luma_width - top_start_x);
        }
        if (top_count > limit) top_count = limit;
        while (available_top < top_count &&
               av2h_chroma_plane_sample_reconstructed_u16(
                   chroma_plane, (size_t)top_start_x + available_top,
                   block_y - 1U)) {
            ++available_top;
        }
        if (!have_left && available_top != 0U && available_top <= width) {
            --available_top;
        }
        if (available_top > width) {
            available_top = (uint16_t)(width +
                ((available_top - width) & ~1U));
        }
        top_count = available_top;
        have_top = (uint8_t)(top_count != 0U);
    }
    if (have_top && (!av2h_chroma_plane_region_reconstructed_u16(
            chroma_plane, top_start_x, (uint16_t)(block_y - 1U),
            top_count, 1U) ||
        !av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, top_start_x, (uint16_t)(block_y - 1U),
            top_count, 1U))) {
        return AV2H_UNSUPPORTED;
    }
    if (have_left && (!av2h_chroma_plane_region_reconstructed_u16(
            chroma_plane, (uint16_t)(block_x - 1U), block_y,
            1U, left_count) ||
        !av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, (uint16_t)(block_x - 1U), block_y,
            1U, left_count))) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mhccp_direction == 1U && have_top &&
        !is_top_sb_boundary && block_y < 2U) {
        return AV2H_UNSUPPORTED;
    }
    for (sample = 0U; sample < top_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(top_start_x + sample);
        uint16_t chroma_y = (uint16_t)(block_y - 1U);
        int32_t center = (int32_t)(mhccp_luma_sample_q3_u16_444(
            luma_plane, chroma_x, chroma_y) >> 3U);
        int32_t vector[3U];
        int first;
        int second;
        if (sample == 0U) top_left_center = center;
        vector[0] = mode->mhccp_direction == 0U ? center :
            mode->mhccp_direction == 1U ?
                (is_top_sb_boundary ? center :
                 (int32_t)(mhccp_luma_sample_q3_u16_444(
                     luma_plane, chroma_x,
                     (uint16_t)(chroma_y - 1U)) >> 3U)) :
                (int32_t)(mhccp_luma_sample_q3_u16_444(
                    luma_plane,
                    sample == 0U && have_left ?
                        (uint16_t)(chroma_x - 1U) :
                        (chroma_x == 0U ? 0U :
                            (uint16_t)(chroma_x - 1U)),
                    chroma_y) >> 3U);
        vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
        vector[2] = domain->midpoint;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    for (sample = have_top ? 0U : 1U; sample < left_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(block_x - 1U);
        uint16_t chroma_y = (uint16_t)(block_y + sample);
        int32_t center = (int32_t)(mhccp_luma_sample_q3_u16_444(
            luma_plane, chroma_x, chroma_y) >> 3U);
        int32_t vector[3U];
        int first;
        int second;
        vector[0] = mode->mhccp_direction == 0U ? center :
            mode->mhccp_direction == 1U ?
                (sample == 0U && have_top ? top_left_center :
                 (int32_t)(mhccp_luma_sample_q3_u16_444(
                     luma_plane, chroma_x,
                     (uint16_t)(chroma_y - 1U)) >> 3U)) :
                (int32_t)(mhccp_luma_sample_q3_u16_444(
                    luma_plane,
                    chroma_x == 0U ? 0U : (uint16_t)(chroma_x - 1U),
                    chroma_y) >> 3U);
        vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
        vector[2] = domain->midpoint;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride_samples + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    if (count == 0U) return AV2H_UNSUPPORTED;
    mhccp_solve_samples(matrix, target, count, domain, parameters);
    for (row = 0U; row < prediction_height; ++row) {
        for (column = 0U; column < prediction_width; ++column) {
            uint16_t x = (uint16_t)(block_x + column);
            uint16_t y = (uint16_t)(block_y + row);
            int32_t center = (int32_t)(mhccp_luma_sample_q3_u16_444(
                luma_plane, x, y) >> 3U);
            int32_t vector[3U];
            int32_t sum = 0;
            int parameter;
            vector[0] = mode->mhccp_direction == 0U ? center :
                mode->mhccp_direction == 1U ?
                    (row == 0U && !have_top ? center :
                     row == 0U && is_top_sb_boundary ?
                        (int32_t)(mhccp_luma_sample_q3_u16_444(
                            luma_plane, x,
                            (uint16_t)(block_y - 1U)) >> 3U) :
                        (int32_t)(mhccp_luma_sample_q3_u16_444(
                            luma_plane, x, (uint16_t)(y - 1U)) >> 3U)) :
                    (int32_t)(mhccp_luma_sample_q3_u16_444(
                        luma_plane,
                        column == 0U && !have_left ? block_x :
                            (uint16_t)(x - 1U), y) >> 3U);
            vector[1] = (center * center + domain->midpoint) >> domain->bit_depth;
            vector[2] = domain->midpoint;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride_samples + column] =
                av2h_sample_clip_to_domain(sum, domain);
        }
    }
    return AV2H_OK;
}

#include "internal/sample_kernel_u8.inc"
#include "internal/chroma_prediction_mhccp_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/chroma_prediction_mhccp_kernel.inc"
#include "internal/sample_kernel_end.inc"

#include "internal/chroma_prediction.h"

#include "internal/sample_math.h"
#include "internal/sample_ops.h"

#include <stddef.h>

/*
 * CfL scaling and downsampling for derived and explicit alpha prediction.
 * The fixed-point derivation and all three 4:2:0 downsampling filters match
 * the pinned AVM reference (av2/common/cfl.c, warped_motion.h divisor LUT).
 */
#define AV2H_CFL_ADD_BITS_ALPHA 5
#define AV2H_CFL_NUM_REF_SAM 8

typedef struct {
    int above;
    int left;
} Av2hCflReferenceCounts;

typedef struct {
    int32_t sum_x;
    int32_t sum_y;
    int32_t sum_xx;
    int32_t sum_xy;
    int count;
} Av2hCflRegression;

static void cfl_predict_samples(const Av2hReconstructionPlane *luma_plane, uint16_t luma_x,
                                uint16_t luma_y, uint16_t width, uint16_t height,
                                uint16_t visible_width, uint16_t visible_height,
                                uint8_t full_resolution, uint8_t filter_index, int32_t alpha,
                                uint8_t alpha_extra_bits, int32_t average_q3, uint8_t *predictor,
                                size_t predictor_stride_samples, const Av2hSampleDomain *domain);

static void cfl_predict_samples_u16(const Av2hReconstructionPlaneU16 *luma_plane, uint16_t luma_x,
                                    uint16_t luma_y, uint16_t width, uint16_t height,
                                    uint16_t visible_width, uint16_t visible_height,
                                    uint8_t full_resolution, uint8_t filter_index, int32_t alpha,
                                    uint8_t alpha_extra_bits, int32_t average_q3,
                                    uint16_t *predictor, size_t predictor_stride_samples,
                                    const Av2hSampleDomain *domain);

static Av2hCflReferenceCounts cfl_reference_counts(uint16_t width, uint16_t height,
                                                   uint8_t have_top, uint8_t have_left) {
    Av2hCflReferenceCounts counts = {0, 0};
    if (have_top && have_left) {
        if (width > (uint16_t)(height * 2U)) {
            counts.above = AV2H_CFL_NUM_REF_SAM;
        } else if (height > (uint16_t)(width * 2U)) {
            counts.left = AV2H_CFL_NUM_REF_SAM;
        } else {
            counts.above = AV2H_CFL_NUM_REF_SAM >> 1;
            counts.left = AV2H_CFL_NUM_REF_SAM >> 1;
        }
    } else {
        counts.above = have_top ? AV2H_CFL_NUM_REF_SAM : 0;
        counts.left = have_left ? AV2H_CFL_NUM_REF_SAM : 0;
    }
    if (counts.above > width)
        counts.above = width;
    if (counts.left > height)
        counts.left = height;
    return counts;
}

static void cfl_regression_add(Av2hCflRegression *regression, int32_t luma, int32_t chroma) {
    regression->sum_x += luma;
    regression->sum_y += chroma;
    regression->sum_xx += luma * luma;
    regression->sum_xy += luma * chroma;
    ++regression->count;
}

/*
 * Derive the CFL_DERIVED_ALPHA predictor for one chroma transform block.
 * `predictor` enters holding the DC base prediction (built from the chroma
 * neighbours) and leaves holding predictor + scaled luma-AC.  Fails closed
 * (leaving the DC base untouched) whenever the required reconstructed luma is
 * unavailable or the geometry is unsupported.
 */

static Av2hStatus cfl_predict_u16_scaled(const Av2hReconstructionPlaneU16 *luma_plane,
                                         uint8_t filter_index, int32_t alpha,
                                         uint8_t alpha_extra_bits, uint16_t superblock_size,
                                         uint16_t block_x, uint16_t block_y, uint16_t width,
                                         uint16_t height, uint8_t have_top, uint8_t have_left,
                                         uint16_t *predictor, size_t predictor_stride_samples,
                                         const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    uint16_t luma_x = (uint16_t)((uint32_t)block_x * 2U);
    uint16_t luma_y = (uint16_t)((uint32_t)block_y * 2U);
    uint16_t luma_width = (uint16_t)((uint32_t)width * 2U);
    uint16_t luma_height = (uint16_t)((uint32_t)height * 2U);
    uint16_t effective_luma_width;
    uint16_t effective_luma_height;
    uint16_t visible_luma_width;
    uint16_t visible_luma_height;
    int32_t luma_left_q3[64U];
    int32_t luma_above_q3[64U];
    int32_t avg_l;
    uint16_t row;
    uint16_t column;
    uint8_t is_top_sb_boundary;
    int top_offset;
    int bottom_offset;
    if (luma_plane == NULL || luma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || predictor == NULL ||
        filter_index > 2U || width == 0U || height == 0U ||
        width > 64U || height > 64U ||
        predictor_stride_samples < width ||
        luma_x >= luma_plane->width || luma_y >= luma_plane->height ||
        luma_plane->stride_samples < luma_plane->width) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)(
        (luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)(
        (luma_plane->reference_height + 7U) & ~(size_t)7U);
    if (effective_luma_width > luma_plane->width) {
        effective_luma_width = luma_plane->width;
    }
    if (effective_luma_height > luma_plane->height) {
        effective_luma_height = luma_plane->height;
    }
    visible_luma_width = luma_width < effective_luma_width - luma_x ?
        luma_width : (uint16_t)(effective_luma_width - luma_x);
    visible_luma_height = luma_height < effective_luma_height - luma_y ?
        luma_height : (uint16_t)(effective_luma_height - luma_y);
    if (!av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, luma_x, luma_y,
            visible_luma_width, visible_luma_height)) {
        return AV2H_UNSUPPORTED;
    }
    if (have_left != 0U) {
        uint16_t reference_width = filter_index == 0U ? 2U : 3U;
        uint16_t reference_x = filter_index == 0U ?
            (uint16_t)(luma_x - 2U) : (uint16_t)(luma_x - 3U);
        if (luma_x < reference_width ||
            !av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, reference_x, luma_y,
                reference_width, visible_luma_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            size_t center_x = (size_t)luma_x - 2U;
            size_t center_y = (size_t)luma_y + (size_t)row * 2U;
            if (filter_index == 1U) {
                luma_left_q3[row] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x - 1U, center_y) +
                    2U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x - 1U, center_y + 1U) +
                    2U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y + 1U) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y + 1U));
            } else if (filter_index == 2U) {
                size_t top_y = row == 0U ? center_y : center_y - 1U;
                luma_left_q3[row] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x - 1U, center_y) +
                    4U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, top_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y + 1U));
            } else {
                luma_left_q3[row] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y + 1U) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y + 1U)) << 1U;
            }
            if (row >= visible_luma_height / 2U && row != 0U) {
                luma_left_q3[row] = luma_left_q3[row - 1U];
            }
        }
    }
    if (have_top != 0U) {
        is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
            luma_y % superblock_size == 0U);
        top_offset = 2 - is_top_sb_boundary;
        bottom_offset = 1 - is_top_sb_boundary;
        if (luma_y < (uint16_t)top_offset ||
            !av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, luma_x, (uint16_t)(luma_y - top_offset),
                luma_width < luma_plane->width - luma_x ?
                    luma_width :
                    (uint16_t)(luma_plane->width - luma_x),
                4U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            size_t center_x = (size_t)luma_x + (size_t)column * 2U;
            size_t center_y = (size_t)luma_y - (size_t)top_offset;
            size_t left_x = column == 0U ? center_x : center_x - 1U;
            size_t bottom_y = center_y + (size_t)bottom_offset;
            if (filter_index == 1U) {
                luma_above_q3[column] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, left_x, center_y) +
                    2U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, left_x, bottom_y) +
                    2U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, bottom_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, bottom_y));
            } else if (filter_index == 2U) {
                size_t top_y = is_top_sb_boundary ?
                    center_y : center_y - 1U;
                luma_above_q3[column] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, left_x, center_y) +
                    4U * av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, top_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, bottom_y));
            } else {
                luma_above_q3[column] = (int32_t)(
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, center_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x, bottom_y) +
                    av2h_chroma_plane_coded_sample_u16(
                        luma_plane, center_x + 1U, bottom_y)) << 1U;
            }
            if (column >= visible_luma_width / 2U && column != 0U) {
                luma_above_q3[column] = luma_above_q3[column - 1U];
            }
        }
    }
    {
        int32_t sum = 0;
        int count = 0;
        int horizontal_step = width > 32U ? 2 : 1;
        int vertical_step = height > 32U ? 2 : 1;
        if (have_top != 0U) {
            for (column = 0U; column < width;
                 column += (uint16_t)horizontal_step) {
                sum += luma_above_q3[column];
                ++count;
            }
        }
        if (have_left != 0U) {
            for (row = 0U; row < height;
                 row += (uint16_t)vertical_step) {
                sum += luma_left_q3[row];
                ++count;
            }
        }
        if (count != 0) {
            int divisor_shift = 0;
            uint16_t divisor = av2h_sample_resolve_divisor_32(
                (uint32_t)count, &divisor_shift);
            int32_t rounding = (1 << divisor_shift) >> 1;
            int32_t value = (int32_t)(
                ((int64_t)sum * divisor + rounding) >> divisor_shift);
            int32_t maximum = av2h_sample_q3_maximum(domain);
            avg_l = value > maximum ? maximum : value;
        } else {
            avg_l = (int32_t)domain->midpoint << 3U;
        }
    }
    cfl_predict_samples_u16(luma_plane, luma_x, luma_y, width, height, width, height, 0U,
                            filter_index, alpha, alpha_extra_bits, avg_l, predictor,
                            predictor_stride_samples, domain);
    return AV2H_OK;
}

Av2hStatus av2h_cfl_explicit_predict_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                         uint8_t filter_index, int32_t alpha_q3,
                                         uint16_t superblock_size, uint16_t block_x,
                                         uint16_t block_y, uint16_t width, uint16_t height,
                                         uint8_t have_top, uint8_t have_left, uint16_t *predictor,
                                         size_t predictor_stride_samples,
                                         const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    return cfl_predict_u16_scaled(luma_plane, filter_index, alpha_q3, 0U, superblock_size, block_x,
                                  block_y, width, height, have_top, have_left, predictor,
                                  predictor_stride_samples, domain);
}

Av2hStatus av2h_cfl_explicit_predict_u16_444(const Av2hReconstructionPlaneU16 *luma_plane,
                                             int32_t alpha_q3, uint16_t block_x, uint16_t block_y,
                                             uint16_t width, uint16_t height, uint8_t have_top,
                                             uint8_t have_left, uint16_t *predictor,
                                             size_t predictor_stride_samples,
                                             const Av2hSampleDomain *domain) {
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int32_t luma_left_q3[64U];
    int32_t luma_above_q3[64U];
    uint16_t effective_width;
    uint16_t effective_height;
    uint16_t visible_width;
    uint16_t visible_height;
    int32_t avg_l;
    uint16_t row;
    uint16_t column;
    if (luma_plane == NULL || luma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || predictor == NULL ||
        width == 0U || height == 0U || width > 64U || height > 64U ||
        predictor_stride_samples < width ||
        block_x >= luma_plane->width || block_y >= luma_plane->height ||
        luma_plane->stride_samples < luma_plane->width) {
        return AV2H_UNSUPPORTED;
    }
    effective_width = (uint16_t)(
        (luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_height = (uint16_t)(
        (luma_plane->reference_height + 7U) & ~(size_t)7U);
    if (effective_width > luma_plane->width) {
        effective_width = luma_plane->width;
    }
    if (effective_height > luma_plane->height) {
        effective_height = luma_plane->height;
    }
    visible_width = width < effective_width - block_x ?
        width : (uint16_t)(effective_width - block_x);
    visible_height = height < effective_height - block_y ?
        height : (uint16_t)(effective_height - block_y);
    if (!av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, block_x, block_y, visible_width, visible_height)) {
        return AV2H_UNSUPPORTED;
    }
    if (have_left != 0U) {
        if (block_x == 0U || !av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, (uint16_t)(block_x - 1U), block_y,
                1U, visible_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t sample_y = row < visible_height ?
                (uint16_t)(block_y + row) :
                (uint16_t)(block_y + visible_height - 1U);
            luma_left_q3[row] = (int32_t)av2h_chroma_plane_coded_sample_u16(
                luma_plane, block_x - 1U, sample_y) << 3U;
        }
    }
    if (have_top != 0U) {
        if (block_y == 0U || !av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, block_x, (uint16_t)(block_y - 1U),
                visible_width, 1U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            uint16_t sample_x = column < visible_width ?
                (uint16_t)(block_x + column) :
                (uint16_t)(block_x + visible_width - 1U);
            luma_above_q3[column] = (int32_t)av2h_chroma_plane_coded_sample_u16(
                luma_plane, sample_x, block_y - 1U) << 3U;
        }
    }
    {
        int32_t sum = 0;
        int count = 0;
        uint16_t horizontal_step = width > 32U ? 2U : 1U;
        uint16_t vertical_step = height > 32U ? 2U : 1U;
        if (have_top != 0U) {
            for (column = 0U; column < width; column += horizontal_step) {
                sum += luma_above_q3[column];
                ++count;
            }
        }
        if (have_left != 0U) {
            for (row = 0U; row < height; row += vertical_step) {
                sum += luma_left_q3[row];
                ++count;
            }
        }
        if (count != 0) {
            int divisor_shift = 0;
            uint16_t divisor = av2h_sample_resolve_divisor_32(
                (uint32_t)count, &divisor_shift);
            int32_t rounding = (1 << divisor_shift) >> 1;
            int32_t value = (int32_t)(
                ((int64_t)sum * divisor + rounding) >> divisor_shift);
            int32_t maximum = av2h_sample_q3_maximum(domain);
            avg_l = value > maximum ? maximum : value;
        } else {
            avg_l = (int32_t)domain->midpoint << 3U;
        }
    }
    cfl_predict_samples_u16(luma_plane, block_x, block_y, width, height, visible_width,
                            visible_height, 1U, 0U, alpha_q3, 0U, avg_l, predictor,
                            predictor_stride_samples, domain);
    return AV2H_OK;
}

Av2hStatus av2h_cfl_derived_predict_u16_444(const Av2hReconstructionPlaneU16 *luma_plane,
                                            const Av2hReconstructionPlaneU16 *chroma_plane,
                                            uint16_t block_x, uint16_t block_y, uint16_t width,
                                            uint16_t height, uint8_t have_top, uint8_t have_left,
                                            uint16_t *predictor, size_t predictor_stride_samples,
                                            const Av2hSampleDomain *domain) {
    Av2hCflRegression regression = {0};
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int32_t luma_left_q3[64U];
    int32_t luma_above_q3[64U];
    int32_t chroma_left[64U];
    int32_t chroma_above[64U];
    uint16_t effective_luma_width;
    uint16_t effective_luma_height;
    uint16_t effective_chroma_width;
    uint16_t effective_chroma_height;
    uint16_t row;
    uint16_t column;
    int32_t sum = 0;
    int dc_count = 0;
    int32_t average_luma_q3;
    int numb_up = 0;
    int numb_left = 0;
    int32_t alpha;
    uint16_t visible_width;
    uint16_t visible_height;
    if (luma_plane == NULL || chroma_plane == NULL || predictor == NULL ||
        luma_plane->data == NULL || chroma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || width == 0U || height == 0U ||
        width > 64U || height > 64U ||
        predictor_stride_samples < width ||
        (have_top != 0U && block_y == 0U) ||
        (have_left != 0U && block_x == 0U) ||
        block_x >= luma_plane->width || block_y >= luma_plane->height ||
        block_x >= chroma_plane->width || block_y >= chroma_plane->height ||
        height > luma_plane->height - block_y ||
        height > chroma_plane->height - block_y) {
        return AV2H_UNSUPPORTED;
    }
    visible_width = width < luma_plane->width - block_x ?
        width : (uint16_t)(luma_plane->width - block_x);
    visible_height = height < luma_plane->height - block_y ?
        height : (uint16_t)(luma_plane->height - block_y);
    if (!av2h_chroma_plane_region_reconstructed_u16(
            luma_plane, block_x, block_y,
            visible_width, visible_height)) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)(
        (luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)(
        (luma_plane->reference_height + 7U) & ~(size_t)7U);
    effective_chroma_width = (uint16_t)(
        (chroma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_chroma_height = (uint16_t)(
        (chroma_plane->reference_height + 7U) & ~(size_t)7U);
    if (effective_luma_width > luma_plane->width) {
        effective_luma_width = luma_plane->width;
    }
    if (effective_luma_height > luma_plane->height) {
        effective_luma_height = luma_plane->height;
    }
    if (effective_chroma_width > chroma_plane->width) {
        effective_chroma_width = chroma_plane->width;
    }
    if (effective_chroma_height > chroma_plane->height) {
        effective_chroma_height = chroma_plane->height;
    }
    if (have_left != 0U) {
        if (!av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, (uint16_t)(block_x - 1U), block_y,
                1U, height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            size_t y = (size_t)block_y + row;
            luma_left_q3[row] = (int32_t)av2h_chroma_plane_coded_sample_u16(
                luma_plane, block_x - 1U, y) << 3U;
            chroma_left[row] = av2h_chroma_plane_coded_sample_u16(
                chroma_plane, block_x - 1U, y);
            if (block_y >= effective_luma_height) {
                luma_left_q3[row] = domain->midpoint << 3U;
            } else if (y >= effective_luma_height && row != 0U) {
                luma_left_q3[row] = luma_left_q3[row - 1U];
            }
            if (block_y >= effective_chroma_height) {
                chroma_left[row] = domain->midpoint;
            } else if (y >= effective_chroma_height && row != 0U) {
                chroma_left[row] = chroma_left[row - 1U];
            }
        }
    }
    if (have_top != 0U) {
        if (!av2h_chroma_plane_region_reconstructed_u16(
                luma_plane, block_x, (uint16_t)(block_y - 1U),
                visible_width, 1U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            size_t x = (size_t)block_x + column;
            luma_above_q3[column] = (int32_t)av2h_chroma_plane_coded_sample_u16(
                luma_plane, x, block_y - 1U) << 3U;
            chroma_above[column] = av2h_chroma_plane_coded_sample_u16(
                chroma_plane, x, block_y - 1U);
            if (block_x >= effective_luma_width) {
                luma_above_q3[column] = domain->midpoint << 3U;
            } else if (x >= effective_luma_width && column != 0U) {
                luma_above_q3[column] = luma_above_q3[column - 1U];
            }
            if (block_x >= effective_chroma_width) {
                chroma_above[column] = domain->midpoint;
            } else if (x >= effective_chroma_width && column != 0U) {
                chroma_above[column] = chroma_above[column - 1U];
            }
        }
    }
    if (have_top != 0U) {
        uint16_t step = width > 32U ? 2U : 1U;
        for (column = 0U; column < width; column += step) {
            sum += luma_above_q3[column];
            ++dc_count;
        }
    }
    if (have_left != 0U) {
        uint16_t step = height > 32U ? 2U : 1U;
        for (row = 0U; row < height; row += step) {
            sum += luma_left_q3[row];
            ++dc_count;
        }
    }
    if (dc_count != 0) {
        int divisor_shift = 0;
        uint16_t divisor = av2h_sample_resolve_divisor_32(
            (uint32_t)dc_count, &divisor_shift);
        int32_t value = (int32_t)(((int64_t)sum * divisor +
            ((int32_t)1 << (divisor_shift - 1))) >> divisor_shift);
        int32_t maximum_q3 = av2h_sample_q3_maximum(domain);
        average_luma_q3 = value > maximum_q3 ? maximum_q3 : value;
    } else {
        average_luma_q3 = domain->midpoint << 3U;
    }
    {
        Av2hCflReferenceCounts counts = cfl_reference_counts(width, height, have_top, have_left);
        numb_up = counts.above;
        numb_left = counts.left;
    }
    if (numb_up != 0) {
        int step = (int)width / numb_up;
        int sample;
        if (step < 1) step = 1;
        sample = step == 1 ? 0 : step >> 1;
        for (; sample < (int)width; sample += step) {
            int32_t x = luma_above_q3[sample] >> 3U;
            int32_t y = chroma_above[sample];
            cfl_regression_add(&regression, x, y);
        }
    }
    if (numb_left != 0) {
        int step = (int)height / numb_left;
        int sample;
        if (step < 1) step = 1;
        sample = step == 1 ? 0 : step >> 1;
        for (; sample < (int)height; sample += step) {
            int32_t x = luma_left_q3[sample] >> 3U;
            int32_t y = chroma_left[sample];
            cfl_regression_add(&regression, x, y);
        }
    }
    alpha = av2h_sample_regression_slope(regression.sum_x, regression.sum_y, regression.sum_xx,
                                         regression.sum_xy, regression.count,
                                         3 + AV2H_CFL_ADD_BITS_ALPHA);
    cfl_predict_samples_u16(luma_plane, block_x, block_y, width, height, width, height, 1U, 0U,
                            alpha, AV2H_CFL_ADD_BITS_ALPHA, average_luma_q3, predictor,
                            predictor_stride_samples, domain);
    return AV2H_OK;
}

Av2hStatus av2h_cfl_derived_predict(
    const Av2hReconstructionPlane *luma_plane, const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index, uint8_t derive_alpha, int32_t supplied_alpha_q3, uint16_t superblock_size,
    uint8_t chroma_format_idc, uint16_t block_x, uint16_t block_y, uint16_t width, uint16_t height,
    uint8_t have_top, uint8_t have_left, uint8_t *predictor, size_t predictor_stride_samples) {
    Av2hCflRegression regression = {0};
    const Av2hSampleDomain *domain = &av2h_sample_domain_8bit;
    const int bit_depth = av2h_sample_domain_8bit.bit_depth;
    uint8_t full_resolution = (uint8_t)(chroma_format_idc == 2U);
    unsigned int scale = full_resolution ? 1U : 2U;
    uint16_t luma_x = (uint16_t)((uint32_t)block_x * scale);
    uint16_t luma_y = (uint16_t)((uint32_t)block_y * scale);
    uint16_t luma_width = (uint16_t)((uint32_t)width * scale);
    uint16_t luma_height = (uint16_t)((uint32_t)height * scale);
    int32_t luma_left_ds[64];
    int32_t luma_above_ds[64];
    int32_t chroma_left[64];
    int32_t chroma_above[64];
    uint8_t is_top_sb_boundary;
    int top_offset;
    int bottom_offset;
    uint16_t row;
    uint16_t column;
    int32_t avg_l;
    int32_t alpha;
    int numb_up;
    int numb_left;
    int shift;
    uint16_t visible_luma_width;
    uint16_t visible_luma_height;
    uint16_t effective_luma_width;
    uint16_t effective_luma_height;
    uint16_t effective_chroma_width;
    uint16_t effective_chroma_height;
    if (luma_plane == NULL || luma_plane->data == NULL ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) || filter_index > 2U || width > 64U ||
        height > 64U || width == 0U || height == 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (luma_x >= luma_plane->width || luma_y >= luma_plane->height ||
        luma_plane->stride_samples < luma_plane->width || luma_plane->reconstructed == NULL) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)((luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)((luma_plane->reference_height + 7U) & ~(size_t)7U);
    effective_chroma_width =
        (uint16_t)((chroma_plane->reference_width + (full_resolution ? 7U : 3U)) &
                   ~(size_t)(full_resolution ? 7U : 3U));
    effective_chroma_height =
        (uint16_t)((chroma_plane->reference_height + (full_resolution ? 7U : 3U)) &
                   ~(size_t)(full_resolution ? 7U : 3U));
    if (effective_luma_width > luma_plane->width) {
        effective_luma_width = luma_plane->width;
    }
    if (effective_luma_height > luma_plane->height) {
        effective_luma_height = luma_plane->height;
    }
    if (effective_chroma_width > chroma_plane->width) {
        effective_chroma_width = chroma_plane->width;
    }
    if (effective_chroma_height > chroma_plane->height) {
        effective_chroma_height = chroma_plane->height;
    }
    visible_luma_width = luma_x >= effective_luma_width ? 0U
                         : luma_width < effective_luma_width - luma_x
                             ? luma_width
                             : (uint16_t)(effective_luma_width - luma_x);
    visible_luma_height = luma_y >= effective_luma_height ? 0U
                          : luma_height < effective_luma_height - luma_y
                              ? luma_height
                              : (uint16_t)(effective_luma_height - luma_y);
    if (!av2h_chroma_plane_region_reconstructed(luma_plane, luma_x, luma_y, visible_luma_width,
                                                visible_luma_height)) {
        return AV2H_UNSUPPORTED;
    }
    if (have_left) {
        uint16_t reference_x = full_resolution      ? (uint16_t)(luma_x - 1U)
                               : filter_index == 0U ? (uint16_t)(luma_x - 2U)
                                                    : (uint16_t)(luma_x - 3U);
        uint16_t reference_width = full_resolution ? 1U : (filter_index == 0U ? 2U : 3U);
        if (luma_x < reference_width ||
            !av2h_chroma_plane_region_reconstructed(luma_plane, reference_x, luma_y,
                                                    reference_width, visible_luma_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            if (full_resolution) {
                luma_left_ds[row] = (int32_t)av2h_chroma_plane_coded_sample(luma_plane, luma_x - 1U,
                                                                            (size_t)luma_y + row)
                                    << 3U;
            } else if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                luma_left_ds[row] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U,
                                                             center_y + 1U) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x,
                                                                  center_y + 1U) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U,
                                                             center_y + 1U));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                size_t top_y = row == 0U ? center_y : center_y - 1U;
                luma_left_ds[row] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                              4U * av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x, top_y) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x, center_y + 1U));
            } else {
                size_t r0 = (size_t)luma_y + (size_t)row * 2U;
                const uint8_t *base =
                    &luma_plane->data[r0 * luma_plane->stride_samples + luma_x - 2U];
                luma_left_ds[row] = (int32_t)(base[0] + base[1] + base[luma_plane->stride_samples] +
                                              base[luma_plane->stride_samples + 1U])
                                    << 1;
            }
            chroma_left[row] =
                av2h_chroma_plane_coded_sample(chroma_plane, block_x - 1U, (size_t)block_y + row);
            if (luma_y >= effective_luma_height) {
                luma_left_ds[row] = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
            } else if (row >= visible_luma_height / scale && row != 0U) {
                luma_left_ds[row] = luma_left_ds[row - 1U];
            }
            if (block_y >= effective_chroma_height) {
                chroma_left[row] = domain->midpoint;
            } else if (row >= effective_chroma_height - block_y && row != 0U) {
                chroma_left[row] = chroma_left[row - 1U];
            }
        }
    }
    if (have_top) {
        is_top_sb_boundary = (uint8_t)(superblock_size != 0U && (luma_y % superblock_size) == 0U);
        top_offset = full_resolution ? 1 : 2 - is_top_sb_boundary;
        bottom_offset = full_resolution ? 0 : 1 - is_top_sb_boundary;
        if (luma_y < (uint16_t)top_offset ||
            !av2h_chroma_plane_region_reconstructed(
                luma_plane, luma_x, (uint16_t)(luma_y - top_offset),
                luma_width < luma_plane->width - luma_x ? luma_width
                                                        : (uint16_t)(luma_plane->width - luma_x),
                full_resolution ? 1U : 4U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            if (full_resolution) {
                luma_above_ds[column] = (int32_t)av2h_chroma_plane_coded_sample(
                                            luma_plane, (size_t)luma_x + column, luma_y - 1U)
                                        << 3U;
            } else if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, left_x, top_row) +
                              2U * av2h_chroma_plane_coded_sample(luma_plane, center_x, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, left_x, bottom_row) +
                              2U *
                                  av2h_chroma_plane_coded_sample(luma_plane, center_x, bottom_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, center_x + 1U,
                                                             bottom_row));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t center_y = (size_t)luma_y - (size_t)top_offset;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_y = is_top_sb_boundary ? center_y : center_y - 1U;
                size_t bottom_y = center_y + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(luma_plane->data[center_y * luma_plane->stride_samples + left_x] +
                              4U * luma_plane
                                       ->data[center_y * luma_plane->stride_samples + center_x] +
                              luma_plane
                                  ->data[center_y * luma_plane->stride_samples + center_x + 1U] +
                              luma_plane->data[top_y * luma_plane->stride_samples + center_x] +
                              luma_plane->data[bottom_y * luma_plane->stride_samples + center_x]);
            } else {
                size_t c0 = (size_t)luma_x + (size_t)column * 2U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] =
                    (int32_t)(av2h_chroma_plane_coded_sample(luma_plane, c0, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0 + 1U, top_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0, bottom_row) +
                              av2h_chroma_plane_coded_sample(luma_plane, c0 + 1U, bottom_row))
                    << 1;
            }
            chroma_above[column] = av2h_chroma_plane_coded_sample(
                chroma_plane, (size_t)block_x + column, (size_t)block_y - 1U);
            if (luma_x >= effective_luma_width) {
                luma_above_ds[column] = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
            } else if (column >= visible_luma_width / scale && column != 0U) {
                luma_above_ds[column] = luma_above_ds[column - 1U];
            }
            if (block_x >= effective_chroma_width) {
                chroma_above[column] = domain->midpoint;
            } else if (column >= effective_chroma_width - block_x && column != 0U) {
                chroma_above[column] = chroma_above[column - 1U];
            }
        }
    }
    {
        int ss_hor = width > 32U ? 2 : 1;
        int ss_ver = height > 32U ? 2 : 1;
        int32_t sum = 0;
        int dc_count = 0;
        if (have_top) {
            for (column = 0U; column < width; column += (uint16_t)ss_hor) {
                sum += luma_above_ds[column];
                ++dc_count;
            }
        }
        if (have_left) {
            for (row = 0U; row < height; row += (uint16_t)ss_ver) {
                sum += luma_left_ds[row];
                ++dc_count;
            }
        }
        if (dc_count > 0) {
            int div_shift = 0;
            uint16_t scale = av2h_sample_resolve_divisor_32((uint32_t)dc_count, &div_shift);
            int32_t rounding = (1 << div_shift) >> 1;
            int32_t val = (int32_t)(((int64_t)sum * scale + rounding) >> div_shift);
            int32_t max_v = (1 << (bit_depth + 3)) - 1;
            avg_l = val > max_v ? max_v : val;
        } else {
            avg_l = (int32_t)av2h_sample_domain_8bit.midpoint << 3;
        }
    }
    numb_up = 0;
    numb_left = 0;
    {
        Av2hCflReferenceCounts counts = cfl_reference_counts(width, height, have_top, have_left);
        numb_up = counts.above;
        numb_left = counts.left;
    }
    regression.count = 0;
    regression.sum_x = 0;
    regression.sum_y = 0;
    regression.sum_xx = 0;
    regression.sum_xy = 0;
    if (numb_up > 0) {
        int step = (int)width / numb_up;
        int start;
        int i;
        if (step < 1)
            step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)width; i += step) {
            int32_t lx = luma_above_ds[i] >> 3;
            int32_t cy = chroma_above[i];
            cfl_regression_add(&regression, lx, cy);
        }
    }
    if (numb_left > 0) {
        int step = (int)height / numb_left;
        int start;
        int i;
        if (step < 1)
            step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)height; i += step) {
            int32_t lx = luma_left_ds[i] >> 3;
            int32_t cy = chroma_left[i];
            cfl_regression_add(&regression, lx, cy);
        }
    }
    shift = 3 + AV2H_CFL_ADD_BITS_ALPHA;
    alpha = derive_alpha ? av2h_sample_regression_slope(regression.sum_x, regression.sum_y,
                                                        regression.sum_xx, regression.sum_xy,
                                                        regression.count, shift)
                         : supplied_alpha_q3 * (1 << AV2H_CFL_ADD_BITS_ALPHA);
    cfl_predict_samples(luma_plane, luma_x, luma_y, width, height, width, height, full_resolution,
                        filter_index, alpha, AV2H_CFL_ADD_BITS_ALPHA, avg_l, predictor,
                        predictor_stride_samples, domain);
    return AV2H_OK;
}

Av2hStatus av2h_cfl_derived_predict_u16(const Av2hReconstructionPlaneU16 *luma_plane,
                                        const Av2hReconstructionPlaneU16 *chroma_plane,
                                        uint8_t filter_index, uint16_t superblock_size,
                                        uint16_t block_x, uint16_t block_y, uint16_t width,
                                        uint16_t height, uint8_t have_top, uint8_t have_left,
                                        uint16_t *predictor, size_t predictor_stride_samples,
                                        const Av2hSampleDomain *domain) {
    Av2hCflRegression regression = {0};
    if (!av2h_sample_domain_valid(domain))
        return AV2H_INVALID_ARGUMENT;
    int numb_up = 0;
    int numb_left = 0;
    int32_t alpha;
    uint8_t is_top_sb_boundary;
    if (luma_plane == NULL || chroma_plane == NULL || predictor == NULL ||
        luma_plane->data == NULL || chroma_plane->data == NULL ||
        luma_plane->reconstructed == NULL || filter_index > 2U || width == 0U || height == 0U ||
        width > 64U || height > 64U || predictor_stride_samples < width ||
        (have_top && block_y == 0U) || (have_left && block_x == 0U)) {
        return AV2H_UNSUPPORTED;
    }
    {
        Av2hCflReferenceCounts counts = cfl_reference_counts(width, height, have_top, have_left);
        numb_up = counts.above;
        numb_left = counts.left;
    }
    is_top_sb_boundary =
        (uint8_t)(superblock_size != 0U && ((uint32_t)block_y * 2U) % superblock_size == 0U);
    if (numb_up != 0) {
        int step = (int)width / numb_up;
        int sample = step == 1 ? 0 : step >> 1;
        if (step < 1)
            step = 1;
        for (; sample < (int)width; sample += step) {
            uint16_t chroma_x = (uint16_t)(block_x + sample);
            int32_t luma;
            int32_t chroma;
            if (chroma_x >= chroma_plane->width) {
                chroma_x = (uint16_t)(chroma_plane->width - 1U);
            }
            if (is_top_sb_boundary) {
                luma = (int32_t)((sample == 0 ? av2h_chroma_luma_sb_top_left_clamped_q3_u16(
                                                    luma_plane, filter_index, chroma_x, block_y)
                                              : av2h_chroma_luma_sb_top_sample_q3_u16(
                                                    luma_plane, filter_index, chroma_x, block_y)) >>
                                 3U);
            } else {
                luma = (int32_t)((sample == 0 ? av2h_chroma_luma_left_clamped_q3_u16(
                                                    luma_plane, filter_index, chroma_x,
                                                    (uint16_t)(block_y - 1U))
                                              : av2h_chroma_luma_sample_q3_u16(
                                                    luma_plane, filter_index, chroma_x,
                                                    (uint16_t)(block_y - 1U))) >>
                                 3U);
            }
            chroma = chroma_plane
                         ->data[((size_t)block_y - 1U) * chroma_plane->stride_samples + chroma_x];
            cfl_regression_add(&regression, luma, chroma);
        }
    }
    if (numb_left != 0) {
        int step = (int)height / numb_left;
        int sample = step == 1 ? 0 : step >> 1;
        if (step < 1)
            step = 1;
        for (; sample < (int)height; sample += step) {
            uint16_t chroma_y = (uint16_t)(block_y + sample);
            int32_t luma =
                (int32_t)(av2h_chroma_luma_sample_q3_u16(luma_plane, filter_index,
                                                         (uint16_t)(block_x - 1U), chroma_y) >>
                          3U);
            int32_t chroma =
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride_samples + block_x - 1U];
            cfl_regression_add(&regression, luma, chroma);
        }
    }
    alpha = av2h_sample_regression_slope(regression.sum_x, regression.sum_y, regression.sum_xx,
                                         regression.sum_xy, regression.count,
                                         3 + AV2H_CFL_ADD_BITS_ALPHA);
    return cfl_predict_u16_scaled(luma_plane, filter_index, alpha, AV2H_CFL_ADD_BITS_ALPHA,
                                  superblock_size, block_x, block_y, width, height, have_top,
                                  have_left, predictor, predictor_stride_samples, domain);
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/chroma_prediction_cfl_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/chroma_prediction_cfl_kernel.inc"
#include "internal/sample_kernel_end.inc"

#include "encoder/m3.h"
#include "encoder/directional.h"

#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

#define ENC_M3_FULL_DIRECTIONAL_SHORTLIST 29U

static int round_shift(int value, unsigned int bits) {
    int divisor = 1 << bits;
    int shifted = value + (divisor >> 1);
    return shifted >= 0
        ? shifted / divisor
        : -((-shifted + divisor - 1) / divisor);
}

static int clip_int(int low, int high, int value) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static const uint16_t ac_qlookup[25U] = {
    64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U, 52U, 54U, 55U,
    57U, 59U, 60U, 62U, 64U, 66U, 68U, 70U, 72U, 74U, 76U, 78U
};

static uint32_t qlookup(unsigned int qindex) {
    if (qindex < 25U) return ac_qlookup[qindex];
    return (uint32_t)ac_qlookup[((qindex - 1U) % 24U) + 1U]
        << ((qindex - 1U) / 24U);
}

static uint32_t qlookup_delta(unsigned int qindex, int delta) {
    int effective = (int)qindex + delta;
    if (qindex == 0U && delta <= 0) return ac_qlookup[0U];
    effective = clip_int(1, 255, effective);
    return qlookup((unsigned int)effective);
}

static void qdelta_config(
    EncQuantizerDeltaConfig *config, int separate) {
    config->separate_uv_delta_q = separate;
    config->equal_ac_dc_q = 1;
    config->base_y_dc_delta_q = 0;
    config->y_dc_delta_q_enabled = 0;
    config->base_uv_dc_delta_q = 0;
    config->uv_dc_delta_q_enabled = 0;
    config->base_uv_ac_delta_q = 0;
    config->uv_ac_delta_q_enabled = 1;
}

static int dc_residual(int level, uint32_t q) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    uint32_t dequant_magnitude =
        ((uint32_t)magnitude * q + 4U) >> 3U;
    int dequant = level < 0
        ? -(int)dequant_magnitude : (int)dequant_magnitude;
    int row;
    dequant = clip_int(-32768, 32767, dequant);
    row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
    return clip_int(-256, 255, round_shift(64 * row, 11U));
}

static uint8_t reconstructed_value(
    int level, uint8_t prediction, uint32_t q) {
    int value = (int)prediction + dc_residual(level, q);
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static uint8_t reconstructed_chroma_value(
    int level, uint8_t prediction, uint32_t q, unsigned int shift) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    uint32_t dequant_magnitude =
        ((uint32_t)magnitude * q + 4U) >> 3U;
    int dequant = level < 0
        ? -(int)dequant_magnitude : (int)dequant_magnitude;
    int row;
    int residual;
    dequant = clip_int(-32768, 32767, dequant);
    row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
    residual = clip_int(-256, 255, round_shift(64 * row, shift));
    return (uint8_t)clip_int(0, 255, (int)prediction + residual);
}

static uint8_t reconstructed_chroma_rect_value(
    int level, uint8_t prediction, uint32_t q, unsigned int shift,
    int rectangular) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    uint32_t dequant_magnitude =
        ((uint32_t)magnitude * q + 4U) >> 3U;
    int dequant = level < 0
        ? -(int)dequant_magnitude : (int)dequant_magnitude;
    int row;
    int residual;
    dequant = clip_int(-32768, 32767, dequant);
    if (rectangular) {
        dequant = round_shift(2896 * dequant, 12U);
    }
    row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
    residual = clip_int(-256, 255, round_shift(64 * row, shift));
    return (uint8_t)clip_int(0, 255, (int)prediction + residual);
}

static uint64_t block_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, uint8_t reconstructed) {
    uint64_t error = 0U;
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            size_t source_x = block_x + x < width
                ? block_x + x : width - 1U;
            size_t source_y = block_y + y < height
                ? block_y + y : height - 1U;
            int difference =
                (int)input[source_y * stride + source_x] -
                (int)reconstructed;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

static uint64_t active_block_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, uint8_t reconstructed) {
    uint64_t error = 0U;
    size_t block_width =
        block_x + 8U <= width ? 8U : width - block_x;
    size_t block_height =
        block_y + 8U <= height ? 8U : height - block_y;
    size_t y;
    size_t x;
    for (y = 0U; y < block_height; ++y) {
        for (x = 0U; x < block_width; ++x) {
            int difference =
                (int)input[(block_y + y) * stride + block_x + x] -
                (int)reconstructed;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

#define ENC_M3_DIRECTIONAL_PREFILTER_RATIO_Q12 UINT64_C(3686)
#define ENC_M3_DIRECTIONAL_SMALL_PIXELS UINT64_C(4096)

static uint64_t directional_prefilter_score(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    uint8_t mode) {
    uint64_t total = 0U;
    size_t block_y;
    size_t block_x;
    for (block_y = 0U; block_y < height; block_y += 8U) {
        for (block_x = 0U; block_x < width; block_x += 8U) {
            int residual_sum = 0;
            int residual;
            size_t y;
            size_t x;
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    size_t source_x =
                        block_x + x < width ? block_x + x : width - 1U;
                    size_t source_y =
                        block_y + y < height ? block_y + y : height - 1U;
                    int prediction;
                    if (mode == 1U && block_y != 0U) {
                        prediction = input[
                            (block_y - 1U) * stride + source_x];
                    } else if (mode == 2U && block_x != 0U) {
                        prediction = input[source_y * stride + block_x - 1U];
                    } else {
                        uint32_t sum = 0U;
                        size_t edge;
                        size_t count = 0U;
                        if (block_y != 0U) {
                            for (edge = 0U; edge < 8U; ++edge) {
                                size_t edge_x = block_x + edge < width
                                    ? block_x + edge : width - 1U;
                                sum += input[
                                    (block_y - 1U) * stride + edge_x];
                            }
                            count += 8U;
                        }
                        if (block_x != 0U) {
                            for (edge = 0U; edge < 8U; ++edge) {
                                size_t edge_y = block_y + edge < height
                                    ? block_y + edge : height - 1U;
                                sum += input[
                                    edge_y * stride + block_x - 1U];
                            }
                            count += 8U;
                        }
                        prediction = count == 0U
                            ? 128 : (int)((sum + count / 2U) / count);
                    }
                    residual_sum +=
                        (int)input[source_y * stride + source_x] - prediction;
                }
            }
            residual = residual_sum >= 0
                ? (residual_sum + 32) / 64
                : -((-residual_sum + 32) / 64);
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    size_t source_x =
                        block_x + x < width ? block_x + x : width - 1U;
                    size_t source_y =
                        block_y + y < height ? block_y + y : height - 1U;
                    int prediction;
                    int difference;
                    if (mode == 1U && block_y != 0U) {
                        prediction = input[
                            (block_y - 1U) * stride + source_x];
                    } else if (mode == 2U && block_x != 0U) {
                        prediction = input[source_y * stride + block_x - 1U];
                    } else {
                        uint32_t sum = 0U;
                        size_t edge;
                        size_t count = 0U;
                        if (block_y != 0U) {
                            for (edge = 0U; edge < 8U; ++edge) {
                                size_t edge_x = block_x + edge < width
                                    ? block_x + edge : width - 1U;
                                sum += input[
                                    (block_y - 1U) * stride + edge_x];
                            }
                            count += 8U;
                        }
                        if (block_x != 0U) {
                            for (edge = 0U; edge < 8U; ++edge) {
                                size_t edge_y = block_y + edge < height
                                    ? block_y + edge : height - 1U;
                                sum += input[
                                    edge_y * stride + block_x - 1U];
                            }
                            count += 8U;
                        }
                        prediction = count == 0U
                            ? 128 : (int)((sum + count / 2U) / count);
                    }
                    prediction = clip_int(0, 255, prediction + residual);
                    difference =
                        (int)input[source_y * stride + source_x] - prediction;
                    total += (uint64_t)(difference * difference);
                }
            }
        }
    }
    return total;
}

static int directional_prefilter_shortlists(
    const uint8_t *input, size_t width, size_t height, size_t stride) {
    uint64_t pixels = (uint64_t)width * (uint64_t)height;
    uint64_t dc;
    uint64_t vertical;
    uint64_t horizontal;
    uint64_t directional;
    if (pixels <= ENC_M3_DIRECTIONAL_SMALL_PIXELS) return 1;
    dc = directional_prefilter_score(input, width, height, stride, 0U);
    vertical = directional_prefilter_score(input, width, height, stride, 1U);
    horizontal = directional_prefilter_score(input, width, height, stride, 2U);
    directional = vertical < horizontal ? vertical : horizontal;
    return dc != 0U &&
        directional * UINT64_C(4096) <=
            dc * ENC_M3_DIRECTIONAL_PREFILTER_RATIO_Q12;
}

static uint64_t full_directional_prefilter_score(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    uint8_t token) {
    uint64_t total = 0U;
    size_t block_y;
    size_t block_x;
    for (block_y = 0U; block_y < height; block_y += 8U) {
        for (block_x = 0U; block_x < width; block_x += 8U) {
            uint8_t above[17U];
            uint8_t left[17U];
            uint8_t prediction[64U];
            const uint8_t *above_edge = 0;
            const uint8_t *left_edge = 0;
            uint8_t corner = 128U;
            int residual_sum = 0;
            int residual;
            size_t index;
            size_t y;
            size_t x;
            if (block_y != 0U) {
                for (index = 0U; index < 17U; ++index) {
                    size_t source_x = block_x + index < width
                        ? block_x + index : width - 1U;
                    above[index] =
                        input[(block_y - 1U) * stride + source_x];
                }
                above_edge = above;
            }
            if (block_x != 0U) {
                for (index = 0U; index < 17U; ++index) {
                    size_t source_y = block_y + index < height
                        ? block_y + index : height - 1U;
                    left[index] = input[source_y * stride + block_x - 1U];
                }
                left_edge = left;
            }
            if (above_edge != 0 && left_edge != 0) {
                corner = input[(block_y - 1U) * stride + block_x - 1U];
            } else if (above_edge != 0) {
                corner = above[0U];
            } else if (left_edge != 0) {
                corner = left[0U];
            }
            if (!enc_directional_predict(
                    token, 0U, above_edge, above_edge != 0 ? 17U : 0U,
                    left_edge, left_edge != 0 ? 17U : 0U,
                    corner, 8U, 8U, prediction)) {
                return UINT64_MAX;
            }
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    size_t source_x =
                        block_x + x < width ? block_x + x : width - 1U;
                    size_t source_y =
                        block_y + y < height ? block_y + y : height - 1U;
                    residual_sum +=
                        (int)input[source_y * stride + source_x] -
                        (int)prediction[y * 8U + x];
                }
            }
            residual = residual_sum >= 0
                ? (residual_sum + 32) / 64
                : -((-residual_sum + 32) / 64);
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    size_t source_x =
                        block_x + x < width ? block_x + x : width - 1U;
                    size_t source_y =
                        block_y + y < height ? block_y + y : height - 1U;
                    int predicted = clip_int(
                        0, 255, (int)prediction[y * 8U + x] + residual);
                    int difference =
                        (int)input[source_y * stride + source_x] - predicted;
                    total += (uint64_t)(difference * difference);
                }
            }
        }
    }
    return total;
}

static size_t full_directional_candidates(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    int exact, uint8_t candidates[58U]) {
    uint64_t scores[ENC_M3_FULL_DIRECTIONAL_SHORTLIST];
    size_t count = 0U;
    uint8_t token;
    if (exact) {
        for (token = ENC_DIRECTIONAL_FIRST_TOKEN;
             token <= ENC_DIRECTIONAL_LAST_TOKEN; ++token) {
            candidates[count++] = token;
        }
        return count;
    }
    for (token = ENC_DIRECTIONAL_FIRST_TOKEN;
         token <= ENC_DIRECTIONAL_LAST_TOKEN; ++token) {
        if (token < 26U || token > 54U) continue;
        uint64_t score = full_directional_prefilter_score(
            input, width, height, stride, token);
        size_t position;
        if (count < ENC_M3_FULL_DIRECTIONAL_SHORTLIST) {
            position = count++;
        } else {
            size_t last = ENC_M3_FULL_DIRECTIONAL_SHORTLIST - 1U;
            if (score > scores[last] ||
                (score == scores[last] && token >= candidates[last])) continue;
            position = last;
        }
        while (position != 0U &&
               (score < scores[position - 1U] ||
                (score == scores[position - 1U] &&
                 token < candidates[position - 1U]))) {
            if (position < ENC_M3_FULL_DIRECTIONAL_SHORTLIST) {
                scores[position] = scores[position - 1U];
                candidates[position] = candidates[position - 1U];
            }
            --position;
        }
        scores[position] = score;
        candidates[position] = token;
    }
    return count;
}

uint8_t enc_m3_dc_prediction(
    const uint8_t *above, const uint8_t *left, size_t block_size) {
    uint32_t sum = 0U;
    size_t index;
    if (block_size == 0U) return 128U;
    if (above != 0) {
        for (index = 0U; index < block_size; ++index) sum += above[index];
    }
    if (left != 0) {
        for (index = 0U; index < block_size; ++index) sum += left[index];
    }
    if (above != 0 && left != 0) {
        return (uint8_t)((sum + block_size) / (2U * block_size));
    }
    if (above != 0 || left != 0) {
        return (uint8_t)((sum + block_size / 2U) / block_size);
    }
    return 128U;
}

static void clear_result(EncM3Result *result) {
    if (result == 0) return;
    result->stream_size = 0U;
    result->squared_error = 0U;
    result->quant_level = 0U;
    result->quant_sign = 0U;
    result->reconstructed_sample = 0U;
    result->block_count = 0U;
    result->pixel_count = 0U;
    result->workspace_size = 0U;
    result->qindex = 0U;
    result->intra_mode = 0U;
}

static int align_size(size_t value, size_t alignment, size_t *result) {
    size_t remainder;
    size_t padding;
    if (alignment == 0U || result == 0) return 0;
    remainder = value % alignment;
    padding = remainder == 0U ? 0U : alignment - remainder;
    return enc_size_add(value, padding, result);
}

static int ranges_overlap(
    const void *left, size_t left_size,
    const void *right, size_t right_size) {
    uintptr_t left_address;
    uintptr_t right_address;
    uintptr_t left_end;
    uintptr_t right_end;
    if (left_size == 0U || right_size == 0U) return 0;
    left_address = (uintptr_t)left;
    right_address = (uintptr_t)right;
    if (left_size > UINTPTR_MAX - left_address ||
        right_size > UINTPTR_MAX - right_address) {
        return 1;
    }
    left_end = left_address + left_size;
    right_end = right_address + right_size;
    return left_address < right_end && right_address < left_end;
}

static void clear_chroma_result(EncM3ChromaResult *result) {
        size_t plane;
        if (result == 0) return;
        result->stream_size = 0U;
        result->workspace_size = 0U;
        result->qindex = 0U;
        result->format = ENC_M3_PIXEL_I400;
        result->luma_mode = 0U;
        result->chroma_mode = 0U;
        result->u_qdelta = 0;
        result->v_qdelta = 0;
        for (plane = 0U; plane < 3U; ++plane) {
            result->plane_size[plane] = 0U;
            result->transform_count[plane] = 0U;
            result->squared_error[plane] = 0U;
            result->dc_level[plane] = 0;
            result->reconstructed_sample[plane] = 0U;
        }
    }

static int chroma_axis_admitted(size_t dimension) {
        return dimension <= ENC_M3_CHROMA_MAX_DIMENSION;
    }

EncStatus enc_m3_query_chroma_workspace(
        EncM3PixelFormat format, size_t width, size_t height,
        EncM3ChromaWorkspaceLayout *layout) {
        EncM3ChromaWorkspaceLayout candidate;
        int single_leaf_i420;
        size_t plane;
        size_t total = 0U;
        if (layout == 0 || width == 0U || height == 0U ||
            (format != ENC_M3_PIXEL_I420 && format != ENC_M3_PIXEL_I444)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (width > ENC_M3_CHROMA_MAX_DIMENSION ||
            height > ENC_M3_CHROMA_MAX_DIMENSION) {
            return ENC_UNSUPPORTED;
        }
        if (!((format == ENC_M3_PIXEL_I420 &&
               width <= 8U && height <= 8U) ||
              (width == 8U && height == 8U) ||
              (chroma_axis_admitted(width) &&
               chroma_axis_admitted(height)))) {
            return ENC_UNSUPPORTED;
        }
        single_leaf_i420 =
            format == ENC_M3_PIXEL_I420 &&
            (width != 8U || height != 8U) &&
            width < 16U && height < 16U &&
            ((width + 7U) & ~(size_t)7U) ==
                ((height + 7U) & ~(size_t)7U);
        candidate.alignment =
            width == 8U && height == 8U ? 1U : ENC_M3_WORKSPACE_ALIGNMENT;
        candidate.stream_capacity = ENC_M3_LEGACY_MAX_STREAM_SIZE;
        candidate.plane_width[0] = width;
        candidate.plane_height[0] = height;
        candidate.plane_width[1] =
            format == ENC_M3_PIXEL_I420 ? (width + 1U) / 2U : width;
        candidate.plane_height[1] =
            format == ENC_M3_PIXEL_I420 ? (height + 1U) / 2U : height;
        candidate.plane_width[2] = candidate.plane_width[1];
        candidate.plane_height[2] = candidate.plane_height[1];
        for (plane = 0U; plane < 3U; ++plane) {
            if (!enc_size_multiply(
                    candidate.plane_width[plane],
                    candidate.plane_height[plane],
                    &candidate.plane_size[plane]) ||
                !enc_size_add(total, candidate.plane_size[plane], &total)) {
                return ENC_OVERFLOW;
            }
            candidate.coded_plane_width[plane] =
                width == 8U && height == 8U
                    ? candidate.plane_width[plane]
                    : plane == 0U
                        ? (candidate.plane_width[plane] + 7U) & ~(size_t)7U
                        : format == ENC_M3_PIXEL_I420
                            ? ((width + 7U) & ~(size_t)7U) / 2U
                            : (width + 7U) & ~(size_t)7U;
            candidate.coded_plane_height[plane] =
                width == 8U && height == 8U
                    ? candidate.plane_height[plane]
                    : plane == 0U
                        ? (candidate.plane_height[plane] + 7U) & ~(size_t)7U
                        : format == ENC_M3_PIXEL_I420
                            ? ((height + 7U) & ~(size_t)7U) / 2U
                            : (height + 7U) & ~(size_t)7U;
            if (!enc_size_multiply(
                    candidate.coded_plane_width[plane],
                    candidate.coded_plane_height[plane],
                    &candidate.coded_plane_size[plane])) {
                return ENC_OVERFLOW;
            }
            candidate.transform_count[plane] =
                plane == 0U
                    ? (candidate.coded_plane_width[plane] / 8U) *
                      (candidate.coded_plane_height[plane] / 8U)
                    : width == 8U && height == 8U
                        ? 1U
                        : single_leaf_i420
                            ? 1U
                        : ((width + 15U) / 16U) *
                          ((height + 15U) / 16U);
        }
        if (candidate.transform_count[0U] > 1U) {
            size_t stream_bytes;
            if (!enc_size_multiply(
                    candidate.transform_count[0U], 64U, &stream_bytes) ||
                !enc_size_add(
                    stream_bytes, 256U, &candidate.stream_capacity)) {
                return ENC_OVERFLOW;
            }
        }
        candidate.reconstruction_size = total;
        if (width == 8U && height == 8U) {
            if (!enc_size_add(
                    total, candidate.stream_capacity, &candidate.size)) {
                return ENC_OVERFLOW;
            }
        } else {
            size_t coded_total = 0U;
            size_t level_count = 0U;
            size_t level_bytes;
            size_t staged_stream_bytes;
            for (plane = 0U; plane < 3U; ++plane) {
                if (!enc_size_add(
                        coded_total, candidate.coded_plane_size[plane],
                        &coded_total) ||
                    !enc_size_add(
                        level_count, candidate.transform_count[plane],
                        &level_count)) {
                    return ENC_OVERFLOW;
                }
            }
            if (!enc_size_multiply(
                    level_count, sizeof(int16_t), &level_bytes) ||
                !enc_size_add(coded_total, total, &candidate.size) ||
                !align_size(
                    candidate.size, candidate.alignment, &candidate.size) ||
                !enc_size_add(candidate.size, level_bytes, &candidate.size) ||
                !enc_size_multiply(
                    candidate.stream_capacity,
                    candidate.transform_count[0U] > 1U ? 3U : 1U,
                    &staged_stream_bytes) ||
                !enc_size_add(
                    candidate.size, staged_stream_bytes,
                    &candidate.size)) {
                return ENC_OVERFLOW;
            }
        }
        *layout = candidate;
        return ENC_OK;
    }

static int plane_span_size(
        size_t width, size_t height, size_t stride, size_t *size) {
        size_t last_row;
        if (width == 0U || height == 0U || stride < width || size == 0) return 0;
        return enc_size_multiply(height - 1U, stride, &last_row) &&
            enc_size_add(last_row, width, size);
    }

static void search_chroma_plane(
        const EncM3Plane8 *plane, size_t width, size_t height,
        uint32_t q, unsigned int shift,
        int16_t *best_level, uint8_t *best_value, uint64_t *best_error) {
        unsigned int max_level =
            (unsigned int)((UINT32_C(32768) * 8U + q - 1U) / q + 1U);
        int level;
        size_t y;
        size_t x;
        *best_level = 0;
        *best_value = 128U;
        *best_error = 0U;
        for (y = 0U; y < height; ++y) {
            for (x = 0U; x < width; ++x) {
                int difference = (int)plane->data[y * plane->stride + x] - 128;
                *best_error += (uint64_t)(difference * difference);
            }
        }
        for (level = 1; level <= (int)max_level && *best_error != 0U; ++level) {
            int sign;
            for (sign = 0; sign < 2; ++sign) {
                int candidate_level = sign == 0 ? level : -level;
                uint8_t candidate_value = reconstructed_chroma_value(
                    candidate_level, 128U, q, shift);
                uint64_t error = 0U;
                for (y = 0U; y < height; ++y) {
                    for (x = 0U; x < width; ++x) {
                        int difference =
                            (int)plane->data[y * plane->stride + x] -
                            (int)candidate_value;
                        error += (uint64_t)(difference * difference);
                    }
                }
                if (error < *best_error) {
                    *best_error = error;
                    *best_level = (int16_t)candidate_level;
                    *best_value = candidate_value;
                }
            }
        }
    }

static void search_i420_transform(
        const EncM3Plane8 *plane, size_t active_width, size_t active_height,
        size_t block_x, size_t block_y, size_t transform_size,
        uint8_t prediction, uint32_t q, unsigned int shift,
        int16_t *best_level, uint8_t *best_value,
        uint64_t *active_error) {
        unsigned int max_level =
            (unsigned int)((UINT32_C(32768) * 8U + q - 1U) / q + 1U);
        uint64_t best_coded_error = 0U;
        int level;
        size_t y;
        size_t x;
        *best_level = 0;
        *best_value = prediction;
        for (y = 0U; y < transform_size; ++y) {
            size_t source_y = block_y + y < active_height
                ? block_y + y : active_height - 1U;
            for (x = 0U; x < transform_size; ++x) {
                size_t source_x = block_x + x < active_width
                    ? block_x + x : active_width - 1U;
                int difference =
                    (int)plane->data[source_y * plane->stride + source_x] -
                    (int)prediction;
                best_coded_error += (uint64_t)(difference * difference);
            }
        }
        for (level = 1;
             level <= (int)max_level && best_coded_error != 0U; ++level) {
            int sign;
            for (sign = 0; sign < 2; ++sign) {
                int candidate_level = sign == 0 ? level : -level;
                uint8_t candidate_value = reconstructed_chroma_value(
                    candidate_level, prediction, q, shift);
                uint64_t error = 0U;
                for (y = 0U; y < transform_size; ++y) {
                    size_t source_y = block_y + y < active_height
                        ? block_y + y : active_height - 1U;
                    for (x = 0U; x < transform_size; ++x) {
                        size_t source_x = block_x + x < active_width
                            ? block_x + x : active_width - 1U;
                        int difference = (int)plane->data[
                            source_y * plane->stride + source_x] -
                            (int)candidate_value;
                        error += (uint64_t)(difference * difference);
                    }

                }
                if (error < best_coded_error) {
                    best_coded_error = error;
                    *best_level = (int16_t)candidate_level;
                    *best_value = candidate_value;
                }
            }
        }
        *active_error = 0U;
        for (y = block_y;
             y < block_y + transform_size && y < active_height; ++y) {
            for (x = block_x;
                 x < block_x + transform_size && x < active_width; ++x) {
                int difference =
                    (int)plane->data[y * plane->stride + x] -
                    (int)*best_value;
                *active_error += (uint64_t)(difference * difference);
            }
        }
    }

static EncStatus encode_i420_single_leaf(
        const EncM3Image8 *input, unsigned int qindex,
        int qdelta_enabled, int u_qdelta, int v_qdelta,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[16U];
        uint8_t tile[224U];
        uint8_t frame_payload[232U];
        uint8_t *workspace_bytes = (uint8_t *)workspace;
        uint8_t *padded[3U];
        uint8_t *staged[3U];
        uint8_t *staged_stream;
        int16_t *levels;
        size_t input_span[3U];
        size_t output_span[3U];
        size_t coded_total = 0U;
        size_t active_total = 0U;
        size_t level_offset;
        size_t stream_offset;
        size_t sequence_size;
        size_t frame_header_size;
        size_t tile_size;
        size_t plane;
        size_t level_index = 0U;
        uint32_t plane_q[3U];
        EncByteWriter payload_writer;
        EncByteWriter writer;
        EncStatus status;
        clear_chroma_result(&candidate);
        status = enc_m3_query_chroma_workspace(
            input->format, input->width, input->height, &layout);
        if (status != ENC_OK) return status;
        if ((uintptr_t)workspace % layout.alignment != 0U) {
            return ENC_INVALID_ARGUMENT;
        }
        if (workspace_capacity < layout.size) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            if (input->planes[plane].data == 0 ||
                reconstruction->planes[plane].data == 0 ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    input->planes[plane].stride, &input_span[plane]) ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    reconstruction->planes[plane].stride,
                    &output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
            if (input->planes[plane].capacity < input_span[plane] ||
                reconstruction->planes[plane].capacity < output_span[plane]) {
                return ENC_OUT_OF_SPACE;
            }
            if (ranges_overlap(
                    workspace, layout.size,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    workspace, layout.size,
                    reconstruction->planes[plane].data, output_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    reconstruction->planes[plane].data, output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
            padded[plane] = workspace_bytes + coded_total;
            coded_total += layout.coded_plane_size[plane];
        }
        for (plane = 0U; plane < 3U; ++plane) {
            size_t other;
            staged[plane] = workspace_bytes + coded_total + active_total;
            active_total += layout.plane_size[plane];
            for (other = 0U; other < 3U; ++other) {
                if (ranges_overlap(
                        reconstruction->planes[plane].data,
                        output_span[plane],
                        input->planes[other].data, input_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
                if (other > plane && ranges_overlap(
                        reconstruction->planes[plane].data,
                        output_span[plane],
                        reconstruction->planes[other].data,
                        output_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
            }
        }
        if (ranges_overlap(
                workspace, layout.size, stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!align_size(
                coded_total + active_total, layout.alignment, &level_offset)) {
            return ENC_OVERFLOW;
        }
        levels = (int16_t *)(void *)(workspace_bytes + level_offset);
        if (!enc_size_multiply(
                layout.transform_count[0] + 2U,
                sizeof(int16_t), &stream_offset) ||
            !enc_size_add(level_offset, stream_offset, &stream_offset)) {
            return ENC_OVERFLOW;
        }
        staged_stream = workspace_bytes + stream_offset;
        plane_q[0U] = qlookup(qindex);
        plane_q[1U] = qlookup_delta(qindex, u_qdelta);
        plane_q[2U] = qlookup_delta(qindex, v_qdelta);
        for (plane = 0U; plane < 3U; ++plane) {
            size_t transform_size = plane == 0U ? 8U :
                layout.coded_plane_width[plane];
            size_t block_y;
            size_t block_x;
            candidate.plane_size[plane] = layout.plane_size[plane];
            candidate.transform_count[plane] = layout.transform_count[plane];
            for (block_y = 0U;
                 block_y < layout.coded_plane_height[plane];
                 block_y += transform_size) {
                for (block_x = 0U;
                     block_x < layout.coded_plane_width[plane];
                     block_x += transform_size) {
                    uint8_t above[8U];
                    uint8_t left[8U];
                    const uint8_t *above_edge = 0;
                    const uint8_t *left_edge = 0;
                    uint8_t prediction;
                    uint8_t value;
                    int16_t level;
                    uint64_t error;
                    size_t edge;
                    size_t y;
                    size_t x;
                    if (block_y != 0U) {
                        for (edge = 0U; edge < transform_size; ++edge) {
                            above[edge] = padded[plane][
                                (block_y - 1U) *
                                    layout.coded_plane_width[plane] +
                                block_x + edge];
                        }
                        above_edge = above;
                    }
                    if (block_x != 0U) {
                        for (edge = 0U; edge < transform_size; ++edge) {
                            left[edge] = padded[plane][
                                (block_y + edge) *
                                    layout.coded_plane_width[plane] +
                                block_x - 1U];
                        }
                        left_edge = left;
                    }
                    prediction = enc_m3_dc_prediction(
                        above_edge, left_edge, transform_size);
                    if (plane == 0U) {
                        search_i420_transform(
                            &input->planes[plane],
                            layout.plane_width[plane],
                            layout.plane_height[plane],
                            block_x, block_y, transform_size, prediction,
                            plane_q[plane],
                            11U, &level, &value, &error);
                    } else {
                        if (qdelta_enabled) {
                            search_i420_transform(
                                &input->planes[plane],
                                layout.plane_width[plane],
                                layout.plane_height[plane],
                                block_x, block_y, transform_size, prediction,
                                plane_q[plane], 10U,
                                &level, &value, &error);
                        } else {
                            size_t active_y;
                            size_t active_x;
                            level = 0;
                            value = prediction;
                            error = 0U;
                            for (active_y = 0U;
                                 active_y < layout.plane_height[plane];
                                 ++active_y) {
                                for (active_x = 0U;
                                     active_x < layout.plane_width[plane];
                                     ++active_x) {
                                    int difference =
                                        (int)input->planes[plane].data[
                                            active_y *
                                                input->planes[plane].stride +
                                            active_x] - (int)value;
                                    error += (uint64_t)(
                                        difference * difference);
                                }
                            }
                        }
                    }
                    if (level_index == 0U || plane != 0U) {
                        candidate.dc_level[plane] = level;
                        candidate.reconstructed_sample[plane] = value;
                    }
                    levels[level_index++] = level;
                    candidate.squared_error[plane] += error;
                    for (y = 0U; y < transform_size; ++y) {
                        for (x = 0U; x < transform_size; ++x) {
                            padded[plane][
                                (block_y + y) *
                                    layout.coded_plane_width[plane] +
                                block_x + x] = value;
                        }
                    }
                }
            }
            {
                size_t y;
                for (y = 0U; y < layout.plane_height[plane]; ++y) {
                    size_t x;
                    for (x = 0U; x < layout.plane_width[plane]; ++x) {
                        staged[plane][y * layout.plane_width[plane] + x] =
                            padded[plane][
                                y * layout.coded_plane_width[plane] + x];
                    }
                }
            }
        }
        if (qdelta_enabled) {
            EncQuantizerDeltaConfig config;
            EncFrameQuantizerDeltas deltas;
            qdelta_config(&config, u_qdelta != v_qdelta);
            deltas.y_dc = 0;
            deltas.u_dc = (int16_t)u_qdelta;
            deltas.u_ac = (int16_t)u_qdelta;
            deltas.v_dc = (int16_t)v_qdelta;
            deltas.v_ac = (int16_t)v_qdelta;
            status = enc_m3_sequence_header_for_qdelta(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format, &config);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_qdelta_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex, &config, &deltas);
        } else {
            status = enc_m3_sequence_header_for_format(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_q_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex);
        }
        if (status != ENC_OK) return status;
        status = enc_m3_dc_i420_single_leaf_tile_q(
            tile, sizeof(tile), &tile_size, input->width, input->height,
            levels, layout.transform_count[0],
            levels[layout.transform_count[0]],
            levels[layout.transform_count[0] + 1U], qindex);
        if (status != ENC_OK) return status;
        enc_byte_writer_init(
            &payload_writer, frame_payload, sizeof(frame_payload));
        if (enc_byte_writer_write(
                &payload_writer, frame_header, frame_header_size) != ENC_OK ||
            enc_byte_writer_write(
                &payload_writer, tile, tile_size) != ENC_OK) {
            return payload_writer.status;
        }
        enc_byte_writer_init(
            &writer, staged_stream, layout.stream_capacity);
        if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
            enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
            enc_obu_write(
                &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
            return writer.status;
        }
        if (writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            size_t y;
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                status = enc_memory_copy(
                    reconstruction->planes[plane].data +
                        y * reconstruction->planes[plane].stride,
                    reconstruction->planes[plane].capacity -
                        y * reconstruction->planes[plane].stride,
                    staged[plane] + y * layout.plane_width[plane],
                    layout.plane_width[plane]);
                if (status != ENC_OK) return status;
            }
        }
        status = enc_memory_copy(
            stream, stream_capacity, staged_stream, writer.position);
        if (status != ENC_OK) return status;
        candidate.stream_size = writer.position;
        candidate.workspace_size = layout.size;
        candidate.qindex = qindex;
        candidate.format = input->format;
        candidate.u_qdelta = (int8_t)u_qdelta;
        candidate.v_qdelta = (int8_t)v_qdelta;
        *result = candidate;
        return ENC_OK;
    }

typedef struct {
    const EncM3Image8 *input;
    const EncM3ChromaWorkspaceLayout *layout;
    uint8_t *padded[3U];
    int16_t *levels[3U];
    size_t transform_index[3U];
    uint32_t q[3U];
    EncM3ChromaResult *result;
    uint8_t luma_mode;
    uint8_t chroma_mode;
    int fast_search;
    int fast_luma_search;
    size_t superblock_x;
    size_t superblock_y;
    uint8_t reconstructed_cells[3U][32U];
} EncM3ColorSearchState;

static int search_sample_reconstructed(
    size_t superblock_x, size_t superblock_y,
    size_t superblock_size,
    const uint8_t reconstructed_cells[32U],
    size_t sample_x, size_t sample_y) {
    size_t cell;
    size_t cells_per_side = superblock_size / 4U;
    if (sample_y < superblock_y) return 1;
    if (sample_y >= superblock_y + superblock_size) return 0;
    if (sample_x < superblock_x) return 1;
    if (sample_x >= superblock_x + superblock_size) return 0;
    cell = ((sample_y - superblock_y) / 4U) * cells_per_side +
           (sample_x - superblock_x) / 4U;
    return (reconstructed_cells[cell / 8U] >> (cell % 8U)) & 1U;
}

static void search_mark_reconstructed(
    size_t superblock_x, size_t superblock_y,
    size_t superblock_size,
    uint8_t reconstructed_cells[32U],
    size_t block_x, size_t block_y, size_t width, size_t height) {
    size_t y;
    size_t x;
    size_t cells_per_side = superblock_size / 4U;
    for (y = block_y; y < block_y + height; y += 4U) {
        for (x = block_x; x < block_x + width; x += 4U) {
            if (x < superblock_x || x >= superblock_x + superblock_size ||
                y < superblock_y || y >= superblock_y + superblock_size) {
                continue;
            }
            size_t cell =
                          ((y - superblock_y) / 4U) * cells_per_side +
                          (x - superblock_x) / 4U;
            reconstructed_cells[cell / 8U] |=
                (uint8_t)(1U << (cell % 8U));
        }
    }
}

static uint8_t color_dc_prediction(
    const uint8_t *above, const uint8_t *left,
    size_t width, size_t height) {
    uint32_t sum = 0U;
    size_t index;
    if (above != 0) {
        for (index = 0U; index < width; ++index) sum += above[index];
    }
    if (left != 0) {
        for (index = 0U; index < height; ++index) sum += left[index];
    }
    if (above != 0 && left != 0) {
        return (uint8_t)((sum + (width + height) / 2U) / (width + height));
    }
    if (above != 0) return (uint8_t)((sum + width / 2U) / width);
    if (left != 0) return (uint8_t)((sum + height / 2U) / height);
    return 128U;
}

static uint8_t actual_intra_mode(uint8_t mode) {
    return mode >= 5U && mode <= 8U ? (uint8_t)(mode - 2U) : mode;
}

static uint8_t effective_chroma_directional_token(
    uint8_t luma_token, uint8_t chroma_token) {
    uint8_t luma_joint;
    uint8_t chroma_joint;
    uint8_t luma_mode;
    uint8_t chroma_mode;
    int8_t luma_delta;
    int8_t chroma_delta;
    if (!enc_directional_token_to_joint(luma_token, &luma_joint) ||
        !enc_directional_token_to_joint(chroma_token, &chroma_joint) ||
        !enc_directional_joint_to_mode(
            luma_joint, &luma_mode, &luma_delta) ||
        !enc_directional_joint_to_mode(
            chroma_joint, &chroma_mode, &chroma_delta)) {
        return chroma_token;
    }
    return luma_mode == chroma_mode &&
        enc_directional_joint_is_directional(luma_joint)
            ? luma_token : chroma_token;
}

void enc_m3_intra_prediction(
    uint8_t mode, unsigned int plane,
    const uint8_t *above, size_t above_count,
    const uint8_t *left, size_t left_count,
    uint8_t corner, size_t width, size_t height, uint8_t *output) {
    size_t y;
    size_t x;
    uint8_t joint_mode;
    if (enc_directional_token_to_joint(mode, &joint_mode) &&
        enc_directional_joint_is_directional(joint_mode)) {
        (void)enc_directional_predict(
            mode, plane, above, above_count, left, left_count,
            corner, width, height, output);
        return;
    }
    mode = actual_intra_mode(mode);
    if (mode == 0U) {
        uint8_t value = color_dc_prediction(above, left, width, height);
        for (y = 0U; y < width * height; ++y) output[y] = value;
        return;
    }
    if (mode >= 3U) {
        uint8_t top_right = above != 0
            ? above[above_count > width ? width : width - 1U]
            : left != 0 ? left[0U] : 127U;
        uint8_t bottom_left = left != 0
            ? left[left_count > height ? height : height - 1U]
            : above != 0 ? above[0U] : 129U;
        unsigned int log2_width = 0U;
        unsigned int log2_height = 0U;
        unsigned int scale;
        while (((size_t)1U << log2_width) < width) ++log2_width;
        while (((size_t)1U << log2_height) < height) ++log2_height;
        scale = (unsigned int)round_shift(
            (int)(log2_width + log2_height) - 4, 2U);
        for (y = 0U; y < height; ++y) {
            int top_weight = 32 >> (
                ((y << 1U) >> scale) < 6U
                    ? ((y << 1U) >> scale) : 6U);
            for (x = 0U; x < width; ++x) {
                int top = above != 0
                    ? above[x] : left != 0 ? left[0U] : 127;
                int side = left != 0
                    ? left[y] : above != 0 ? above[0U] : 129;
                if (mode == 6U) {
                    int base = top + side - corner;
                    int side_distance = base > side
                        ? base - side : side - base;
                    int top_distance = base > top
                        ? base - top : top - base;
                    int corner_distance = base > corner
                        ? base - corner : corner - base;
                    output[y * width + x] = (uint8_t)(
                        side_distance <= top_distance &&
                        side_distance <= corner_distance ? side :
                        top_distance <= corner_distance ? top : corner);
                } else {
                    int left_weight = 32 >> (
                        ((x << 1U) >> scale) < 6U
                            ? ((x << 1U) >> scale) : 6U);
                    int horizontal = (int)top_right + round_shift(
                        (side - (int)top_right) *
                            (int)(width - 1U - x),
                        log2_width);
                    int vertical = (int)bottom_left + round_shift(
                        (top - (int)bottom_left) *
                            (int)(height - 1U - y),
                        log2_height);
                    horizontal += round_shift(
                        (side - horizontal) * left_weight, 6U);
                    vertical += round_shift(
                        (top - vertical) * top_weight, 6U);
                    output[y * width + x] = (uint8_t)(
                        mode == 4U ? vertical :
                        mode == 5U ? horizontal :
                        round_shift(vertical + horizontal, 1U));
                }
            }
        }
        return;
    }
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            output[y * width + x] = mode == 1U
                ? (above != 0 ? above[x] : left != 0 ? left[0U] : 127U)
                : (left != 0 ? left[y] : above != 0 ? above[0U] : 129U);
        }
    }
}

static void search_color_prediction(
    const EncM3Plane8 *plane, size_t active_width, size_t active_height,
    size_t block_x, size_t block_y, size_t width, size_t height,
    const uint8_t prediction[256U], uint32_t q, unsigned int shift,
    int optimize,
    int16_t *best_level, uint8_t reconstruction[256U],
    uint64_t *active_error) {
    unsigned int max_level =
        (unsigned int)((UINT32_C(32768) * 8U + q - 1U) / q + 1U);
    int rectangular = width != height;
    uint64_t best_error = UINT64_MAX;
    int low;
    int high;
    int level;
    int best = 0;
    size_t y;
    size_t x;
    int64_t difference_sum = 0;
    for (y = 0U; y < height; ++y) {
        size_t source_y = block_y + y < active_height
            ? block_y + y : active_height - 1U;
        for (x = 0U; x < width; ++x) {
            size_t source_x = block_x + x < active_width
                ? block_x + x : active_width - 1U;
            difference_sum +=
                (int)plane->data[source_y * plane->stride + source_x] -
                (int)prediction[y * width + x];
        }
    }
    low = -(int)max_level;
    high = (int)max_level;
    while (optimize && low + 1 < high) {
        int middle = low + (high - low) / 2;
        int residual = (int)reconstructed_chroma_rect_value(
            middle, 128U, q, shift, rectangular) - 128;
        if ((int64_t)residual * (int64_t)(width * height) <
            difference_sum) {
            low = middle;
        } else {
            high = middle;
        }
    }
    if (optimize) {
        low = low - 2 < -(int)max_level ? -(int)max_level : low - 2;
        high = high + 2 > (int)max_level ? (int)max_level : high + 2;
    }
    for (level = low; level <= high; ++level) {
        uint64_t error = 0U;
        size_t py;
        for (py = 0U; py < height; ++py) {
            size_t source_y = block_y + py < active_height
                ? block_y + py : active_height - 1U;
            for (x = 0U; x < width; ++x) {
                size_t source_x = block_x + x < active_width
                    ? block_x + x : active_width - 1U;
                uint8_t value = reconstructed_chroma_rect_value(
                    level, prediction[py * width + x], q, shift, rectangular);
                int difference =
                    (int)plane->data[source_y * plane->stride + source_x] -
                    (int)value;
                error += (uint64_t)(difference * difference);
            }
        }
        if (error < best_error ||
            (error == best_error &&
             (level == 0 ? 0 : level > 0 ? 2 * level - 1 : -2 * level) <
             (best == 0 ? 0 : best > 0 ? 2 * best - 1 : -2 * best))) {
            best_error = error;
            best = level;
        }
    }
    if (optimize && (0 < low || 0 > high)) {
        uint64_t error = 0U;
        for (y = 0U; y < height; ++y) {
            size_t source_y = block_y + y < active_height
                ? block_y + y : active_height - 1U;
            for (x = 0U; x < width; ++x) {
                size_t source_x = block_x + x < active_width
                    ? block_x + x : active_width - 1U;
                int difference =
                    (int)plane->data[source_y * plane->stride + source_x] -
                    (int)prediction[y * width + x];
                error += (uint64_t)(difference * difference);
            }
        }
        if (error < best_error) {
            best_error = error;
            best = 0;
        }
    }
    *best_level = (int16_t)best;
    *active_error = 0U;
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            uint8_t value = reconstructed_chroma_rect_value(
                best, prediction[y * width + x], q, shift, rectangular);
            reconstruction[y * width + x] = value;
            if (block_y + y < active_height && block_x + x < active_width) {
                int difference = (int)plane->data[
                    (block_y + y) * plane->stride + block_x + x] -
                    (int)value;
                *active_error += (uint64_t)(difference * difference);
            }
        }
    }
}

static int color_search_forced_axis(
    size_t rows, size_t columns, size_t row, size_t column,
    size_t width, size_t height) {
    int has_rows = row + height / 2U < rows;
    int has_columns = column + width / 2U < columns;
    if (has_rows && has_columns) return 0;
    if (width == height) return has_rows && !has_columns ? 2 : 1;
    if (height > width) {
        if (!has_rows) return 1;
        if (!has_columns && width >= 4U &&
            column + width / 4U >= columns) return 1;
    } else {
        if (!has_columns) return 2;
        if (!has_rows && height >= 4U &&
            row + height / 4U >= rows) return 2;
    }
    return 0;
}

static void search_color_transform(
    EncM3ColorSearchState *state, size_t plane,
    size_t block_x, size_t block_y,
    size_t transform_width, size_t transform_height, uint8_t mode) {
    size_t coded_width = state->layout->coded_plane_width[plane];
    uint8_t above_storage[35U] = {0U};
    uint8_t left[34U] = {0U};
    uint8_t *above = &above_storage[1U];
    const uint8_t *above_edge = 0;
    const uint8_t *left_edge = 0;
    uint8_t corner = 128U;
    uint8_t prediction[256U];
    uint8_t reconstructed[256U];
    int16_t level;
    uint64_t error;
    size_t index;
    size_t y;
    size_t x;
    size_t above_count = 0U;
    size_t left_count = 0U;
    if (block_y != 0U) {
        size_t limit = mode >= 5U && mode <= 7U
            ? 2U * transform_width
            : transform_width + transform_height + 1U;
        for (index = 0U; index < limit; ++index) {
            size_t edge_x = block_x + index <
                    state->layout->plane_width[plane]
                ? block_x + index
                : state->layout->plane_width[plane] - 1U;
            if (mode >= 5U && mode <= 7U &&
                !search_sample_reconstructed(
                    state->superblock_x >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    state->superblock_y >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    64U >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    state->reconstructed_cells[plane],
                    edge_x, block_y - 1U)) {
                break;
            }
            above[index] = state->padded[plane][
                (block_y - 1U) * coded_width + edge_x];
        }
        above_count = index;
        above[-1] = block_x == 0U ? above[0U] :
            state->padded[plane][
                (block_y - 1U) * coded_width + block_x - 1U];
        above_edge = above_count == 0U ? 0 : above;
    }
    if (block_x != 0U) {
        size_t limit = mode >= 5U && mode <= 7U &&
                block_y + transform_height <
                    state->layout->plane_height[plane]
            ? transform_height + 1U
            : mode >= 5U && mode <= 7U
                ? transform_height
                : transform_width + transform_height + 1U;
        for (index = 0U; index < limit; ++index) {
            size_t edge_y = block_y + index <
                    state->layout->plane_height[plane]
                ? block_y + index
                : state->layout->plane_height[plane] - 1U;
            if (mode >= 5U && mode <= 7U &&
                !search_sample_reconstructed(
                    state->superblock_x >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    state->superblock_y >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    64U >> (plane != 0U &&
                        state->input->format == ENC_M3_PIXEL_I420),
                    state->reconstructed_cells[plane],
                    block_x - 1U, edge_y)) {
                break;
            }
            left[index] = state->padded[plane][
                edge_y * coded_width + block_x - 1U];
        }
        left_count = index;
        left_edge = left_count == 0U ? 0 : left;
    }
    corner = above_edge != 0 && left_edge != 0 ? above[-1] :
             above_edge != 0 ? above[0U] :
             left_edge != 0 ? left[0U] : 128U;
    enc_m3_intra_prediction(
        mode, (unsigned int)plane,
        above_edge, above_edge != 0 ? above_count : 0U,
        left_edge, left_edge != 0 ? left_count : 0U, corner,
        transform_width, transform_height, prediction);
    search_color_prediction(
        &state->input->planes[plane],
        state->layout->plane_width[plane],
        state->layout->plane_height[plane],
        block_x, block_y, transform_width, transform_height,
        prediction,         state->q[plane],
        transform_width >= 16U && transform_height >= 16U ? 12U :
        transform_width >= 8U && transform_height >= 8U ? 11U : 10U,
        mode != 0U ||
            (plane == 0U
                ? state->fast_luma_search : state->fast_search),
        &level, reconstructed, &error);
    index = state->transform_index[plane]++;
    state->levels[plane][index] = level;
    state->result->squared_error[plane] += error;
    if (index == 0U) {
        state->result->dc_level[plane] = level;
        state->result->reconstructed_sample[plane] = reconstructed[0U];
    }
    for (y = 0U; y < transform_height; ++y) {
        for (x = 0U; x < transform_width; ++x) {
            state->padded[plane][
                (block_y + y) * coded_width + block_x + x] =
                reconstructed[y * transform_width + x];
        }
    }
    search_mark_reconstructed(
        state->superblock_x >> (plane != 0U &&
            state->input->format == ENC_M3_PIXEL_I420),
        state->superblock_y >> (plane != 0U &&
            state->input->format == ENC_M3_PIXEL_I420),
        64U >> (plane != 0U &&
            state->input->format == ENC_M3_PIXEL_I420),
        state->reconstructed_cells[plane],
        block_x, block_y, transform_width, transform_height);
}

static void search_luma_partition(
    EncM3ColorSearchState *state, size_t row, size_t column,
    size_t width, size_t height) {
    size_t rows = ((state->input->height + 7U) / 8U) * 2U;
    size_t columns = ((state->input->width + 7U) / 8U) * 2U;
    int forced;
    int axis;
    if (row >= rows || column >= columns) return;
    forced = color_search_forced_axis(
        rows, columns, row, column, width, height);
    if (forced != 0) {
        axis = forced;
    } else if (width <= 4U && height <= 4U) {
        uint8_t mode = state->luma_mode == 3U
            ? (row == 0U ? 0U : 1U)
            : state->luma_mode == 4U
                ? (column == 0U ? 0U : 2U) : state->luma_mode;
        size_t tx_y;
        size_t tx_x;
        for (tx_y = 0U; tx_y < height * 4U; tx_y += 8U) {
            for (tx_x = 0U; tx_x < width * 4U; tx_x += 8U) {
                search_color_transform(
                    state, 0U, column * 4U + tx_x,
                    row * 4U + tx_y, 8U, 8U, mode);
            }
        }
        return;
    } else {
        axis = height >= width ? 1 : 2;
    }
    if (axis == 1) {
        search_luma_partition(
            state, row, column, width, height / 2U);
        search_luma_partition(
            state, row + height / 2U, column, width, height / 2U);
    } else {
        search_luma_partition(
            state, row, column, width / 2U, height);
        search_luma_partition(
            state, row, column + width / 2U, width / 2U, height);
    }
}

static void search_chroma_partition(
    EncM3ColorSearchState *state, size_t row, size_t column,
    size_t width, size_t height) {
    size_t rows = ((state->input->height + 7U) / 8U) * 2U;
    size_t columns = ((state->input->width + 7U) / 8U) * 2U;
    int forced;
    int axis;
    if (row >= rows || column >= columns) return;
    forced = color_search_forced_axis(
        rows, columns, row, column, width, height);
    if (forced != 0) {
        axis = forced;
    } else if (width <= 4U && height <= 4U) {
        size_t subsampling =
            state->input->format == ENC_M3_PIXEL_I420 ? 1U : 0U;
        size_t transform_width =
            (width * 4U) >> subsampling;
        size_t transform_height =
            (height * 4U) >> subsampling;
        size_t block_x = column * 4U >> subsampling;
        size_t block_y = row * 4U >> subsampling;
        uint8_t chroma_mode = effective_chroma_directional_token(
            state->luma_mode, state->chroma_mode);
        if (block_x == 0U || block_y == 0U) chroma_mode = 0U;
        search_color_transform(
            state, 1U, block_x, block_y,
            transform_width, transform_height, chroma_mode);
        search_color_transform(
            state, 2U, block_x, block_y,
            transform_width, transform_height, chroma_mode);
        return;
    } else {
        axis = height >= width ? 1 : 2;
    }
    if (axis == 1) {
        search_chroma_partition(
            state, row, column, width, height / 2U);
        search_chroma_partition(
            state, row + height / 2U, column, width, height / 2U);
    } else {
        search_chroma_partition(
            state, row, column, width / 2U, height);
        search_chroma_partition(
            state, row, column + width / 2U, width / 2U, height);
    }
}

static EncStatus encode_even_chroma_multileaf(
        const EncM3Image8 *input, unsigned int qindex,
        uint8_t luma_mode, uint8_t chroma_mode,
        int qdelta_enabled, int u_qdelta, int v_qdelta,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result,
        int16_t *saved_u_levels, int16_t *saved_v_levels,
        size_t saved_level_capacity) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[16U];
        uint8_t *workspace_bytes = (uint8_t *)workspace;
        uint8_t *padded[3U];
        uint8_t *staged[3U];
        int16_t *levels;
        int16_t *plane_levels[3U];
        uint8_t *tile;
        uint8_t *frame_payload;
        uint8_t *staged_stream;
        size_t input_span[3U];
        size_t output_span[3U];
        size_t coded_total = 0U;
        size_t active_total = 0U;
        size_t level_offset;
        size_t buffer_offset;
        size_t sequence_size;
        size_t frame_header_size;
        size_t tile_size;
        size_t plane;
        size_t transform_index[3U] = {0U, 0U, 0U};
        EncByteWriter payload_writer;
        EncByteWriter writer;
        EncStatus status;
        clear_chroma_result(&candidate);
        status = enc_m3_query_chroma_workspace(
            input->format, input->width, input->height, &layout);
        if (status != ENC_OK) return status;
        if ((uintptr_t)workspace % layout.alignment != 0U) {
            return ENC_INVALID_ARGUMENT;
        }
        if (workspace_capacity < layout.size) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            if (input->planes[plane].data == 0 ||
                reconstruction->planes[plane].data == 0 ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    input->planes[plane].stride, &input_span[plane]) ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    reconstruction->planes[plane].stride,
                    &output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
            if (input->planes[plane].capacity < input_span[plane] ||
                reconstruction->planes[plane].capacity < output_span[plane]) {
                return ENC_OUT_OF_SPACE;
            }
            if (ranges_overlap(
                    workspace, layout.size,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    workspace, layout.size,
                    reconstruction->planes[plane].data, output_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    reconstruction->planes[plane].data, output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
            padded[plane] = workspace_bytes + coded_total;
            coded_total += layout.coded_plane_size[plane];
        }
        for (plane = 0U; plane < 3U; ++plane) {
            size_t other;
            staged[plane] = workspace_bytes + coded_total + active_total;
            active_total += layout.plane_size[plane];
            for (other = 0U; other < 3U; ++other) {
                if (ranges_overlap(
                        reconstruction->planes[plane].data,
                        output_span[plane],
                        input->planes[other].data, input_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
                if (other > plane && ranges_overlap(
                        reconstruction->planes[plane].data,
                        output_span[plane],
                        reconstruction->planes[other].data,
                        output_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
            }
        }
        if (ranges_overlap(
                workspace, layout.size, stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!align_size(
                coded_total + active_total, layout.alignment, &level_offset)) {
            return ENC_OVERFLOW;
        }
        levels = (int16_t *)(void *)(workspace_bytes + level_offset);
        plane_levels[0U] = levels;
        plane_levels[1U] =
            plane_levels[0U] + layout.transform_count[0U];
        plane_levels[2U] =
            plane_levels[1U] + layout.transform_count[1U];
        if (!enc_size_add(
                layout.transform_count[0U], layout.transform_count[1U],
                &buffer_offset) ||
            !enc_size_add(
                buffer_offset, layout.transform_count[2U],
                &buffer_offset) ||
            !enc_size_multiply(
                buffer_offset, sizeof(int16_t), &buffer_offset) ||
            !enc_size_add(level_offset, buffer_offset, &buffer_offset)) {
            return ENC_OVERFLOW;
        }
        tile = workspace_bytes + buffer_offset;
        frame_payload = tile + layout.stream_capacity;
        staged_stream = frame_payload + layout.stream_capacity;
        {
            EncM3ColorSearchState search;
            size_t sb_y;
            size_t sb_x;
            search.input = input;
            search.layout = &layout;
            search.q[0U] = qlookup(qindex);
            search.q[1U] = qlookup_delta(qindex, u_qdelta);
            search.q[2U] = qlookup_delta(qindex, v_qdelta);
            search.result = &candidate;
            search.luma_mode = luma_mode;
            search.chroma_mode = chroma_mode;
            search.fast_search = luma_mode != 0U || chroma_mode != 0U;
            search.fast_luma_search = 0;
            for (plane = 0U; plane < 3U; ++plane) {
                search.padded[plane] = padded[plane];
                search.levels[plane] = plane_levels[plane];
                search.transform_index[plane] = 0U;
            }
            for (sb_y = 0U; sb_y < input->height; sb_y += 64U) {
                for (sb_x = 0U; sb_x < input->width; sb_x += 64U) {
                    size_t row = sb_y / 4U;
                    size_t column = sb_x / 4U;
                    size_t chroma_rows =
                        ((input->height + 15U) / 16U) * 4U;
                    size_t chroma_columns =
                        ((input->width + 15U) / 16U) * 4U;
                    size_t clear_index;
                    search.superblock_x = sb_x;
                    search.superblock_y = sb_y;
                    for (plane = 0U; plane < 3U; ++plane) {
                        for (clear_index = 0U;
                             clear_index < 32U; ++clear_index) {
                            search.reconstructed_cells[plane][clear_index] =
                                0U;
                        }
                    }
                    search_luma_partition(
                        &search, row, column, 16U, 16U);
                    if (chroma_columns - column > 8U &&
                        chroma_rows - row > 8U) {
                        search_chroma_partition(
                            &search, row, column, 16U, 8U);
                        search_chroma_partition(
                            &search, row + 8U, column, 16U, 8U);
                    } else {
                        search_chroma_partition(
                            &search, row, column, 16U, 16U);
                    }
                }
            }
            for (plane = 0U; plane < 3U; ++plane) {
                transform_index[plane] = search.transform_index[plane];
            }
        }
        for (plane = 0U; plane < 3U; ++plane) {
            if (transform_index[plane] != layout.transform_count[plane]) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        if (saved_u_levels != 0 || saved_v_levels != 0) {
            if (saved_u_levels == 0 || saved_v_levels == 0 ||
                saved_level_capacity < layout.transform_count[1U]) {
                return ENC_OUT_OF_SPACE;
            }
            for (plane = 0U; plane < layout.transform_count[1U]; ++plane) {
                saved_u_levels[plane] = plane_levels[1U][plane];
                saved_v_levels[plane] = plane_levels[2U][plane];
            }
        }
        for (plane = 0U; plane < 3U; ++plane) {
            size_t y;
            candidate.plane_size[plane] = layout.plane_size[plane];
            candidate.transform_count[plane] = layout.transform_count[plane];
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                size_t x;
                for (x = 0U; x < layout.plane_width[plane]; ++x) {
                    staged[plane][y * layout.plane_width[plane] + x] =
                        padded[plane][
                            y * layout.coded_plane_width[plane] + x];
                }
            }
        }
        if (qdelta_enabled) {
            EncQuantizerDeltaConfig config;
            EncFrameQuantizerDeltas deltas;
            qdelta_config(&config, u_qdelta != v_qdelta);
            deltas.y_dc = 0;
            deltas.u_dc = (int16_t)u_qdelta;
            deltas.u_ac = (int16_t)u_qdelta;
            deltas.v_dc = (int16_t)v_qdelta;
            deltas.v_ac = (int16_t)v_qdelta;
            status = enc_m3_sequence_header_for_qdelta(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format, &config);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_qdelta_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex, &config, &deltas);
        } else {
            status = enc_m3_sequence_header_for_format(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_q_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex);
        }
        if (status != ENC_OK) return status;
        status = (luma_mode == 0U && chroma_mode == 0U)
            ? enc_m3_dc_yuv_grid_tile_q(
            tile, layout.stream_capacity, &tile_size,
            input->width, input->height,
            plane_levels[0U], plane_levels[1U], plane_levels[2U],
            layout.transform_count[0U],
            (unsigned int)input->format, qindex)
            : enc_m3_intra_yuv_grid_tile_q(
                tile, layout.stream_capacity, &tile_size,
                input->width, input->height,
                plane_levels[0U], plane_levels[1U], plane_levels[2U],
                layout.transform_count[0U],
                &luma_mode, 1U, &chroma_mode, 1U,
                (unsigned int)input->format, qindex);
        if (status != ENC_OK) return status;
        enc_byte_writer_init(
            &payload_writer, frame_payload, layout.stream_capacity);
        if (enc_byte_writer_write(
                &payload_writer, frame_header, frame_header_size) != ENC_OK ||
            enc_byte_writer_write(
                &payload_writer, tile, tile_size) != ENC_OK) {
            return payload_writer.status;
        }
        enc_byte_writer_init(
            &writer, staged_stream, layout.stream_capacity);
        if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
            enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
            enc_obu_write(
                &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
            return writer.status;
        }
        if (writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            size_t y;
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                status = enc_memory_copy(
                    reconstruction->planes[plane].data +
                        y * reconstruction->planes[plane].stride,
                    reconstruction->planes[plane].capacity -
                        y * reconstruction->planes[plane].stride,
                    staged[plane] + y * layout.plane_width[plane],
                    layout.plane_width[plane]);
                if (status != ENC_OK) return status;
            }
        }
        status = enc_memory_copy(
            stream, stream_capacity, staged_stream, writer.position);
        if (status != ENC_OK) return status;
        candidate.stream_size = writer.position;
        candidate.workspace_size = layout.size;
        candidate.qindex = qindex;
        candidate.format = input->format;
        candidate.luma_mode = luma_mode;
        candidate.chroma_mode = chroma_mode;
        candidate.u_qdelta = (int8_t)u_qdelta;
        candidate.v_qdelta = (int8_t)v_qdelta;
        *result = candidate;
        return ENC_OK;
    }

static EncStatus encode_yuv_qdelta(
        const EncM3Image8 *input, unsigned int qindex,
        int qdelta_enabled, int u_qdelta, int v_qdelta,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[16U];
        uint8_t tile[224U];
        uint8_t frame_payload[232U];
        uint8_t *staged_reconstruction = (uint8_t *)workspace;
        uint8_t *staged_stream;
        EncByteWriter payload_writer;
        EncByteWriter writer;
        size_t input_span[3U];
        size_t output_span[3U];
        size_t offsets[3U] = {0U, 0U, 0U};
        size_t sequence_size;
        size_t frame_header_size;
        size_t tile_size;
        size_t plane;
        uint32_t plane_q[3U];
        EncStatus status;
        clear_chroma_result(result);
        clear_chroma_result(&candidate);
        if (input == 0 || reconstruction == 0 || result == 0 ||
            workspace == 0 || (stream == 0 && stream_capacity != 0U) ||
            qindex < ENC_M3_MIN_QINDEX || qindex > ENC_M3_MAX_QINDEX ||
            input->format != reconstruction->format ||
            input->width != reconstruction->width ||
            input->height != reconstruction->height) {
            return ENC_INVALID_ARGUMENT;
        }
        if (input->format == ENC_M3_PIXEL_I420 &&
            (input->width != 8U || input->height != 8U) &&
            input->width < 16U && input->height < 16U &&
            ((input->width + 7U) & ~(size_t)7U) ==
                ((input->height + 7U) & ~(size_t)7U)) {
            return encode_i420_single_leaf(
                input, qindex, qdelta_enabled, u_qdelta, v_qdelta,
                workspace, workspace_capacity,
                stream, stream_capacity, reconstruction, result);
        }
        if (input->width != 8U || input->height != 8U) {
            return encode_even_chroma_multileaf(
                input, qindex, 0U, 0U,
                qdelta_enabled, u_qdelta, v_qdelta,
                workspace, workspace_capacity,
                stream, stream_capacity, reconstruction, result,
                0, 0, 0U);
        }
        status = enc_m3_query_chroma_workspace(
            input->format, input->width, input->height, &layout);
        if (status != ENC_OK) return status;
        if (workspace_capacity < layout.size) return ENC_OUT_OF_SPACE;
        staged_stream = staged_reconstruction + layout.reconstruction_size;
        offsets[1] = layout.plane_size[0];
        offsets[2] = offsets[1] + layout.plane_size[1];
        for (plane = 0U; plane < 3U; ++plane) {
            if (input->planes[plane].data == 0 ||
                reconstruction->planes[plane].data == 0 ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    input->planes[plane].stride, &input_span[plane]) ||
                !plane_span_size(
                    layout.plane_width[plane], layout.plane_height[plane],
                    reconstruction->planes[plane].stride,
                    &output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
            if (input->planes[plane].capacity < input_span[plane] ||
                reconstruction->planes[plane].capacity < output_span[plane]) {
                return ENC_OUT_OF_SPACE;
            }
            if (ranges_overlap(
                    workspace, layout.size,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    workspace, layout.size,
                    reconstruction->planes[plane].data, output_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    input->planes[plane].data, input_span[plane]) ||
                ranges_overlap(
                    stream, stream_capacity,
                    reconstruction->planes[plane].data, output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        for (plane = 0U; plane < 3U; ++plane) {
            size_t other;
            for (other = plane + 1U; other < 3U; ++other) {
                if (ranges_overlap(
                        reconstruction->planes[plane].data, output_span[plane],
                        reconstruction->planes[other].data, output_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
                if (ranges_overlap(
                        reconstruction->planes[plane].data, output_span[plane],
                        input->planes[other].data, input_span[other]) ||
                    ranges_overlap(
                        reconstruction->planes[other].data, output_span[other],
                        input->planes[plane].data, input_span[plane])) {
                    return ENC_INVALID_ARGUMENT;
                }
            }
        }
        if (ranges_overlap(
                workspace, layout.size, stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        for (plane = 0U; plane < 3U; ++plane) {
            if (ranges_overlap(
                    reconstruction->planes[plane].data, output_span[plane],
                    input->planes[plane].data, input_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        plane_q[0U] = qlookup(qindex);
        plane_q[1U] = qlookup_delta(qindex, u_qdelta);
        plane_q[2U] = qlookup_delta(qindex, v_qdelta);
        for (plane = 0U; plane < 3U; ++plane) {
            uint8_t value;
            uint64_t error;
            int16_t level;
            size_t y;
            size_t x;
            search_chroma_plane(
                &input->planes[plane],
                layout.plane_width[plane], layout.plane_height[plane],
                plane_q[plane],
                plane == 0U || input->format == ENC_M3_PIXEL_I444 ? 11U : 10U,
                &level, &value, &error);
            candidate.dc_level[plane] = level;
            candidate.reconstructed_sample[plane] = value;
            candidate.squared_error[plane] = error;
            candidate.plane_size[plane] = layout.plane_size[plane];
            candidate.transform_count[plane] = 1U;
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                for (x = 0U; x < layout.plane_width[plane]; ++x) {
                    staged_reconstruction[offsets[plane] +
                        y * layout.plane_width[plane] + x] = value;
                }
            }
        }
        if (qdelta_enabled) {
            EncQuantizerDeltaConfig config;
            EncFrameQuantizerDeltas deltas;
            qdelta_config(&config, u_qdelta != v_qdelta);
            deltas.y_dc = 0;
            deltas.u_dc = (int16_t)u_qdelta;
            deltas.u_ac = (int16_t)u_qdelta;
            deltas.v_dc = (int16_t)v_qdelta;
            deltas.v_ac = (int16_t)v_qdelta;
            status = enc_m3_sequence_header_for_qdelta(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format, &config);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_qdelta_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex, &config, &deltas);
        } else {
            status = enc_m3_sequence_header_for_format(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height,
                (unsigned int)input->format);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_q_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex);
        }
        if (status != ENC_OK) return status;
        status = enc_m3_dc_i420_i444_8x8_tile_q(
            tile, sizeof(tile), &tile_size,
            candidate.dc_level[0], candidate.dc_level[1], candidate.dc_level[2],
            (unsigned int)input->format, qindex);
        if (status != ENC_OK) return status;
        enc_byte_writer_init(
            &payload_writer, frame_payload, sizeof(frame_payload));
        if (enc_byte_writer_write(
                &payload_writer, frame_header, frame_header_size) != ENC_OK ||
            enc_byte_writer_write(
                &payload_writer, tile, tile_size) != ENC_OK) {
            return payload_writer.status;
        }
        enc_byte_writer_init(
            &writer, staged_stream, layout.stream_capacity);
        if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
            enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
            enc_obu_write(
                &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
            return writer.status;
        }
        if (writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            size_t y;
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                status = enc_memory_copy(
                    reconstruction->planes[plane].data +
                        y * reconstruction->planes[plane].stride,
                    reconstruction->planes[plane].capacity -
                        y * reconstruction->planes[plane].stride,
                    staged_reconstruction + offsets[plane] +
                        y * layout.plane_width[plane],
                    layout.plane_width[plane]);
                if (status != ENC_OK) return status;
            }
        }
        status = enc_memory_copy(
            stream, stream_capacity, staged_stream, writer.position);
        if (status != ENC_OK) return status;
        candidate.stream_size = writer.position;
        candidate.workspace_size = layout.size;
        candidate.qindex = qindex;
        candidate.format = input->format;
        candidate.u_qdelta = (int8_t)u_qdelta;
        candidate.v_qdelta = (int8_t)v_qdelta;
        *result = candidate;
        return ENC_OK;
    }

EncStatus enc_m3_dc_yuv8_q_workspace(
        const EncM3Image8 *input, unsigned int qindex,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
    return encode_yuv_qdelta(
        input, qindex, 0, 0, 0,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, result);
}

EncStatus enc_m3_plane_qdelta_yuv8_q_workspace(
        const EncM3Image8 *input, unsigned int qindex,
        int u_qdelta, int v_qdelta,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
    if (u_qdelta < -64 || u_qdelta > 63 ||
        v_qdelta < -64 || v_qdelta > 63) {
        return ENC_INVALID_ARGUMENT;
    }
    return encode_yuv_qdelta(
        input, qindex, u_qdelta != 0 || v_qdelta != 0,
        u_qdelta, v_qdelta,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, result);
}

EncStatus enc_m3_dc_yuv8_q_workspace_levels(
    const EncM3Image8 *input, unsigned int qindex,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    int16_t *u_levels, int16_t *v_levels, size_t level_capacity,
    EncM3ChromaResult *result) {
    if (input == 0 || input->width == 8U || input->height == 8U) {
        return ENC_INVALID_ARGUMENT;
    }
    return encode_even_chroma_multileaf(
        input, qindex, 0U, 0U, 0, 0, 0,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, result, u_levels, v_levels, level_capacity);
}

EncStatus enc_m3_intra_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex, EncM3IntraSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncM3ChromaResult *result) {
    EncM3ChromaResult best;
    EncM3ChromaResult trial;
    uint8_t best_luma = 0U;
    uint8_t best_chroma = 0U;
    uint8_t luma;
    uint8_t chroma;
    uint8_t luma_candidates[58U];
    size_t luma_candidate_count = 0U;
    size_t luma_candidate_index;
    EncStatus status;
    if (search != ENC_M3_INTRA_SEARCH_OFF &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT &&
        search != ENC_M3_INTRA_SEARCH_PLANE_QDELTA &&
        search != ENC_M3_INTRA_SEARCH_INTRA &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) {
        return ENC_INVALID_ARGUMENT;
    }
    if (search == ENC_M3_INTRA_SEARCH_PLANE_QDELTA) {
        static const int8_t delta_values[5U] = {0, -16, -8, 8, 16};
        EncM3ChromaResult baseline;
        EncM3ChromaResult candidate;
        int best_u_delta = 0;
        int best_v_delta = 0;
        uint64_t best_u_sse = UINT64_MAX;
        uint64_t best_v_sse = UINT64_MAX;
        int chosen_u = 0;
        int chosen_v = 0;
        size_t index;
        status = enc_m3_dc_yuv8_q_workspace(
            input, qindex, workspace, workspace_capacity,
            stream, stream_capacity, reconstruction, &baseline);
        if (status != ENC_OK) return status;
        best = baseline;
        for (index = 0U; index < 5U; ++index) {
            int delta = delta_values[index];
            uint64_t candidate_sse;
            uint64_t best_sse;
            if (delta == 0) {
                candidate = baseline;
            } else {
                status = encode_yuv_qdelta(
                    input, qindex, 1, delta, delta,
                    workspace, workspace_capacity, stream, stream_capacity,
                    reconstruction, &candidate);
                if (status == ENC_OUT_OF_SPACE) continue;
                if (status != ENC_OK) return status;
            }
            if (candidate.squared_error[1U] < best_u_sse ||
                (candidate.squared_error[1U] == best_u_sse &&
                 (delta < 0 ? -delta : delta) <
                 (best_u_delta < 0 ? -best_u_delta : best_u_delta)) ||
                (candidate.squared_error[1U] == best_u_sse &&
                 (delta < 0 ? -delta : delta) ==
                 (best_u_delta < 0 ? -best_u_delta : best_u_delta) &&
                 delta < best_u_delta)) {
                best_u_sse = candidate.squared_error[1U];
                best_u_delta = delta;
            }
            if (candidate.squared_error[2U] < best_v_sse ||
                (candidate.squared_error[2U] == best_v_sse &&
                 (delta < 0 ? -delta : delta) <
                 (best_v_delta < 0 ? -best_v_delta : best_v_delta)) ||
                (candidate.squared_error[2U] == best_v_sse &&
                 (delta < 0 ? -delta : delta) ==
                 (best_v_delta < 0 ? -best_v_delta : best_v_delta) &&
                 delta < best_v_delta)) {
                best_v_sse = candidate.squared_error[2U];
                best_v_delta = delta;
            }
            if (candidate.stream_size > baseline.stream_size + 2U) continue;
            candidate_sse =
                candidate.squared_error[1U] + candidate.squared_error[2U];
            best_sse = best.squared_error[1U] + best.squared_error[2U];
            if (candidate_sse < best_sse ||
                (candidate_sse == best_sse &&
                 (candidate.stream_size < best.stream_size ||
                  (candidate.stream_size == best.stream_size &&
                   ((delta < 0 ? -delta : delta) * 2 <
                    (chosen_u < 0 ? -chosen_u : chosen_u) +
                    (chosen_v < 0 ? -chosen_v : chosen_v) ||
                    ((delta < 0 ? -delta : delta) * 2 ==
                     (chosen_u < 0 ? -chosen_u : chosen_u) +
                     (chosen_v < 0 ? -chosen_v : chosen_v) &&
                     (delta < chosen_u ||
                      (delta == chosen_u && delta < chosen_v)))))))) {
                best = candidate;
                chosen_u = delta;
                chosen_v = delta;
            }
        }
        if (best_u_delta != best_v_delta) {
            uint64_t candidate_sse;
            uint64_t best_sse;
            status = encode_yuv_qdelta(
                input, qindex, 1, best_u_delta, best_v_delta,
                workspace, workspace_capacity, stream, stream_capacity,
                reconstruction, &candidate);
            if (status == ENC_OUT_OF_SPACE) {
                return chosen_u == 0 && chosen_v == 0
                    ? enc_m3_dc_yuv8_q_workspace(
                          input, qindex, workspace, workspace_capacity,
                          stream, stream_capacity, reconstruction, result)
                    : encode_yuv_qdelta(
                          input, qindex, 1, chosen_u, chosen_v,
                          workspace, workspace_capacity,
                          stream, stream_capacity, reconstruction, result);
            }
            if (status != ENC_OK) return status;
            candidate_sse =
                candidate.squared_error[1U] + candidate.squared_error[2U];
            best_sse = best.squared_error[1U] + best.squared_error[2U];
            if (candidate.stream_size <= baseline.stream_size + 2U &&
                (candidate_sse < best_sse ||
                 (candidate_sse == best_sse &&
                  (candidate.stream_size < best.stream_size ||
                   (candidate.stream_size == best.stream_size &&
                    ((best_u_delta < 0 ? -best_u_delta : best_u_delta) +
                     (best_v_delta < 0 ? -best_v_delta : best_v_delta) <
                     (chosen_u < 0 ? -chosen_u : chosen_u) +
                     (chosen_v < 0 ? -chosen_v : chosen_v) ||
                     ((best_u_delta < 0 ? -best_u_delta : best_u_delta) +
                      (best_v_delta < 0 ? -best_v_delta : best_v_delta) ==
                      (chosen_u < 0 ? -chosen_u : chosen_u) +
                      (chosen_v < 0 ? -chosen_v : chosen_v) &&
                      (best_u_delta < chosen_u ||
                       (best_u_delta == chosen_u &&
                        best_v_delta < chosen_v))))))))) {
                best = candidate;
                chosen_u = best_u_delta;
                chosen_v = best_v_delta;
            }
        }
        return chosen_u == 0 && chosen_v == 0
            ? enc_m3_dc_yuv8_q_workspace(
                  input, qindex, workspace, workspace_capacity,
                  stream, stream_capacity, reconstruction, result)
            : encode_yuv_qdelta(
                  input, qindex, 1, chosen_u, chosen_v,
                  workspace, workspace_capacity, stream, stream_capacity,
                  reconstruction, result);
    }
    status = enc_m3_dc_yuv8_q_workspace(
        input, qindex, workspace, workspace_capacity,
        stream, stream_capacity, reconstruction, &best);
    if (status != ENC_OK || search == ENC_M3_INTRA_SEARCH_OFF) {
        if (status == ENC_OK) *result = best;
        return status;
    }
    if (search == ENC_M3_INTRA_SEARCH_DIRECTIONAL &&
        !directional_prefilter_shortlists(
            input->planes[0U].data, input->width, input->height,
            input->planes[0U].stride)) {
        *result = best;
        return ENC_OK;
    }
    if (search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT &&
        search != ENC_M3_INTRA_SEARCH_INTRA &&
        ((input->width == 8U && input->height == 8U) ||
         (input->format == ENC_M3_PIXEL_I420 &&
          input->width < 16U && input->height < 16U))) {
        *result = best;
        return ENC_OK;
    }
    if (search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL ||
        search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) {
        luma_candidate_count = full_directional_candidates(
            input->planes[0U].data, input->width, input->height,
            input->planes[0U].stride,
            search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT,
            luma_candidates);
    } else {
        uint8_t last = search == ENC_M3_INTRA_SEARCH_INTRA ? 8U : 4U;
        for (luma = 3U; luma <= last; ++luma) {
            luma_candidates[luma_candidate_count++] = luma;
        }
    }
    for (luma_candidate_index = 0U;
         luma_candidate_index < luma_candidate_count;
         ++luma_candidate_index) {
        luma = luma_candidates[luma_candidate_index];
        for (chroma = 0U; chroma == 0U; ++chroma) {
            uint64_t best_sse;
            uint64_t trial_sse;
            uint64_t best_cost;
            uint64_t trial_cost;
            if (luma == 0U && chroma == 0U) continue;
            status = encode_even_chroma_multileaf(
                input, qindex, luma, chroma,
                0, 0, 0,
                workspace, workspace_capacity, stream, stream_capacity,
                reconstruction, &trial, 0, 0, 0U);
            if (status != ENC_OK) return status;
            if (((search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL &&
                  search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) &&
                 trial.stream_size > best.stream_size) ||
                trial.squared_error[0U] > best.squared_error[0U] ||
                trial.squared_error[1U] > best.squared_error[1U] ||
                trial.squared_error[2U] > best.squared_error[2U]) {
                continue;
            }
            best_sse = best.squared_error[0U] + best.squared_error[1U] +
                       best.squared_error[2U];
            trial_sse = trial.squared_error[0U] + trial.squared_error[1U] +
                        trial.squared_error[2U];
            best_cost = (best_sse << 8U) +
                        (uint64_t)best.stream_size * 8U;
            if ((trial_sse << 8U) > best_cost) continue;
            trial_cost = (trial_sse << 8U) +
                         (uint64_t)trial.stream_size * 8U;
            if (trial_cost < best_cost ||
                (trial_cost == best_cost &&
                 (trial_sse < best_sse ||
                  (trial_sse == best_sse &&
                   trial.stream_size < best.stream_size)))) {
                best = trial;
                best_luma = luma;
                best_chroma = chroma;
            }
        }
    }
    if (search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) {
        static const uint8_t base_chroma[5U] = {
            26U, 33U, 40U, 47U, 54U
        };
        uint8_t chroma_candidates[9U];
        size_t chroma_count = 0U;
        size_t chroma_index;
        uint8_t luma_joint = 0U;
        uint8_t luma_base = 0U;
        int8_t luma_delta = 0;
        if (best_luma != 0U &&
            enc_directional_token_to_joint(best_luma, &luma_joint) &&
            enc_directional_joint_to_mode(
                luma_joint, &luma_base, &luma_delta) &&
            luma_delta != 0) {
            chroma_candidates[chroma_count++] = best_luma;
        }
        for (chroma_index = 0U; chroma_index < 5U; ++chroma_index) {
            uint8_t candidate = base_chroma[chroma_index];
            if (chroma_count == 0U || candidate != chroma_candidates[0U]) {
                chroma_candidates[chroma_count++] = candidate;
            }
        }
        for (chroma_index = 0U;
             chroma_index < chroma_count; ++chroma_index) {
            uint64_t best_sse;
            uint64_t trial_sse;
            uint64_t best_cost;
            uint64_t trial_cost;
            chroma = chroma_candidates[chroma_index];
            status = encode_even_chroma_multileaf(
                input, qindex, best_luma, chroma,
                0, 0, 0,
                workspace, workspace_capacity, stream, stream_capacity,
                reconstruction, &trial, 0, 0, 0U);
            if (status != ENC_OK) return status;
            if (trial.squared_error[0U] > best.squared_error[0U] ||
                trial.squared_error[1U] > best.squared_error[1U] ||
                trial.squared_error[2U] > best.squared_error[2U]) {
                continue;
            }
            best_sse = best.squared_error[0U] + best.squared_error[1U] +
                best.squared_error[2U];
            trial_sse = trial.squared_error[0U] + trial.squared_error[1U] +
                trial.squared_error[2U];
            best_cost = (best_sse << 8U) +
                (uint64_t)best.stream_size * 8U;
            trial_cost = (trial_sse << 8U) +
                (uint64_t)trial.stream_size * 8U;
            if (trial_cost < best_cost) {
                best = trial;
                best_chroma = chroma;
            }
        }
    }
    if (best_luma == 0U && best_chroma == 0U) {
        return enc_m3_dc_yuv8_q_workspace(
            input, qindex, workspace, workspace_capacity,
            stream, stream_capacity, reconstruction, result);
    }
    return encode_even_chroma_multileaf(
        input, qindex, best_luma, best_chroma,
        0, 0, 0,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, result, 0, 0, 0U);
}
EncStatus enc_m3_query_workspace(
    size_t width, size_t height, EncM3WorkspaceLayout *layout) {
    EncM3WorkspaceLayout candidate;
    size_t block_columns;
    size_t block_rows;
    size_t level_bytes;
    size_t stream_bytes;
    size_t size;
    if (layout == 0 || width == 0U || height == 0U) {
        return ENC_INVALID_ARGUMENT;
    }
    block_columns = width / 8U + (width % 8U != 0U);
    block_rows = height / 8U + (height % 8U != 0U);
    if (!enc_size_multiply(block_columns, 8U, &candidate.padded_width) ||
        !enc_size_multiply(block_rows, 8U, &candidate.padded_height) ||
        !enc_size_multiply(width, height, &candidate.pixel_count) ||
        !enc_size_multiply(
            candidate.padded_width, candidate.padded_height,
            &candidate.padded_pixel_count) ||
        !enc_size_multiply(block_columns, block_rows, &candidate.block_count) ||
        !enc_size_multiply(
            candidate.block_count, sizeof(int16_t), &level_bytes) ||
        !enc_size_multiply(candidate.block_count, 64U, &stream_bytes) ||
        !enc_size_add(stream_bytes, 256U, &candidate.stream_capacity) ||
        !enc_size_add(
            candidate.padded_pixel_count, candidate.pixel_count, &size) ||
        !align_size(size, ENC_M3_WORKSPACE_ALIGNMENT, &size) ||
        !enc_size_add(size, level_bytes, &size)) {
        return ENC_OVERFLOW;
    }
    if (width <= ENC_M3_LEGACY_MAX_DIMENSION &&
        height <= ENC_M3_LEGACY_MAX_DIMENSION) {
        candidate.stream_capacity = ENC_M3_LEGACY_MAX_STREAM_SIZE;
        if (!enc_size_add(size, candidate.stream_capacity, &size)) {
            return ENC_OVERFLOW;
        }
    } else {
        if (!enc_size_multiply(
                candidate.stream_capacity, 3U, &stream_bytes) ||
            !enc_size_add(size, stream_bytes, &size)) {
            return ENC_OVERFLOW;
        }
    }
    candidate.size = size;
    candidate.alignment = ENC_M3_WORKSPACE_ALIGNMENT;
    *layout = candidate;
    return ENC_OK;
}

typedef struct {
    const uint8_t *input;
    size_t width;
    size_t height;
    size_t stride;
    size_t padded_width;
    uint32_t q;
    unsigned int max_level;
    uint8_t *reconstruction;
    int16_t *levels;
    size_t level_count;
    uint64_t error;
    uint8_t mode;
    size_t superblock_x;
    size_t superblock_y;
    uint8_t reconstructed_cells[32U];
} EncM3SearchState;

static void intra_prediction8(
    uint8_t mode, const uint8_t *above, size_t above_count,
    const uint8_t *left, size_t left_count,
    uint8_t corner, uint8_t prediction[64U]) {
    enc_m3_intra_prediction(
        mode, 0U,
        above, above_count, left, left_count,
        corner, 8U, 8U, prediction);
}

static uint64_t predictive_block_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, const uint8_t prediction[64U],
    int residual, int active_only, uint8_t reconstruction[64U]) {
    uint64_t error = 0U;
    size_t limit_width = active_only && block_x + 8U > width
        ? width - block_x : 8U;
    size_t limit_height = active_only && block_y + 8U > height
        ? height - block_y : 8U;
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int value = clip_int(0, 255, (int)prediction[y * 8U + x] + residual);
            size_t source_x = block_x + x < width
                ? block_x + x : width - 1U;
            size_t source_y = block_y + y < height
                ? block_y + y : height - 1U;
            if (reconstruction != 0) {
                reconstruction[y * 8U + x] = (uint8_t)value;
            }
            if (y < limit_height && x < limit_width) {
                int difference =
                    (int)input[source_y * stride + source_x] - value;
                error += (uint64_t)(difference * difference);
            }
        }
    }
    return error;
}

static void search_predicted_transform(
    EncM3SearchState *state, size_t block_x, size_t block_y,
    const uint8_t prediction[64U], int non_dc) {
    uint8_t best_reconstruction[64U];
    int best_level = 0;
    uint64_t best_error;
    int level;
    size_t y;
    size_t x;
    best_error = predictive_block_error(
        state->input, state->width, state->height, state->stride,
        block_x, block_y, prediction, 0, 0, best_reconstruction);
    if (non_dc && best_error != 0U) {
        int64_t difference_sum = 0;
        int low = -(int)state->max_level;
        int high = (int)state->max_level;
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                size_t source_x = block_x + x < state->width
                    ? block_x + x : state->width - 1U;
                size_t source_y = block_y + y < state->height
                    ? block_y + y : state->height - 1U;
                difference_sum +=
                    (int)state->input[source_y * state->stride + source_x] -
                    (int)prediction[y * 8U + x];
            }
        }
        while (low + 1 < high) {
            int middle = low + (high - low) / 2;
            if ((int64_t)dc_residual(middle, state->q) * 64 <
                difference_sum) {
                low = middle;
            } else {
                high = middle;
            }
        }
        low = low - 2 < -(int)state->max_level
            ? -(int)state->max_level : low - 2;
        high = high + 2 > (int)state->max_level
            ? (int)state->max_level : high + 2;
        for (level = low; level <= high; ++level) {
            int residual = dc_residual(level, state->q);
            uint64_t error = predictive_block_error(
                state->input, state->width, state->height, state->stride,
                block_x, block_y, prediction, residual, 0, 0);
            if (error < best_error) {
                best_error = error;
                best_level = level;
                (void)predictive_block_error(
                    state->input, state->width, state->height, state->stride,
                    block_x, block_y, prediction, residual, 0,
                    best_reconstruction);
            }
        }
    } else for (level = 1;
         best_error != 0U && level <= (int)state->max_level; ++level) {
        int residual = dc_residual(level, state->q);
        uint64_t error = predictive_block_error(
            state->input, state->width, state->height, state->stride,
            block_x, block_y, prediction, residual, 0, 0);
        if (error < best_error) {
            best_error = error;
            best_level = level;
            (void)predictive_block_error(
                state->input, state->width, state->height, state->stride,
                block_x, block_y, prediction, residual, 0,
                best_reconstruction);
        }
        residual = dc_residual(-level, state->q);
        error = predictive_block_error(
            state->input, state->width, state->height, state->stride,
            block_x, block_y, prediction, residual, 0, 0);
        if (error < best_error) {
            best_error = error;
            best_level = -level;
            (void)predictive_block_error(
                state->input, state->width, state->height, state->stride,
                block_x, block_y, prediction, residual, 0,
                best_reconstruction);
        }
    }
    state->levels[state->level_count++] = (int16_t)best_level;
    state->error += predictive_block_error(
        state->input, state->width, state->height, state->stride,
        block_x, block_y, prediction, dc_residual(best_level, state->q),
        1, 0);
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            state->reconstruction[
                (block_y + y) * state->padded_width + block_x + x] =
                best_reconstruction[y * 8U + x];
        }
    }
    search_mark_reconstructed(
        state->superblock_x, state->superblock_y, 64U,
        state->reconstructed_cells, block_x, block_y, 8U, 8U);
}

static void search_transform(
    EncM3SearchState *state, size_t block_x, size_t block_y,
    uint8_t actual_mode, int extended_edges_unavailable) {
    uint8_t above_storage[17U];
    uint8_t left[16U];
    uint8_t *above = &above_storage[1U];
    const uint8_t *above_edge = 0;
    const uint8_t *left_edge = 0;
    uint8_t corner = 128U;
    uint8_t prediction[64U];
    size_t edge;
    size_t above_count = 0U;
    size_t left_count = 0U;
    if (block_y != 0U) {
        size_t limit = actual_mode >= 5U && actual_mode <= 7U &&
                extended_edges_unavailable
            ? 8U : 16U;
        for (edge = 0U; edge < limit; ++edge) {
            size_t edge_x = block_x + edge < state->width
                ? block_x + edge : state->width - 1U;
            if (actual_mode >= 5U && actual_mode <= 7U &&
                !search_sample_reconstructed(
                    state->superblock_x, state->superblock_y, 64U,
                    state->reconstructed_cells, edge_x, block_y - 1U)) {
                break;
            }
            above[edge] = state->reconstruction[
                (block_y - 1U) * state->padded_width + edge_x];
        }
        above_count = edge;
        above_edge = above_count == 0U ? 0 : above;
        above[-1] = above_edge == 0 ? 128U :
            block_x == 0U ? above[0U] :
                state->reconstruction[
                    (block_y - 1U) * state->padded_width + block_x - 1U];
    }
    if (block_x != 0U) {
        size_t limit = actual_mode >= 5U && actual_mode <= 6U &&
                !extended_edges_unavailable &&
                block_y + 8U < state->height
            ? 9U :
            actual_mode >= 5U && actual_mode <= 7U ? 8U : 16U;
        for (edge = 0U; edge < limit; ++edge) {
            size_t edge_y = block_y + edge < state->height
                ? block_y + edge
                : state->height - 1U;
            if (actual_mode >= 5U && actual_mode <= 7U &&
                !search_sample_reconstructed(
                    state->superblock_x, state->superblock_y, 64U,
                    state->reconstructed_cells, block_x - 1U, edge_y)) {
                break;
            }
            left[edge] = state->reconstruction[
                edge_y * state->padded_width + block_x - 1U];
        }
        left_count = edge;
        left_edge = left_count == 0U ? 0 : left;
    }
    corner = above_edge != 0 && left_edge != 0 ? above[-1] :
             above_edge != 0 ? above[0U] :
             left_edge != 0 ? left[0U] : 128U;
    intra_prediction8(
        actual_mode, above_edge, above_count,
        left_edge, left_count, corner, prediction);
    search_predicted_transform(
        state, block_x, block_y, prediction, actual_mode != 0U);
}

static int search_forced_axis(
    size_t mi_rows, size_t mi_columns, size_t row, size_t column,
    size_t width, size_t height) {
    int has_rows = row + height / 2U < mi_rows;
    int has_columns = column + width / 2U < mi_columns;
    if (has_rows && has_columns) return 0;
    if (width == height) return has_rows && !has_columns ? 2 : 1;
    if (height > width) {
        if (!has_rows) return 1;
        if (!has_columns && width >= 4U &&
            column + width / 4U >= mi_columns) return 1;
    } else {
        if (!has_columns) return 2;
        if (!has_rows && height >= 4U &&
            row + height / 4U >= mi_rows) return 2;
    }
    return 0;
}

static void search_large_partition(
    EncM3SearchState *state, size_t row, size_t column,
    size_t width, size_t height) {
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    int axis;
    size_t y;
    size_t x;
    if (row >= mi_rows || column >= mi_columns) return;
    axis = search_forced_axis(
        mi_rows, mi_columns, row, column, width, height);
    if (axis == 1) {
        search_large_partition(state, row, column, width, height / 2U);
        search_large_partition(
            state, row + height / 2U, column, width, height / 2U);
        return;
    }

    if (axis == 2) {
        search_large_partition(state, row, column, width / 2U, height);
        search_large_partition(
            state, row, column + width / 2U, width / 2U, height);
        return;
    }
    if (width <= 4U && height <= 4U) {
        uint8_t mode = state->mode == 3U
            ? (row == 0U ? 0U : 1U)
            : state->mode == 4U
                ? (column == 0U ? 0U : 2U) : state->mode;
        for (y = 0U; y < height * 4U; y += 8U) {
            for (x = 0U; x < width * 4U; x += 8U) {
                search_transform(
                    state, column * 4U + x, row * 4U + y, mode,
                    0);
            }
        }
        return;
    }
    axis = height >= width ? 1 : 2;
    if (axis == 1) {
        search_large_partition(state, row, column, width, height / 2U);
        search_large_partition(
            state, row + height / 2U, column, width, height / 2U);
    } else {
        search_large_partition(state, row, column, width / 2U, height);
        search_large_partition(
            state, row, column + width / 2U, width / 2U, height);
    }
}

static void search_tile_superblocks(EncM3SearchState *state) {
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    size_t row;
    size_t column;
    for (row = 0U; row < mi_rows; row += 16U) {
        for (column = 0U; column < mi_columns; column += 16U) {
            size_t index;
            state->superblock_x = column * 4U;
            state->superblock_y = row * 4U;
            for (index = 0U; index < 32U; ++index) {
                state->reconstructed_cells[index] = 0U;
            }
            search_large_partition(state, row, column, 16U, 16U);
        }
    }
}

static EncStatus encode_i400_mode(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, uint8_t mode,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    uint8_t legacy_tile[224U];
    uint8_t legacy_frame_payload[232U];
    uint8_t *tile = legacy_tile;
    uint8_t *frame_payload = legacy_frame_payload;
    size_t tile_capacity = sizeof(legacy_tile);
    size_t frame_payload_capacity = sizeof(legacy_frame_payload);
    uint8_t *workspace_bytes = (uint8_t *)workspace;
    uint8_t *padded_reconstruction;
    uint8_t *staged_reconstruction;
    uint8_t *staged_stream;
    int16_t *levels;
    EncM3WorkspaceLayout layout;
    EncByteWriter payload_writer;
    EncByteWriter writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    uint64_t total_error = 0U;
    size_t block_columns;
    size_t block_count;
    size_t pixel_count;
    size_t levels_offset;
    size_t stream_offset;
    size_t last_input_row;
    size_t last_input_offset;
    size_t input_size;
    size_t block;
    uint32_t q;
    unsigned int max_level;
    EncStatus status;
    clear_result(result);
    if (input == 0 ||
        width < ENC_M3_MIN_DIMENSION ||
        height < ENC_M3_MIN_DIMENSION ||
        stride < width ||
        workspace == 0 ||
        (uintptr_t)workspace % ENC_M3_WORKSPACE_ALIGNMENT != 0U ||
        (stream == 0 && stream_capacity != 0U) ||
        reconstruction == 0 ||
        result == 0 ||
        mode > ENC_DIRECTIONAL_LAST_TOKEN ||
        qindex < ENC_M3_MIN_QINDEX || qindex > ENC_M3_MAX_QINDEX) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_m3_query_workspace(width, height, &layout);
    if (status != ENC_OK) return status;
    if (!enc_size_multiply(height - 1U, stride, &last_input_row) ||
        !enc_size_add(last_input_row, width - 1U, &last_input_offset) ||
        !enc_size_add(last_input_offset, 1U, &input_size)) {
        return ENC_OVERFLOW;
    }
    if (width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
        return ENC_UNSUPPORTED;
    }
    pixel_count = layout.pixel_count;
    if (reconstruction_capacity < pixel_count) return ENC_OUT_OF_SPACE;
    if (workspace_capacity < layout.size) return ENC_OUT_OF_SPACE;
    if (ranges_overlap(workspace, layout.size, input, input_size) ||
        ranges_overlap(workspace, layout.size, stream, stream_capacity) ||
        ranges_overlap(
            workspace, layout.size, reconstruction, pixel_count) ||
        ranges_overlap(stream, stream_capacity, reconstruction, pixel_count)) {
        return ENC_INVALID_ARGUMENT;
    }
    padded_reconstruction = workspace_bytes;
    staged_reconstruction =
        padded_reconstruction + layout.padded_pixel_count;
    if (!align_size(
            layout.padded_pixel_count + layout.pixel_count,
            ENC_M3_WORKSPACE_ALIGNMENT, &levels_offset) ||
        !enc_size_multiply(
            layout.block_count, sizeof(int16_t), &stream_offset) ||
        !enc_size_add(levels_offset, stream_offset, &stream_offset)) {
        return ENC_OVERFLOW;
    }
    levels = (int16_t *)(void *)(workspace_bytes + levels_offset);
    staged_stream = workspace_bytes + stream_offset;
    if (width > ENC_M3_LEGACY_MAX_DIMENSION ||
        height > ENC_M3_LEGACY_MAX_DIMENSION) {
        tile = staged_stream;
        tile_capacity = layout.stream_capacity;
        frame_payload = staged_stream + layout.stream_capacity;
        frame_payload_capacity = layout.stream_capacity;
        staged_stream += layout.stream_capacity * 2U;
    }
    q = qlookup(qindex);
    max_level = (unsigned int)(
        (UINT32_C(32768) * 8U + q - 1U) / q + 1U);
    block_columns = width / 8U + (width % 8U != 0U);
    block_count = layout.block_count;
    if (width > ENC_M3_LEGACY_MAX_DIMENSION ||
        height > ENC_M3_LEGACY_MAX_DIMENSION) {
        EncM3SearchState search;
        search.input = input;
        search.width = width;
        search.height = height;
        search.stride = stride;
        search.padded_width = layout.padded_width;
        search.q = q;
        search.max_level = max_level;
        search.reconstruction = padded_reconstruction;
        search.levels = levels;
        search.level_count = 0U;
        search.error = 0U;
        search.mode = mode;
        search_tile_superblocks(&search);
        if (search.level_count != block_count) return ENC_INVALID_ARGUMENT;
        total_error = search.error;
    } else if (mode != 0U) {
        EncM3SearchState search;
        size_t index;
        search.input = input;
        search.width = width;
        search.height = height;
        search.stride = stride;
        search.padded_width = layout.padded_width;
        search.q = q;
        search.max_level = max_level;
        search.reconstruction = padded_reconstruction;
        search.levels = levels;
        search.level_count = 0U;
        search.error = 0U;
        search.mode = mode;
        search.superblock_x = 0U;
        search.superblock_y = 0U;
        for (index = 0U; index < 32U; ++index) {
            search.reconstructed_cells[index] = 0U;
        }
        for (block = 0U; block < block_count; ++block) {
            size_t block_x = (block % block_columns) * 8U;
            size_t block_y = (block / block_columns) * 8U;
            search_transform(
                &search, block_x, block_y,
                mode == 3U ? (block_y == 0U ? 0U : 1U) :
                mode == 4U ? (block_x == 0U ? 0U : 2U) : mode, 0);
        }
        total_error = search.error;
    } else for (block = 0U; block < block_count; ++block) {
        size_t block_x = (block % block_columns) * 8U;
        size_t block_y = (block / block_columns) * 8U;
        uint8_t above[8U];
        uint8_t left[8U];
        const uint8_t *above_edge = 0;
        const uint8_t *left_edge = 0;
        uint8_t prediction;
        int best_level = 0;
        uint8_t best_value;
        uint64_t best_error;
        int level;
        size_t edge;
        size_t y;
        size_t x;
        if (block_y != 0U) {
            for (edge = 0U; edge < 8U; ++edge) {
                above[edge] =
                    padded_reconstruction[
                        (block_y - 1U) * layout.padded_width +
                        block_x + edge];
            }
            above_edge = above;
        }
        if (block_x != 0U) {
            for (edge = 0U; edge < 8U; ++edge) {
                left[edge] =
                    padded_reconstruction[
                        (block_y + edge) * layout.padded_width +
                        block_x - 1U];
            }
            left_edge = left;
        }
        prediction = enc_m3_dc_prediction(above_edge, left_edge, 8U);
        best_value = prediction;
        best_error = block_error(
            input, width, height, stride, block_x, block_y, best_value);
        for (level = 1; level <= (int)max_level; ++level) {
            uint8_t value = reconstructed_value(level, prediction, q);
            uint64_t error = block_error(
                input, width, height, stride, block_x, block_y, value);
            if (error < best_error) {
                best_error = error;
                best_level = level;
                best_value = value;
            }
            value = reconstructed_value(-level, prediction, q);
            error = block_error(
                input, width, height, stride, block_x, block_y, value);
            if (error < best_error) {
                best_error = error;
                best_level = -level;
                best_value = value;
            }
        }
        levels[block] = (int16_t)best_level;
        total_error += active_block_error(
            input, width, height, stride, block_x, block_y, best_value);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                padded_reconstruction[
                    (block_y + y) * layout.padded_width + block_x + x] =
                    best_value;
            }
        }
    }
    for (block = 0U; block < height; ++block) {
        size_t x;
        for (x = 0U; x < width; ++x) {
            staged_reconstruction[block * width + x] =
                padded_reconstruction[block * layout.padded_width + x];
        }
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size,
        width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q_for_size(
        frame_header, sizeof(frame_header), &frame_header_size,
        width, height, qindex);
    if (status != ENC_OK) return status;
    status = mode == 0U
        ? enc_m3_dc_grid_tile_q(
              tile, tile_capacity, &tile_size,
              width, height, levels, block_count, qindex)
        : enc_m3_intra_grid_tile_q(
              tile, tile_capacity, &tile_size,
              width, height, levels, block_count, &mode, 1U, qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, frame_payload, frame_payload_capacity);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(&writer, staged_stream, layout.stream_capacity);
    if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
        return writer.status;
    }
    if (writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
    status = enc_memory_copy(
        stream, stream_capacity, staged_stream, writer.position);
    if (status != ENC_OK) return status;
    status = enc_memory_copy(
        reconstruction, reconstruction_capacity,
        staged_reconstruction, pixel_count);
    if (status != ENC_OK) return status;
    result->stream_size = writer.position;
    result->squared_error = total_error;
    result->quant_level = levels[0] < 0
        ? (unsigned int)(-(int)levels[0]) : (unsigned int)levels[0];
    result->quant_sign = levels[0] < 0 ? 1U : 0U;
    result->reconstructed_sample = staged_reconstruction[0];
    result->block_count = block_count;
    result->pixel_count = pixel_count;
    result->workspace_size = layout.size;
    result->qindex = qindex;
    result->intra_mode = mode;
    return ENC_OK;
}

EncStatus enc_m3_dc_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    return encode_i400_mode(
        input, width, height, stride, qindex, 0U,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, result);
}

EncStatus enc_m3_intra_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, EncM3IntraSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    EncM3Result trial;
    EncM3Result best;
    uint8_t best_mode = 0U;
    uint8_t mode;
    uint8_t candidates[58U];
    size_t candidate_count = 0U;
    size_t candidate_index;
    EncStatus status;
    if (search != ENC_M3_INTRA_SEARCH_OFF &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_EXACT &&
        search != ENC_M3_INTRA_SEARCH_INTRA &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL &&
        search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) {
        return ENC_INVALID_ARGUMENT;
    }
    status = encode_i400_mode(
        input, width, height, stride, qindex, 0U,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, &best);
    if (status != ENC_OK || search == ENC_M3_INTRA_SEARCH_OFF) {
        if (status == ENC_OK) *result = best;
        return status;
    }
    if (search == ENC_M3_INTRA_SEARCH_DIRECTIONAL &&
        !directional_prefilter_shortlists(
            input, width, height, stride)) {
        *result = best;
        return ENC_OK;
    }
    if (search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL ||
        search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) {
        candidate_count = full_directional_candidates(
            input, width, height, stride,
            search == ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT,
            candidates);
    } else {
        uint8_t last = search == ENC_M3_INTRA_SEARCH_INTRA ? 8U : 4U;
        for (mode = 3U; mode <= last; ++mode) {
            candidates[candidate_count++] = mode;
        }
    }
    for (candidate_index = 0U;
         candidate_index < candidate_count; ++candidate_index) {
        mode = candidates[candidate_index];
        uint64_t best_cost = (best.squared_error << 8U) +
            (uint64_t)best.stream_size * 8U;
        uint64_t trial_cost;
        status = encode_i400_mode(
            input, width, height, stride, qindex, mode,
            workspace, workspace_capacity, stream, stream_capacity,
            reconstruction, reconstruction_capacity, &trial);
        if (status != ENC_OK) return status;
        if (((search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL &&
              search != ENC_M3_INTRA_SEARCH_DIRECTIONAL_FULL_EXACT) &&
             trial.stream_size > best.stream_size) ||
            trial.squared_error > best.squared_error) {
            continue;
        }
        trial_cost = (trial.squared_error << 8U) +
            (uint64_t)trial.stream_size * 8U;
        if ((trial.squared_error << 8U) > best_cost) continue;
        if (trial_cost < best_cost ||
            (trial_cost == best_cost &&
             (trial.squared_error < best.squared_error ||
              (trial.squared_error == best.squared_error &&
               trial.stream_size < best.stream_size)))) {
            best = trial;
            best_mode = mode;
        }
    }
    status = encode_i400_mode(
        input, width, height, stride, qindex, best_mode,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, result);
    return status;
}

EncStatus enc_m3_dc_i400_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    return enc_m3_dc_i400_q_workspace(
        input, width, height, stride, ENC_M3_DEFAULT_QINDEX,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, result);
}

EncStatus enc_m3_dc_i400_q(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    uint16_t aligned_workspace[ENC_M3_MAX_WORKSPACE_SIZE / 2U];
    if (width > ENC_M3_MAX_DIMENSION ||
        height > ENC_M3_MAX_DIMENSION ||
        ((width > ENC_M3_LEGACY_MAX_DIMENSION ||
          height > ENC_M3_LEGACY_MAX_DIMENSION) &&
         (width < 17U || height < 17U))) {
        clear_result(result);
        return ENC_UNSUPPORTED;
    }
    return enc_m3_dc_i400_q_workspace(
        input, width, height, stride, qindex,
        aligned_workspace, sizeof(aligned_workspace),
        stream, stream_capacity, reconstruction, reconstruction_capacity,
        result);
}

EncStatus enc_m3_dc_i400(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    return enc_m3_dc_i400_q(
        input, width, height, stride, ENC_M3_DEFAULT_QINDEX,
        stream, stream_capacity, reconstruction, reconstruction_capacity,
        result);
}

#include "encoder/m3.h"
#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

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

static void search_chroma_rect_transform(
        const EncM3Plane8 *plane, size_t active_width, size_t active_height,
        size_t block_x, size_t block_y,
        size_t transform_width, size_t transform_height,
        uint8_t prediction, uint32_t q, unsigned int shift,
        int16_t *best_level, uint8_t *best_value,
        uint64_t *active_error) {
        unsigned int max_level =
            (unsigned int)((UINT32_C(32768) * 8U + q - 1U) / q + 1U);
        uint64_t best_coded_error = 0U;
        int rectangular = transform_width != transform_height;
        int level;
        size_t y;
        size_t x;
        *best_level = 0;
        *best_value = prediction;
        for (y = 0U; y < transform_height; ++y) {
            size_t source_y = block_y + y < active_height
                ? block_y + y : active_height - 1U;
            for (x = 0U; x < transform_width; ++x) {
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
                uint8_t candidate_value = reconstructed_chroma_rect_value(
                    candidate_level, prediction, q, shift, rectangular);
                uint64_t error = 0U;
                for (y = 0U; y < transform_height; ++y) {
                    size_t source_y = block_y + y < active_height
                        ? block_y + y : active_height - 1U;
                    for (x = 0U; x < transform_width; ++x) {
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
             y < block_y + transform_height && y < active_height; ++y) {
            for (x = block_x;
                 x < block_x + transform_width && x < active_width; ++x) {
                int difference =
                    (int)plane->data[y * plane->stride + x] -
                    (int)*best_value;
                *active_error += (uint64_t)(difference * difference);
            }
        }
    }

static EncStatus encode_i420_single_leaf(
        const EncM3Image8 *input, unsigned int qindex,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[8U];
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
        uint32_t q;
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
        q = qlookup(qindex);
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
                            block_x, block_y, transform_size, prediction, q,
                            11U, &level, &value, &error);
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
                                int difference = (int)input->planes[plane].data[
                                    active_y * input->planes[plane].stride +
                                    active_x] - (int)value;
                                error +=
                                    (uint64_t)(difference * difference);
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
        status = enc_m3_sequence_header_for_format(
            sequence, sizeof(sequence), &sequence_size,
            input->width, input->height, (unsigned int)input->format);
        if (status != ENC_OK) return status;
        status = enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            input->width, input->height, qindex);
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
        *result = candidate;
        return ENC_OK;
    }

typedef struct {
    const EncM3Image8 *input;
    const EncM3ChromaWorkspaceLayout *layout;
    uint8_t *padded[3U];
    int16_t *levels[3U];
    size_t transform_index[3U];
    uint32_t q;
    EncM3ChromaResult *result;
} EncM3ColorSearchState;

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
    size_t transform_width, size_t transform_height) {
    size_t coded_width = state->layout->coded_plane_width[plane];
    uint8_t above[16U] = {0U};
    uint8_t left[16U] = {0U};
    const uint8_t *above_edge = 0;
    const uint8_t *left_edge = 0;
    uint8_t prediction;
    uint8_t value;
    int16_t level;
    uint64_t error;
    size_t index;
    size_t y;
    size_t x;
    if (block_y != 0U) {
        for (index = 0U; index < transform_width; ++index) {
            above[index] = state->padded[plane][
                (block_y - 1U) * coded_width + block_x + index];
        }
        above_edge = above;
    }
    if (block_x != 0U) {
        for (index = 0U; index < transform_height; ++index) {
            left[index] = state->padded[plane][
                (block_y + index) * coded_width + block_x - 1U];
        }
        left_edge = left;
    }
    if (above_edge != 0 && left_edge != 0) {
        uint32_t sum = 0U;
        for (index = 0U; index < transform_width; ++index) {
            sum += above[index];
        }
        for (index = 0U; index < transform_height; ++index) {
            sum += left[index];
        }
        prediction = (uint8_t)(
            (sum + (transform_width + transform_height) / 2U) /
            (transform_width + transform_height));
    } else if (above_edge != 0) {
        prediction = enc_m3_dc_prediction(
            above_edge, 0, transform_width);
    } else if (left_edge != 0) {
        prediction = enc_m3_dc_prediction(
            0, left_edge, transform_height);
    } else {
        prediction = 128U;
    }
    search_chroma_rect_transform(
        &state->input->planes[plane],
        state->layout->plane_width[plane],
        state->layout->plane_height[plane],
        block_x, block_y, transform_width, transform_height,
        prediction, state->q,
        transform_width >= 16U && transform_height >= 16U ? 12U :
        transform_width >= 8U && transform_height >= 8U ? 11U : 10U,
        &level, &value, &error);
    index = state->transform_index[plane]++;
    state->levels[plane][index] = level;
    state->result->squared_error[plane] += error;
    if (index == 0U) {
        state->result->dc_level[plane] = level;
        state->result->reconstructed_sample[plane] = value;
    }
    for (y = 0U; y < transform_height; ++y) {
        for (x = 0U; x < transform_width; ++x) {
            state->padded[plane][
                (block_y + y) * coded_width + block_x + x] = value;
        }
    }
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
        size_t tx_y;
        size_t tx_x;
        for (tx_y = 0U; tx_y < height * 4U; tx_y += 8U) {
            for (tx_x = 0U; tx_x < width * 4U; tx_x += 8U) {
                search_color_transform(
                    state, 0U, column * 4U + tx_x,
                    row * 4U + tx_y, 8U, 8U);
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
        search_color_transform(
            state, 1U, block_x, block_y,
            transform_width, transform_height);
        search_color_transform(
            state, 2U, block_x, block_y,
            transform_width, transform_height);
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
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[8U];
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
        uint32_t q;
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
        q = qlookup(qindex);
        {
            EncM3ColorSearchState search;
            size_t sb_y;
            size_t sb_x;
            search.input = input;
            search.layout = &layout;
            search.q = q;
            search.result = &candidate;
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
        status = enc_m3_sequence_header_for_format(
            sequence, sizeof(sequence), &sequence_size,
            input->width, input->height, (unsigned int)input->format);
        if (status != ENC_OK) return status;
        status = enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            input->width, input->height, qindex);
        if (status != ENC_OK) return status;
        status = enc_m3_dc_yuv_grid_tile_q(
            tile, layout.stream_capacity, &tile_size,
            input->width, input->height,
            plane_levels[0U], plane_levels[1U], plane_levels[2U],
            layout.transform_count[0U],
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
        *result = candidate;
        return ENC_OK;
    }

EncStatus enc_m3_dc_yuv8_q_workspace(
        const EncM3Image8 *input, unsigned int qindex,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncM3ChromaResult *result) {
        EncM3ChromaWorkspaceLayout layout;
        EncM3ChromaResult candidate;
        uint8_t sequence[16U];
        uint8_t frame_header[8U];
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
        uint32_t q;
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
                input, qindex, workspace, workspace_capacity,
                stream, stream_capacity, reconstruction, result);
        }
        if (input->width != 8U || input->height != 8U) {
            return encode_even_chroma_multileaf(
                input, qindex, workspace, workspace_capacity,
                stream, stream_capacity, reconstruction, result);
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
        q = qlookup(qindex);
        for (plane = 0U; plane < 3U; ++plane) {
            uint8_t value;
            uint64_t error;
            int16_t level;
            size_t y;
            size_t x;
            search_chroma_plane(
                &input->planes[plane],
                layout.plane_width[plane], layout.plane_height[plane], q,
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
        status = enc_m3_sequence_header_for_format(
            sequence, sizeof(sequence), &sequence_size,
            input->width, input->height, (unsigned int)input->format);
        if (status != ENC_OK) return status;
        status = enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            input->width, input->height, qindex);
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
        *result = candidate;
        return ENC_OK;
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
} EncM3SearchState;

static void search_transform(
    EncM3SearchState *state, size_t block_x, size_t block_y) {
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
            above[edge] = state->reconstruction[
                (block_y - 1U) * state->padded_width + block_x + edge];
        }
        above_edge = above;
    }
    if (block_x != 0U) {
        for (edge = 0U; edge < 8U; ++edge) {
            left[edge] = state->reconstruction[
                (block_y + edge) * state->padded_width + block_x - 1U];
        }
        left_edge = left;
    }
    prediction = enc_m3_dc_prediction(above_edge, left_edge, 8U);
    best_value = prediction;
    best_error = block_error(
        state->input, state->width, state->height, state->stride,
        block_x, block_y, best_value);
    for (level = 1;
         best_error != 0U && level <= (int)state->max_level; ++level) {
        uint8_t value = reconstructed_value(level, prediction, state->q);
        uint64_t error = block_error(
            state->input, state->width, state->height, state->stride,
            block_x, block_y, value);
        if (error < best_error) {
            best_error = error;
            best_level = level;
            best_value = value;
        }
        value = reconstructed_value(-level, prediction, state->q);
        error = block_error(
            state->input, state->width, state->height, state->stride,
            block_x, block_y, value);
        if (error < best_error) {
            best_error = error;
            best_level = -level;
            best_value = value;
        }
    }
    state->levels[state->level_count++] = (int16_t)best_level;
    state->error += active_block_error(
        state->input, state->width, state->height, state->stride,
        block_x, block_y, best_value);
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            state->reconstruction[
                (block_y + y) * state->padded_width + block_x + x] =
                best_value;
        }
    }
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
        for (y = 0U; y < height * 4U; y += 8U) {
            for (x = 0U; x < width * 4U; x += 8U) {
                search_transform(state, column * 4U + x, row * 4U + y);
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
            search_large_partition(state, row, column, 16U, 16U);
        }
    }
}

EncStatus enc_m3_dc_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
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
        search_tile_superblocks(&search);
        if (search.level_count != block_count) return ENC_INVALID_ARGUMENT;
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
    status = enc_m3_dc_grid_tile_q(
        tile, tile_capacity, &tile_size,
        width, height, levels, block_count, qindex);
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
    return ENC_OK;
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

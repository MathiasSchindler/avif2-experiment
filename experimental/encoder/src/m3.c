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

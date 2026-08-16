#include "encoder/ac8x8.h"
#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static const int16_t dct8[8U][8U] = {
    {64, 64, 64, 64, 64, 64, 64, 64},
    {89, 75, 50, 18, -18, -50, -75, -89},
    {83, 35, -35, -83, -83, -35, 35, 83},
    {75, -18, -89, -50, 50, 89, 18, -75},
    {64, -64, -64, 64, 64, -64, -64, 64},
    {50, -89, 18, 75, -75, -18, 89, -50},
    {35, -83, 83, -35, -35, 83, -83, 35},
    {18, -50, 75, -89, 89, -75, 50, -18}
};

static const uint8_t scan8[64U] = {
    0U, 8U, 1U, 16U, 9U, 2U, 24U, 17U, 10U, 3U, 32U, 25U, 18U, 11U,
    4U, 40U, 33U, 26U, 19U, 12U, 5U, 48U, 41U, 34U, 27U, 20U, 13U,
    6U, 56U, 49U, 42U, 35U, 28U, 21U, 14U, 7U, 57U, 50U, 43U, 36U,
    29U, 22U, 15U, 58U, 51U, 44U, 37U, 30U, 23U, 59U, 52U, 45U,
    38U, 31U, 60U, 53U, 46U, 39U, 61U, 54U, 47U, 62U, 55U, 63U
};

static const uint16_t ac_qlookup[25U] = {
    64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U, 52U, 54U, 55U,
    57U, 59U, 60U, 62U, 64U, 66U, 68U, 70U, 72U, 74U, 76U, 78U
};

static int clip_int(int low, int high, int value) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int round_shift(int value, unsigned int bits) {
    int divisor = 1 << bits;
    int shifted = value + (divisor >> 1);
    return shifted >= 0
        ? shifted / divisor
        : -((-shifted + divisor - 1) / divisor);
}

static uint32_t qlookup(unsigned int qindex) {
    if (qindex < 25U) return ac_qlookup[qindex];
    return (uint32_t)ac_qlookup[((qindex - 1U) % 24U) + 1U]
        << ((qindex - 1U) / 24U);
}

static size_t scan_rank(size_t position) {
    size_t rank = 0U;
    while (rank < 64U && scan8[rank] != position) ++rank;
    return rank;
}

static int overlap(
    const void *a, size_t a_size, const void *b, size_t b_size) {
    uintptr_t aa = (uintptr_t)a;
    uintptr_t bb = (uintptr_t)b;
    if (a_size == 0U || b_size == 0U) return 0;
    if (a_size > UINTPTR_MAX - aa || b_size > UINTPTR_MAX - bb) return 1;
    return aa < bb + b_size && bb < aa + a_size;
}

static void clear_result(EncAc8x8Result *result) {
    if (result == 0) return;
    result->stream_size = 0U;
    result->squared_error = 0U;
    result->pixel_count = 0U;
    result->workspace_size = 0U;
    result->searched_blocks = 0U;
    result->ac_blocks = 0U;
    result->qindex = 0U;
    result->admitted = 0;
}

EncStatus enc_ac8x8_query_workspace(
    size_t width, size_t height, EncAc8x8WorkspaceLayout *layout) {
    EncM3WorkspaceLayout legacy;
    EncAc8x8WorkspaceLayout candidate;
    size_t padded;
    size_t coeff_count;
    size_t coeff_bytes;
    size_t size;
    size_t streams;
    EncStatus status;
    if (layout == 0) return ENC_INVALID_ARGUMENT;
    status = enc_m3_query_workspace(width, height, &legacy);
    if (status != ENC_OK) return status;
    if (!enc_size_multiply(legacy.padded_width, legacy.padded_height, &padded) ||
        !enc_size_multiply(legacy.block_count, 64U, &coeff_count) ||
        !enc_size_multiply(coeff_count, sizeof(int16_t), &coeff_bytes) ||
        !enc_size_multiply(legacy.stream_capacity, 4U, &streams) ||
        !enc_size_add(legacy.size, legacy.pixel_count, &size) ||
        !enc_size_add(size, legacy.stream_capacity, &size) ||
        !enc_size_add(size, padded, &size) ||
        !enc_size_add(size, ENC_M3_WORKSPACE_ALIGNMENT - 1U, &size) ||
        !enc_size_add(size, coeff_bytes, &size) ||
        !enc_size_add(size, streams, &size)) {
        return ENC_OVERFLOW;
    }
    candidate.size = size;
    candidate.alignment = ENC_M3_WORKSPACE_ALIGNMENT;
    candidate.stream_capacity = legacy.stream_capacity;
    candidate.pixel_count = legacy.pixel_count;
    candidate.block_count = legacy.block_count;
    *layout = candidate;
    return ENC_OK;
}

static uint8_t prediction(
    const uint8_t *reconstruction, size_t padded_width,
    size_t block_x, size_t block_y) {
    uint8_t above[8U];
    uint8_t left[8U];
    size_t index;
    if (block_y == 0U && block_x == 0U) return 128U;
    if (block_y != 0U) {
        for (index = 0U; index < 8U; ++index) {
            above[index] = reconstruction[
                (block_y - 1U) * padded_width + block_x + index];
        }
    }
    if (block_x != 0U) {
        for (index = 0U; index < 8U; ++index) {
            left[index] = reconstruction[
                (block_y + index) * padded_width + block_x - 1U];
        }
    }
    return enc_m3_dc_prediction(
        block_y != 0U ? above : 0, block_x != 0U ? left : 0, 8U);
}

static void source_block(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, uint8_t samples[64U]) {
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        size_t sy = block_y + y < height ? block_y + y : height - 1U;
        for (x = 0U; x < 8U; ++x) {
            size_t sx = block_x + x < width ? block_x + x : width - 1U;
            samples[y * 8U + x] = input[sy * stride + sx];
        }
    }
}

static void forward8(
    const uint8_t samples[64U], uint8_t pred, int analysis[64U]) {
    size_t v;
    size_t u;
    size_t y;
    size_t x;
    for (v = 0U; v < 8U; ++v) {
        for (u = 0U; u < 8U; ++u) {
            int sum = 0;
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    sum += ((int)samples[y * 8U + x] - (int)pred) *
                           dct8[v][y] * dct8[u][x];
                }
            }
            analysis[v * 8U + u] = round_shift(sum, 12U);
        }
    }
}

static uint64_t reconstruct8(
    const uint8_t samples[64U], const int16_t levels[64U],
    uint8_t pred, uint32_t q, uint8_t output[64U]) {
    int coeff[64U];
    int rows[64U];
    uint64_t error = 0U;
    size_t y;
    size_t x;
    size_t index;
    for (index = 0U; index < 64U; ++index) {
        int level = levels[index];
        unsigned int magnitude =
            level < 0 ? (unsigned int)(-level) : (unsigned int)level;
        uint32_t dequant = (magnitude * q + 4U) >> 3U;
        coeff[index] = clip_int(
            -32768, 32767, level < 0 ? -(int)dequant : (int)dequant);
    }
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int sum = 0;
            for (index = 0U; index < 8U; ++index) {
                sum += dct8[index][x] * coeff[y * 8U + index];
            }
            rows[y * 8U + x] =
                clip_int(-32768, 32767, round_shift(sum, 7U));
        }
    }
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int sum = 0;
            int value;
            int difference;
            for (index = 0U; index < 8U; ++index) {
                sum += dct8[index][y] * rows[index * 8U + x];
            }
            value = clip_int(
                0, 255, (int)pred +
                clip_int(-256, 255, round_shift(sum, 11U)));
            output[y * 8U + x] = (uint8_t)value;
            difference = (int)samples[y * 8U + x] - value;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

static size_t nonzero_count(const int16_t levels[64U]) {
    size_t count = 0U;
    size_t index;
    for (index = 0U; index < 64U; ++index) count += levels[index] != 0;
    return count;
}

static uint64_t rd_cost(
    uint64_t sse, size_t bits, uint32_t q) {
    uint64_t lambda = (uint64_t)q * q;
    if (lambda < 1024U) lambda = 1024U;
    return sse * 4096U + lambda * bits;
}

static int candidate_better(
    uint64_t cost, uint64_t sse, size_t rate, const int16_t levels[64U],
    uint64_t best_cost, uint64_t best_sse, size_t best_rate,
    const int16_t best[64U]) {
    size_t index;
    unsigned int magnitude = 0U;
    unsigned int best_magnitude = 0U;
    if (cost != best_cost) return cost < best_cost;
    if (sse != best_sse) return sse < best_sse;
    if (rate != best_rate) return rate < best_rate;
    for (index = 0U; index < 64U; ++index) {
        magnitude += levels[index] < 0 ?
            (unsigned int)(-levels[index]) : (unsigned int)levels[index];
        best_magnitude += best[index] < 0 ?
            (unsigned int)(-best[index]) : (unsigned int)best[index];
    }
    if (magnitude != best_magnitude) return magnitude < best_magnitude;
    for (index = 0U; index < 64U; ++index) {
        if (levels[index] != best[index]) return levels[index] < best[index];
    }
    return 0;
}

static void search_block(
    const uint8_t samples[64U], uint8_t pred, unsigned int qindex,
    int full_search, int16_t best[64U], uint8_t reconstructed[64U]) {
    int analysis[64U];
    uint8_t trial_reconstruction[64U];
    uint32_t q = qlookup(qindex);
    uint64_t best_sse;
    uint64_t best_cost;
    size_t best_rate;
    size_t positions[ENC_AC8X8_MAX_POSITIONS];
    size_t position_count = full_search ? ENC_AC8X8_MAX_POSITIONS : 1U;
    size_t index;
    forward8(samples, pred, analysis);
    for (index = 0U; index < 64U; ++index) best[index] = 0;
    for (index = 0U; index < position_count; ++index) {
        size_t candidate;
        size_t selected = 0U;
        int selected_abs = -1;
        for (candidate = 0U; candidate < 64U; ++candidate) {
            int used = 0;
            int value;
            size_t previous;
            for (previous = 0U; previous < index; ++previous) {
                if (positions[previous] == candidate) used = 1;
            }
            if (used || (!full_search && candidate != 0U)) continue;
            value = analysis[candidate] < 0 ?
                -analysis[candidate] : analysis[candidate];
            if (value > selected_abs ||
                (value == selected_abs &&
                 scan_rank(candidate) < scan_rank(selected))) {
                selected = candidate;
                selected_abs = value;
            }
        }
        positions[index] = selected;
    }
    best_sse = reconstruct8(samples, best, pred, q, reconstructed);
    if (enc_ac8x8_coeff_rate(
            best, qindex, 0U, 0U, &best_rate) != ENC_OK) {
        best_rate = SIZE_MAX / 2U;
    }
    best_cost = rd_cost(best_sse, best_rate, q);
    for (index = 0U; index < position_count; ++index) {
        size_t position = positions[index];
        int ideal = round_shift(analysis[position] * 8, 0U) / (int)q;
        int choices[6U];
        size_t choice;
        int16_t local[64U];
        uint64_t local_sse = best_sse;
        uint64_t local_cost = best_cost;
        size_t local_rate = best_rate;
        for (choice = 0U; choice < 64U; ++choice) local[choice] = best[choice];
        ideal = clip_int(
            -(int)ENC_AC8X8_LEVEL_LIMIT, (int)ENC_AC8X8_LEVEL_LIMIT, ideal);
        choices[0] = 0;
        choices[1] = ideal - 2;
        choices[2] = ideal - 1;
        choices[3] = ideal;
        choices[4] = ideal + 1;
        choices[5] = ideal + 2;
        for (choice = 0U; choice < 6U; ++choice) {
            int16_t trial[64U];
            uint64_t sse;
            uint64_t cost;
            size_t rate;
            size_t copy;
            int value = clip_int(
                -(int)ENC_AC8X8_LEVEL_LIMIT,
                (int)ENC_AC8X8_LEVEL_LIMIT, choices[choice]);
            for (copy = 0U; copy < 64U; ++copy) trial[copy] = best[copy];
            trial[position] = (int16_t)value;
            if (nonzero_count(trial) > ENC_AC8X8_MAX_NONZERO) continue;
            sse = reconstruct8(samples, trial, pred, q, trial_reconstruction);
            if (enc_ac8x8_coeff_rate(
                    trial, qindex, 0U, 0U, &rate) != ENC_OK) continue;
            cost = rd_cost(sse, rate, q);
            if (candidate_better(
                    cost, sse, rate, trial,
                    local_cost, local_sse, local_rate, local)) {
                for (copy = 0U; copy < 64U; ++copy) local[copy] = trial[copy];
                local_sse = sse;
                local_cost = cost;
                local_rate = rate;
            }
        }
        for (choice = 0U; choice < 64U; ++choice) best[choice] = local[choice];
        best_sse = local_sse;
        best_cost = local_cost;
        best_rate = local_rate;
    }
    (void)reconstruct8(samples, best, pred, q, reconstructed);
}

static uint64_t active_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, const uint8_t block[64U]) {
    uint64_t error = 0U;
    size_t y;
    size_t x;
    size_t bh = block_y + 8U <= height ? 8U : height - block_y;
    size_t bw = block_x + 8U <= width ? 8U : width - block_x;
    for (y = 0U; y < bh; ++y) {
        for (x = 0U; x < bw; ++x) {
            int difference =
                (int)input[(block_y + y) * stride + block_x + x] -
                (int)block[y * 8U + x];
            error += (uint64_t)(difference * difference);
        }

    }
    return error;
}

static int forced_axis(
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

static int find_block(
    size_t rows, size_t columns, size_t row, size_t column,
    size_t width, size_t height, size_t *ordinal,
    size_t *block_x, size_t *block_y) {
    int axis;
    size_t y;
    size_t x;
    if (row >= rows || column >= columns) return 0;
    axis = forced_axis(rows, columns, row, column, width, height);
    if (axis == 1) {
        if (find_block(
                rows, columns, row, column, width, height / 2U,
                ordinal, block_x, block_y)) return 1;
        return find_block(
            rows, columns, row + height / 2U, column, width, height / 2U,
            ordinal, block_x, block_y);
    }
    if (axis == 2) {
        if (find_block(
                rows, columns, row, column, width / 2U, height,
                ordinal, block_x, block_y)) return 1;
        return find_block(
            rows, columns, row, column + width / 2U, width / 2U, height,
            ordinal, block_x, block_y);
    }
    if (width <= 4U && height <= 4U) {
        for (y = 0U; y < height * 4U; y += 8U) {
            for (x = 0U; x < width * 4U; x += 8U) {
                if (*ordinal == 0U) {
                    *block_x = column * 4U + x;
                    *block_y = row * 4U + y;
                    return 1;
                }
                --*ordinal;
            }
        }
        return 0;
    }
    axis = height >= width ? 1 : 2;
    if (axis == 1) {
        if (find_block(
                rows, columns, row, column, width, height / 2U,
                ordinal, block_x, block_y)) return 1;
        return find_block(
            rows, columns, row + height / 2U, column, width, height / 2U,
            ordinal, block_x, block_y);
    }
    if (find_block(
            rows, columns, row, column, width / 2U, height,
            ordinal, block_x, block_y)) return 1;
    return find_block(
        rows, columns, row, column + width / 2U, width / 2U, height,
        ordinal, block_x, block_y);
}

static void block_coordinates(
    size_t ordinal, size_t width, size_t height,
    size_t *block_x, size_t *block_y) {
    size_t columns = ((width + 7U) / 8U) * 2U;
    size_t rows = ((height + 7U) / 8U) * 2U;
    size_t row;
    size_t column;
    if (width <= 16U && height <= 16U) {
        size_t block_columns = (width + 7U) / 8U;
        *block_x = (ordinal % block_columns) * 8U;
        *block_y = (ordinal / block_columns) * 8U;
        return;
    }
    for (row = 0U; row < rows; row += 16U) {
        for (column = 0U; column < columns; column += 16U) {
            if (find_block(
                    rows, columns, row, column, 16U, 16U, &ordinal,
                    block_x, block_y)) return;
        }
    }
    *block_x = 0U;
    *block_y = 0U;
}

EncStatus enc_ac8x8_i400_q_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, EncAc8x8Search search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncAc8x8Result *result) {
    EncAc8x8WorkspaceLayout layout;
    EncM3WorkspaceLayout legacy_layout;
    EncM3Result legacy_result;
    uint8_t *cursor = (uint8_t *)workspace;
    uint8_t *legacy_workspace;
    uint8_t *legacy_reconstruction;
    uint8_t *legacy_stream;
    uint8_t *padded_reconstruction;
    int16_t *levels;
    uint8_t *tile;
    uint8_t *payload;
    uint8_t *candidate_stream;
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    EncByteWriter payload_writer;
    EncByteWriter obu_writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    size_t block;
    size_t offset;
    size_t input_size;
    size_t last_row;
    uint64_t candidate_error = 0U;
    size_t ac_blocks = 0U;
    EncStatus status;
    clear_result(result);
    if (input == 0 || width == 0U || height == 0U || stride < width ||
        workspace == 0 ||
        (uintptr_t)workspace % ENC_M3_WORKSPACE_ALIGNMENT != 0U ||
        (stream == 0 && stream_capacity != 0U) || reconstruction == 0 ||
        result == 0 || qindex == 0U || qindex > 255U ||
        (search != ENC_AC8X8_SEARCH_OFF &&
         search != ENC_AC8X8_SEARCH_BOUNDED)) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_ac8x8_query_workspace(width, height, &layout);
    if (status != ENC_OK) return status;
    status = enc_m3_query_workspace(width, height, &legacy_layout);
    if (status != ENC_OK) return status;
    if (workspace_capacity < layout.size ||
        reconstruction_capacity < layout.pixel_count) {
        return ENC_OUT_OF_SPACE;
    }
    if (!enc_size_multiply(height - 1U, stride, &last_row) ||
        !enc_size_add(last_row, width, &input_size)) {
        return ENC_OVERFLOW;
    }
    if (overlap(workspace, layout.size, input, input_size) ||
        overlap(workspace, layout.size, stream, stream_capacity) ||
        overlap(workspace, layout.size, reconstruction, layout.pixel_count) ||
        overlap(stream, stream_capacity, reconstruction, layout.pixel_count)) {
        return ENC_INVALID_ARGUMENT;
    }
    legacy_workspace = cursor;
    cursor += legacy_layout.size;
    legacy_reconstruction = cursor;
    cursor += legacy_layout.pixel_count;
    legacy_stream = cursor;
    cursor += legacy_layout.stream_capacity;
    padded_reconstruction = cursor;
    cursor += legacy_layout.padded_pixel_count;
    if ((uintptr_t)cursor % ENC_M3_WORKSPACE_ALIGNMENT != 0U) ++cursor;
    levels = (int16_t *)(void *)cursor;
    cursor += legacy_layout.block_count * 64U * sizeof(int16_t);
    tile = cursor;
    cursor += legacy_layout.stream_capacity;
    payload = cursor;
    cursor += legacy_layout.stream_capacity;
    candidate_stream = cursor;
    status = enc_m3_dc_i400_q_workspace(
        input, width, height, stride, qindex,
        legacy_workspace, legacy_layout.size,
        legacy_stream, legacy_layout.stream_capacity,
        legacy_reconstruction, legacy_layout.pixel_count, &legacy_result);
    if (status != ENC_OK) return status;
    if (search == ENC_AC8X8_SEARCH_OFF) {
        if (legacy_result.stream_size > stream_capacity) return ENC_OUT_OF_SPACE;
        status = enc_memory_copy(
            stream, stream_capacity, legacy_stream, legacy_result.stream_size);
        if (status != ENC_OK) return status;
        status = enc_memory_copy(
            reconstruction, reconstruction_capacity,
            legacy_reconstruction, legacy_layout.pixel_count);
        if (status != ENC_OK) return status;
        result->stream_size = legacy_result.stream_size;
        result->squared_error = legacy_result.squared_error;
        result->pixel_count = legacy_layout.pixel_count;
        result->workspace_size = layout.size;
        result->qindex = qindex;
        return ENC_OK;
    }
    for (block = 0U; block < legacy_layout.block_count; ++block) {
        size_t block_x;
        size_t block_y;
        uint8_t samples[64U];
        uint8_t decoded[64U];
        uint8_t pred;
        block_coordinates(block, width, height, &block_x, &block_y);
        pred = prediction(
            padded_reconstruction, legacy_layout.padded_width,
            block_x, block_y);
        size_t y;
        size_t x;
        source_block(
            input, width, height, stride, block_x, block_y, samples);
        search_block(
            samples, pred, qindex,
            block < ENC_AC8X8_MAX_SEARCHED_BLOCKS,
            levels + block * 64U, decoded);
        if (block < ENC_AC8X8_MAX_SEARCHED_BLOCKS) {
            result->searched_blocks++;
        }
        for (offset = 1U; offset < 64U; ++offset) {
            if (levels[block * 64U + offset] != 0) {
                ++ac_blocks;
                break;
            }
        }
        candidate_error += active_error(
            input, width, height, stride, block_x, block_y, decoded);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                padded_reconstruction[
                    (block_y + y) * legacy_layout.padded_width +
                    block_x + x] = decoded[y * 8U + x];
            }
        }
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size, width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q_for_size(
        frame_header, sizeof(frame_header), &frame_header_size,
        width, height, qindex);
    if (status != ENC_OK) return status;
    status = enc_ac8x8_grid_tile_q(
        tile, legacy_layout.stream_capacity, &tile_size,
        width, height, levels, legacy_layout.block_count * 64U, qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, payload, legacy_layout.stream_capacity);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(
        &obu_writer, candidate_stream, legacy_layout.stream_capacity);
    if (enc_obu_write(&obu_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&obu_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &obu_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        return obu_writer.status;
    }
    result->admitted = candidate_error < legacy_result.squared_error;
    if (result->admitted) {
        if (obu_writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
        status = enc_memory_copy(
            stream, stream_capacity, candidate_stream, obu_writer.position);
        if (status != ENC_OK) return status;
        for (block = 0U; block < height; ++block) {
            status = enc_memory_copy(
                reconstruction + block * width, width,
                padded_reconstruction + block * legacy_layout.padded_width,
                width);
            if (status != ENC_OK) return status;
        }
        result->stream_size = obu_writer.position;
        result->squared_error = candidate_error;
        result->ac_blocks = ac_blocks;
    } else {
        if (legacy_result.stream_size > stream_capacity) return ENC_OUT_OF_SPACE;
        status = enc_memory_copy(
            stream, stream_capacity, legacy_stream, legacy_result.stream_size);
        if (status != ENC_OK) return status;
        status = enc_memory_copy(
            reconstruction, reconstruction_capacity,
            legacy_reconstruction, legacy_layout.pixel_count);
        if (status != ENC_OK) return status;
        result->stream_size = legacy_result.stream_size;
        result->squared_error = legacy_result.squared_error;
        result->ac_blocks = 0U;
    }
    result->pixel_count = legacy_layout.pixel_count;
    result->workspace_size = layout.size;
    result->qindex = qindex;
    return ENC_OK;
}

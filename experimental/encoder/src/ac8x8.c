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

static const int16_t dct4[4U][4U] = {
    {64, 64, 64, 64},
    {83, 35, -35, -83},
    {64, -64, -64, 64},
    {35, -83, 83, -35}
};

#include "normative_dct16.inc"

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

static EncStatus enc_ac8x8_i400_rd_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity, uint8_t *reconstruction,
    size_t reconstruction_capacity, EncAc8x8Result *result);

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

static int dct_value(size_t size, size_t frequency, size_t output) {
    return size == 4U ? dct4[frequency][output] :
           size == 8U ? dct8[frequency][output] :
                        dct16[frequency][output];
}

static size_t chroma_scan_position(
    size_t width, size_t height, size_t rank) {
    size_t diagonal;
    size_t seen = 0U;
    for (diagonal = 0U; diagonal < width + height - 1U; ++diagonal) {
        size_t row = diagonal < height ? diagonal : height - 1U;
        size_t minimum = diagonal >= width ? diagonal - width + 1U : 0U;
        for (;;) {
            if (seen == rank) return row * width + diagonal - row;
            ++seen;
            if (row == minimum) break;
            --row;
        }
    }
    return 0U;
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
    size_t index;
    if (result == 0) return;
    result->stream_size = 0U;
    result->baseline_stream_size = 0U;
    result->candidate_stream_size = 0U;
    result->squared_error = 0U;
    result->baseline_squared_error = 0U;
    result->candidate_squared_error = 0U;
    result->pixel_count = 0U;
    result->workspace_size = 0U;
    result->searched_blocks = 0U;
    result->ac_blocks = 0U;
    result->candidate_trials = 0U;
    for (index = 0U; index < 7U; ++index) {
        result->rd_mode_counts[index] = 0U;
    }
    for (index = 0U; index < 3U; ++index) {
        result->rd_residual_counts[index] = 0U;
    }
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
        !enc_size_add(size, legacy.block_count, &size) ||
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
    const uint8_t samples[64U], const uint8_t prediction[64U],
    int analysis[64U]) {
    size_t v;
    size_t u;
    size_t y;
    size_t x;
    for (v = 0U; v < 8U; ++v) {
        for (u = 0U; u < 8U; ++u) {
            int sum = 0;
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    sum += ((int)samples[y * 8U + x] -
                            (int)prediction[y * 8U + x]) *
                           dct8[v][y] * dct8[u][x];
                }
            }
            analysis[v * 8U + u] = round_shift(sum, 12U);
        }
    }
}

static uint64_t reconstruct8(
    const uint8_t samples[64U], const int16_t levels[64U],
    const uint8_t prediction[64U], uint32_t q, uint8_t output[64U]) {
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
                0, 255, (int)prediction[y * 8U + x] +
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
    const uint8_t samples[64U], const uint8_t prediction[64U],
    unsigned int qindex,
    int full_search, int search_policy,
    int16_t best[64U], uint8_t reconstructed[64U]) {
    int analysis[64U];
    uint8_t trial_reconstruction[64U];
    uint32_t q = qlookup(qindex);
    uint64_t best_sse;
    uint64_t best_cost;
    size_t best_rate;
    size_t positions[ENC_AC8X8_MAX_POSITIONS];
    int safe_positions = (search_policy & 1) != 0;
    uint32_t cost_q = (search_policy & 2) != 0 ? 1U : q;
    size_t position_count = full_search
        ? ((search_policy & 4) != 0 ? 2U :
           safe_positions ? 8U : ENC_AC8X8_MAX_POSITIONS)
        : 1U;
    size_t index;
    forward8(samples, prediction, analysis);
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
            if (used || (!full_search && candidate != 0U) ||
                (safe_positions &&
                 candidate != 0U && candidate != 1U &&
                 candidate != 2U && candidate != 8U &&
                 candidate != 9U && candidate != 10U &&
                 candidate != 16U && candidate != 17U)) {
                continue;
            }
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
    best_sse = reconstruct8(samples, best, prediction, q, reconstructed);
    if (enc_ac8x8_coeff_rate(
            best, qindex, 0U, 0U, &best_rate) != ENC_OK) {
        best_rate = SIZE_MAX / 2U;
    }
    best_cost = rd_cost(best_sse, best_rate, cost_q);
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
            sse = reconstruct8(
                samples, trial, prediction, q, trial_reconstruction);
            if (enc_ac8x8_coeff_rate(
                    trial, qindex, 0U, 0U, &rate) != ENC_OK) continue;
            cost = rd_cost(sse, rate, cost_q);
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
    (void)reconstruct8(samples, best, prediction, q, reconstructed);
}

static void constant_prediction(uint8_t value, uint8_t output[64U]) {
    size_t index;
    for (index = 0U; index < 64U; ++index) output[index] = value;
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

static int rd_sample_reconstructed(
    size_t superblock_x, size_t superblock_y,
    const uint8_t cells[32U], size_t sample_x, size_t sample_y,
    int future_superblock_unavailable) {
    size_t cell;
    if (sample_y < superblock_y || sample_x < superblock_x ||
        sample_y >= superblock_y + 64U || sample_x >= superblock_x + 64U) {
        if (future_superblock_unavailable != 0) {
            return sample_y < superblock_y ||
                (sample_y < superblock_y + 64U &&
                 sample_x < superblock_x);
        }
        return 1;
    }
    cell = ((sample_y - superblock_y) / 4U) * 16U +
           (sample_x - superblock_x) / 4U;
    return (cells[cell / 8U] >> (cell % 8U)) & 1U;
}

static void rd_mark_reconstructed(
    size_t superblock_x, size_t superblock_y, uint8_t cells[32U],
    size_t block_x, size_t block_y, size_t block_width, size_t block_height) {
    size_t y;
    size_t x;
    for (y = block_y; y < block_y + block_height; y += 4U) {
        for (x = block_x; x < block_x + block_width; x += 4U) {
            size_t cell;
            if (x < superblock_x || y < superblock_y ||
                x >= superblock_x + 64U || y >= superblock_y + 64U) {
                continue;
            }
            cell = ((y - superblock_y) / 4U) * 16U +
                   (x - superblock_x) / 4U;
            cells[cell / 8U] |= (uint8_t)(1U << (cell % 8U));
        }
    }
}

static void rd_intra_prediction(
    uint8_t mode, const uint8_t *reconstruction, size_t padded_width,
    size_t width, size_t height, size_t block_x, size_t block_y,
    size_t superblock_x, size_t superblock_y, const uint8_t cells[32U],
    size_t block_width, size_t block_height,
    int future_superblock_unavailable, uint8_t output[256U]) {
    uint8_t above_storage[33U];
    uint8_t left[32U];
    uint8_t *above = above_storage + 1U;
    const uint8_t *above_edge = 0;
    const uint8_t *left_edge = 0;
    uint8_t corner = 128U;
    size_t edge;
    size_t above_count = 0U;
    size_t left_count = 0U;
    if (block_y != 0U) {
        size_t limit = mode >= 5U && mode <= 7U
            ? 2U * block_width : block_width;
        for (edge = 0U; edge < limit; ++edge) {
            size_t edge_x = block_x + edge < width
                ? block_x + edge : width - 1U;
            if (mode >= 5U && mode <= 7U &&
                !rd_sample_reconstructed(
                    superblock_x, superblock_y, cells, edge_x, block_y - 1U,
                    future_superblock_unavailable)) {
                break;
            }
            above[edge] = reconstruction[
                (block_y - 1U) * padded_width + edge_x];
        }
        above_count = edge;
        above_edge = above_count == 0U ? 0 : above;
        above[-1] = above_edge == 0 ? 128U :
            block_x == 0U ? above[0U] :
                reconstruction[
                    (block_y - 1U) * padded_width + block_x - 1U];
    }
    if (block_x != 0U) {
        size_t limit = mode >= 5U && mode <= 6U &&
                block_y + block_height < height
            ? block_height + 1U
            : mode >= 5U && mode <= 7U ? block_height : block_height;
        for (edge = 0U; edge < limit; ++edge) {
            size_t edge_y = block_y + edge < height
                ? block_y + edge : height - 1U;
            if (mode >= 5U && mode <= 7U &&
                !rd_sample_reconstructed(
                    superblock_x, superblock_y, cells, block_x - 1U, edge_y,
                    future_superblock_unavailable)) {
                break;
            }
            left[edge] = reconstruction[edge_y * padded_width + block_x - 1U];
        }
        left_count = edge;
        left_edge = left_count == 0U ? 0 : left;
    }
    corner = above_edge != 0 && left_edge != 0 ? above[-1] :
             above_edge != 0 ? above[0U] :
             left_edge != 0 ? left[0U] : 128U;
    enc_m3_intra_prediction(
        mode, 0U, above_edge, above_count, left_edge, left_count, corner,
        block_width, block_height, output);
}

static int rd_evaluate_leaf_candidate(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    uint8_t *padded_reconstruction, size_t padded_width,
    size_t leaf_x, size_t leaf_y, size_t leaf_width, size_t leaf_height,
    size_t superblock_x, size_t superblock_y, uint8_t cells[32U],
    unsigned int qindex, int search_policy, uint8_t mode,
    size_t residual_kind, int16_t levels[4U][64U],
    uint8_t reconstruction[256U], uint64_t *squared_error) {
    uint8_t saved_reconstruction[256U];
    uint8_t saved_cells[32U];
    size_t transform_count = (leaf_width / 8U) * (leaf_height / 8U);
    uint32_t q = qlookup(qindex);
    size_t tx;
    size_t y;
    size_t x;
    if (input == 0 || padded_reconstruction == 0 || cells == 0 ||
        levels == 0 || reconstruction == 0 || squared_error == 0 ||
        leaf_width == 0U || leaf_width > 16U ||
        leaf_height == 0U || leaf_height > 16U ||
        transform_count == 0U || transform_count > 4U ||
        residual_kind > 2U) {
        return 0;
    }
    for (y = 0U; y < leaf_height; ++y) {
        for (x = 0U; x < leaf_width; ++x) {
            saved_reconstruction[y * leaf_width + x] =
                padded_reconstruction[
                    (leaf_y + y) * padded_width + leaf_x + x];
        }
    }
    for (x = 0U; x < sizeof(saved_cells); ++x) {
        saved_cells[x] = cells[x];
    }
    *squared_error = 0U;
    for (tx = 0U; tx < transform_count; ++tx) {
        size_t tx_x = (tx % (leaf_width / 8U)) * 8U;
        size_t tx_y = (tx / (leaf_width / 8U)) * 8U;
        uint8_t samples[64U];
        uint8_t prediction[256U];
        uint8_t decoded[64U];
        uint8_t unused[64U];
        int16_t ac_levels[64U];
        size_t index;
        source_block(
            input, width, height, stride,
            leaf_x + tx_x, leaf_y + tx_y, samples);
        rd_intra_prediction(
            mode, padded_reconstruction, padded_width, width, height,
            leaf_x + tx_x, leaf_y + tx_y, superblock_x, superblock_y,
            cells, 8U, 8U, 1, prediction);
        if (residual_kind != 0U) {
            search_block(
                samples, prediction, qindex, 1, search_policy,
                ac_levels, unused);
        }
        for (index = 0U; index < 64U; ++index) {
            levels[tx][index] = residual_kind == 0U ? 0 :
                residual_kind == 1U && index != 0U ? 0 : ac_levels[index];
        }
        (void)reconstruct8(samples, levels[tx], prediction, q, decoded);
        *squared_error += active_error(
            input, width, height, stride,
            leaf_x + tx_x, leaf_y + tx_y, decoded);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                padded_reconstruction[
                    (leaf_y + tx_y + y) * padded_width +
                    leaf_x + tx_x + x] = decoded[y * 8U + x];
                reconstruction[
                    (tx_y + y) * leaf_width + tx_x + x] =
                    decoded[y * 8U + x];
            }
        }
        rd_mark_reconstructed(
            superblock_x, superblock_y, cells,
            leaf_x + tx_x, leaf_y + tx_y, 8U, 8U);
    }
    for (y = 0U; y < leaf_height; ++y) {
        for (x = 0U; x < leaf_width; ++x) {
            padded_reconstruction[
                (leaf_y + y) * padded_width + leaf_x + x] =
                saved_reconstruction[y * leaf_width + x];
        }
    }
    for (x = 0U; x < sizeof(saved_cells); ++x) {
        cells[x] = saved_cells[x];
    }
    return 1;
}

/*
 * RD is intentionally integer-only: accumulated active luma SSE is scaled by
 * 4096 and added to q(qindex)^2 times exact range-writer bits. Candidates are
 * visited in this order and equal objectives never replace an earlier one:
 * DC, vertical, horizontal, Smooth, Smooth-V, Smooth-H, Paeth; for each
 * predictor skip, DC-only, and bounded AC are visited in that order.
 */
static int rd_select_leaf(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    uint8_t *padded_reconstruction, size_t padded_width,
    const uint8_t *incumbent_reconstruction, size_t incumbent_stride,
    size_t leaf_x, size_t leaf_y, size_t leaf_width, size_t leaf_height,
    size_t superblock_x, size_t superblock_y, uint8_t cells[32U],
    unsigned int qindex, int16_t *levels, uint8_t *modes,
    size_t level_index, size_t leaf_index, size_t block_count,
    size_t leaf_count, uint64_t accumulated_error, uint8_t *tile,
    size_t tile_capacity, int16_t selected_levels[4U][64U],
    uint8_t selected_reconstruction[256U], uint8_t *selected_mode,
    size_t *trial_count, uint64_t *selected_error) {
    static const uint8_t candidate_modes[7U] = {
        0U, 1U, 2U, 5U, 6U, 7U, 8U
    };
    size_t transform_count = (leaf_width / 8U) * (leaf_height / 8U);
    uint64_t best_cost = UINT64_MAX;
    int selected = 0;
    size_t mode_index;
    uint32_t q = qlookup(qindex);
    uint64_t incumbent_error = 0U;
    size_t active_height =
        leaf_y + leaf_height <= height ? leaf_height : height - leaf_y;
    size_t active_width =
        leaf_x + leaf_width <= width ? leaf_width : width - leaf_x;
    size_t y;
    size_t x;
    if (transform_count == 0U || transform_count > 4U ||
        level_index + transform_count > block_count ||
        leaf_index >= leaf_count || incumbent_reconstruction == 0 ||
        incumbent_stride < width) {
        return 0;
    }
    for (y = 0U; y < active_height; ++y) {
        for (x = 0U; x < active_width; ++x) {
            int difference =
                (int)input[(leaf_y + y) * stride + leaf_x + x] -
                (int)incumbent_reconstruction[
                    (leaf_y + y) * incumbent_stride + leaf_x + x];
            incumbent_error += (uint64_t)(difference * difference);
        }
    }
    for (mode_index = 0U; mode_index < 7U; ++mode_index) {
        size_t residual_kind;
        for (residual_kind = 0U; residual_kind < 3U; ++residual_kind) {
            int16_t trial_levels[4U][64U];
            uint8_t trial_reconstruction[256U];
            uint64_t error;
            uint64_t cost;
            size_t tile_size;
            size_t tx;
            if (!rd_evaluate_leaf_candidate(
                    input, width, height, stride,
                    padded_reconstruction, padded_width,
                    leaf_x, leaf_y, leaf_width, leaf_height,
                    superblock_x, superblock_y, cells, qindex, 1,
                    candidate_modes[mode_index], residual_kind,
                    trial_levels, trial_reconstruction, &error)) {
                continue;
            }
            for (tx = 0U; tx < transform_count; ++tx) {
                size_t index;
                for (index = 0U; index < 64U; ++index) {
                    levels[(level_index + tx) * 64U + index] =
                        trial_levels[tx][index];
                }
            }
            modes[leaf_index] = candidate_modes[mode_index];
            if (enc_ac8x8_intra_grid_tile_q(
                    tile, tile_capacity, &tile_size, width, height, levels,
                    block_count * 64U, modes, leaf_count, qindex) != ENC_OK) {
                continue;
            }
            ++*trial_count;
            if (error > incumbent_error) continue;
            cost = rd_cost(accumulated_error + error, tile_size * 8U, q);
            if (!selected || cost < best_cost) {
                size_t index;
                size_t tx;
                best_cost = cost;
                for (tx = 0U; tx < transform_count; ++tx) {
                    for (index = 0U; index < 64U; ++index) {
                        selected_levels[tx][index] = trial_levels[tx][index];
                    }
                }
                for (index = 0U; index < leaf_width * leaf_height; ++index) {
                    selected_reconstruction[index] = trial_reconstruction[index];
                }
                *selected_mode = candidate_modes[mode_index];
                *selected_error = error;
                selected = 1;
            }
        }
    }
    if (selected) {
        size_t tx;
        for (tx = 0U; tx < transform_count; ++tx) {
            size_t index;
            for (index = 0U; index < 64U; ++index) {
                levels[(level_index + tx) * 64U + index] =
                    selected_levels[tx][index];
            }
        }
        modes[leaf_index] = *selected_mode;
    }
    return selected;
}

typedef struct {
    const uint8_t *input;
    size_t width;
    size_t height;
    size_t stride;
    uint8_t *padded_reconstruction;
    size_t padded_width;
    unsigned int qindex;
    size_t superblock_x;
    size_t superblock_y;
    uint8_t reconstructed_cells[32U];
    uint64_t squared_error;
    size_t searched_blocks;
    size_t ac_blocks;
    size_t trials;
    int16_t *stored_levels;
    uint8_t *stored_modes;
    size_t stored_block_count;
    size_t stored_level_index;
    size_t stored_mode_index;
    const uint8_t *incumbent_reconstruction;
    size_t incumbent_stride;
    int replay;
    size_t mode_counts[7U];
    size_t residual_counts[3U];
} EncAc8x8RdSearchState;

static void rd_record_search_telemetry(
    EncAc8x8Result *result, const EncAc8x8RdSearchState *search) {
    size_t index;
    result->candidate_trials = search->trials;
    for (index = 0U; index < 7U; ++index) {
        result->rd_mode_counts[index] = search->mode_counts[index];
    }
    for (index = 0U; index < 3U; ++index) {
        result->rd_residual_counts[index] = search->residual_counts[index];
    }
}

static uint64_t rd_incumbent_leaf_error(
    const EncAc8x8RdSearchState *state,
    size_t leaf_x, size_t leaf_y, size_t leaf_width, size_t leaf_height) {
    uint64_t error = 0U;
    size_t active_height =
        leaf_y + leaf_height <= state->height
            ? leaf_height : state->height - leaf_y;
    size_t active_width =
        leaf_x + leaf_width <= state->width
            ? leaf_width : state->width - leaf_x;
    size_t y;
    size_t x;
    for (y = 0U; y < active_height; ++y) {
        for (x = 0U; x < active_width; ++x) {
            int difference =
                (int)state->input[(leaf_y + y) * state->stride + leaf_x + x] -
                (int)state->incumbent_reconstruction[
                    (leaf_y + y) * state->incumbent_stride + leaf_x + x];
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

/*
 * RD is integer-only: each leaf compares active SSE scaled by 4096 plus
 * q(qindex)^2 times the exact terminated warm-prefix size. Predictors are
 * visited in stable order, followed by skip, DC-only, and bounded-AC residual
 * forms; strict comparison keeps earlier candidates on ties.
 */
static EncStatus rd_select_transaction_leaf(
    void *opaque, void *transaction,
    size_t leaf_x, size_t leaf_y, size_t leaf_width, size_t leaf_height,
    uint8_t *selected_mode, int16_t selected_levels[4U][64U]) {
    EncAc8x8RdSearchState *state = (EncAc8x8RdSearchState *)opaque;
    static const uint8_t candidate_modes[7U] = {
        0U, 1U, 2U, 5U, 6U, 7U, 8U
    };
    size_t transform_count = (leaf_width / 8U) * (leaf_height / 8U);
    uint64_t best_cost = UINT64_MAX;
    uint64_t fallback_cost = UINT64_MAX;
    uint64_t selected_error = 0U;
    uint64_t fallback_error = 0U;
    uint8_t selected_reconstruction[256U];
    uint8_t fallback_reconstruction[256U];
    int16_t fallback_levels[4U][64U];
    int selected = 0;
    int fallback_selected = 0;
    size_t selected_mode_index = 0U;
    size_t selected_residual_kind = 0U;
    size_t fallback_mode_index = 0U;
    size_t fallback_residual_kind = 0U;
    uint32_t q;
    size_t mode_index;
    size_t superblock_x;
    size_t superblock_y;
    int search_policy;
    uint64_t incumbent_error;
    if (state == 0 || selected_mode == 0 || transaction == 0 ||
        transform_count == 0U || transform_count > 4U) {
        return ENC_INVALID_ARGUMENT;
    }
    if (state->replay != 0) {
        size_t superblock_x = leaf_x & ~(size_t)63U;
        size_t superblock_y = leaf_y & ~(size_t)63U;
        size_t tx;
        size_t index;
        if (state->stored_levels == 0 || state->stored_modes == 0 ||
            state->stored_level_index + transform_count >
                state->stored_block_count ||
            state->stored_mode_index >= state->stored_block_count) {
            return ENC_INVALID_ARGUMENT;
        }
        if (superblock_x != state->superblock_x ||
            superblock_y != state->superblock_y) {
            enc_memory_fill(
                state->reconstructed_cells, 0U,
                sizeof(state->reconstructed_cells));
            state->superblock_x = superblock_x;
            state->superblock_y = superblock_y;
        }
        *selected_mode = state->stored_modes[state->stored_mode_index];
        for (tx = 0U; tx < transform_count; ++tx) {
            size_t tx_x = (tx % (leaf_width / 8U)) * 8U;
            size_t tx_y = (tx / (leaf_width / 8U)) * 8U;
            const int16_t *levels;
            uint8_t samples[64U];
            uint8_t prediction[256U];
            uint8_t decoded[64U];
            size_t y;
            size_t x;
            levels = state->stored_levels +
                (state->stored_level_index + tx) * 64U;
            for (index = 0U; index < 64U; ++index) {
                selected_levels[tx][index] = levels[index];
            }
            source_block(
                state->input, state->width, state->height, state->stride,
                leaf_x + tx_x, leaf_y + tx_y, samples);
            rd_intra_prediction(
                *selected_mode, state->padded_reconstruction,
                state->padded_width, state->width, state->height,
                leaf_x + tx_x, leaf_y + tx_y,
                state->superblock_x, state->superblock_y,
                state->reconstructed_cells, 8U, 8U, 1, prediction);
            (void)reconstruct8(
                samples, selected_levels[tx], prediction,
                qlookup(state->qindex), decoded);
            state->squared_error += active_error(
                state->input, state->width, state->height, state->stride,
                leaf_x + tx_x, leaf_y + tx_y, decoded);
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    state->padded_reconstruction[
                        (leaf_y + tx_y + y) * state->padded_width +
                        leaf_x + tx_x + x] = decoded[y * 8U + x];
                }
            }
            rd_mark_reconstructed(
                state->superblock_x, state->superblock_y,
                state->reconstructed_cells,
                leaf_x + tx_x, leaf_y + tx_y, 8U, 8U);
            for (index = 1U; index < 64U; ++index) {
                if (selected_levels[tx][index] != 0) {
                    ++state->ac_blocks;
                    break;
                }
            }
        }
        state->stored_level_index += transform_count;
        ++state->stored_mode_index;
        state->searched_blocks += transform_count;
        return ENC_OK;
    }
    superblock_x = leaf_x & ~(size_t)63U;
    superblock_y = leaf_y & ~(size_t)63U;
    if (superblock_x != state->superblock_x ||
        superblock_y != state->superblock_y) {
        enc_memory_fill(
            state->reconstructed_cells, 0U, sizeof(state->reconstructed_cells));
        state->superblock_x = superblock_x;
        state->superblock_y = superblock_y;
    }
    q = qlookup(state->qindex);
    if (state->incumbent_reconstruction == 0 ||
        state->incumbent_stride < state->width) {
        return ENC_INVALID_ARGUMENT;
    }
    incumbent_error = rd_incumbent_leaf_error(
        state, leaf_x, leaf_y, leaf_width, leaf_height);
    /*
     * Keep the coefficient shortlist identical to bounded search, then
     * compare its skip, DC-only, and AC forms for every admitted predictor.
     * The transaction supplies exact terminated-prefix bits. Prefer
     * non-regressing candidates, but retain the best legal fallback so a leaf
     * with no locally non-regressing form cannot disable the complete-stream
     * candidate. Final admission remains the strict full-stream guard.
     */
    search_policy =
        (state->width > 64U || state->height > 64U ? 1 : 0) |
        (state->qindex >= 141U && state->width >= 64U &&
         state->height >= 64U ? 2 : 0);
    for (mode_index = 0U; mode_index < 7U; ++mode_index) {
        size_t residual_kind;
        for (residual_kind = 0U; residual_kind < 3U; ++residual_kind) {
            int16_t trial_levels[4U][64U];
            uint8_t trial_reconstruction[256U];
            uint64_t error;
            uint64_t cost;
            size_t tell_bits;
            if (!rd_evaluate_leaf_candidate(
                    state->input, state->width, state->height, state->stride,
                    state->padded_reconstruction, state->padded_width,
                    leaf_x, leaf_y, leaf_width, leaf_height,
                    state->superblock_x, state->superblock_y,
                    state->reconstructed_cells, state->qindex, search_policy,
                    candidate_modes[mode_index], residual_kind,
                    trial_levels, trial_reconstruction, &error)) {
                continue;
            }
            if (enc_ac8x8_rd_transaction_trial(
                    transaction, candidate_modes[mode_index],
                    &trial_levels[0U][0U],
                    transform_count, &tell_bits) != ENC_OK) {
                continue;
            }
            ++state->trials;
            cost = rd_cost(error, tell_bits, q);
            if (!fallback_selected || cost < fallback_cost) {
                size_t index;
                size_t tx;
                fallback_cost = cost;
                fallback_error = error;
                for (tx = 0U; tx < transform_count; ++tx) {
                    for (index = 0U; index < 64U; ++index) {
                        fallback_levels[tx][index] = trial_levels[tx][index];
                    }
                }
                for (index = 0U; index < leaf_width * leaf_height; ++index) {
                    fallback_reconstruction[index] =
                        trial_reconstruction[index];
                }
                fallback_mode_index = mode_index;
                fallback_residual_kind = residual_kind;
                fallback_selected = 1;
            }
            if (error > incumbent_error) continue;
            if (!selected || cost < best_cost) {
                size_t index;
                size_t tx;
                best_cost = cost;
                selected_error = error;
                for (tx = 0U; tx < transform_count; ++tx) {
                    for (index = 0U; index < 64U; ++index) {
                        selected_levels[tx][index] = trial_levels[tx][index];
                    }
                }
                for (index = 0U; index < leaf_width * leaf_height; ++index) {
                    selected_reconstruction[index] = trial_reconstruction[index];
                }
                *selected_mode = candidate_modes[mode_index];
                selected_mode_index = mode_index;
                selected_residual_kind = residual_kind;
                selected = 1;
            }
        }
    }
    if (!selected && fallback_selected) {
        size_t index;
        size_t tx;
        selected_error = fallback_error;
        for (tx = 0U; tx < transform_count; ++tx) {
            for (index = 0U; index < 64U; ++index) {
                selected_levels[tx][index] = fallback_levels[tx][index];
            }
        }
        for (index = 0U; index < leaf_width * leaf_height; ++index) {
            selected_reconstruction[index] = fallback_reconstruction[index];
        }
        *selected_mode = candidate_modes[fallback_mode_index];
        selected_mode_index = fallback_mode_index;
        selected_residual_kind = fallback_residual_kind;
        selected = 1;
    }
    if (!selected) return ENC_OUT_OF_SPACE;
    state->squared_error += selected_error;
    ++state->mode_counts[selected_mode_index];
    ++state->residual_counts[selected_residual_kind];
    if (state->stored_levels != 0 && state->stored_modes != 0) {
        size_t tx;
        size_t index;
        if (state->stored_level_index + transform_count >
                state->stored_block_count ||
            state->stored_mode_index >= state->stored_block_count) {
            return ENC_INVALID_ARGUMENT;
        }
        for (tx = 0U; tx < transform_count; ++tx) {
            for (index = 0U; index < 64U; ++index) {
                state->stored_levels[
                    (state->stored_level_index + tx) * 64U + index] =
                    selected_levels[tx][index];
            }
        }
        state->stored_modes[state->stored_mode_index] = *selected_mode;
        state->stored_level_index += transform_count;
        ++state->stored_mode_index;
    }
    for (size_t tx = 0U; tx < transform_count; ++tx) {
        for (size_t index = 1U; index < 64U; ++index) {
            if (selected_levels[tx][index] != 0) {
                ++state->ac_blocks;
                break;
            }
        }
    }
    for (size_t y = 0U; y < leaf_height; ++y) {
        for (size_t x = 0U; x < leaf_width; ++x) {
            state->padded_reconstruction[
                (leaf_y + y) * state->padded_width + leaf_x + x] =
                selected_reconstruction[y * leaf_width + x];
        }
    }
    rd_mark_reconstructed(
        state->superblock_x, state->superblock_y,
        state->reconstructed_cells, leaf_x, leaf_y, leaf_width, leaf_height);
    state->searched_blocks += transform_count;
    return ENC_OK;
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
    if (search == ENC_AC8X8_SEARCH_RD) {
        return enc_ac8x8_i400_rd_workspace(
            input, width, height, stride, qindex, workspace,
            workspace_capacity, stream, stream_capacity, reconstruction,
            reconstruction_capacity, result);
    }
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
    padded_reconstruction = cursor;
    cursor += legacy_layout.padded_pixel_count;
    if ((uintptr_t)cursor % ENC_M3_WORKSPACE_ALIGNMENT != 0U) ++cursor;
    levels = (int16_t *)(void *)cursor;
    cursor += legacy_layout.block_count * 64U * sizeof(int16_t);
    cursor += legacy_layout.block_count;
    legacy_stream = cursor;
    cursor += legacy_layout.stream_capacity;
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
    result->baseline_stream_size = legacy_result.stream_size;
    result->baseline_squared_error = legacy_result.squared_error;
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
        result->candidate_stream_size = legacy_result.stream_size;
        result->squared_error = legacy_result.squared_error;
        result->candidate_squared_error = legacy_result.squared_error;
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
        uint8_t block_prediction[64U];
        uint8_t pred;
        block_coordinates(block, width, height, &block_x, &block_y);
        pred = prediction(
            padded_reconstruction, legacy_layout.padded_width,
            block_x, block_y);
        constant_prediction(pred, block_prediction);
        size_t y;
        size_t x;
        source_block(
            input, width, height, stride, block_x, block_y, samples);
        search_block(
            samples, block_prediction, qindex, 1,
            (width > 64U || height > 64U ? 1 : 0) |
            (qindex >= 141U && width >= 64U && height >= 64U ? 2 : 0),
            levels + block * 64U, decoded);
        result->searched_blocks++;
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
    result->candidate_stream_size = obu_writer.position;
    result->candidate_squared_error = candidate_error;
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

static EncStatus enc_ac8x8_i400_rd_transaction_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity, uint8_t *reconstruction,
    size_t reconstruction_capacity, EncAc8x8Result *result) {
    EncAc8x8Result current;
    EncAc8x8WorkspaceLayout layout;
    EncM3WorkspaceLayout m3_layout;
    EncAc8x8RdSearchState search;
    uint8_t *cursor;
    uint8_t *padded_reconstruction;
    int16_t *levels;
    uint8_t *modes;
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
    size_t row;
    EncStatus status;
    status = enc_ac8x8_i400_q_workspace(
        input, width, height, stride, qindex, ENC_AC8X8_SEARCH_BOUNDED,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, &current);
    if (status != ENC_OK) return status;
    current.baseline_stream_size = current.stream_size;
    current.baseline_squared_error = current.squared_error;
    status = enc_ac8x8_query_workspace(width, height, &layout);
    if (status != ENC_OK) return status;
    status = enc_m3_query_workspace(width, height, &m3_layout);
    if (status != ENC_OK) return status;
    cursor = (uint8_t *)workspace + m3_layout.size + m3_layout.pixel_count;
    padded_reconstruction = cursor;
    cursor += m3_layout.padded_pixel_count;
    if ((uintptr_t)cursor % ENC_M3_WORKSPACE_ALIGNMENT != 0U) ++cursor;
    levels = (int16_t *)(void *)cursor;
    cursor += m3_layout.block_count * 64U * sizeof(*levels);
    modes = cursor;
    cursor += m3_layout.block_count;
    cursor += m3_layout.stream_capacity;
    tile = cursor;
    cursor += m3_layout.stream_capacity;
    payload = cursor;
    cursor += m3_layout.stream_capacity;
    candidate_stream = cursor;
    if ((size_t)(cursor + m3_layout.stream_capacity - (uint8_t *)workspace) >
        layout.size) {
        return ENC_OVERFLOW;
    }
    enc_memory_fill(
        padded_reconstruction, 0U, m3_layout.padded_pixel_count);
    search.input = input;
    search.width = width;
    search.height = height;
    search.stride = stride;
    search.padded_reconstruction = padded_reconstruction;
    search.padded_width = m3_layout.padded_width;
    search.qindex = qindex;
    search.superblock_x = SIZE_MAX;
    search.superblock_y = SIZE_MAX;
    enc_memory_fill(
        search.reconstructed_cells, 0U, sizeof(search.reconstructed_cells));
    search.squared_error = 0U;
    search.searched_blocks = 0U;
    search.ac_blocks = 0U;
    search.trials = 0U;
    search.stored_levels = levels;
    search.stored_modes = modes;
    search.stored_block_count = m3_layout.block_count;
    search.stored_level_index = 0U;
    search.stored_mode_index = 0U;
    search.incumbent_reconstruction = reconstruction;
    search.incumbent_stride = width;
    search.replay = 0;
    for (row = 0U; row < 7U; ++row) search.mode_counts[row] = 0U;
    for (row = 0U; row < 3U; ++row) search.residual_counts[row] = 0U;
    enc_memory_fill(
        levels, 0U, m3_layout.block_count * 64U * sizeof(*levels));
    enc_memory_fill(modes, 0U, m3_layout.block_count);
    status = enc_ac8x8_rd_grid_tile_q(
        tile, m3_layout.stream_capacity, &tile_size, width, height, qindex,
        rd_select_transaction_leaf, &search);
    if (status != ENC_OK) {
        *result = current;
        rd_record_search_telemetry(result, &search);
        return ENC_OK;
    }
    enc_memory_fill(
        padded_reconstruction, 0U, m3_layout.padded_pixel_count);
    search.superblock_x = SIZE_MAX;
    search.superblock_y = SIZE_MAX;
    enc_memory_fill(
        search.reconstructed_cells, 0U, sizeof(search.reconstructed_cells));
    search.squared_error = 0U;
    search.searched_blocks = 0U;
    search.ac_blocks = 0U;
    search.stored_level_index = 0U;
    search.stored_mode_index = 0U;
    search.replay = 1;
    status = enc_ac8x8_rd_grid_tile_q(
        tile, m3_layout.stream_capacity, &tile_size, width, height, qindex,
        rd_select_transaction_leaf, &search);
    if (status != ENC_OK) {
        *result = current;
        rd_record_search_telemetry(result, &search);
        return ENC_OK;
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size, width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q_for_size(
        frame_header, sizeof(frame_header), &frame_header_size,
        width, height, qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, payload, m3_layout.stream_capacity);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(
        &obu_writer, candidate_stream, m3_layout.stream_capacity);
    if (enc_obu_write(&obu_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&obu_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &obu_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        return obu_writer.status;
    }
    /*
     * Admission is a complete-objective comparison: retain the incumbent
     * unless candidate SSE is no worse and SSE*4096 + q^2*exact bits is
     * strictly lower. Partial 16-pixel boundary rows/columns still have a
     * local reconstruction mismatch in the RD replay, so keep their fully
     * searched candidate as telemetry-only until boundary parity is exact.
     */
    if ((width & 15U) != 0U || (height & 15U) != 0U ||
        obu_writer.position > stream_capacity ||
        search.squared_error > current.squared_error ||
        rd_cost(search.squared_error, obu_writer.position * 8U, qlookup(qindex)) >=
            rd_cost(current.squared_error, current.stream_size * 8U,
                    qlookup(qindex))) {
        *result = current;
        result->admitted = 0;
        result->baseline_stream_size = current.stream_size;
        result->baseline_squared_error = current.squared_error;
        result->candidate_stream_size = obu_writer.position;
        result->candidate_squared_error = search.squared_error;
        rd_record_search_telemetry(result, &search);
        return ENC_OK;
    }
    for (row = 0U; row < height; ++row) {
        status = enc_memory_copy(
            reconstruction + row * width, width,
            padded_reconstruction + row * m3_layout.padded_width, width);
        if (status != ENC_OK) return status;
    }
    status = enc_memory_copy(
        stream, stream_capacity, candidate_stream, obu_writer.position);
    if (status != ENC_OK) return status;
    *result = current;
    result->stream_size = obu_writer.position;
    result->baseline_stream_size = current.stream_size;
    result->candidate_stream_size = obu_writer.position;
    result->squared_error = search.squared_error;
    result->baseline_squared_error = current.squared_error;
    result->candidate_squared_error = search.squared_error;
    result->searched_blocks = search.searched_blocks;
    result->ac_blocks = search.ac_blocks;
    rd_record_search_telemetry(result, &search);
    result->workspace_size = layout.size;
    result->qindex = qindex;
    result->admitted = 1;
    return ENC_OK;
}

static EncStatus enc_ac8x8_i400_rd_workspace(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex, void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity, uint8_t *reconstruction,
    size_t reconstruction_capacity, EncAc8x8Result *result) {
    EncAc8x8Result current;
    EncAc8x8WorkspaceLayout layout;
    EncM3WorkspaceLayout m3_layout;
    uint8_t *cursor;
    uint8_t *padded_reconstruction;
    int16_t *levels;
    uint8_t *modes;
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
    uint64_t candidate_error = 0U;
    size_t ac_blocks = 0U;
    size_t trials = 0U;
    size_t rd_leaf_count = 0U;
    size_t superblock_x = SIZE_MAX;
    size_t superblock_y = SIZE_MAX;
    uint8_t reconstructed_cells[32U];
    EncStatus status;

    return enc_ac8x8_i400_rd_transaction_workspace(
        input, width, height, stride, qindex, workspace, workspace_capacity,
        stream, stream_capacity, reconstruction, reconstruction_capacity,
        result);

    status = enc_ac8x8_i400_q_workspace(
        input, width, height, stride, qindex, ENC_AC8X8_SEARCH_BOUNDED,
        workspace, workspace_capacity, stream, stream_capacity,
        reconstruction, reconstruction_capacity, &current);
    if (status != ENC_OK) return status;
    status = enc_ac8x8_query_workspace(width, height, &layout);
    if (status != ENC_OK) return status;
    status = enc_m3_query_workspace(width, height, &m3_layout);
    if (status != ENC_OK) return status;
    cursor = (uint8_t *)workspace + m3_layout.size + m3_layout.pixel_count;
    padded_reconstruction = cursor;
    cursor += m3_layout.padded_pixel_count;
    if ((uintptr_t)cursor % ENC_M3_WORKSPACE_ALIGNMENT != 0U) ++cursor;
    levels = (int16_t *)(void *)cursor;
    cursor += m3_layout.block_count * 64U * sizeof(int16_t);
    modes = cursor;
    cursor += m3_layout.block_count;
    cursor += m3_layout.stream_capacity; /* Legacy stream staging. */
    tile = cursor;
    cursor += m3_layout.stream_capacity;
    payload = cursor;
    cursor += m3_layout.stream_capacity;
    candidate_stream = cursor;
    if ((size_t)(cursor + m3_layout.stream_capacity - (uint8_t *)workspace) >
        layout.size) {
        return ENC_OVERFLOW;
    }
    enc_memory_fill(
        padded_reconstruction, 0U, m3_layout.padded_pixel_count);
    enc_memory_fill(levels, 0U, m3_layout.block_count * 64U * sizeof(*levels));
    enc_memory_fill(modes, 0U, m3_layout.block_count);
    enc_memory_fill(reconstructed_cells, 0U, sizeof(reconstructed_cells));

    {
        size_t coded_width = m3_layout.padded_width;
        size_t coded_height = m3_layout.padded_height;
        size_t leaf_span =
            width <= ENC_M3_LEGACY_MAX_DIMENSION &&
            height <= ENC_M3_LEGACY_MAX_DIMENSION ? 8U : 16U;
        size_t leaf_count =
            ((coded_width + leaf_span - 1U) / leaf_span) *
            ((coded_height + leaf_span - 1U) / leaf_span);
        size_t leaf_index = 0U;
        block = 0U;
        for (size_t leaf_y = 0U; leaf_y < coded_height; leaf_y += leaf_span) {
            for (size_t leaf_x = 0U;
                 leaf_x < coded_width; leaf_x += leaf_span) {
                size_t leaf_width = leaf_x + leaf_span <= coded_width
                    ? leaf_span : 8U;
                size_t leaf_height = leaf_y + leaf_span <= coded_height
                    ? leaf_span : 8U;
                size_t leaf_transforms =
                    (leaf_width / 8U) * (leaf_height / 8U);
                size_t leaf_superblock_x = leaf_x & ~(size_t)63U;
                size_t leaf_superblock_y = leaf_y & ~(size_t)63U;
                int16_t selected_levels[4U][64U];
                uint8_t selected_reconstruction[256U];
                uint8_t selected_mode;
                uint64_t leaf_error;
                size_t y;
                size_t x;
                if (leaf_superblock_x != superblock_x ||
                    leaf_superblock_y != superblock_y) {
                    enc_memory_fill(
                        reconstructed_cells, 0U, sizeof(reconstructed_cells));
                    superblock_x = leaf_superblock_x;
                    superblock_y = leaf_superblock_y;
                }
                if (!rd_select_leaf(
                        input, width, height, stride, padded_reconstruction,
                        m3_layout.padded_width, reconstruction, width,
                        leaf_x, leaf_y,
                        leaf_width, leaf_height, superblock_x, superblock_y,
                        reconstructed_cells, qindex, levels, modes, block,
                        leaf_index, m3_layout.block_count, leaf_count,
                        candidate_error, tile, m3_layout.stream_capacity,
                        selected_levels, selected_reconstruction, &selected_mode,
                        &trials, &leaf_error)) {
                    *result = current;
                    return ENC_OK;
                }
                (void)selected_mode;
                candidate_error += leaf_error;
                for (x = 0U; x < leaf_transforms; ++x) {
                    for (y = 1U; y < 64U; ++y) {
                        if (selected_levels[x][y] != 0) {
                            ++ac_blocks;
                            break;
                        }
                    }
                }
                for (y = 0U; y < leaf_height; ++y) {
                    for (x = 0U; x < leaf_width; ++x) {
                        padded_reconstruction[
                            (leaf_y + y) * m3_layout.padded_width +
                            leaf_x + x] =
                            selected_reconstruction[y * leaf_width + x];
                    }
                }
                rd_mark_reconstructed(
                    superblock_x, superblock_y, reconstructed_cells,
                    leaf_x, leaf_y, leaf_width, leaf_height);
                block += leaf_transforms;
                ++leaf_index;
            }
        }
        if (block != m3_layout.block_count || leaf_index != leaf_count) {
            *result = current;
            return ENC_OK;
        }
        rd_leaf_count = leaf_count;
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size, width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q_for_size(
        frame_header, sizeof(frame_header), &frame_header_size,
        width, height, qindex);
    if (status != ENC_OK) return status;
    status = enc_ac8x8_intra_grid_tile_q(
        tile, m3_layout.stream_capacity, &tile_size, width, height, levels,
        ((width + 7U) / 8U) * ((height + 7U) / 8U) * 64U,
        modes, rd_leaf_count, qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, payload, m3_layout.stream_capacity);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(
        &obu_writer, candidate_stream, m3_layout.stream_capacity);
    if (enc_obu_write(&obu_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&obu_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &obu_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        return obu_writer.status;
    }
    if (obu_writer.position <= stream_capacity &&
        (candidate_error < current.squared_error ||
         (candidate_error == current.squared_error &&
          obu_writer.position < current.stream_size))) {
        for (block = 0U; block < height; ++block) {
            status = enc_memory_copy(
                reconstruction + block * width, width,
                padded_reconstruction + block * m3_layout.padded_width,
                width);
            if (status != ENC_OK) return status;
        }
        status = enc_memory_copy(
            stream, stream_capacity, candidate_stream, obu_writer.position);
        if (status != ENC_OK) return status;
        *result = current;
        result->stream_size = obu_writer.position;
        result->candidate_stream_size = obu_writer.position;
        result->squared_error = candidate_error;
        result->candidate_squared_error = candidate_error;
        result->searched_blocks = m3_layout.block_count;
        result->ac_blocks = ac_blocks;
        result->workspace_size = layout.size;
        result->qindex = qindex;
        result->admitted = 1;
    } else {
        *result = current;
    }
    (void)trials;
    return ENC_OK;
}

    static void clear_yuv_result(EncAc8x8YuvResult *result) {
        size_t plane;
        size_t index;
        if (result == 0) return;
        result->stream_size = 0U;
        result->baseline_stream_size = 0U;
        result->candidate_stream_size = 0U;
        result->workspace_size = 0U;
        result->searched_blocks = 0U;
        result->ac_blocks = 0U;
        result->candidate_trials = 0U;
        for (index = 0U; index < 7U; ++index) {
            result->rd_mode_counts[index] = 0U;
        }
        for (index = 0U; index < 3U; ++index) {
            result->rd_residual_counts[index] = 0U;
        }
        result->rd_qdelta_trials = 0U;
        result->qindex = 0U;
        result->format = ENC_M3_PIXEL_I400;
        result->u_qdelta = 0;
        result->v_qdelta = 0;
        result->rd_qdelta_y_preserved = 0;
        result->luma_mode = 0U;
        result->chroma_mode = 0U;
        result->admitted = 0;
        for (plane = 0U; plane < 3U; ++plane) {
            result->plane_size[plane] = 0U;
            result->transform_count[plane] = 0U;
            result->squared_error[plane] = 0U;
            result->baseline_squared_error[plane] = 0U;
            result->candidate_squared_error[plane] = 0U;
        }
    }

    EncStatus enc_ac8x8_query_yuv8_workspace(
        EncM3PixelFormat format, size_t width, size_t height,
        EncAc8x8YuvWorkspaceLayout *layout) {
        EncAc8x8YuvWorkspaceLayout candidate = {0};
        EncM3ChromaWorkspaceLayout m3;
        EncAc8x8WorkspaceLayout i400;
        size_t offset = 0U;
        size_t plane;
        size_t bytes;
        if (layout == 0 || width == 0U || height == 0U ||
            (format != ENC_M3_PIXEL_I400 && format != ENC_M3_PIXEL_I420 &&
             format != ENC_M3_PIXEL_I444)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
            height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
            return ENC_UNSUPPORTED;
        }
        candidate.alignment = ENC_M3_WORKSPACE_ALIGNMENT;
        if (format == ENC_M3_PIXEL_I400) {
            EncM3WorkspaceLayout legacy;
            EncStatus status = enc_ac8x8_query_workspace(width, height, &i400);
            if (status != ENC_OK) return status;
            status = enc_m3_query_workspace(width, height, &legacy);
            if (status != ENC_OK) return status;
            candidate.size = i400.size;
            candidate.stream_capacity = i400.stream_capacity;
            candidate.plane_width[0U] = width;
            candidate.plane_height[0U] = height;
            candidate.plane_size[0U] = i400.pixel_count;
            candidate.coded_plane_width[0U] = (width + 7U) & ~(size_t)7U;
            candidate.coded_plane_height[0U] = (height + 7U) & ~(size_t)7U;
            candidate.coded_plane_size[0U] =
                candidate.coded_plane_width[0U] *
                candidate.coded_plane_height[0U];
            candidate.transform_count[0U] = i400.block_count;
            candidate.entropy_state_offset = 0U;
            candidate.entropy_state_size = legacy.size;
            candidate.baseline_reconstruction_offset = legacy.size;
            if (!enc_size_add(
                    legacy.size, legacy.pixel_count, &offset)) {
                return ENC_OVERFLOW;
            }
            candidate.coded_reconstruction_offset = offset;
            if (!enc_size_add(
                    offset, legacy.padded_pixel_count, &offset)) {
                return ENC_OVERFLOW;
            }
            if ((offset & 1U) != 0U &&
                !enc_size_add(offset, 1U, &offset)) {
                return ENC_OVERFLOW;
            }
            candidate.luma_coefficients_offset = offset;
            if (!enc_size_multiply(
                    legacy.block_count, 64U * sizeof(int16_t), &bytes) ||
                !enc_size_add(offset, bytes, &offset)) {
                return ENC_OVERFLOW;
            }
                    candidate.luma_modes_offset = offset;
                    if (!enc_size_add(offset, legacy.block_count, &offset)) {
                        return ENC_OVERFLOW;
                    }
                    if ((offset & 1U) != 0U &&
                        !enc_size_add(offset, 1U, &offset)) {
                        return ENC_OVERFLOW;
                    }
                    candidate.chroma_dc_levels_offset = offset;
            candidate.chroma_coefficients_offset = offset;
            candidate.stream_staging_offset = offset;
            candidate.stream_staging_count = 4U;
            *layout = candidate;
            return ENC_OK;
        }
        {
            EncStatus status =
                enc_m3_query_chroma_workspace(format, width, height, &m3);
            if (status != ENC_OK) return status;
        }
        candidate.stream_capacity = m3.stream_capacity;
        for (plane = 0U; plane < 3U; ++plane) {
            candidate.plane_width[plane] = m3.plane_width[plane];
            candidate.plane_height[plane] = m3.plane_height[plane];
            candidate.plane_size[plane] = m3.plane_size[plane];
            candidate.coded_plane_width[plane] = m3.coded_plane_width[plane];
            candidate.coded_plane_height[plane] = m3.coded_plane_height[plane];
            candidate.coded_plane_size[plane] = m3.coded_plane_size[plane];
            candidate.transform_count[plane] = m3.transform_count[plane];
            if (!enc_size_add(offset, m3.coded_plane_size[plane], &offset)) {
                return ENC_OVERFLOW;
            }
        }
        candidate.coded_reconstruction_offset = 0U;
        candidate.baseline_reconstruction_offset = offset;
        if (!enc_size_add(offset, m3.reconstruction_size, &offset)) {
            return ENC_OVERFLOW;
        }
        if ((offset & 1U) != 0U && !enc_size_add(offset, 1U, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.luma_coefficients_offset = offset;
        if (!enc_size_multiply(
                m3.transform_count[0U], 64U * sizeof(int16_t), &bytes) ||
            !enc_size_add(offset, bytes, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.luma_modes_offset = offset;
        if (!enc_size_add(offset, m3.transform_count[0U], &offset)) {
            return ENC_OVERFLOW;
        }
        if ((offset & 1U) != 0U && !enc_size_add(offset, 1U, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.chroma_dc_levels_offset = offset;
        if (!enc_size_add(
                m3.transform_count[1U], m3.transform_count[2U], &bytes) ||
            !enc_size_multiply(bytes, sizeof(int16_t), &bytes) ||
            !enc_size_add(offset, bytes, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.chroma_coefficients_offset = offset;
        if (!enc_size_add(
                m3.coded_plane_size[1U], m3.coded_plane_size[2U], &bytes) ||
            !enc_size_multiply(bytes, sizeof(int16_t), &bytes) ||
            !enc_size_add(offset, bytes, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.entropy_state_offset = offset;
        candidate.entropy_state_size = m3.size;
        if (!enc_size_add(offset, m3.size, &offset)) return ENC_OVERFLOW;
        candidate.stream_staging_offset = offset;
        candidate.stream_staging_count = 4U;
        if (!enc_size_multiply(m3.stream_capacity, 4U, &bytes) ||
            !enc_size_add(offset, bytes, &offset)) {
            return ENC_OVERFLOW;
        }
        candidate.size = offset;
        *layout = candidate;
        return ENC_OK;
    }

    static uint8_t staged_sample(
        const uint8_t *plane, size_t width, size_t height,
        size_t x, size_t y) {
        if (x >= width) x = width - 1U;
        if (y >= height) y = height - 1U;
        return plane[y * width + x];
    }

    static uint8_t chroma_rect_value(
        int level, uint8_t prediction, uint32_t q, unsigned int shift,
        int rectangular) {
        unsigned int magnitude =
            level < 0 ? (unsigned int)(-level) : (unsigned int)level;
        int dequant = (int)((magnitude * q + 4U) >> 3U);
        int row;
        int residual;
        if (level < 0) dequant = -dequant;
        dequant = clip_int(-32768, 32767, dequant);
        row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
        if (rectangular) row = round_shift(row * 181, 7U);
        residual = clip_int(-256, 255, round_shift(64 * row, shift));
        return (uint8_t)clip_int(0, 255, (int)prediction + residual);
    }

    static int16_t chroma_level_from_value(
        uint8_t prediction, uint8_t value, uint32_t q,
        size_t width, size_t height) {
        unsigned int max_level =
            (unsigned int)((UINT32_C(32768) * 8U + q - 1U) / q + 1U);
        unsigned int shift =
            width >= 16U && height >= 16U ? 12U :
            width >= 8U && height >= 8U ? 11U : 10U;
        int low = -(int)max_level;
        int high = (int)max_level;
        while (low < high) {
            int middle = low + (high - low) / 2;
            if (chroma_rect_value(
                    middle, prediction, q, shift, width != height) < value) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
        return (int16_t)low;
    }

    static uint8_t chroma_prediction(
        const uint8_t *coded, size_t coded_width,
        size_t x, size_t y, size_t width, size_t height) {
        uint32_t sum = 0U;
        size_t count = 0U;
        size_t edge;
        if (y != 0U) {
            for (edge = 0U; edge < width; ++edge) {
                sum += coded[(y - 1U) * coded_width + x + edge];
            }
            count += width;
        }
        if (x != 0U) {
            for (edge = 0U; edge < height; ++edge) {
                sum += coded[(y + edge) * coded_width + x - 1U];
            }
            count += height;
        }
        return count == 0U ? 128U :
            (uint8_t)((sum + count / 2U) / count);
    }

    static void chroma_source_block(
        const uint8_t *input, size_t plane_width, size_t plane_height,
        size_t stride, size_t x, size_t y,
        size_t width, size_t height, uint8_t samples[256U]) {
        size_t py;
        size_t px;
        for (py = 0U; py < height; ++py) {
            size_t sy = y + py < plane_height ? y + py : plane_height - 1U;
            for (px = 0U; px < width; ++px) {
                size_t sx = x + px < plane_width ? x + px : plane_width - 1U;
                samples[py * width + px] = input[sy * stride + sx];
            }
        }
    }

    static uint64_t reconstruct_chroma(
        const uint8_t samples[256U], const int16_t levels[256U],
        size_t width, size_t height, uint8_t prediction,
        uint32_t q, uint8_t output[256U]) {
        int coefficients[256U];
        int rows[256U];
        uint64_t error = 0U;
        unsigned int row_shift =
            width == 16U && height == 16U ? 6U : 7U;
        unsigned int column_shift =
            width == 16U && height == 16U ? 13U :
            width >= 8U && height >= 8U ? 11U : 10U;
        size_t count = width * height;
        size_t index;
        size_t y;
        size_t x;
        for (index = 0U; index < count; ++index) {
            int level = levels[index];
            unsigned int magnitude =
                level < 0 ? (unsigned int)(-level) : (unsigned int)level;
            int value = (int)((magnitude * q + 4U) >> 3U);
            if (level < 0) value = -value;
            coefficients[index] = clip_int(-32768, 32767, value);
        }
        for (y = 0U; y < height; ++y) {
            for (x = 0U; x < width; ++x) {
                int sum = 0;
                for (index = 0U; index < width; ++index) {
                    int value = coefficients[y * width + index];
                    if (width != height) {
                        value = round_shift(value * 2896, 12U);
                    }
                    sum += dct_value(width, index, x) * value;
                }
                rows[y * width + x] =
                    clip_int(-32768, 32767, round_shift(sum, row_shift));
            }
        }
        for (y = 0U; y < height; ++y) {
            for (x = 0U; x < width; ++x) {
                int sum = 0;
                int value;
                int difference;
                for (index = 0U; index < height; ++index) {
                    sum += dct_value(height, index, y) *
                           rows[index * width + x];
                }
                value = clip_int(
                    0, 255, (int)prediction +
                    clip_int(-256, 255, round_shift(sum, column_shift)));
                output[y * width + x] = (uint8_t)value;
                difference = (int)samples[y * width + x] - value;
                error += (uint64_t)(difference * difference);
            }
        }
        return error;
    }

    static void search_chroma_block(
        const uint8_t samples[256U], size_t width, size_t height,
        uint8_t prediction, uint32_t q, int16_t dc_level,
        int16_t best[256U], uint8_t reconstructed[256U],
        size_t *trials) {
        int analysis[256U];
        uint8_t trial_reconstruction[256U];
        size_t count = width * height;
        size_t position_count = count < 8U ? count : 8U;
        uint64_t best_sse;
        size_t v;
        size_t u;
        size_t y;
        size_t x;
        size_t index;
        unsigned int shift = 6U;
        size_t scale = count;
        while (scale > 1U) {
            ++shift;
            scale >>= 1U;
        }
        for (index = 0U; index < count; ++index) best[index] = 0;
        best[0U] = dc_level;
        for (v = 0U; v < height; ++v) {
            for (u = 0U; u < width; ++u) {
                int sum = 0;
                for (y = 0U; y < height; ++y) {
                    for (x = 0U; x < width; ++x) {
                        sum += ((int)samples[y * width + x] -
                                (int)prediction) *
                               dct_value(height, v, y) *
                               dct_value(width, u, x);
                    }
                }
                analysis[v * width + u] = round_shift(sum, shift);
                if (width != height) {
                    analysis[v * width + u] =
                        round_shift(analysis[v * width + u] * 5793, 12U);
                }
            }
        }
        best_sse = reconstruct_chroma(
            samples, best, width, height, prediction, q, reconstructed);
        for (index = 0U; index < position_count; ++index) {
            size_t position = chroma_scan_position(width, height, index);
            int ideal = analysis[position] * 8 / (int)q;
            int choices[6U];
            int16_t local[256U];
            uint64_t local_sse = best_sse;
            size_t choice;
            size_t copy;
            ideal = clip_int(
                -(int)ENC_AC8X8_LEVEL_LIMIT,
                (int)ENC_AC8X8_LEVEL_LIMIT, ideal);
            choices[0U] = 0;
            choices[1U] = ideal - 2;
            choices[2U] = ideal - 1;
            choices[3U] = ideal;
            choices[4U] = ideal + 1;
            choices[5U] = ideal + 2;
            for (copy = 0U; copy < count; ++copy) local[copy] = best[copy];
            for (choice = 0U; choice < 6U; ++choice) {
                int16_t trial[256U];
                uint64_t sse;
                int value = clip_int(
                    -(int)ENC_AC8X8_LEVEL_LIMIT,
                    (int)ENC_AC8X8_LEVEL_LIMIT, choices[choice]);
                for (copy = 0U; copy < count; ++copy) trial[copy] = best[copy];
                trial[position] = (int16_t)value;
                sse = reconstruct_chroma(
                    samples, trial, width, height,
                    prediction, q, trial_reconstruction);
                ++*trials;
                if (sse < local_sse) {
                    for (copy = 0U; copy < count; ++copy) {
                        local[copy] = trial[copy];
                    }
                    local_sse = sse;
                }
            }
            for (copy = 0U; copy < count; ++copy) best[copy] = local[copy];
            best_sse = local_sse;
        }
        (void)reconstruct_chroma(
            samples, best, width, height, prediction, q, reconstructed);
    }

    typedef struct {
        const EncM3ChromaWorkspaceLayout *layout;
        EncM3PixelFormat format;
        const EncM3Image8 *input;
        const uint8_t *reconstruction[3U];
        uint8_t *coded[3U];
        int16_t *levels[3U];
        int16_t *coefficients[3U];
        size_t index[3U];
        size_t coefficient_offset[3U];
        uint64_t error[3U];
        size_t ac_blocks;
        size_t trials;
        uint32_t q[3U];
    } EncAc8x8ChromaLevels;

    static void derive_chroma_transform(
        EncAc8x8ChromaLevels *state, size_t plane,
        size_t x, size_t y, size_t width, size_t height) {
        const uint8_t *reconstruction = state->reconstruction[plane];
        uint8_t *coded = state->coded[plane];
        size_t plane_width = state->layout->plane_width[plane];
        size_t plane_height = state->layout->plane_height[plane];
        size_t coded_width = state->layout->coded_plane_width[plane];
        uint8_t prediction;
        size_t level_index = state->index[plane]++;
        prediction = chroma_prediction(
            coded, coded_width, x, y, width, height);
        if (state->coefficients[plane] != 0) {
            uint8_t samples[256U] = {0};
            uint8_t decoded[256U] = {0};
            int16_t *coefficients =
                state->coefficients[plane] +
                state->coefficient_offset[plane];
            size_t count = width * height;
            size_t py;
            size_t px;
            size_t index;
            int has_ac = 0;
            chroma_source_block(
                state->input->planes[plane].data,
                plane_width, plane_height,
                state->input->planes[plane].stride,
                x, y, width, height, samples);
            search_chroma_block(
                samples, width, height, prediction, state->q[plane],
                state->levels[plane][level_index],
                coefficients, decoded, &state->trials);
            for (index = 1U; index < count; ++index) {
                if (coefficients[index] != 0) {
                    has_ac = 1;
                    break;
                }
            }
            state->ac_blocks += (size_t)has_ac;
            state->coefficient_offset[plane] += count;
            for (py = 0U; py < height; ++py) {
                for (px = 0U; px < width; ++px) {
                    size_t sx = x + px;
                    size_t sy = y + py;
                    uint8_t value = decoded[py * width + px];
                    coded[sy * coded_width + sx] = value;
                    if (sx < plane_width && sy < plane_height) {
                        int difference =
                            (int)state->input->planes[plane].data[
                                sy * state->input->planes[plane].stride + sx] -
                            (int)value;
                        state->error[plane] +=
                            (uint64_t)(difference * difference);
                    }
                }
            }
            return;
        }
        {
            uint8_t value =
                staged_sample(reconstruction, plane_width, plane_height, x, y);
            size_t py;
            size_t px;
            state->levels[plane][level_index] =
                chroma_level_from_value(
                    prediction, value, state->q[plane], width, height);
            for (py = 0U; py < height; ++py) {
                for (px = 0U; px < width; ++px) {
                    coded[(y + py) * coded_width + x + px] = value;
                }
            }
        }
    }

    static void derive_chroma_partition(
        EncAc8x8ChromaLevels *state, size_t row, size_t column,
        size_t width, size_t height) {
        size_t rows =
            ((state->layout->plane_height[0U] + 7U) / 8U) * 2U;
        size_t columns =
            ((state->layout->plane_width[0U] + 7U) / 8U) * 2U;
        int axis;
        if (row >= rows || column >= columns) return;
        axis = forced_axis(rows, columns, row, column, width, height);
        if (axis == 0 && width <= 4U && height <= 4U) {
            size_t subsampling =
                state->format == ENC_M3_PIXEL_I420 ? 1U : 0U;
            size_t tx_width = width * 4U >> subsampling;
            size_t tx_height = height * 4U >> subsampling;
            size_t x = column * 4U >> subsampling;
            size_t y = row * 4U >> subsampling;
            derive_chroma_transform(state, 1U, x, y, tx_width, tx_height);
            derive_chroma_transform(state, 2U, x, y, tx_width, tx_height);
            return;
        }
        if (axis == 0) axis = height >= width ? 1 : 2;
        if (axis == 1) {
            derive_chroma_partition(state, row, column, width, height / 2U);
            derive_chroma_partition(
                state, row + height / 2U, column, width, height / 2U);
        } else {
            derive_chroma_partition(state, row, column, width / 2U, height);
            derive_chroma_partition(
                state, row, column + width / 2U, width / 2U, height);
        }
    }

    static EncStatus plane_span(
        size_t width, size_t height, size_t stride, size_t *span) {
        size_t last;
        if (width == 0U || height == 0U || stride < width ||
            !enc_size_multiply(height - 1U, stride, &last) ||
            !enc_size_add(last, width, span)) {
            return ENC_INVALID_ARGUMENT;
        }
        return ENC_OK;
    }

    static EncStatus enc_ac8x8_yuv8_q_workspace_inner(
        const EncM3Image8 *input, unsigned int qindex,
        EncAc8x8CoefficientSearch search,
        void *workspace, size_t workspace_capacity,
        void *stream, size_t stream_capacity,
        EncM3MutableImage8 *reconstruction,
        EncAc8x8YuvResult *result,
        int qdelta_enabled, int u_qdelta, int v_qdelta,
        int replay_luma, uint64_t *luma_selection_error) {
        EncAc8x8YuvWorkspaceLayout layout;
        EncM3ChromaWorkspaceLayout m3_layout;
        EncM3ChromaResult baseline;
        uint8_t *base = (uint8_t *)workspace;
        uint8_t *coded[3U];
        uint8_t *baseline_planes[3U];
        uint8_t *baseline_stream;
        uint8_t *tile;
        uint8_t *payload;
        uint8_t *candidate_stream;
        int16_t *y_levels;
        uint8_t *y_modes;
        int16_t *u_levels;
        int16_t *v_levels;
        int16_t *u_coefficients;
        int16_t *v_coefficients;
        EncM3MutableImage8 staged_reconstruction;
        EncByteWriter payload_writer;
        EncByteWriter stream_writer;
        uint8_t sequence[16U];
        uint8_t frame_header[16U];
        size_t sequence_size;
        size_t frame_header_size;
        size_t tile_size;
        size_t input_span[3U];
        size_t output_span[3U];
        size_t plane;
        size_t offset;
        size_t block;
        uint64_t candidate_error = 0U;
        uint64_t admission_luma_error = 0U;
        uint64_t chroma_error[2U] = {0U, 0U};
        size_t ac_blocks = 0U;
        int exact_chroma_levels = 0;
        uint8_t rd_cells[32U];
        size_t rd_superblock_x = SIZE_MAX;
        size_t rd_superblock_y = SIZE_MAX;
        size_t rd_next_block = 0U;
        size_t rd_leaf_index = 0U;
        size_t rd_leaf_count = 0U;
        EncStatus status;
        if (result == 0) return ENC_INVALID_ARGUMENT;
        if (input == 0 || reconstruction == 0 ||
            overlap(result, sizeof(*result), input, sizeof(*input)) ||
            overlap(result, sizeof(*result), reconstruction,
                    sizeof(*reconstruction))) {
            return ENC_INVALID_ARGUMENT;
        }
        if (overlap(result, sizeof(*result), workspace, workspace_capacity) ||
            overlap(result, sizeof(*result), stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (overlap(input, sizeof(*input), reconstruction,
                    sizeof(*reconstruction)) ||
            overlap(input, sizeof(*input), workspace, workspace_capacity) ||
            overlap(input, sizeof(*input), stream, stream_capacity) ||
            overlap(reconstruction, sizeof(*reconstruction),
                    workspace, workspace_capacity) ||
            overlap(reconstruction, sizeof(*reconstruction),
                    stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        for (plane = 0U; plane < 3U; ++plane) {
            if (overlap(result, sizeof(*result),
                        input->planes[plane].data,
                        input->planes[plane].capacity) ||
                overlap(result, sizeof(*result),
                        reconstruction->planes[plane].data,
                        reconstruction->planes[plane].capacity)) {
                return ENC_INVALID_ARGUMENT;
            }
            if (overlap(input, sizeof(*input),
                        input->planes[plane].data,
                        input->planes[plane].capacity) ||
                overlap(input, sizeof(*input),
                        reconstruction->planes[plane].data,
                        reconstruction->planes[plane].capacity) ||
                overlap(reconstruction, sizeof(*reconstruction),
                        input->planes[plane].data,
                        input->planes[plane].capacity) ||
                overlap(reconstruction, sizeof(*reconstruction),
                        reconstruction->planes[plane].data,
                        reconstruction->planes[plane].capacity)) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        clear_yuv_result(result);
        if (input->format == ENC_M3_PIXEL_I400) {
            EncAc8x8Result mono;
            EncAc8x8YuvWorkspaceLayout mono_layout;
            status = enc_ac8x8_query_yuv8_workspace(
                ENC_M3_PIXEL_I400, input->width, input->height, &mono_layout);
            if (status != ENC_OK) return status;
            status = enc_ac8x8_i400_q_workspace(
                input->planes[0U].data, input->width, input->height,
                input->planes[0U].stride, qindex,
                search == ENC_AC8X8_COEFFICIENT_SEARCH_RD
                    ? ENC_AC8X8_SEARCH_RD
                    : (EncAc8x8Search)search,
                workspace, workspace_capacity, stream, stream_capacity,
                reconstruction->planes[0U].data,
                reconstruction->planes[0U].capacity, &mono);
            if (status != ENC_OK) return status;
            result->stream_size = mono.stream_size;
            result->baseline_stream_size = mono.baseline_stream_size;
            result->candidate_stream_size = mono.candidate_stream_size;
            result->workspace_size = mono.workspace_size;
            result->plane_size[0U] = mono.pixel_count;
            result->transform_count[0U] =
                mono_layout.transform_count[0U];
            result->squared_error[0U] = mono.squared_error;
            result->baseline_squared_error[0U] =
                mono.baseline_squared_error;
            result->candidate_squared_error[0U] =
                mono.candidate_squared_error;
            result->searched_blocks = mono.searched_blocks;
            result->ac_blocks = mono.ac_blocks;
            result->candidate_trials = mono.candidate_trials;
            for (plane = 0U; plane < 7U; ++plane) {
                result->rd_mode_counts[plane] = mono.rd_mode_counts[plane];
            }
            for (plane = 0U; plane < 3U; ++plane) {
                result->rd_residual_counts[plane] =
                    mono.rd_residual_counts[plane];
            }
            result->qindex = mono.qindex;
            result->format = ENC_M3_PIXEL_I400;
            result->admitted = mono.admitted;
            return ENC_OK;
        }
        if (qindex == 0U || qindex > 255U ||
            (qdelta_enabled != 0 &&
             (u_qdelta < -64 || u_qdelta > 63 ||
              v_qdelta < -64 || v_qdelta > 63)) ||
            (search != ENC_AC8X8_COEFFICIENT_SEARCH_OFF &&
             search != ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED &&
             search != ENC_AC8X8_COEFFICIENT_SEARCH_RD) ||
            input->format != reconstruction->format ||
            input->width != reconstruction->width ||
            input->height != reconstruction->height ||
            workspace == 0 ||
            (uintptr_t)workspace % ENC_M3_WORKSPACE_ALIGNMENT != 0U ||
            (stream == 0 && stream_capacity != 0U)) {
            return ENC_INVALID_ARGUMENT;
        }
        status = enc_ac8x8_query_yuv8_workspace(
            input->format, input->width, input->height, &layout);
        if (status != ENC_OK) return status;
        status = enc_m3_query_chroma_workspace(
            input->format, input->width, input->height, &m3_layout);
        if (status != ENC_OK) return status;
        if (workspace_capacity < layout.size) return ENC_OUT_OF_SPACE;
        for (plane = 0U; plane < 3U; ++plane) {
            if (input->planes[plane].data == 0 ||
                reconstruction->planes[plane].data == 0 ||
                plane_span(
                    layout.plane_width[plane], layout.plane_height[plane],
                    input->planes[plane].stride, &input_span[plane]) != ENC_OK ||
                plane_span(
                    layout.plane_width[plane], layout.plane_height[plane],
                    reconstruction->planes[plane].stride,
                    &output_span[plane]) != ENC_OK) {
                return ENC_INVALID_ARGUMENT;
            }
            if (input->planes[plane].capacity < input_span[plane] ||
                reconstruction->planes[plane].capacity < output_span[plane]) {
                return ENC_OUT_OF_SPACE;
            }
            if (overlap(workspace, layout.size,
                        input->planes[plane].data, input_span[plane]) ||
                overlap(workspace, layout.size,
                        reconstruction->planes[plane].data, output_span[plane]) ||
                overlap(stream, stream_capacity,
                        input->planes[plane].data, input_span[plane]) ||
                overlap(stream, stream_capacity,
                        reconstruction->planes[plane].data, output_span[plane]) ||
                overlap(result, sizeof(*result),
                        input->planes[plane].data, input_span[plane]) ||
                overlap(result, sizeof(*result),
                        reconstruction->planes[plane].data, output_span[plane])) {
                return ENC_INVALID_ARGUMENT;
            }
    }
    for (plane = 0U; plane < 3U; ++plane) {
            size_t other;
            for (other = 0U; other < 3U; ++other) {
                if (other != plane &&
                    overlap(input->planes[plane].data, input_span[plane],
                            input->planes[other].data, input_span[other])) {
                    return ENC_INVALID_ARGUMENT;
                }
                if (overlap(reconstruction->planes[plane].data,
                            output_span[plane],
                            input->planes[other].data, input_span[other]) ||
                    (other > plane &&
                     overlap(reconstruction->planes[plane].data,
                             output_span[plane],
                             reconstruction->planes[other].data,
                             output_span[other]))) {
                    return ENC_INVALID_ARGUMENT;
                }
            }
        }
        if (overlap(workspace, layout.size, stream, stream_capacity) ||
            overlap(result, sizeof(*result), workspace, layout.size) ||
            overlap(result, sizeof(*result), stream, stream_capacity)) {
            return ENC_INVALID_ARGUMENT;
        }
        offset = layout.coded_reconstruction_offset;
        for (plane = 0U; plane < 3U; ++plane) {
            coded[plane] = base + offset;
            offset += layout.coded_plane_size[plane];
        }
        offset = layout.baseline_reconstruction_offset;
        staged_reconstruction.format = input->format;
        staged_reconstruction.width = input->width;
        staged_reconstruction.height = input->height;
        for (plane = 0U; plane < 3U; ++plane) {
            baseline_planes[plane] = base + offset;
            staged_reconstruction.planes[plane].data = baseline_planes[plane];
            staged_reconstruction.planes[plane].stride =
                layout.plane_width[plane];
            staged_reconstruction.planes[plane].capacity =
                layout.plane_size[plane];
            offset += layout.plane_size[plane];
        }
        y_levels =
            (int16_t *)(void *)(base + layout.luma_coefficients_offset);
        y_modes = base + layout.luma_modes_offset;
        u_levels =
            (int16_t *)(void *)(base + layout.chroma_dc_levels_offset);
        v_levels = u_levels + layout.transform_count[1U];
        u_coefficients =
            (int16_t *)(void *)(base + layout.chroma_coefficients_offset);
        v_coefficients =
            u_coefficients + layout.coded_plane_size[1U];
        baseline_stream = base + layout.stream_staging_offset;
        tile = baseline_stream + layout.stream_capacity;
        payload = tile + layout.stream_capacity;
        candidate_stream = payload + layout.stream_capacity;
        if (input->width != 8U && input->height != 8U) {
            status = enc_m3_dc_yuv8_q_workspace_levels(
                input, qindex, base + layout.entropy_state_offset,
                layout.entropy_state_size,
                baseline_stream, layout.stream_capacity,
                &staged_reconstruction, u_levels, v_levels,
                layout.transform_count[1U], &baseline);
            exact_chroma_levels = status == ENC_OK;
        } else {
            status = enc_m3_dc_yuv8_q_workspace(
                input, qindex, base + layout.entropy_state_offset,
                layout.entropy_state_size,
                baseline_stream, layout.stream_capacity,
                &staged_reconstruction, &baseline);
        }
        if (status != ENC_OK) return status;
        result->baseline_stream_size = baseline.stream_size;
        for (plane = 0U; plane < 3U; ++plane) {
            result->plane_size[plane] = layout.plane_size[plane];
            result->transform_count[plane] = layout.transform_count[plane];
            result->baseline_squared_error[plane] =
                baseline.squared_error[plane];
        }
        if (search == ENC_AC8X8_COEFFICIENT_SEARCH_OFF) {
            if (baseline.stream_size > stream_capacity) return ENC_OUT_OF_SPACE;
            for (plane = 0U; plane < 3U; ++plane) {
                size_t y;
                for (y = 0U; y < layout.plane_height[plane]; ++y) {
                    status = enc_memory_copy(
                        reconstruction->planes[plane].data +
                            y * reconstruction->planes[plane].stride,
                        reconstruction->planes[plane].capacity -
                            y * reconstruction->planes[plane].stride,
                        baseline_planes[plane] +
                            y * layout.plane_width[plane],
                        layout.plane_width[plane]);
                    if (status != ENC_OK) return status;
                }
                result->squared_error[plane] = baseline.squared_error[plane];
            }
            status = enc_memory_copy(
                stream, stream_capacity, baseline_stream, baseline.stream_size);
            if (status != ENC_OK) return status;
            result->stream_size = baseline.stream_size;
            result->workspace_size = layout.size;
            result->qindex = qindex;
            result->format = input->format;
            return ENC_OK;
        }
        if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD &&
            replay_luma == 0) {
            enc_memory_fill(
                y_levels, 0U,
                layout.transform_count[0U] * 64U * sizeof(*y_levels));
            enc_memory_fill(y_modes, 0U, layout.transform_count[0U]);
            enc_memory_fill(rd_cells, 0U, sizeof(rd_cells));
            rd_leaf_count =
                ((layout.coded_plane_width[0U] + 15U) / 16U) *
                ((layout.coded_plane_height[0U] + 15U) / 16U);
        }
        for (block = 0U; block < layout.transform_count[0U]; ++block) {
            size_t block_x;
            size_t block_y;
            uint8_t samples[64U];
            uint8_t decoded[64U];
            uint8_t block_prediction[64U];
            uint8_t pred;
            size_t y;
            size_t x;
            if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD &&
                input->width > 8U && input->height > 8U) {
                continue;
            }
            block_coordinates(
                block, input->width, input->height, &block_x, &block_y);
            source_block(
                input->planes[0U].data, input->width, input->height,
                input->planes[0U].stride, block_x, block_y, samples);
            if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD) {
                size_t coded_width = layout.coded_plane_width[0U];
                size_t coded_height = layout.coded_plane_height[0U];
                size_t leaf_width;
                size_t leaf_height;
                size_t leaf_transforms;
                size_t block_superblock_x;
                size_t block_superblock_y;
                uint8_t selected_leaf_reconstruction[256U];
                int16_t selected_leaf_levels[4U][64U];
                uint8_t selected_mode;
                uint64_t leaf_error;
                size_t leaf_y;
                size_t leaf_x;
                if (block < rd_next_block) continue;
                leaf_width = block_x + 16U <= coded_width ? 16U : 8U;
                leaf_height = block_y + 16U <= coded_height ? 16U : 8U;
                leaf_transforms = (leaf_width / 8U) * (leaf_height / 8U);
                block_superblock_x = block_x & ~(size_t)63U;
                block_superblock_y = block_y & ~(size_t)63U;
                if (block_superblock_x != rd_superblock_x ||
                    block_superblock_y != rd_superblock_y) {
                    enc_memory_fill(rd_cells, 0U, sizeof(rd_cells));
                    rd_superblock_x = block_superblock_x;
                    rd_superblock_y = block_superblock_y;
                }
                if (!rd_select_leaf(
                        input->planes[0U].data, input->width, input->height,
                        input->planes[0U].stride, coded[0U],
                        layout.coded_plane_width[0U],
                        baseline_planes[0U], layout.plane_width[0U],
                        block_x, block_y,
                        leaf_width, leaf_height, rd_superblock_x,
                        rd_superblock_y, rd_cells, qindex, y_levels, y_modes,
                        block, rd_leaf_index, layout.transform_count[0U],
                        rd_leaf_count, candidate_error, tile,
                        layout.stream_capacity, selected_leaf_levels,
                        selected_leaf_reconstruction, &selected_mode,
                        &result->candidate_trials, &leaf_error)) {
                    return ENC_OUT_OF_SPACE;
                }
                (void)selected_mode;
                candidate_error += leaf_error;
                for (x = 0U; x < leaf_transforms; ++x) {
                    for (offset = 1U; offset < 64U; ++offset) {
                        if (selected_leaf_levels[x][offset] != 0) {
                            ++ac_blocks;
                            break;
                        }
                    }
                }
                for (leaf_y = 0U; leaf_y < leaf_height; ++leaf_y) {
                    for (leaf_x = 0U; leaf_x < leaf_width; ++leaf_x) {
                        coded[0U][(block_y + leaf_y) *
                            layout.coded_plane_width[0U] + block_x + leaf_x] =
                            selected_leaf_reconstruction[
                                leaf_y * leaf_width + leaf_x];
                    }
                }
                rd_mark_reconstructed(
                    rd_superblock_x, rd_superblock_y, rd_cells,
                    block_x, block_y, leaf_width, leaf_height);
                result->searched_blocks += leaf_transforms;
                rd_next_block = block + leaf_transforms;
                ++rd_leaf_index;
                continue;
            }
            pred = prediction(
                coded[0U], layout.coded_plane_width[0U], block_x, block_y);
            constant_prediction(pred, block_prediction);
            search_block(
                samples, block_prediction, qindex, 1,
                1 | (qindex >= 141U && input->width >= 64U &&
                     input->height >= 64U ? 2 : 0),
                y_levels + block * 64U, decoded);
            ++result->searched_blocks;
            result->candidate_trials += 8U * 6U;
            for (offset = 1U; offset < 64U; ++offset) {
                if (y_levels[block * 64U + offset] != 0) {
                    ++ac_blocks;
                    break;
                }
            }
            candidate_error += active_error(
                input->planes[0U].data, input->width, input->height,
                input->planes[0U].stride, block_x, block_y, decoded);
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    coded[0U][(block_y + y) *
                        layout.coded_plane_width[0U] + block_x + x] =
                        decoded[y * 8U + x];
                }
            }
        }
        if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD &&
            (input->width <= 8U || input->height <= 8U) &&
            (rd_next_block != layout.transform_count[0U] ||
             rd_leaf_index != rd_leaf_count)) {
            return ENC_INVALID_ARGUMENT;
        }
        if (!exact_chroma_levels) {
            EncAc8x8ChromaLevels derivation;
            size_t sb_y;
            size_t sb_x;
            derivation.layout = &m3_layout;
            derivation.format = input->format;
            derivation.input = input;
            derivation.q[0U] = qlookup(qindex);
            derivation.q[1U] = qlookup(
                (unsigned int)clip_int(1, 255, (int)qindex + u_qdelta));
            derivation.q[2U] = qlookup(
                (unsigned int)clip_int(1, 255, (int)qindex + v_qdelta));
            derivation.ac_blocks = 0U;
            derivation.trials = 0U;
            for (plane = 0U; plane < 3U; ++plane) {
                derivation.reconstruction[plane] = baseline_planes[plane];
                derivation.coded[plane] = coded[plane];
                derivation.levels[plane] =
                    plane == 1U ? u_levels : plane == 2U ? v_levels : 0;
                derivation.coefficients[plane] = 0;
                derivation.index[plane] = 0U;
                derivation.coefficient_offset[plane] = 0U;
                derivation.error[plane] = 0U;
            }
            if (input->width == 8U && input->height == 8U) {
                derive_chroma_transform(
                    &derivation, 1U, 0U, 0U,
                    layout.coded_plane_width[1U],
                    layout.coded_plane_height[1U]);
                derive_chroma_transform(
                    &derivation, 2U, 0U, 0U,
                    layout.coded_plane_width[2U],
                    layout.coded_plane_height[2U]);
            } else {
                for (sb_y = 0U; sb_y < input->height; sb_y += 64U) {
                    for (sb_x = 0U; sb_x < input->width; sb_x += 64U) {
                        size_t row = sb_y / 4U;
                        size_t column = sb_x / 4U;
                        size_t rows = ((input->height + 7U) / 8U) * 2U;
                        size_t columns = ((input->width + 7U) / 8U) * 2U;
                        if (columns - column > 8U && rows - row > 8U) {
                            derive_chroma_partition(
                                &derivation, row, column, 16U, 8U);
                            derive_chroma_partition(
                                &derivation, row + 8U, column, 16U, 8U);
                        } else {
                            derive_chroma_partition(
                                &derivation, row, column, 16U, 16U);
                        }
                    }
                }
            }
            if (derivation.index[1U] != layout.transform_count[1U] ||
                derivation.index[2U] != layout.transform_count[2U]) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        {
            EncAc8x8ChromaLevels chroma_search;
            size_t sb_y;
            size_t sb_x;
            chroma_search.layout = &m3_layout;
            chroma_search.format = input->format;
            chroma_search.input = input;
            chroma_search.q[0U] = qlookup(qindex);
            chroma_search.q[1U] = qlookup(
                (unsigned int)clip_int(1, 255, (int)qindex + u_qdelta));
            chroma_search.q[2U] = qlookup(
                (unsigned int)clip_int(1, 255, (int)qindex + v_qdelta));
            chroma_search.ac_blocks = 0U;
            chroma_search.trials = 0U;
            for (plane = 0U; plane < 3U; ++plane) {
                chroma_search.reconstruction[plane] =
                    baseline_planes[plane];
                chroma_search.coded[plane] = coded[plane];
                chroma_search.levels[plane] =
                    plane == 1U ? u_levels : plane == 2U ? v_levels : 0;
                chroma_search.coefficients[plane] =
                    plane == 1U ? u_coefficients :
                    plane == 2U ? v_coefficients : 0;
                chroma_search.index[plane] = 0U;
                chroma_search.coefficient_offset[plane] = 0U;
                chroma_search.error[plane] = 0U;
            }
            if (input->width == 8U && input->height == 8U) {
                derive_chroma_transform(
                    &chroma_search, 1U, 0U, 0U,
                    layout.coded_plane_width[1U],
                    layout.coded_plane_height[1U]);
                derive_chroma_transform(
                    &chroma_search, 2U, 0U, 0U,
                    layout.coded_plane_width[2U],
                    layout.coded_plane_height[2U]);
            } else {
                for (sb_y = 0U; sb_y < input->height; sb_y += 64U) {
                    for (sb_x = 0U; sb_x < input->width; sb_x += 64U) {
                        size_t row = sb_y / 4U;
                        size_t column = sb_x / 4U;
                        size_t rows = ((input->height + 7U) / 8U) * 2U;
                        size_t columns = ((input->width + 7U) / 8U) * 2U;
                        if (columns - column > 8U && rows - row > 8U) {
                            derive_chroma_partition(
                                &chroma_search, row, column, 16U, 8U);
                            derive_chroma_partition(
                                &chroma_search,
                                row + 8U, column, 16U, 8U);
                        } else {
                            derive_chroma_partition(
                                &chroma_search, row, column, 16U, 16U);
                        }
                    }
                }
            }
            if (chroma_search.index[1U] != layout.transform_count[1U] ||
                chroma_search.index[2U] != layout.transform_count[2U] ||
                chroma_search.coefficient_offset[1U] !=
                    layout.coded_plane_size[1U] ||
                chroma_search.coefficient_offset[2U] !=
                    layout.coded_plane_size[2U]) {
                return ENC_INVALID_ARGUMENT;
            }
            chroma_error[0U] = chroma_search.error[1U];
            chroma_error[1U] = chroma_search.error[2U];
            ac_blocks += chroma_search.ac_blocks;
            result->searched_blocks +=
                layout.transform_count[1U] + layout.transform_count[2U];
            result->candidate_trials += chroma_search.trials;
        }
        if (qdelta_enabled) {
            EncQuantizerDeltaConfig config;
            EncFrameQuantizerDeltas deltas;
            config.separate_uv_delta_q = u_qdelta != v_qdelta;
            config.equal_ac_dc_q = 1;
            config.base_y_dc_delta_q = 0;
            config.y_dc_delta_q_enabled = 0;
            config.base_uv_dc_delta_q = 0;
            config.uv_dc_delta_q_enabled = 0;
            config.base_uv_ac_delta_q = 0;
            config.uv_ac_delta_q_enabled = 1;
            deltas.y_dc = 0;
            deltas.u_dc = (int16_t)u_qdelta;
            deltas.u_ac = (int16_t)u_qdelta;
            deltas.v_dc = (int16_t)v_qdelta;
            deltas.v_ac = (int16_t)v_qdelta;
            status = enc_m3_sequence_header_for_qdelta(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height, (unsigned int)input->format,
                &config);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_qdelta_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex, &config, &deltas);
        } else {
            status = enc_m3_sequence_header_for_format(
                sequence, sizeof(sequence), &sequence_size,
                input->width, input->height, (unsigned int)input->format);
            if (status != ENC_OK) return status;
            status = enc_m3_frame_header_q_for_size(
                frame_header, sizeof(frame_header), &frame_header_size,
                input->width, input->height, qindex);
        }
        if (status != ENC_OK) return status;
        if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD &&
            input->width > 8U && input->height > 8U) {
            EncAc8x8RdSearchState rd_search;
            size_t pass;
            size_t pass_count = replay_luma != 0 ? 1U : 2U;
            for (pass = 0U; pass < pass_count; ++pass) {
                int replay_pass = replay_luma != 0 || pass != 0U;
                enc_memory_fill(
                    coded[0U], 0U, layout.coded_plane_size[0U]);
                rd_search.input = input->planes[0U].data;
                rd_search.width = input->width;
                rd_search.height = input->height;
                rd_search.stride = input->planes[0U].stride;
                rd_search.padded_reconstruction = coded[0U];
                rd_search.padded_width = layout.coded_plane_width[0U];
                rd_search.qindex = qindex;
                rd_search.superblock_x = SIZE_MAX;
                rd_search.superblock_y = SIZE_MAX;
                enc_memory_fill(
                    rd_search.reconstructed_cells, 0U,
                    sizeof(rd_search.reconstructed_cells));
                rd_search.squared_error = 0U;
                rd_search.searched_blocks = 0U;
                rd_search.ac_blocks = 0U;
                rd_search.trials = 0U;
                rd_search.stored_levels = y_levels;
                rd_search.stored_modes = y_modes;
                rd_search.stored_block_count = layout.transform_count[0U];
                rd_search.stored_level_index = 0U;
                rd_search.stored_mode_index = 0U;
                rd_search.incumbent_reconstruction = baseline_planes[0U];
                rd_search.incumbent_stride = layout.plane_width[0U];
                rd_search.replay = replay_pass;
                for (offset = 0U; offset < 7U; ++offset) {
                    rd_search.mode_counts[offset] = 0U;
                }
                for (offset = 0U; offset < 3U; ++offset) {
                    rd_search.residual_counts[offset] = 0U;
                }
                status = enc_ac8x8_rd_grid_tile_q(
                    tile, layout.stream_capacity, &tile_size,
                    input->width, input->height, qindex,
                    rd_select_transaction_leaf, &rd_search);
                if (status != ENC_OK) break;
                if (!replay_pass) {
                    if (luma_selection_error != 0) {
                        *luma_selection_error = rd_search.squared_error;
                    }
                    result->candidate_trials += rd_search.trials;
                    for (offset = 0U; offset < 7U; ++offset) {
                        result->rd_mode_counts[offset] =
                            rd_search.mode_counts[offset];
                    }
                    for (offset = 0U; offset < 3U; ++offset) {
                        result->rd_residual_counts[offset] =
                            rd_search.residual_counts[offset];
                    }
                } else {
                    candidate_error = rd_search.squared_error;
                    ac_blocks += rd_search.ac_blocks;
                    result->searched_blocks += rd_search.searched_blocks;
                }
            }
            if (status == ENC_OK) {
                status = enc_ac8x8_intra_yuv_chroma_grid_tile_q(
                    tile, layout.stream_capacity, &tile_size,
                    input->width, input->height,
                    y_levels, u_coefficients, v_coefficients,
                    layout.transform_count[0U], layout.coded_plane_size[1U],
                    y_modes,
                    ((layout.coded_plane_width[0U] + 15U) / 16U) *
                        ((layout.coded_plane_height[0U] + 15U) / 16U),
                    (unsigned int)input->format, qindex);
            }
        } else if (search == ENC_AC8X8_COEFFICIENT_SEARCH_RD) {
            status = enc_ac8x8_intra_yuv_chroma_grid_tile_q(
                tile, layout.stream_capacity, &tile_size,
                input->width, input->height,
                y_levels, u_coefficients, v_coefficients,
                layout.transform_count[0U], layout.coded_plane_size[1U],
                y_modes, rd_leaf_count,
                (unsigned int)input->format, qindex);
        } else {
            status = enc_ac8x8_yuv_chroma_grid_tile_q(
                tile, layout.stream_capacity, &tile_size,
                input->width, input->height,
                y_levels, u_coefficients, v_coefficients,
                layout.transform_count[0U], layout.coded_plane_size[1U],
                (unsigned int)input->format, qindex);
        }
        if (status != ENC_OK) return status;
        enc_byte_writer_init(&payload_writer, payload, layout.stream_capacity);
        if (enc_byte_writer_write(
                &payload_writer, frame_header, frame_header_size) != ENC_OK ||
            enc_byte_writer_write(
                &payload_writer, tile, tile_size) != ENC_OK) {
            return payload_writer.status;
        }
        enc_byte_writer_init(
            &stream_writer, candidate_stream, layout.stream_capacity);
        if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
            enc_obu_write(
                &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
            enc_obu_write(
                &stream_writer, 4U, payload,
                payload_writer.position) != ENC_OK) {
            return stream_writer.status;
        }
        result->candidate_stream_size = stream_writer.position;
        result->candidate_squared_error[0U] = candidate_error;
        result->candidate_squared_error[1U] = chroma_error[0U];
        result->candidate_squared_error[2U] = chroma_error[1U];
        admission_luma_error = candidate_error;
        /*
         * Keep every plane non-regressing, then require a strict complete
         * stream RD win so a lower-SSE, larger-rate candidate is rejected.
         */
        result->admitted =
            admission_luma_error <= baseline.squared_error[0U] &&
            chroma_error[0U] <= baseline.squared_error[1U] &&
            chroma_error[1U] <= baseline.squared_error[2U] &&
            rd_cost(
                admission_luma_error + chroma_error[0U] + chroma_error[1U],
                stream_writer.position * 8U, qlookup(qindex)) <
                rd_cost(
                    baseline.squared_error[0U] +
                        baseline.squared_error[1U] +
                        baseline.squared_error[2U],
                    baseline.stream_size * 8U, qlookup(qindex));
        if ((result->admitted ? stream_writer.position : baseline.stream_size) >
            stream_capacity) {
            clear_yuv_result(result);
            return ENC_OUT_OF_SPACE;
        }
        for (plane = 0U; plane < 3U; ++plane) {
            size_t y;
            const uint8_t *selected =
                result->admitted ? coded[plane] : baseline_planes[plane];
            size_t selected_stride =
                result->admitted
                    ? layout.coded_plane_width[plane] :
                      layout.plane_width[plane];
            for (y = 0U; y < layout.plane_height[plane]; ++y) {
                status = enc_memory_copy(
                    reconstruction->planes[plane].data +
                        y * reconstruction->planes[plane].stride,
                    reconstruction->planes[plane].capacity -
                        y * reconstruction->planes[plane].stride,
                    selected + y * selected_stride,
                    layout.plane_width[plane]);
                if (status != ENC_OK) return status;
            }
            result->squared_error[plane] = result->admitted
                ? (plane == 0U ? candidate_error : chroma_error[plane - 1U])
                : baseline.squared_error[plane];
        }
        status = enc_memory_copy(
            stream, stream_capacity,
            result->admitted ? candidate_stream : baseline_stream,
            result->admitted ? stream_writer.position : baseline.stream_size);
        if (status != ENC_OK) return status;
        result->stream_size =
            result->admitted ? stream_writer.position : baseline.stream_size;
        result->workspace_size = layout.size;
        result->ac_blocks = result->admitted ? ac_blocks : 0U;
        result->qindex = qindex;
        result->format = input->format;
        result->u_qdelta = result->admitted ? (int8_t)u_qdelta : 0;
        result->v_qdelta = result->admitted ? (int8_t)v_qdelta : 0;
        return ENC_OK;
    }

EncStatus enc_ac8x8_yuv8_q_workspace(
    const EncM3Image8 *input, unsigned int qindex,
    EncAc8x8CoefficientSearch search,
    void *workspace, size_t workspace_capacity,
    void *stream, size_t stream_capacity,
    EncM3MutableImage8 *reconstruction,
    EncAc8x8YuvResult *result) {
    static const int8_t qdelta_values[5U] = {0, -16, -8, 8, 16};
    EncAc8x8YuvResult best;
    EncAc8x8YuvResult trial;
    EncAc8x8YuvResult luma_stats;
    uint64_t luma_selection_error = 0U;
    int best_u = 0;
    int best_v = 0;
    int qdelta_y_preserved = 1;
    size_t qdelta_trials = 1U;
    size_t delta_index;
    EncStatus status;
    if (search != ENC_AC8X8_COEFFICIENT_SEARCH_RD ||
        input == 0 || input->format == ENC_M3_PIXEL_I400) {
        return enc_ac8x8_yuv8_q_workspace_inner(
            input, qindex, search, workspace, workspace_capacity,
            stream, stream_capacity, reconstruction, result, 0, 0, 0, 0, 0);
    }
    status = enc_ac8x8_yuv8_q_workspace_inner(
        input, qindex, search, workspace, workspace_capacity,
        stream, stream_capacity, reconstruction, &best, 0, 0, 0, 0,
        &luma_selection_error);
    if (status != ENC_OK) return status;
    luma_stats = best;
    /*
     * qdelta changes only one chroma plane at a time. Search U around the
     * mandatory (0,0) first candidate, then V around the selected U. Every
     * point remains a complete-stream trial with replayed luma decisions,
     * while avoiding the 25-point Cartesian product.
     */
    for (delta_index = 1U; delta_index < 5U; ++delta_index) {
        uint64_t best_error;
        uint64_t trial_error;
        uint64_t best_cost;
        uint64_t trial_cost;
        int u_delta = qdelta_values[delta_index];
        ++qdelta_trials;
        status = enc_ac8x8_yuv8_q_workspace_inner(
            input, qindex, search, workspace, workspace_capacity,
            stream, stream_capacity, reconstruction, &trial,
            1, u_delta, 0, 1, &luma_selection_error);
        if (status == ENC_OUT_OF_SPACE) {
            qdelta_y_preserved = 0;
            continue;
        }
        if (status != ENC_OK) return status;
        if (trial.candidate_squared_error[0U] != luma_selection_error) {
            qdelta_y_preserved = 0;
        }
        if (trial.squared_error[0U] != best.squared_error[0U] ||
            trial.squared_error[1U] > best.squared_error[1U] ||
            trial.squared_error[2U] > best.squared_error[2U]) {
            continue;
        }
        best_error = best.squared_error[0U] + best.squared_error[1U] +
                     best.squared_error[2U];
        trial_error = trial.squared_error[0U] + trial.squared_error[1U] +
                      trial.squared_error[2U];
        best_cost = rd_cost(
            best_error, best.stream_size * 8U, qlookup(qindex));
        trial_cost = rd_cost(
            trial_error, trial.stream_size * 8U, qlookup(qindex));
        if (trial_cost < best_cost) {
            best = trial;
            best_u = u_delta;
        }
    }
    for (delta_index = 1U; delta_index < 5U; ++delta_index) {
        uint64_t best_error;
        uint64_t trial_error;
        uint64_t best_cost;
        uint64_t trial_cost;
        int v_delta = qdelta_values[delta_index];
        ++qdelta_trials;
        status = enc_ac8x8_yuv8_q_workspace_inner(
            input, qindex, search, workspace, workspace_capacity,
            stream, stream_capacity, reconstruction, &trial,
            1, best_u, v_delta, 1, &luma_selection_error);
        if (status == ENC_OUT_OF_SPACE) {
            qdelta_y_preserved = 0;
            continue;
        }
        if (status != ENC_OK) return status;
        if (trial.candidate_squared_error[0U] != luma_selection_error) {
            qdelta_y_preserved = 0;
        }
        if (trial.squared_error[0U] != best.squared_error[0U] ||
            trial.squared_error[1U] > best.squared_error[1U] ||
            trial.squared_error[2U] > best.squared_error[2U]) {
            continue;
        }
        best_error = best.squared_error[0U] + best.squared_error[1U] +
                     best.squared_error[2U];
        trial_error = trial.squared_error[0U] + trial.squared_error[1U] +
                      trial.squared_error[2U];
        best_cost = rd_cost(
            best_error, best.stream_size * 8U, qlookup(qindex));
        trial_cost = rd_cost(
            trial_error, trial.stream_size * 8U, qlookup(qindex));
        if (trial_cost < best_cost) {
            best = trial;
            best_v = v_delta;
        }
    }
    status = enc_ac8x8_yuv8_q_workspace_inner(
        input, qindex, search, workspace, workspace_capacity,
        stream, stream_capacity, reconstruction, result,
        best_u != 0 || best_v != 0, best_u, best_v, 1,
        &luma_selection_error);
    if (status == ENC_OK) {
        result->rd_qdelta_trials = qdelta_trials;
        result->rd_qdelta_y_preserved =
            qdelta_y_preserved &&
            result->candidate_squared_error[0U] == luma_selection_error;
    }
    if (status == ENC_OK && result->admitted) {
        for (delta_index = 0U; delta_index < 7U; ++delta_index) {
            result->rd_mode_counts[delta_index] =
                luma_stats.rd_mode_counts[delta_index];
        }
        for (delta_index = 0U; delta_index < 3U; ++delta_index) {
            result->rd_residual_counts[delta_index] =
                luma_stats.rd_residual_counts[delta_index];
        }
    }
    return status;
}

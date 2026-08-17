#include <stdio.h>

#include "../src/ac8x8.c"

#define ORACLE_MAX_WIDTH 16U
#define ORACLE_MAX_HEIGHT 16U
#define ORACLE_MAX_PIXELS (ORACLE_MAX_WIDTH * ORACLE_MAX_HEIGHT)
#define ORACLE_STREAM_CAPACITY 4096U
#define ORACLE_WORKSPACE_CAPACITY 262144U
#define ORACLE_MAX_LEAVES 4U
#define ORACLE_MULTI_WIDTH (ORACLE_MAX_LEAVES * 16U)
#define ORACLE_MULTI_HEIGHT 16U
#define ORACLE_MULTI_PIXELS (ORACLE_MULTI_WIDTH * ORACLE_MULTI_HEIGHT)
#define ORACLE_MULTI_TRANSFORMS (ORACLE_MULTI_PIXELS / 64U)

typedef union {
    uint64_t alignment;
    uint8_t bytes[ORACLE_WORKSPACE_CAPACITY];
} OracleWorkspace;

typedef struct {
    const uint8_t *input;
    size_t width;
    size_t height;
    size_t stride;
    unsigned int qindex;
    uint8_t mode;
    int16_t levels[4U][64U];
    uint64_t squared_error;
    uint8_t reconstruction[ORACLE_MAX_PIXELS];
    size_t calls;
} OracleCandidate;

static uint8_t oracle_input[ORACLE_MAX_PIXELS];
static uint8_t oracle_baseline_reconstruction[ORACLE_MAX_PIXELS];
static uint8_t oracle_local_reconstruction[ORACLE_MAX_PIXELS];
static uint8_t oracle_baseline_stream[ORACLE_STREAM_CAPACITY];
static uint8_t oracle_local_stream[ORACLE_STREAM_CAPACITY];
static uint8_t oracle_tile[ORACLE_STREAM_CAPACITY];
static uint8_t oracle_payload[ORACLE_STREAM_CAPACITY];
static uint8_t oracle_stream[ORACLE_STREAM_CAPACITY];
static uint8_t oracle_best_stream[ORACLE_STREAM_CAPACITY];
static OracleWorkspace oracle_workspace;

typedef struct {
    const uint8_t *input;
    const uint8_t *incumbent;
    size_t width;
    size_t height;
    size_t stride;
    unsigned int qindex;
    uint8_t reconstruction[ORACLE_MULTI_PIXELS];
    uint8_t reconstructed_cells[32U];
    uint8_t choices[ORACLE_MAX_LEAVES];
    uint8_t modes[ORACLE_MAX_LEAVES];
    int16_t levels[ORACLE_MULTI_TRANSFORMS][64U];
    size_t leaf_count;
    size_t level_count;
    uint64_t squared_error;
    int valid;
} MultiOracleState;

static uint8_t multi_input[ORACLE_MULTI_PIXELS];
static uint8_t multi_incumbent[ORACLE_MULTI_PIXELS];
static uint8_t multi_local_reconstruction[ORACLE_MULTI_PIXELS];
static uint8_t multi_local_stream[ORACLE_STREAM_CAPACITY];
static uint8_t qdelta_input[64U * 64U * 3U];
static uint8_t qdelta_reconstruction[64U * 64U * 3U];
static uint8_t qdelta_frozen_y[64U * 64U];

static EncStatus oracle_fixed_candidate(
    void *opaque, void *transaction,
    size_t block_x, size_t block_y,
    size_t block_width, size_t block_height,
    uint8_t *selected_mode, int16_t selected_levels[4U][64U]) {
    OracleCandidate *candidate = (OracleCandidate *)opaque;
    size_t transform_count = (block_width / 8U) * (block_height / 8U);
    size_t tx;
    size_t index;
    (void)transaction;
    if (candidate == 0 || selected_mode == 0 ||
        block_x != 0U || block_y != 0U ||
        block_width != candidate->width ||
        block_height != candidate->height ||
        transform_count == 0U || transform_count > 4U) {
        return ENC_INVALID_ARGUMENT;
    }
    *selected_mode = candidate->mode;
    for (tx = 0U; tx < transform_count; ++tx) {
        for (index = 0U; index < 64U; ++index) {
            selected_levels[tx][index] = candidate->levels[tx][index];
        }
    }
    ++candidate->calls;
    return ENC_OK;
}

static int oracle_build_candidate(
    OracleCandidate *candidate, uint8_t mode, size_t residual_kind) {
    uint8_t padded_reconstruction[ORACLE_MAX_PIXELS] = {0};
    uint8_t reconstructed_cells[32U] = {0};
    size_t transform_count =
        (candidate->width / 8U) * (candidate->height / 8U);
    uint32_t q = qlookup(candidate->qindex);
    size_t tx;
    size_t y;
    size_t x;
    candidate->mode = mode;
    candidate->squared_error = 0U;
    for (tx = 0U; tx < transform_count; ++tx) {
        size_t tx_x = (tx % (candidate->width / 8U)) * 8U;
        size_t tx_y = (tx / (candidate->width / 8U)) * 8U;
        uint8_t samples[64U];
        uint8_t prediction8[256U];
        uint8_t decoded[64U];
        uint8_t unused[64U];
        int16_t ac_levels[64U];
        size_t index;
        source_block(
            candidate->input, candidate->width, candidate->height,
            candidate->stride, tx_x, tx_y, samples);
        rd_intra_prediction(
            mode, padded_reconstruction, candidate->width,
            candidate->width, candidate->height, tx_x, tx_y, 0U, 0U,
            reconstructed_cells, 8U, 8U, 1, prediction8);
        if (residual_kind != 0U) {
            search_block(
                samples, prediction8, candidate->qindex, 1, 0,
                ac_levels, unused);
        }
        for (index = 0U; index < 64U; ++index) {
            candidate->levels[tx][index] = residual_kind == 0U ? 0 :
                residual_kind == 1U && index != 0U ? 0 : ac_levels[index];
        }
        (void)reconstruct8(
            samples, candidate->levels[tx], prediction8, q, decoded);
        candidate->squared_error += active_error(
            candidate->input, candidate->width, candidate->height,
            candidate->stride, tx_x, tx_y, decoded);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                candidate->reconstruction[
                    (tx_y + y) * candidate->width + tx_x + x] =
                    decoded[y * 8U + x];
                padded_reconstruction[
                    (tx_y + y) * candidate->width + tx_x + x] =
                    decoded[y * 8U + x];
            }
        }
        rd_mark_reconstructed(
            0U, 0U, reconstructed_cells, tx_x, tx_y, 8U, 8U);
    }
    return 1;
}

static EncStatus oracle_complete_stream(
    OracleCandidate *candidate, size_t *stream_size) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    EncStatus status;
    candidate->calls = 0U;
    status = enc_ac8x8_rd_grid_tile_q(
        oracle_tile, sizeof(oracle_tile), &tile_size,
        candidate->width, candidate->height, candidate->qindex,
        oracle_fixed_candidate, candidate);
    if (status != ENC_OK || candidate->calls != 1U) {
        return status != ENC_OK ? status : ENC_INVALID_ARGUMENT;
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size,
        candidate->width, candidate->height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q_for_size(
        frame_header, sizeof(frame_header), &frame_header_size,
        candidate->width, candidate->height, candidate->qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, oracle_payload, sizeof(oracle_payload));
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, oracle_tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(
        &stream_writer, oracle_stream, sizeof(oracle_stream));
    if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 4U, oracle_payload,
            payload_writer.position) != ENC_OK) {
        return stream_writer.status;
    }
    *stream_size = stream_writer.position;
    return ENC_OK;
}

static int equal_bytes(const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static void multi_candidate_levels(
        size_t leaf_index, size_t candidate_index, size_t transform_count,
        int16_t levels[4U][64U]) {
        size_t residual = candidate_index % 3U;
        size_t mode_index = candidate_index / 3U;
        size_t tx;
        size_t coefficient;
        for (tx = 0U; tx < 4U; ++tx) {
            for (coefficient = 0U; coefficient < 64U; ++coefficient) {
                levels[tx][coefficient] = 0;
            }
        }
        if (residual == 0U) return;
        for (tx = 0U; tx < transform_count; ++tx) {
            int sign = ((leaf_index + mode_index + tx) & 1U) != 0U ? -1 : 1;
            levels[tx][0U] = (int16_t)(sign * (1 + (int)((leaf_index + tx) & 1U)));
            if (residual == 2U) {
                size_t position = ((mode_index + tx) & 1U) != 0U ? 1U : 8U;
                levels[tx][position] = (int16_t)-sign;
            }
        }
    }

    static uint64_t multi_incumbent_error(
        const MultiOracleState *state,
        size_t block_x, size_t block_y,
        size_t block_width, size_t block_height) {
        uint64_t error = 0U;
        size_t y;
        size_t x;
        for (y = 0U; y < block_height; ++y) {
            for (x = 0U; x < block_width; ++x) {
                int difference =
                    (int)state->input[(block_y + y) * state->stride + block_x + x] -
                    (int)state->incumbent[
                        (block_y + y) * state->stride + block_x + x];
                error += (uint64_t)(difference * difference);
            }
        }
        return error;
    }

    static int multi_apply_candidate(
        MultiOracleState *state, size_t candidate_index,
        size_t block_x, size_t block_y,
        size_t block_width, size_t block_height,
        int commit, int16_t selected_levels[4U][64U],
        uint64_t *squared_error) {
        static const uint8_t modes[7U] = {0U, 1U, 2U, 5U, 6U, 7U, 8U};
        uint8_t saved_reconstruction[256U];
        uint8_t saved_cells[32U];
        int16_t levels[4U][64U];
        size_t transform_count = (block_width / 8U) * (block_height / 8U);
        uint32_t q = qlookup(state->qindex);
        size_t tx;
        size_t y;
        size_t x;
        if (candidate_index >= 21U || transform_count == 0U ||
            transform_count > 4U || state->leaf_count >= ORACLE_MAX_LEAVES) {
            return 0;
        }
        multi_candidate_levels(
            state->leaf_count, candidate_index, transform_count, levels);
        for (y = 0U; y < block_height; ++y) {
            for (x = 0U; x < block_width; ++x) {
                saved_reconstruction[y * block_width + x] =
                    state->reconstruction[
                        (block_y + y) * state->stride + block_x + x];
            }
        }
        for (x = 0U; x < sizeof(saved_cells); ++x) {
            saved_cells[x] = state->reconstructed_cells[x];
        }
        *squared_error = 0U;
        for (tx = 0U; tx < transform_count; ++tx) {
            size_t tx_x = (tx % (block_width / 8U)) * 8U;
            size_t tx_y = (tx / (block_width / 8U)) * 8U;
            uint8_t samples[64U];
            uint8_t prediction[256U];
            uint8_t decoded[64U];
            source_block(
                state->input, state->width, state->height, state->stride,
                block_x + tx_x, block_y + tx_y, samples);
            rd_intra_prediction(
                modes[candidate_index / 3U], state->reconstruction, state->stride,
                state->width, state->height, block_x + tx_x, block_y + tx_y,
                0U, 0U, state->reconstructed_cells, 8U, 8U, 1, prediction);
            (void)reconstruct8(samples, levels[tx], prediction, q, decoded);
            *squared_error += active_error(
                state->input, state->width, state->height, state->stride,
                block_x + tx_x, block_y + tx_y, decoded);
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    state->reconstruction[
                        (block_y + tx_y + y) * state->stride +
                        block_x + tx_x + x] = decoded[y * 8U + x];
                }
            }
            rd_mark_reconstructed(
                0U, 0U, state->reconstructed_cells,
                block_x + tx_x, block_y + tx_y, 8U, 8U);
        }
        if (commit) {
            state->modes[state->leaf_count] = modes[candidate_index / 3U];
            for (tx = 0U; tx < transform_count; ++tx) {
                size_t coefficient;
                for (coefficient = 0U; coefficient < 64U; ++coefficient) {
                    selected_levels[tx][coefficient] = levels[tx][coefficient];
                    state->levels[state->level_count + tx][coefficient] =
                        levels[tx][coefficient];
                }
            }
            state->level_count += transform_count;
            ++state->leaf_count;
            state->squared_error += *squared_error;
            return 1;
        }
        for (y = 0U; y < block_height; ++y) {
            for (x = 0U; x < block_width; ++x) {
                state->reconstruction[
                    (block_y + y) * state->stride + block_x + x] =
                    saved_reconstruction[y * block_width + x];
            }
        }
        for (x = 0U; x < sizeof(saved_cells); ++x) {
            state->reconstructed_cells[x] = saved_cells[x];
        }
        return 1;
    }

    static EncStatus multi_fixed_callback(
        void *opaque, void *transaction,
        size_t block_x, size_t block_y,
        size_t block_width, size_t block_height,
        uint8_t *selected_mode, int16_t selected_levels[4U][64U]) {
        MultiOracleState *state = (MultiOracleState *)opaque;
        size_t leaf_index = state->leaf_count;
        size_t choice;
        uint64_t error;
        uint64_t incumbent_error;
        (void)transaction;
        if (leaf_index >= ORACLE_MAX_LEAVES) return ENC_INVALID_ARGUMENT;
        choice = state->choices[leaf_index];
        incumbent_error = multi_incumbent_error(
            state, block_x, block_y, block_width, block_height);
        if (!multi_apply_candidate(
                state, choice, block_x, block_y, block_width, block_height,
                1, selected_levels, &error)) {
            return ENC_INVALID_ARGUMENT;
        }
        *selected_mode = state->modes[leaf_index];
        if (error > incumbent_error) state->valid = 0;
        return ENC_OK;
    }

    static EncStatus multi_local_callback(
        void *opaque, void *transaction,
        size_t block_x, size_t block_y,
        size_t block_width, size_t block_height,
        uint8_t *selected_mode, int16_t selected_levels[4U][64U]) {
        MultiOracleState *state = (MultiOracleState *)opaque;
        uint64_t incumbent_error = multi_incumbent_error(
            state, block_x, block_y, block_width, block_height);
        uint64_t best_cost = UINT64_MAX;
        size_t best = SIZE_MAX;
        size_t candidate;
        for (candidate = 0U; candidate < 21U; ++candidate) {
            int16_t levels[4U][64U];
            uint64_t error;
            size_t bits;
            uint64_t cost;
            size_t transform_count = (block_width / 8U) * (block_height / 8U);
            multi_candidate_levels(
                state->leaf_count, candidate, transform_count, levels);
            if (!multi_apply_candidate(
                    state, candidate, block_x, block_y,
                    block_width, block_height, 0, levels, &error) ||
                enc_ac8x8_rd_transaction_trial(
                    transaction, (uint8_t[]){0U, 1U, 2U, 5U, 6U, 7U, 8U}
                        [candidate / 3U],
                    &levels[0U][0U], transform_count, &bits) != ENC_OK) {
                return ENC_INVALID_ARGUMENT;
            }
            cost = rd_cost(error, bits, qlookup(state->qindex));
            if (error <= incumbent_error && (best == SIZE_MAX || cost < best_cost)) {
                best = candidate;
                best_cost = cost;
            }
        }
        if (best == SIZE_MAX) return ENC_INVALID_ARGUMENT;
        {
            uint64_t error;
            state->choices[state->leaf_count] = (uint8_t)best;
            if (!multi_apply_candidate(
                    state, best, block_x, block_y, block_width, block_height,
                    1, selected_levels, &error)) {
                return ENC_INVALID_ARGUMENT;
            }
        }
        *selected_mode = state->modes[state->leaf_count - 1U];
        return ENC_OK;
    }

    static void multi_init_state(
        MultiOracleState *state, size_t width, unsigned int qindex) {
        size_t index;
        state->input = multi_input;
        state->incumbent = multi_incumbent;
        state->width = width;
        state->height = ORACLE_MULTI_HEIGHT;
        state->stride = width;
        state->qindex = qindex;
        state->leaf_count = 0U;
        state->level_count = 0U;
        state->squared_error = 0U;
        state->valid = 1;
        for (index = 0U; index < width * ORACLE_MULTI_HEIGHT; ++index) {
            state->reconstruction[index] = 0U;
        }
        for (index = 0U; index < sizeof(state->reconstructed_cells); ++index) {
            state->reconstructed_cells[index] = 0U;
        }
    }

    static EncStatus complete_stream_for_tile(
        size_t width, size_t height, unsigned int qindex,
        const uint8_t *tile, size_t tile_size,
        uint8_t *stream, size_t stream_capacity, size_t *stream_size) {
        uint8_t sequence[16U];
        uint8_t frame_header[8U];
        EncByteWriter payload_writer;
        EncByteWriter stream_writer;
        size_t sequence_size;
        size_t frame_header_size;
        EncStatus status;
        status = enc_m3_sequence_header(
            sequence, sizeof(sequence), &sequence_size, width, height);
        if (status != ENC_OK) return status;
        status = enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            width, height, qindex);
        if (status != ENC_OK) return status;
        enc_byte_writer_init(
            &payload_writer, oracle_payload, sizeof(oracle_payload));
        if (enc_byte_writer_write(
                &payload_writer, frame_header, frame_header_size) != ENC_OK ||
            enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
            return payload_writer.status;
        }
        enc_byte_writer_init(&stream_writer, stream, stream_capacity);
        if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
            enc_obu_write(
                &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
            enc_obu_write(
                &stream_writer, 4U, oracle_payload,
                payload_writer.position) != ENC_OK) {
            return stream_writer.status;
        }
        *stream_size = stream_writer.position;
        return ENC_OK;
    }

    static void multi_generate_input(size_t width) {
        size_t y;
        size_t x;
        for (y = 0U; y < ORACLE_MULTI_HEIGHT; ++y) {
            for (x = 0U; x < width; ++x) {
                multi_input[y * width + x] = 128U;
            }
        }
    }

    static int multi_build_incumbent(
        size_t width, unsigned int qindex, size_t expected_leaves) {
        MultiOracleState state;
        size_t leaf;
        size_t tile_size;
        EncStatus status;
        multi_init_state(&state, width, qindex);
        state.incumbent = state.reconstruction;
        for (leaf = 0U; leaf < expected_leaves; ++leaf) {
            state.choices[leaf] = 2U;
        }
        status = enc_ac8x8_rd_grid_tile_q(
            oracle_tile, sizeof(oracle_tile), &tile_size,
            width, ORACLE_MULTI_HEIGHT, qindex, multi_fixed_callback, &state);
        if (status != ENC_OK || state.leaf_count != expected_leaves) return 0;
        for (leaf = 0U; leaf < width * ORACLE_MULTI_HEIGHT; ++leaf) {
            multi_incumbent[leaf] = state.reconstruction[leaf];
        }
        return 1;
    }

    static int run_multi_oracle(size_t expected_leaves) {
        const unsigned int qindex = 100U;
        size_t width = expected_leaves * 16U;
        MultiOracleState local;
        MultiOracleState trial;
        size_t local_size = 0U;
        size_t tile_size;
        size_t leaf;
        size_t index;
        EncStatus status;
        multi_generate_input(width);
        if (!multi_build_incumbent(width, qindex, expected_leaves)) return 0;
        multi_init_state(&local, width, qindex);
        status = enc_ac8x8_rd_grid_tile_q(
            oracle_tile, sizeof(oracle_tile), &tile_size,
            width, ORACLE_MULTI_HEIGHT, qindex, multi_local_callback, &local);
        if (status != ENC_OK || local.leaf_count != expected_leaves ||
            complete_stream_for_tile(
                width, ORACLE_MULTI_HEIGHT, qindex, oracle_tile, tile_size,
                multi_local_stream, sizeof(multi_local_stream),
                &local_size) != ENC_OK) {
            return 0;
        }
        for (index = 0U; index < width * ORACLE_MULTI_HEIGHT; ++index) {
            multi_local_reconstruction[index] = local.reconstruction[index];
        }
        multi_init_state(&trial, width, qindex);
        for (index = 0U; index < expected_leaves; ++index) {
            trial.choices[index] = local.choices[index];
        }
        status = enc_ac8x8_rd_grid_tile_q(
            oracle_tile, sizeof(oracle_tile), &tile_size,
            width, ORACLE_MULTI_HEIGHT, qindex, multi_fixed_callback, &trial);
        if (status != ENC_OK || trial.leaf_count != expected_leaves ||
            complete_stream_for_tile(
                width, ORACLE_MULTI_HEIGHT, qindex, oracle_tile, tile_size,
                oracle_stream, sizeof(oracle_stream), &index) != ENC_OK ||
            index != local_size ||
            trial.squared_error != local.squared_error ||
            !equal_bytes(multi_local_stream, oracle_stream, local_size) ||
            !equal_bytes(
                multi_local_reconstruction, trial.reconstruction,
                width * ORACLE_MULTI_HEIGHT) ||
            !equal_bytes(local.modes, trial.modes, expected_leaves) ||
            !equal_bytes(
                (const uint8_t *)local.levels, (const uint8_t *)trial.levels,
                local.level_count * 64U * sizeof(int16_t))) {
            return 0;
        }
        for (leaf = 0U; leaf < expected_leaves; ++leaf) {
            uint64_t best_cost = UINT64_MAX;
            size_t best_choice = SIZE_MAX;
            size_t candidate;
            for (candidate = 0U; candidate < 21U; ++candidate) {
                size_t stream_size;
                uint64_t cost;
                multi_init_state(&trial, width, qindex);
                for (index = 0U; index < expected_leaves; ++index) {
                    trial.choices[index] = local.choices[index];
                }
                trial.choices[leaf] = (uint8_t)candidate;
                status = enc_ac8x8_rd_grid_tile_q(
                    oracle_tile, sizeof(oracle_tile), &tile_size,
                    width, ORACLE_MULTI_HEIGHT, qindex,
                    multi_fixed_callback, &trial);
                if (status != ENC_OK || trial.leaf_count != expected_leaves ||
                    complete_stream_for_tile(
                        width, ORACLE_MULTI_HEIGHT, qindex,
                        oracle_tile, tile_size, oracle_stream,
                        sizeof(oracle_stream), &stream_size) != ENC_OK) {
                    return 0;
                }
                if (!trial.valid) continue;
                cost = rd_cost(
                    trial.squared_error, stream_size * 8U, qlookup(qindex));
                if (cost < best_cost) {
                    best_cost = cost;
                    best_choice = candidate;
                }
            }
            if (best_choice != local.choices[leaf]) {
                printf(
                    "multi-oracle mismatch leaves=%zu leaf=%zu "
                    "local_choice=%u best_choice=%zu\n",
                    expected_leaves, leaf, local.choices[leaf], best_choice);
                return 0;
            }
        }
        printf(
            "multi-oracle leaves=%zu trials=%zu bytes=%zu sse=%llu\n",
            expected_leaves, expected_leaves * 21U, local_size,
            (unsigned long long)local.squared_error);
        return 1;
    }

static void generate_fixture(size_t fixture) {
    size_t y;
    size_t x;
    for (y = 0U; y < ORACLE_MAX_HEIGHT; ++y) {
        for (x = 0U; x < ORACLE_MAX_WIDTH; ++x) {
            unsigned int value;
            switch (fixture % 8U) {
                case 0U:
                    value = 16U + x * 12U + y * 3U;
                    break;
                case 1U:
                    value = 32U + x * 4U + y * 11U;
                    break;
                case 2U:
                    value = x < 8U ? 24U + y * 5U : 220U - y * 4U;
                    break;
                case 3U:
                    value = y < 8U ? 24U + x * 5U : 220U - x * 4U;
                    break;
                case 4U:
                    value = ((x + y) & 1U) != 0U ? 224U : 32U;
                    break;
                case 5U:
                    value = 128U + (int)(x * y) - 4 * (int)x - 3 * (int)y;
                    break;
                case 6U:
                    value = x > y ? 224U : 32U;
                    break;
                default:
                    value = 128U + ((x * 29U + y * 47U + fixture * 13U) & 63U);
                    break;
            }
            oracle_input[y * ORACLE_MAX_WIDTH + x] =
                (uint8_t)clip_int(0, 255, (int)value);
        }
    }
}

static int run_fixture(size_t fixture, unsigned int qindex, int verbose) {
    static const uint8_t modes[7U] = {0U, 1U, 2U, 5U, 6U, 7U, 8U};
    OracleCandidate candidates[21U];
    EncAc8x8Result baseline;
    EncAc8x8Result local;
    EncAc8x8WorkspaceLayout layout;
    uint64_t best_cost;
    size_t best = SIZE_MAX;
    size_t best_size;
    size_t mode_index;
    size_t residual_kind;
    size_t index;
    EncStatus status;
    generate_fixture(fixture);
    status = enc_ac8x8_query_workspace(
        ORACLE_MAX_WIDTH, ORACLE_MAX_HEIGHT, &layout);
    if (status != ENC_OK || layout.size > sizeof(oracle_workspace.bytes)) {
        return 0;
    }
    status = enc_ac8x8_i400_q_workspace(
        oracle_input, ORACLE_MAX_WIDTH, ORACLE_MAX_HEIGHT,
        ORACLE_MAX_WIDTH, qindex, ENC_AC8X8_SEARCH_BOUNDED,
        oracle_workspace.bytes, sizeof(oracle_workspace.bytes),
        oracle_baseline_stream, sizeof(oracle_baseline_stream),
        oracle_baseline_reconstruction,
        sizeof(oracle_baseline_reconstruction), &baseline);
    if (status != ENC_OK) return 0;
    best_cost = rd_cost(
        baseline.squared_error, baseline.stream_size * 8U, qlookup(qindex));
    best_size = baseline.stream_size;
    for (index = 0U; index < best_size; ++index) {
        oracle_best_stream[index] = oracle_baseline_stream[index];
    }
    for (mode_index = 0U; mode_index < 7U; ++mode_index) {
        for (residual_kind = 0U; residual_kind < 3U; ++residual_kind) {
            OracleCandidate *candidate =
                &candidates[mode_index * 3U + residual_kind];
            size_t stream_size = 0U;
            uint64_t cost;
            candidate->input = oracle_input;
            candidate->width = ORACLE_MAX_WIDTH;
            candidate->height = ORACLE_MAX_HEIGHT;
            candidate->stride = ORACLE_MAX_WIDTH;
            candidate->qindex = qindex;
            if (!oracle_build_candidate(
                    candidate, modes[mode_index], residual_kind) ||
                oracle_complete_stream(candidate, &stream_size) != ENC_OK) {
                return 0;
            }
            cost = rd_cost(candidate->squared_error, stream_size * 8U,
                           qlookup(qindex));
            if (candidate->squared_error <= baseline.squared_error &&
                cost < best_cost) {
                best_cost = cost;
                best = mode_index * 3U + residual_kind;
                best_size = stream_size;
                for (index = 0U; index < stream_size; ++index) {
                    oracle_best_stream[index] = oracle_stream[index];
                }
            }
        }
    }
    status = enc_ac8x8_i400_q_workspace(
        oracle_input, ORACLE_MAX_WIDTH, ORACLE_MAX_HEIGHT,
        ORACLE_MAX_WIDTH, qindex, ENC_AC8X8_SEARCH_RD,
        oracle_workspace.bytes, sizeof(oracle_workspace.bytes),
        oracle_local_stream, sizeof(oracle_local_stream),
        oracle_local_reconstruction, sizeof(oracle_local_reconstruction),
        &local);
    if (status != ENC_OK) return 0;
    if (verbose || (best != SIZE_MAX &&
                    (candidates[best].mode != 0U || best % 3U == 2U))) {
        printf(
            "fixture=%zu q=%u best=%zu mode=%u residual=%zu bytes=%zu "
            "sse=%llu baseline_bytes=%zu baseline_sse=%llu "
            "local_admitted=%d local_bytes=%zu local_sse=%llu\n",
            fixture, qindex, best,
            best == SIZE_MAX ? 255U : candidates[best].mode,
            best == SIZE_MAX ? SIZE_MAX : best % 3U, best_size,
            (unsigned long long)(best == SIZE_MAX
                ? baseline.squared_error : candidates[best].squared_error),
            baseline.stream_size, (unsigned long long)baseline.squared_error,
            local.admitted, local.stream_size,
            (unsigned long long)local.squared_error);
    }
    if (local.stream_size != best_size ||
        local.squared_error != (best == SIZE_MAX
            ? baseline.squared_error : candidates[best].squared_error) ||
        local.admitted != (best != SIZE_MAX) ||
        !equal_bytes(oracle_local_stream, oracle_best_stream, best_size) ||
        !equal_bytes(
            oracle_local_reconstruction,
            best == SIZE_MAX
                ? oracle_baseline_reconstruction
                : candidates[best].reconstruction,
            ORACLE_MAX_PIXELS) ||
        (best != SIZE_MAX &&
         (local.rd_mode_counts[best / 3U] != 1U ||
          local.rd_residual_counts[best % 3U] != 1U))) {
        printf("local/oracle mismatch fixture=%zu q=%u\n", fixture, qindex);
        return 0;
    }
    if (fixture == 1U && qindex == 100U &&
        (best != 14U || best_size != 43U ||
         candidates[best].squared_error != 1574U ||
         baseline.stream_size != 45U || baseline.squared_error != 1624U ||
         !local.admitted || local.candidate_trials != 21U)) {
        return 0;
    }
    if (fixture == 0U && qindex == 40U &&
        (best != 20U || best_size != 61U ||
         candidates[best].squared_error != 175153U ||
         baseline.stream_size != 34U || baseline.squared_error != 205696U ||
         !local.admitted || local.candidate_trials != 21U)) {
        return 0;
    }
    if (fixture == 3U && qindex == 140U &&
        (best != 8U || best_size != 33U ||
         candidates[best].squared_error != 10936U ||
         baseline.stream_size != 33U || baseline.squared_error != 11560U ||
         !local.admitted || local.candidate_trials != 21U)) {
        return 0;
    }
    for (index = 0U; index < ORACLE_MAX_PIXELS; ++index) {
        oracle_local_reconstruction[index] = 0U;
    }
    return 1;
}

static int run_no_nonregressing_fallback(void) {
    EncAc8x8RdSearchState search;
    int16_t levels[4U][64U] = {{0}};
    uint8_t modes[4U] = {0U};
    uint8_t reconstruction[ORACLE_MAX_PIXELS] = {0U};
    uint8_t tile[ORACLE_STREAM_CAPACITY];
    size_t tile_size;
    size_t index;
    size_t selected_modes = 0U;
    size_t selected_residuals = 0U;
    generate_fixture(4U);
    search.input = oracle_input;
    search.width = ORACLE_MAX_WIDTH;
    search.height = ORACLE_MAX_HEIGHT;
    search.stride = ORACLE_MAX_WIDTH;
    search.padded_reconstruction = reconstruction;
    search.padded_width = ORACLE_MAX_WIDTH;
    search.qindex = 100U;
    search.superblock_x = SIZE_MAX;
    search.superblock_y = SIZE_MAX;
    for (index = 0U; index < sizeof(search.reconstructed_cells); ++index) {
        search.reconstructed_cells[index] = 0U;
    }
    search.squared_error = 0U;
    search.searched_blocks = 0U;
    search.ac_blocks = 0U;
    search.trials = 0U;
    search.stored_levels = &levels[0U][0U];
    search.stored_modes = modes;
    search.stored_block_count = 4U;
    search.stored_level_index = 0U;
    search.stored_mode_index = 0U;
    search.incumbent_reconstruction = oracle_input;
    search.incumbent_stride = ORACLE_MAX_WIDTH;
    search.replay = 0;
    for (index = 0U; index < 7U; ++index) search.mode_counts[index] = 0U;
    for (index = 0U; index < 3U; ++index) search.residual_counts[index] = 0U;
    if (enc_ac8x8_rd_grid_tile_q(
            tile, sizeof(tile), &tile_size,
            ORACLE_MAX_WIDTH, ORACLE_MAX_HEIGHT, 100U,
            rd_select_transaction_leaf, &search) != ENC_OK ||
        search.trials != 21U || search.squared_error == 0U) {
        return 0;
    }
    for (index = 0U; index < 7U; ++index) {
        selected_modes += search.mode_counts[index];
    }
    for (index = 0U; index < 3U; ++index) {
        selected_residuals += search.residual_counts[index];
    }
    return selected_modes == 1U && selected_residuals == 1U;
}

static int run_qdelta_luma_freeze(void) {
    static const int deltas[8U][2U] = {
        {-16, 0}, {-8, 0}, {8, 0}, {16, 0},
        {8, -16}, {8, -8}, {8, 8}, {8, 16}
    };
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncAc8x8YuvWorkspaceLayout layout;
    EncAc8x8YuvResult baseline;
    EncAc8x8YuvResult trial;
    uint64_t luma_error = 0U;
    uint8_t *coded_y;
    size_t index;
    size_t y;
    size_t x;
    for (y = 0U; y < 64U; ++y) {
        for (x = 0U; x < 64U; ++x) {
            qdelta_input[y * 64U + x] =
                (uint8_t)(96U + ((x * 3U + y * 5U) & 63U));
            qdelta_input[4096U + y * 64U + x] =
                (uint8_t)(32U + ((x * 7U + y * 11U) & 127U));
            qdelta_input[8192U + y * 64U + x] =
                (uint8_t)(224U - ((x * 13U + y * 3U) & 127U));
        }
    }
    input.format = ENC_M3_PIXEL_I444;
    input.width = 64U;
    input.height = 64U;
    reconstruction.format = ENC_M3_PIXEL_I444;
    reconstruction.width = 64U;
    reconstruction.height = 64U;
    for (index = 0U; index < 3U; ++index) {
        input.planes[index] = (EncM3Plane8){
            qdelta_input + index * 4096U, 64U, 4096U
        };
        reconstruction.planes[index] = (EncM3MutablePlane8){
            qdelta_reconstruction + index * 4096U, 64U, 4096U
        };
    }
    if (enc_ac8x8_query_yuv8_workspace(
            ENC_M3_PIXEL_I444, 64U, 64U, &layout) != ENC_OK ||
        layout.size > sizeof(oracle_workspace.bytes) ||
        enc_ac8x8_yuv8_q_workspace_inner(
            &input, 141U, ENC_AC8X8_COEFFICIENT_SEARCH_RD,
            oracle_workspace.bytes, sizeof(oracle_workspace.bytes),
            oracle_local_stream, sizeof(oracle_local_stream),
            &reconstruction, &baseline, 0, 0, 0, 0,
            &luma_error) != ENC_OK) {
        return 0;
    }
    coded_y = oracle_workspace.bytes + layout.coded_reconstruction_offset;
    for (y = 0U; y < 64U; ++y) {
        for (x = 0U; x < 64U; ++x) {
            qdelta_frozen_y[y * 64U + x] =
                coded_y[y * layout.coded_plane_width[0U] + x];
        }
    }
    for (index = 0U; index < 8U; ++index) {
        if (enc_ac8x8_yuv8_q_workspace_inner(
                &input, 141U, ENC_AC8X8_COEFFICIENT_SEARCH_RD,
                oracle_workspace.bytes, sizeof(oracle_workspace.bytes),
                oracle_local_stream, sizeof(oracle_local_stream),
                &reconstruction, &trial, 1,
                deltas[index][0U], deltas[index][1U], 1,
                &luma_error) != ENC_OK ||
            trial.candidate_squared_error[0U] != luma_error) {
            return 0;
        }
        for (y = 0U; y < 64U; ++y) {
            for (x = 0U; x < 64U; ++x) {
                if (coded_y[y * layout.coded_plane_width[0U] + x] !=
                    qdelta_frozen_y[y * 64U + x]) {
                    return 0;
                }
            }
        }
    }
    if (enc_ac8x8_yuv8_q_workspace(
            &input, 141U, ENC_AC8X8_COEFFICIENT_SEARCH_RD,
            oracle_workspace.bytes, sizeof(oracle_workspace.bytes),
            oracle_local_stream, sizeof(oracle_local_stream),
            &reconstruction, &trial) != ENC_OK ||
        trial.rd_qdelta_trials != 9U ||
        !trial.rd_qdelta_y_preserved) {
        return 0;
    }
    return 1;
}

int main(void) {
    size_t fixture;
    for (fixture = 1U; fixture <= ORACLE_MAX_LEAVES; ++fixture) {
        if (!run_multi_oracle(fixture)) return 1;
    }
    if (!run_fixture(0U, 40U, 1) ||
        !run_fixture(1U, 100U, 1) ||
        !run_fixture(3U, 140U, 1) ||
        !run_no_nonregressing_fallback() ||
        !run_qdelta_luma_freeze()) return 1;
    return 0;
}

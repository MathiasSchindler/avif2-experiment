#include "encoder/directional.h"
#include "encoder/m3.h"
#include "encoder/range.h"
#include "encoder/syntax.h"

#include <stdio.h>

#define STREAM_CAPACITY 256U
#define RECON_WIDTH 24U
#define RECON_HEIGHT 24U
#define RECON_SIZE (RECON_WIDTH * RECON_HEIGHT)

enum {
    DIFF_STREAM = 1U << 0,
    DIFF_WRITER = 1U << 1,
    DIFF_CDFS = 1U << 2,
    DIFF_COEFF = 1U << 3,
    DIFF_MODE = 1U << 4,
    DIFF_RECON = 1U << 5,
    DIFF_CELLS = 1U << 6,
    DIFF_INDEXES = 1U << 7
};

typedef struct {
    uint8_t stream[STREAM_CAPACITY];
    EncRangeWriter writer;
    EncSyntaxCdfs cdfs;
    uint8_t coeff_above[ENC_M3_MAX_MI_COLUMNS];
    uint8_t coeff_left[16U];
    uint8_t mode_above[ENC_M3_MAX_MI_COLUMNS];
    uint8_t mode_left[16U];
    uint8_t reconstruction[RECON_SIZE];
    uint8_t reconstructed_cells[32U];
    size_t width;
    size_t height;
    size_t superblock_x;
    size_t superblock_y;
    size_t mode_index;
    size_t level_index;
} TestState;

typedef struct {
    uint8_t joint_mode;
    uint32_t mode_index_symbol;
    uint32_t mrl_symbol;
    int16_t levels[4U];
    uint8_t paint_seed;
    uint32_t trailer_value;
    unsigned int trailer_bits;
} CandidateSpec;

typedef struct {
    TestState baseline;
} CandidateTransaction;

typedef struct {
    int repeat_trial;
    size_t calls;
} LiveTrialState;

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static void copy_state(TestState *destination, const TestState *source) {
    *destination = *source;
    destination->writer.data = destination->stream;
}

static int writer_is_bound(const TestState *state) {
    return state->writer.data == state->stream &&
        state->writer.capacity == STREAM_CAPACITY;
}

static int writer_fields_equal(
    const EncRangeWriter *left, const EncRangeWriter *right) {
    return left->capacity == right->capacity &&
        left->position == right->position &&
        left->symbols_out == right->symbols_out &&
        left->low == right->low &&
        left->range == right->range &&
        left->count == right->count &&
        left->pending_byte == right->pending_byte &&
        left->pending_ff == right->pending_ff &&
        left->status == right->status &&
        left->finished == right->finished &&
        enc_range_writer_tell(left) == enc_range_writer_tell(right);
}

static unsigned int state_diff_mask(
    const TestState *left, const TestState *right) {
    unsigned int mask = 0U;
    if (!bytes_equal(left->stream, right->stream, sizeof(left->stream))) {
        mask |= DIFF_STREAM;
    }
    if (!writer_fields_equal(&left->writer, &right->writer) ||
        !writer_is_bound(left) || !writer_is_bound(right)) {
        mask |= DIFF_WRITER;
    }
    if (!bytes_equal(
            (const uint8_t *)&left->cdfs, (const uint8_t *)&right->cdfs,
            sizeof(left->cdfs))) {
        mask |= DIFF_CDFS;
    }
    if (!bytes_equal(
            left->coeff_above, right->coeff_above,
            sizeof(left->coeff_above)) ||
        !bytes_equal(left->coeff_left, right->coeff_left,
                     sizeof(left->coeff_left))) {
        mask |= DIFF_COEFF;
    }
    if (!bytes_equal(
            left->mode_above, right->mode_above,
            sizeof(left->mode_above)) ||
        !bytes_equal(left->mode_left, right->mode_left,
                     sizeof(left->mode_left))) {
        mask |= DIFF_MODE;
    }
    if (!bytes_equal(
            left->reconstruction, right->reconstruction,
            sizeof(left->reconstruction))) {
        mask |= DIFF_RECON;
    }
    if (!bytes_equal(
            left->reconstructed_cells, right->reconstructed_cells,
            sizeof(left->reconstructed_cells))) {
        mask |= DIFF_CELLS;
    }
    if (left->width != right->width || left->height != right->height ||
        left->superblock_x != right->superblock_x ||
        left->superblock_y != right->superblock_y ||
        left->mode_index != right->mode_index ||
        left->level_index != right->level_index) {
        mask |= DIFF_INDEXES;
    }
    return mask;
}

static void fill_stream(uint8_t *data, size_t size, uint8_t value) {
    size_t index;
    for (index = 0U; index < size; ++index) data[index] = value;
}

static uint8_t clip_u8(int value) {
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static uint8_t cul_level(const int16_t *levels, size_t count) {
    unsigned int sum = 0U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        int16_t level = levels[index];
        sum += level < 0 ? (unsigned int)(-level) : (unsigned int)level;
        if (sum > 7U) sum = 7U;
    }
    if (levels[0] < 0) sum |= 8U;
    else if (levels[0] > 0) sum += 16U;
    return (uint8_t)sum;
}

static size_t or_low_levels(const uint8_t *values, size_t count) {
    size_t result = 0U;
    size_t index;
    for (index = 0U; index < count; ++index) result |= values[index] & 7U;
    return result;
}

static void coeff_context(
    const TestState *state, size_t x, size_t y,
    size_t block_width, size_t block_height,
    size_t *skip_context, size_t *sign_context) {
    size_t column = x / 4U;
    size_t row = (y / 4U) % 16U;
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    size_t columns = column + 2U <= mi_columns ? 2U : mi_columns - column;
    size_t global_row = y / 4U;
    size_t rows = global_row + 2U <= mi_rows ? 2U : mi_rows - global_row;
    size_t top = or_low_levels(state->coeff_above + column, columns);
    size_t side = or_low_levels(state->coeff_left + row, rows);
    int score = 0;
    size_t index;
    static const uint8_t contexts[5U][5U] = {
        {1U, 2U, 2U, 3U, 3U}, {2U, 2U, 3U, 3U, 4U},
        {2U, 3U, 3U, 4U, 4U}, {3U, 3U, 4U, 4U, 5U},
        {3U, 4U, 4U, 5U, 5U}
    };
    for (index = 0U; index < columns; ++index) {
        uint8_t value = state->coeff_above[column + index];
        score += (value & 16U) != 0U ? 1 : (value & 8U) != 0U ? -1 : 0;
    }
    for (index = 0U; index < rows; ++index) {
        uint8_t value = state->coeff_left[row + index];
        score += (value & 16U) != 0U ? 1 : (value & 8U) != 0U ? -1 : 0;
    }
    *skip_context = block_width == 8U && block_height == 8U
        ? 0U : contexts[top > 4U ? 4U : top][side > 4U ? 4U : side];
    *sign_context = score < 0 ? 1U : score > 0 ? 2U : 0U;
}

static void commit_coeff_context(
    TestState *state, size_t x, size_t y, uint8_t value) {
    size_t column = x / 4U;
    size_t global_row = y / 4U;
    size_t row = global_row % 16U;
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    size_t index;
    for (index = column; index < column + 2U && index < mi_columns; ++index) {
        state->coeff_above[index] = value;
    }
    for (index = row;
         index < row + 2U && global_row + index - row < mi_rows;
         ++index) {
        state->coeff_left[index] = value;
    }
}

static size_t mode_context(
    const TestState *state, size_t block_x, size_t block_y) {
    size_t column = block_x / 4U;
    size_t row = (block_y / 4U) % 16U;
    uint8_t left_mode = block_x == 0U ? 0U : state->mode_left[row];
    uint8_t above_mode = block_y == 0U ? 0U : state->mode_above[column];
    return (size_t)enc_directional_joint_is_directional(left_mode) +
        (size_t)enc_directional_joint_is_directional(above_mode);
}

static void commit_mode_context(
    TestState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height, uint8_t joint_mode) {
    size_t column = block_x / 4U;
    size_t row = (block_y / 4U) % 16U;
    size_t width_units = block_width / 4U;
    size_t height_units = block_height / 4U;
    size_t index;
    for (index = 0U;
         index < width_units && column + index < ENC_M3_MAX_MI_COLUMNS;
         ++index) {
        state->mode_above[column + index] = joint_mode;
    }
    for (index = 0U; index < height_units && row + index < 16U; ++index) {
        state->mode_left[row + index] = joint_mode;
    }
}

static void mark_reconstructed(
    size_t superblock_x, size_t superblock_y, size_t superblock_size,
    uint8_t reconstructed_cells[32U],
    size_t block_x, size_t block_y, size_t width, size_t height) {
    size_t cells_per_side = superblock_size / 4U;
    size_t y;
    size_t x;
    for (y = block_y; y < block_y + height; y += 4U) {
        for (x = block_x; x < block_x + width; x += 4U) {
            size_t cell;
            if (x < superblock_x || x >= superblock_x + superblock_size ||
                y < superblock_y || y >= superblock_y + superblock_size) {
                continue;
            }
            cell = ((y - superblock_y) / 4U) * cells_per_side +
                (x - superblock_x) / 4U;
            reconstructed_cells[cell / 8U] |=
                (uint8_t)(1U << (cell % 8U));
        }
    }
}

static EncStatus write_dc_coeff(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    int16_t level, size_t skip_context, size_t sign_context,
    size_t q_context) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-(int)level) : (unsigned int)level;
    if (enc_range_writer_symbol(
            writer, magnitude == 0U ? 1U : 0U,
            enc_syntax_txb_skip_cdf(
                cdfs, q_context, 0U, 1U, skip_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (magnitude == 0U) return ENC_OK;
    if (enc_range_writer_symbol(
            writer, 0U, enc_syntax_eob_pt_64_cdf(cdfs, q_context, 0U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, magnitude > 5U ? 4U : magnitude - 1U,
            enc_syntax_coeff_base_lf_eob_cdf(cdfs, q_context, 1U, 0U),
            5U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (magnitude >= 5U &&
        enc_range_writer_symbol(
            writer, magnitude > 8U ? 3U : magnitude - 5U,
            enc_syntax_coeff_br_lf_cdf(cdfs, q_context, 0U),
            4U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (enc_range_writer_symbol(
            writer, level < 0 ? 1U : 0U,
            enc_syntax_dc_sign_cdf(
                cdfs, q_context, 0U, 0U, sign_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    return ENC_OK;
}

static size_t block_size_group(size_t block_width, size_t block_height) {
    return block_width == block_height
        ? (block_width == 16U ? 4U : 2U) : 3U;
}

static void paint_block(
    TestState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height,
    const int16_t *levels, uint8_t seed) {
    size_t y;
    size_t x;
    for (y = 0U; y < block_height; ++y) {
        for (x = 0U; x < block_width; ++x) {
            size_t tx_column = x / 8U;
            size_t tx_row = y / 8U;
            size_t level_slot = tx_row * (block_width / 8U) + tx_column;
            int16_t level = levels[level_slot];
            unsigned int magnitude =
                level < 0 ? (unsigned int)(-level) : (unsigned int)level;
            int value = 32 + (int)((seed +
                (uint8_t)(x * 3U + y * 5U + magnitude * 7U +
                          tx_column * 11U + tx_row * 13U +
                          (level < 0 ? 19U : 0U))) % 192U);
            state->reconstruction[(block_y + y) * RECON_WIDTH + block_x + x] =
                clip_u8(value);
        }
    }
}

static int apply_leaf_candidate(
    TestState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height,
    const CandidateSpec *candidate) {
    size_t leaf_mode_context;
    size_t level_slot = 0U;
    size_t tx_y;
    size_t tx_x;
    if (!writer_is_bound(state) || candidate == 0 ||
        block_width == 0U || block_height == 0U ||
        block_width % 8U != 0U || block_height % 8U != 0U ||
        block_x + block_width > state->width ||
        block_y + block_height > state->height ||
        candidate->mode_index_symbol > 7U ||
        candidate->mrl_symbol > 3U ||
        candidate->trailer_bits == 0U || candidate->trailer_bits > 16U) {
        return 0;
    }
    leaf_mode_context = mode_context(state, block_x, block_y);
    if (enc_range_writer_symbol(
            &state->writer, 0U,
            enc_syntax_y_mode_set_cdf(&state->cdfs),
            4U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &state->writer, candidate->mode_index_symbol,
            enc_syntax_y_mode_index_cdf(&state->cdfs, leaf_mode_context),
            8U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &state->writer, 0U,
            enc_syntax_fsc_mode_cdf(
                &state->cdfs, 0U,
                block_size_group(block_width, block_height)),
            2U, 1) != ENC_OK) {
        return 0;
    }
    if (enc_directional_joint_is_directional(candidate->joint_mode) &&
        enc_range_writer_symbol(
            &state->writer, candidate->mrl_symbol,
            enc_syntax_mrl_index_cdf(&state->cdfs, 0U),
            4U, 1) != ENC_OK) {
        return 0;
    }
    commit_mode_context(
        state, block_x, block_y, block_width, block_height,
        candidate->joint_mode);
    for (tx_y = 0U; tx_y < block_height; tx_y += 8U) {
        for (tx_x = 0U; tx_x < block_width; tx_x += 8U) {
            size_t skip_context;
            size_t sign_context;
            coeff_context(
                state, block_x + tx_x, block_y + tx_y,
                block_width, block_height, &skip_context, &sign_context);
            if (write_dc_coeff(
                    &state->writer, &state->cdfs,
                    candidate->levels[level_slot],
                    skip_context, sign_context, 0U) != ENC_OK) {
                return 0;
            }
            commit_coeff_context(
                state, block_x + tx_x, block_y + tx_y,
                cul_level(&candidate->levels[level_slot], 1U));
            ++level_slot;
        }
    }
    if (enc_range_writer_literal(
            &state->writer, candidate->trailer_value,
            candidate->trailer_bits) != ENC_OK) {
        return 0;
    }
    paint_block(
        state, block_x, block_y, block_width, block_height,
        candidate->levels, candidate->paint_seed);
    mark_reconstructed(
        state->superblock_x, state->superblock_y, 64U,
        state->reconstructed_cells,
        block_x, block_y, block_width, block_height);
    ++state->mode_index;
    state->level_index += level_slot;
    return 1;
}

static int directional_joint(unsigned int offset, uint8_t *joint_mode) {
    return enc_directional_token_to_joint(
        (uint8_t)(ENC_DIRECTIONAL_FIRST_TOKEN + offset), joint_mode);
}

static int init_state(TestState *state) {
    uint8_t joint_a;
    uint8_t joint_b;
    size_t index;
    size_t y;
    size_t x;
    if (!directional_joint(0U, &joint_a) ||
        !directional_joint(8U, &joint_b)) {
        return 0;
    }
    fill_stream((uint8_t *)state, sizeof(*state), 0U);
    state->width = RECON_WIDTH;
    state->height = RECON_HEIGHT;
    state->superblock_x = 0U;
    state->superblock_y = 0U;
    fill_stream(state->stream, sizeof(state->stream), 0xa5U);
    enc_range_writer_init(&state->writer, state->stream, sizeof(state->stream));
    if (enc_syntax_cdfs_init(&state->cdfs) != ENC_OK) return 0;
    for (index = 0U; index < ENC_M3_MAX_MI_COLUMNS; ++index) {
        state->coeff_above[index] =
            (uint8_t)((index % 5U) | ((index & 1U) != 0U ? 8U : 16U));
        state->mode_above[index] =
            index % 3U == 0U ? 0U : index % 3U == 1U ? joint_a : joint_b;
    }
    for (index = 0U; index < 16U; ++index) {
        state->coeff_left[index] =
            (uint8_t)(((index + 2U) % 5U) |
                      ((index & 1U) != 0U ? 16U : 8U));
        state->mode_left[index] =
            index % 3U == 0U ? joint_b : index % 3U == 1U ? 0U : joint_a;
    }
    for (y = 0U; y < RECON_HEIGHT; ++y) {
        for (x = 0U; x < RECON_WIDTH; ++x) {
            state->reconstruction[y * RECON_WIDTH + x] =
                (uint8_t)(48U + (x * 3U + y * 5U) % 144U);
        }
    }
    return writer_is_bound(state);
}

static void transaction_begin(
    CandidateTransaction *transaction, const TestState *state) {
    copy_state(&transaction->baseline, state);
}

static void transaction_rollback(
    const CandidateTransaction *transaction, TestState *state) {
    copy_state(state, &transaction->baseline);
}

static void transaction_commit(
    CandidateTransaction *transaction, const TestState *state) {
    copy_state(&transaction->baseline, state);
}

static int prime_state(TestState *state) {
    uint8_t joint_a;
    uint8_t joint_b;
    static const CandidateSpec top = {
        0U, 1U, 0U, {2, 0, 0, 0}, 19U, 0x155U, 10U
    };
    static const CandidateSpec left = {
        0U, 2U, 0U, {-6, 0, 0, 0}, 41U, 0x2aaU, 11U
    };
    CandidateSpec top_spec = top;
    CandidateSpec left_spec = left;
    if (!directional_joint(3U, &joint_a) ||
        !directional_joint(11U, &joint_b)) {
        return 0;
    }
    top_spec.joint_mode = joint_a;
    top_spec.mrl_symbol = 1U;
    left_spec.joint_mode = joint_b;
    left_spec.mrl_symbol = 2U;
    if (!apply_leaf_candidate(state, 8U, 0U, 8U, 8U, &top_spec) ||
        !apply_leaf_candidate(state, 0U, 8U, 8U, 8U, &left_spec)) {
        return 0;
    }
    return enc_range_writer_tell(&state->writer) != 0U &&
        enc_range_writer_bytes_used(&state->writer) != 0U &&
        writer_is_bound(state) &&
        state->mode_index == 2U &&
        state->level_index == 2U;
}

static int test_rollback_restores_all_fields(void) {
    static TestState state;
    static TestState baseline;
    uint8_t directional_mode;
    static const CandidateSpec first = {
        0U, 5U, 2U, {5, -4, 0, 7}, 73U, 0xa55U, 12U
    };
    static const CandidateSpec second = {
        0U, 0U, 0U, {0, 3, -2, 1}, 109U, 0x12fU, 9U
    };
    CandidateSpec first_spec = first;
    unsigned int diff;
    if (!directional_joint(19U, &directional_mode) ||
        !init_state(&state) || !prime_state(&state)) {
        return 1;
    }
    copy_state(&baseline, &state);
    first_spec.joint_mode = directional_mode;
    if (!apply_leaf_candidate(&state, 8U, 8U, 16U, 16U, &first_spec)) {
        return 2;
    }
    diff = state_diff_mask(&state, &baseline);
    if ((diff & (DIFF_STREAM | DIFF_WRITER | DIFF_CDFS | DIFF_COEFF |
                 DIFF_MODE | DIFF_RECON | DIFF_CELLS | DIFF_INDEXES)) !=
        (DIFF_STREAM | DIFF_WRITER | DIFF_CDFS | DIFF_COEFF |
         DIFF_MODE | DIFF_RECON | DIFF_CELLS | DIFF_INDEXES)) {
        fprintf(stderr, "rollback trial missing diffs: %#x\n", diff);
        return 3;
    }
    copy_state(&state, &baseline);
    if (state_diff_mask(&state, &baseline) != 0U || !writer_is_bound(&state)) {
        return 4;
    }
    if (!apply_leaf_candidate(&state, 8U, 8U, 16U, 16U, &second)) {
        return 5;
    }
    diff = state_diff_mask(&state, &baseline);
    if ((diff & (DIFF_STREAM | DIFF_WRITER | DIFF_CDFS | DIFF_COEFF |
                 DIFF_MODE | DIFF_RECON | DIFF_CELLS | DIFF_INDEXES)) !=
        (DIFF_STREAM | DIFF_WRITER | DIFF_CDFS | DIFF_COEFF |
         DIFF_MODE | DIFF_RECON | DIFF_CELLS | DIFF_INDEXES)) {
        fprintf(stderr, "second rollback trial missing diffs: %#x\n", diff);
        return 6;
    }
    copy_state(&state, &baseline);
    if (state_diff_mask(&state, &baseline) != 0U || !writer_is_bound(&state)) {
        return 7;
    }
    return 0;
}

static int test_commit_preserves_winner_only(void) {
    static TestState state;
    static TestState baseline;
    static TestState expected;
    static TestState committed;
    static TestState finished_actual;
    static TestState finished_expected;
    CandidateTransaction transaction;
    uint8_t directional_mode;
    static const CandidateSpec loser = {
        0U, 6U, 1U, {6, -3, 4, 0}, 87U, 0xd55U, 12U
    };
    static const CandidateSpec winner = {
        0U, 0U, 0U, {0, 2, -1, 3}, 121U, 0x19bU, 10U
    };
    static const CandidateSpec post_commit = {
        0U, 4U, 2U, {-5, 0, 0, 0}, 149U, 0x3c3U, 11U
    };
    CandidateSpec loser_spec = loser;
    CandidateSpec post_commit_spec = post_commit;
    if (!directional_joint(27U, &directional_mode) ||
        !init_state(&state) || !prime_state(&state)) {
        return 1;
    }
    copy_state(&baseline, &state);
    transaction_begin(&transaction, &state);
    loser_spec.joint_mode = directional_mode;
    if (!apply_leaf_candidate(&state, 8U, 8U, 16U, 16U, &loser_spec)) {
        return 2;
    }
    transaction_rollback(&transaction, &state);
    if (state_diff_mask(&state, &baseline) != 0U || !writer_is_bound(&state)) {
        return 3;
    }
    if (!apply_leaf_candidate(&state, 8U, 8U, 16U, 16U, &winner)) {
        return 4;
    }
    transaction_commit(&transaction, &state);
    copy_state(&committed, &state);
    copy_state(&expected, &baseline);
    if (!apply_leaf_candidate(&expected, 8U, 8U, 16U, 16U, &winner)) {
        return 5;
    }
    if (state_diff_mask(&state, &expected) != 0U || !writer_is_bound(&state)) {
        return 6;
    }
    post_commit_spec.joint_mode = directional_mode;
    if (!apply_leaf_candidate(&state, 16U, 8U, 8U, 8U, &post_commit_spec)) {
        return 7;
    }
    transaction_rollback(&transaction, &state);
    if (state_diff_mask(&state, &committed) != 0U || !writer_is_bound(&state)) {
        return 8;
    }
    copy_state(&finished_actual, &committed);
    copy_state(&finished_expected, &expected);
    if (enc_range_writer_finish(&finished_actual.writer) != ENC_OK ||
        enc_range_writer_finish(&finished_expected.writer) != ENC_OK ||
        state_diff_mask(&finished_actual, &finished_expected) != 0U) {
        return 9;
    }
    return 0;
}

static EncStatus live_trial_callback(
    void *opaque, void *transaction,
    size_t block_x, size_t block_y,
    size_t block_width, size_t block_height,
    uint8_t *selected_mode, int16_t selected_levels[4U][64U]) {
    LiveTrialState *state = (LiveTrialState *)opaque;
    int16_t levels[4U][64U] = {{0}};
    size_t transforms = (block_width / 8U) * (block_height / 8U);
    size_t first_tell;
    size_t second_tell;
    size_t tx;
    size_t coefficient;
    (void)block_x;
    (void)block_y;
    if (state == 0 || selected_mode == 0 || transaction == 0 ||
        transforms == 0U || transforms > 4U ||
        enc_ac8x8_rd_transaction_trial(
            transaction, 0U, &levels[0U][0U], transforms,
            &first_tell) != ENC_OK ||
        first_tell == 0U) {
        return ENC_INVALID_ARGUMENT;
    }
    if (state->repeat_trial != 0 &&
        (enc_ac8x8_rd_transaction_trial(
             transaction, 0U, &levels[0U][0U], transforms,
             &second_tell) != ENC_OK ||
         second_tell != first_tell)) {
        return ENC_INVALID_ARGUMENT;
    }
    for (tx = 0U; tx < 4U; ++tx) {
        for (coefficient = 0U; coefficient < 64U; ++coefficient) {
            selected_levels[tx][coefficient] = 0;
        }
    }
    *selected_mode = 0U;
    ++state->calls;
    return ENC_OK;
}

static int test_live_writer_trial_rolls_back(void) {
    uint8_t one_trial_stream[1024U];
    uint8_t repeated_trial_stream[1024U];
    size_t one_trial_size;
    size_t repeated_trial_size;
    LiveTrialState one_trial = {0, 0U};
    LiveTrialState repeated_trial = {1, 0U};
    if (enc_ac8x8_rd_grid_tile_q(
            one_trial_stream, sizeof(one_trial_stream), &one_trial_size,
            24U, 24U, 100U, live_trial_callback, &one_trial) != ENC_OK ||
        enc_ac8x8_rd_grid_tile_q(
            repeated_trial_stream, sizeof(repeated_trial_stream),
            &repeated_trial_size, 24U, 24U, 100U,
            live_trial_callback, &repeated_trial) != ENC_OK ||
        one_trial.calls == 0U || one_trial.calls != repeated_trial.calls ||
        one_trial_size != repeated_trial_size ||
        !bytes_equal(one_trial_stream, repeated_trial_stream, one_trial_size)) {
        return 1;
    }
    return 0;
}

int main(void) {
    int result = test_rollback_restores_all_fields();
    if (result != 0) return 10 + result;
    result = test_commit_preserves_winner_only();
    if (result != 0) return 20 + result;
    result = test_live_writer_trial_rolls_back();
    if (result != 0) return 30 + result;
    return 0;
}

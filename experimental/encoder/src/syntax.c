#include "encoder/cdf.h"
#include "encoder/range.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static EncStatus write_m3_mode(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    size_t block_size_group);

static EncStatus write_uvlc(EncBitWriter *writer, uint32_t value) {
    uint32_t code;
    unsigned int width = 0U;
    unsigned int leading;
    if (value == UINT32_MAX) {
        return ENC_INVALID_ARGUMENT;
    }
    code = value + 1U;
    {
        uint32_t probe = code;
        while (probe != 0U) {
            ++width;
            probe >>= 1U;
        }
    }
    leading = width - 1U;
    if (enc_bit_writer_write(writer, 0U, leading) != ENC_OK) {
        return writer->status;
    }
    return enc_bit_writer_write(writer, code, width);
}

static EncStatus write_ns(EncBitWriter *writer, uint32_t value,
                          uint32_t domain) {
    unsigned int width = 0U;
    uint32_t cutoff;
    uint32_t probe;
    if (domain == 0U || value >= domain) {
        return ENC_INVALID_ARGUMENT;
    }
    probe = domain - 1U;
    while (probe != 0U) {
        ++width;
        probe >>= 1U;
    }
    cutoff = (UINT32_C(1) << width) - domain;
    if (value < cutoff) {
        return enc_bit_writer_write(writer, value, width - 1U);
    }
    value += cutoff;
    if (enc_bit_writer_write(writer, value >> 1U, width - 1U) != ENC_OK) {
        return writer->status;
    }
    return enc_bit_writer_write(writer, value & 1U, 1U);
}

static EncStatus finish_bits(EncBitWriter *writer, size_t *size) {
    EncStatus status;
    if (size == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_bit_writer_align_zero(writer);
    if (status == ENC_OK) {
        *size = enc_bit_writer_bytes_used(writer);
    }
    return status;
}

static unsigned int dimension_bits(size_t value) {
    unsigned int bits = 0U;
    size_t maximum = value - 1U;
    do {
        ++bits;
        maximum >>= 1U;
    } while (maximum != 0U);
    return bits;
}

static EncStatus write_sequence_header(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int enable_ibp,
    unsigned int enable_intra_ist) {
    EncBitWriter writer;
    unsigned int width_bits;
    unsigned int height_bits;
    uint8_t flags[] = {
        0U, 0U, 0U, 1U, 1U, 0U, 0U, 0U, 0U, 1U, 1U, 1U, 1U, 1U,
        1U, 0U, 0U
    };
    uint8_t transform_filter_flags[] = {
        0U, 1U, 1U, 1U, 1U, 0U, 0U, 0U, 1U, 0U, 0U, 0U, 0U, 0U
    };
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        width == 0U || height == 0U || width > 16384U || height > 16384U) {
        return ENC_INVALID_ARGUMENT;
    }
    width_bits = dimension_bits(width);
    height_bits = dimension_bits(height);
    if (width_bits == 0U || width_bits > 16U ||
        height_bits == 0U || height_bits > 16U) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    flags[13U] = enable_ibp != 0U ? 1U : 0U;
    transform_filter_flags[3U] =
        enable_intra_ist != 0U ? 1U : 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (write_uvlc(&writer, 0U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 5U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 5U) != ENC_OK ||
        write_uvlc(&writer, 1U) != ENC_OK ||
        write_uvlc(&writer, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, width_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, height_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, width - 1U, width_bits) != ENC_OK ||
        enc_bit_writer_write(&writer, height - 1U, height_bits) != ENC_OK) {
        return writer.status;
    }
    for (index = 0U; index < sizeof(flags); ++index) {
        if (enc_bit_writer_write(&writer, flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (write_ns(&writer, 2U, 3U) != ENC_OK) return writer.status;
    for (index = 0U; index < sizeof(transform_filter_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, transform_filter_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_write(&writer, 1U, 2U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK) {
        return writer.status;
    }
    return finish_bits(&writer, size);
}

EncStatus enc_m1_sequence_header(void *data, size_t capacity, size_t *size) {
    return write_sequence_header(data, capacity, size, 8U, 8U, 1U, 1U);
}

EncStatus enc_m3_sequence_header(void *data, size_t capacity, size_t *size,
                                 size_t width, size_t height) {
    return write_sequence_header(data, capacity, size, width, height, 0U, 1U);
}

EncStatus enc_m4_sequence_header(void *data, size_t capacity, size_t *size) {
    return write_sequence_header(data, capacity, size, 8U, 8U, 0U, 0U);
}

EncStatus enc_m1_frame_header(void *data, size_t capacity, size_t *size) {
    EncBitWriter writer;
    static const uint8_t prefix_flags[] = {1U, 0U, 0U, 0U, 1U};
    static const uint8_t suffix_flags[] = {0U, 0U, 1U};
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (write_uvlc(&writer, 0U) != ENC_OK ||
        write_uvlc(&writer, 0U) != ENC_OK) {
        return writer.status;
    }
    for (index = 0U; index < sizeof(prefix_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, prefix_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_write(&writer, 255U, 8U) != ENC_OK) {
        return writer.status;
    }
    for (index = 0U; index < sizeof(suffix_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, suffix_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_write(&writer, 2U, 2U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 2U) != ENC_OK) {
        return writer.status;
    }
    return finish_bits(&writer, size);
}

EncStatus enc_m1_tile(void *data, size_t capacity, size_t *size) {
    EncSyntaxCdfs cdfs;
    uint16_t *do_split;
    uint16_t *delta_q;
    uint16_t *mode_set;
    uint16_t *mode_index;
    uint16_t *fsc;
    uint16_t *txb_skip;
    EncRangeWriter writer;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }

    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) {
        return status;
    }
    do_split = enc_syntax_do_split_cdf(&cdfs, 0U, 0U);
    delta_q = enc_syntax_delta_q_cdf(&cdfs);
    mode_set = enc_syntax_y_mode_set_cdf(&cdfs);
    mode_index = enc_syntax_y_mode_index_cdf(&cdfs, 0U);
    fsc = enc_syntax_fsc_mode_cdf(&cdfs, 0U, 2U);
    txb_skip = enc_syntax_txb_skip_cdf(&cdfs, 3U, 0U, 1U, 0U);

    enc_range_writer_init(&writer, data, capacity);
    if (enc_range_writer_symbol(&writer, 0U, do_split, 2U, 1) != ENC_OK ||
        enc_range_writer_symbol(&writer, 1U, delta_q, 8U, 1) != ENC_OK ||
        enc_range_writer_bypass(&writer, 1U) != ENC_OK ||
        enc_range_writer_symbol(&writer, 0U, mode_set, 4U, 1) != ENC_OK ||
        enc_range_writer_symbol(&writer, 0U, mode_index, 8U, 1) != ENC_OK ||
        enc_range_writer_symbol(&writer, 0U, fsc, 2U, 1) != ENC_OK ||
        enc_range_writer_symbol(&writer, 1U, txb_skip, 2U, 1) != ENC_OK ||
        enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

EncStatus enc_m3_frame_header(void *data, size_t capacity, size_t *size) {
    EncBitWriter writer;
    static const uint8_t prefix_flags[] = {1U, 0U, 0U, 0U, 1U};
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }

    *size = 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (write_uvlc(&writer, 0U) != ENC_OK ||
        write_uvlc(&writer, 0U) != ENC_OK) return writer.status;
    for (index = 0U; index < sizeof(prefix_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, prefix_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_write(&writer, 100U, 8U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 2U) != ENC_OK) {
        return writer.status;
    }
    return finish_bits(&writer, size);
}

EncStatus enc_m4_frame_header(void *data, size_t capacity, size_t *size) {
    EncBitWriter writer;
    static const uint8_t prefix_flags[] = {1U, 0U, 0U, 0U, 1U};
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U)) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (write_uvlc(&writer, 0U) != ENC_OK ||
        write_uvlc(&writer, 0U) != ENC_OK) return writer.status;
    for (index = 0U; index < sizeof(prefix_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, prefix_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_write(&writer, 50U, 8U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 2U) != ENC_OK) {
        return writer.status;
    }
    return finish_bits(&writer, size);
}

EncStatus enc_m4_ac_tile(void *data, size_t capacity, size_t *size,
                         int16_t dc_level, int16_t ac_level) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    unsigned int dc_magnitude =
        dc_level < 0 ? (unsigned int)(-(int)dc_level) :
                       (unsigned int)dc_level;
    unsigned int ac_magnitude =
        ac_level < 0 ? (unsigned int)(-(int)ac_level) :
                       (unsigned int)ac_level;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        dc_magnitude < ENC_M4_MIN_LEVEL_MAGNITUDE ||
        dc_magnitude > ENC_M4_MAX_LEVEL_MAGNITUDE ||
        ac_magnitude < ENC_M4_MIN_LEVEL_MAGNITUDE ||
        ac_magnitude > ENC_M4_MAX_LEVEL_MAGNITUDE) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    enc_range_writer_init(&writer, data, capacity);
    if (enc_range_writer_symbol(
            &writer, 0U, enc_syntax_do_split_cdf(&cdfs, 0U, 0U),
            2U, 1) != ENC_OK ||
        write_m3_mode(&writer, &cdfs, 2U) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U,
            enc_syntax_tx_do_partition_cdf(&cdfs, 0U, 0U, 1U),
            2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U,
            enc_syntax_txb_skip_cdf(&cdfs, 0U, 0U, 1U, 0U),
            2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 1U, enc_syntax_eob_pt_64_cdf(&cdfs, 0U, 0U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U,
            enc_syntax_intra_tx_type_set1_cdf(&cdfs, 1U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, ac_magnitude - 1U,
            enc_syntax_coeff_base_lf_eob_cdf(&cdfs, 0U, 1U, 1U),
            5U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, dc_magnitude,
            enc_syntax_coeff_base_lf_cdf(
                &cdfs, 0U, 1U, (ac_magnitude + 1U) >> 1U, 0U),
            6U, 1) != ENC_OK ||
        enc_range_writer_bypass(&writer, ac_level < 0 ? 1U : 0U) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, dc_level < 0 ? 1U : 0U,
            enc_syntax_dc_sign_cdf(&cdfs, 0U, 0U, 0U, 0U),
            2U, 1) != ENC_OK ||
        enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

static EncStatus write_dc_remainder(
    EncRangeWriter *writer, unsigned int value) {
    unsigned int quotient;
    unsigned int remainder;
    unsigned int length;
    unsigned int base;
    unsigned int index;
    if (value < 10U) {
        quotient = value >> 1U;
        remainder = value & 1U;
        for (index = 0U; index < quotient; ++index) {
            if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
        }
        if (enc_range_writer_bypass(writer, 1U) != ENC_OK ||
            enc_range_writer_bypass(writer, remainder) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        return ENC_OK;
    }
    for (index = 0U; index < 5U; ++index) {
        if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    length = 2U;
    while (length < 20U &&
           value >= 6U + (UINT32_C(1) << (length + 1U))) {
        ++length;
    }
    if (length == 20U &&
        value >= 6U + (UINT32_C(1) << 21U)) {
        return ENC_INVALID_ARGUMENT;
    }
    for (index = 2U; index < length; ++index) {
        if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    if (enc_range_writer_bypass(writer, 1U) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    base = 6U + (UINT32_C(1) << length);
    remainder = value - base;
    for (index = length; index != 0U; --index) {
        if (enc_range_writer_bypass(
                writer, (remainder >> (index - 1U)) & 1U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    return ENC_OK;
}

static EncStatus write_m3_mode(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    size_t block_size_group) {
    if (enc_range_writer_symbol(
            writer, 0U, enc_syntax_y_mode_set_cdf(cdfs), 4U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, 0U, enc_syntax_y_mode_index_cdf(cdfs, 0U),
            8U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, 0U,
            enc_syntax_fsc_mode_cdf(cdfs, 0U, block_size_group),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    return ENC_OK;
}

static EncStatus write_m3_dc_coeff(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    int16_t level, size_t skip_context, size_t sign_context) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-(int)level) : (unsigned int)level;
    if (enc_range_writer_symbol(
            writer, magnitude == 0U ? 1U : 0U,
            enc_syntax_txb_skip_cdf(
                cdfs, 1U, 0U, 1U, skip_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (magnitude == 0U) return ENC_OK;
    if (enc_range_writer_symbol(
            writer, 0U, enc_syntax_eob_pt_64_cdf(cdfs, 1U, 0U),
            7U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, magnitude > 5U ? 4U : magnitude - 1U,
            enc_syntax_coeff_base_lf_eob_cdf(cdfs, 1U, 1U, 0U),
            5U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (magnitude >= 5U &&
        enc_range_writer_symbol(
            writer, magnitude > 8U ? 3U : magnitude - 5U,
            enc_syntax_coeff_br_lf_cdf(cdfs, 1U, 0U),
            4U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (enc_range_writer_symbol(
            writer, level < 0 ? 1U : 0U,
            enc_syntax_dc_sign_cdf(cdfs, 1U, 0U, 0U, sign_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    return magnitude >= 8U
        ? write_dc_remainder(writer, magnitude - 8U) : ENC_OK;
}

typedef struct {
    EncRangeWriter *writer;
    EncSyntaxCdfs *cdfs;
    size_t width;
    size_t height;
    const int16_t *levels;
} EncM3TileState;

static EncStatus write_m3_leaf(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height) {
    size_t block_columns = state->width / 8U;
    size_t tx_columns = block_width / 8U;
    size_t tx_rows = block_height / 8U;
    size_t block_size_group =
        block_width == block_height
            ? (block_width == 16U ? 4U : 2U) : 3U;
    size_t tx_size_group =
        block_width == block_height
            ? (block_width == 16U ? 3U : 1U) : 2U;
    size_t tx_y;
    size_t tx_x;
    EncStatus status;
    status = write_m3_mode(
        state->writer, state->cdfs, block_size_group);
    if (status != ENC_OK) return status;
    if (enc_range_writer_symbol(
            state->writer,
            block_width != 8U || block_height != 8U ? 1U : 0U,
            enc_syntax_tx_do_partition_cdf(
                state->cdfs, 0U, 0U, tx_size_group),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(state->writer);
    }
    if (block_width == 16U && block_height == 16U &&
        enc_range_writer_symbol(
            state->writer, 0U,
            enc_syntax_tx_partition_type_cdf(
                state->cdfs, 0U, 0U, 3U),
            7U, 1) != ENC_OK) {
        return enc_range_writer_status(state->writer);
    }
    if (block_width != block_height &&
        enc_range_writer_symbol(
            state->writer, block_width == 8U ? 1U : 2U,
            enc_syntax_tx_partition_type_cdf(
                state->cdfs, 0U, 0U,
                block_width == 8U ? 1U : 2U),
            7U, 1) != ENC_OK) {
        return enc_range_writer_status(state->writer);
    }
    for (tx_y = 0U; tx_y < tx_rows; ++tx_y) {
        for (tx_x = 0U; tx_x < tx_columns; ++tx_x) {
            size_t column = block_x / 8U + tx_x;
            size_t row = block_y / 8U + tx_y;
            size_t block = row * block_columns + column;
            size_t sign_context = 0U;
            size_t skip_context = 0U;
            int score = 0;
            if (row != 0U) {
                int16_t above = state->levels[block - block_columns];
                size_t magnitude = (size_t)(
                    above < 0 ? -(int)above : (int)above);
                skip_context += magnitude > 4U ? 4U : magnitude;
                score += above < 0 ? -1 : above > 0 ? 1 : 0;
            }
            if (column != 0U) {
                int16_t left = state->levels[block - 1U];
                size_t magnitude = (size_t)(
                    left < 0 ? -(int)left : (int)left);
                skip_context += magnitude > 4U ? 4U : magnitude;
                score += left < 0 ? -1 : left > 0 ? 1 : 0;
            }
            if (block_width != 8U || block_height != 8U) {
                skip_context = (skip_context + 3U) >> 1U;
            }
            if (score < 0) sign_context = 1U;
            if (score > 0) sign_context = 2U;
            status = write_m3_dc_coeff(
                state->writer, state->cdfs, state->levels[block],
                skip_context, sign_context);
            if (status != ENC_OK) return status;
        }
    }
    return ENC_OK;
}

static EncStatus write_m3_partition(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_size) {
    size_t half;
    EncStatus status;
    if (block_x >= state->width || block_y >= state->height) return ENC_OK;
    if (block_size == 16U &&
        block_x + block_size > state->width &&
        block_y + block_size <= state->height) {
        if (enc_range_writer_symbol(
                state->writer, 0U,
                enc_syntax_do_split_cdf(state->cdfs, 0U, 4U),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        return write_m3_leaf(
            state, block_x, block_y, 8U, 16U);
    }
    if (block_size == 16U &&
        block_y + block_size > state->height &&
        block_x + block_size <= state->width) {
        if (enc_range_writer_symbol(
                state->writer, 0U,
                enc_syntax_do_split_cdf(state->cdfs, 0U, 4U),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        return write_m3_leaf(
            state, block_x, block_y, 16U, 8U);
    }
    if (block_size > 8U &&
        (block_x + block_size > state->width ||
         block_y + block_size > state->height)) {
        half = block_size / 2U;
    } else {
        size_t context = block_size == 16U ? 4U : 0U;
        if (enc_range_writer_symbol(
                state->writer, 0U,
                enc_syntax_do_split_cdf(state->cdfs, 0U, context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        return write_m3_leaf(
            state, block_x, block_y, block_size, block_size);
    }
    status = write_m3_partition(
        state, block_x, block_y, half);
    if (status != ENC_OK) return status;
    status = write_m3_partition(
        state, block_x + half, block_y, half);
    if (status != ENC_OK) return status;
    status = write_m3_partition(
        state, block_x, block_y + half, half);
    if (status != ENC_OK) return status;
    return write_m3_partition(
        state, block_x + half, block_y + half, half);
}

EncStatus enc_m3_dc_grid_tile(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3TileState state;
    size_t block_columns;
    size_t block_count;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) || levels == 0 ||
        (width != 8U && width != 16U) ||
        (height != 8U && height != 16U)) {
        return ENC_INVALID_ARGUMENT;
    }
    block_columns = width / 8U;
    block_count = block_columns * (height / 8U);
    if (level_count != block_count) return ENC_INVALID_ARGUMENT;
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    enc_range_writer_init(&writer, data, capacity);
    state.writer = &writer;
    state.cdfs = &cdfs;
    state.width = width;
    state.height = height;
    state.levels = levels;
    status = write_m3_partition(&state, 0U, 0U, 64U);
    if (status != ENC_OK) return status;
    if (enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

EncStatus enc_m3_dc_tile(void *data, size_t capacity, size_t *size,
                         unsigned int quant_level, unsigned int sign) {
    int16_t level;
    if (quant_level > 32767U || sign > 1U) return ENC_INVALID_ARGUMENT;
    level = (int16_t)quant_level;
    if (sign != 0U) level = (int16_t)-level;
    return enc_m3_dc_grid_tile(
        data, capacity, size, 8U, 8U, &level, 1U);
}

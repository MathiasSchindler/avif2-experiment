#include "encoder/cdf.h"
#include "encoder/range.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static EncStatus write_m3_mode(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    size_t block_size_group);

static EncStatus range_syntax_fail(
    EncRangeWriter *writer, EncStatus status) {
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status == ENC_OK) {
        writer->status = status;
    }
    return writer->status;
}

EncStatus enc_range_writer_signed_literal(
    EncRangeWriter *writer, int32_t value, unsigned int magnitude_bits) {
    int64_t minimum;
    int64_t maximum;
    unsigned int total_bits;
    uint32_t encoded;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (magnitude_bits > 31U) {
        return range_syntax_fail(writer, ENC_INVALID_ARGUMENT);
    }
    total_bits = magnitude_bits + 1U;
    minimum = -(INT64_C(1) << magnitude_bits);
    maximum = (INT64_C(1) << magnitude_bits) - 1;
    if ((int64_t)value < minimum || (int64_t)value > maximum) {
        return range_syntax_fail(writer, ENC_INVALID_ARGUMENT);
    }
    encoded = (uint32_t)value;
    if (total_bits < 32U) {
        encoded &= (UINT32_C(1) << total_bits) - 1U;
    }
    return enc_range_writer_literal(writer, encoded, total_bits);
}

EncStatus enc_range_writer_ns(
    EncRangeWriter *writer, uint32_t value, uint32_t symbol_count) {
    unsigned int width = 0U;
    uint32_t cutoff;
    uint32_t probe;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (symbol_count == 0U || value >= symbol_count) {
        return range_syntax_fail(writer, ENC_INVALID_ARGUMENT);
    }
    if (symbol_count == 1U) {
        return enc_range_writer_status(writer);
    }
    probe = symbol_count - 1U;
    while (probe != 0U) {
        ++width;
        probe >>= 1U;
    }
    cutoff = (uint32_t)((UINT64_C(1) << width) - symbol_count);
    if (value < cutoff) {
        return enc_range_writer_literal(writer, value, width - 1U);
    }
    value += cutoff;
    if (enc_range_writer_literal(writer, value >> 1U, width - 1U) !=
        ENC_OK) {
        return enc_range_writer_status(writer);
    }
    return enc_range_writer_bypass(writer, value & 1U);
}

static unsigned int floor_log2_u32(uint32_t value) {
    unsigned int result = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

EncStatus enc_range_writer_coeff_remainder(
    EncRangeWriter *writer, uint32_t value, uint32_t predicted_level) {
    unsigned int m;
    unsigned int k;
    unsigned int cmax;
    unsigned int quotient;
    unsigned int length;
    unsigned int index;
    uint64_t xbase;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    m = floor_log2_u32(predicted_level);
    if (m < 1U) m = 1U;
    if (m > 6U) m = 6U;
    k = m + 1U;
    cmax = m + 4U < 6U ? m + 4U : 6U;
    quotient = value >> m;
    if (quotient < cmax) {
        for (index = 0U; index < quotient; ++index) {
            if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
        }
        if (enc_range_writer_bypass(writer, 1U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        return enc_range_writer_literal(
            writer, value & ((UINT32_C(1) << m) - 1U), m);
    }
    for (index = 0U; index < cmax; ++index) {
        if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    length = k;
    for (;;) {
        uint64_t next_base =
            ((uint64_t)cmax << m) +
            (UINT64_C(1) << (length + 1U)) -
            (UINT64_C(1) << k);
        if ((uint64_t)value < next_base) {
            break;
        }
        ++length;
    }
    for (index = k; index < length; ++index) {
        if (enc_range_writer_bypass(writer, 0U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    if (enc_range_writer_bypass(writer, 1U) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    xbase = ((uint64_t)cmax << m) +
            (UINT64_C(1) << length) -
            (UINT64_C(1) << k);
    return enc_range_writer_literal(
        writer, (uint32_t)((uint64_t)value - xbase), length);
}

EncStatus enc_range_writer_coeff_value(
    EncRangeWriter *writer, uint32_t magnitude, int is_eob,
    uint32_t base_levels, uint32_t base_range, uint32_t predicted_level,
    uint16_t *base_cdf, uint16_t *br_cdf, int update_cdf,
    EncCoeffValueCoding *coding) {
    uint32_t offset = is_eob != 0 ? 1U : 0U;
    uint32_t base_symbol_max;
    uint32_t base_symbol;
    uint32_t level;
    uint32_t br_symbol = 0U;
    uint32_t max_level;
    EncStatus status;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (base_levels == 0U || base_levels > 14U ||
        base_range == 0U || base_range > 15U ||
        base_cdf == 0 || br_cdf == 0 || magnitude < offset) {
        return range_syntax_fail(writer, ENC_INVALID_ARGUMENT);
    }
    base_symbol_max = base_levels + 1U - offset;
    base_symbol = magnitude - offset;
    if (base_symbol > base_symbol_max) {
        base_symbol = base_symbol_max;
    }
    status = enc_range_writer_symbol(
        writer, base_symbol, base_cdf, base_symbol_max + 1U, update_cdf);
    if (status != ENC_OK) {
        return status;
    }
    level = base_symbol + offset;
    if (level > base_levels) {
        br_symbol = magnitude - level;
        if (br_symbol > base_range) {
            br_symbol = base_range;
        }
        status = enc_range_writer_symbol(
            writer, br_symbol, br_cdf, base_range + 1U, update_cdf);
        if (status != ENC_OK) {
            return status;
        }
        level += br_symbol;
    }
    max_level = base_levels + base_range + 1U;
    if (level >= max_level) {
        status = enc_range_writer_coeff_remainder(
            writer, magnitude - level, predicted_level);
        if (status != ENC_OK) {
            return status;
        }
    }
    if (coding != 0) {
        coding->base_symbol = base_symbol;
        coding->br_symbol = br_symbol;
        coding->coded_level = level;
        coding->remainder = magnitude - level;
    }
    return ENC_OK;
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
    unsigned int enable_intra_ist,
    unsigned int enable_ext_partitions) {
    EncBitWriter writer;
    unsigned int width_bits;
    unsigned int height_bits;
    unsigned int level_index;
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
    level_index = width * height > 552960U ? 1U : 0U;
    if (width_bits == 0U || width_bits > 16U ||
        height_bits == 0U || height_bits > 16U) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    flags[13U] = enable_ibp != 0U ? 1U : 0U;
    flags[3U] = enable_ext_partitions != 0U ? 1U : 0U;
    transform_filter_flags[3U] =
        enable_intra_ist != 0U ? 1U : 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (enc_bit_writer_uvlc(&writer, 0U) != ENC_OK ||
        enc_bit_writer_write(&writer, 0U, 5U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
    enc_bit_writer_write(&writer, level_index, 5U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 1U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, width_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, height_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, width - 1U, width_bits) != ENC_OK ||
        enc_bit_writer_write(&writer, height - 1U, height_bits) != ENC_OK) {
        return writer.status;
    }
    for (index = 0U; index < sizeof(flags); ++index) {
        if (index == 4U && enable_ext_partitions == 0U) continue;
        if (enc_bit_writer_write(&writer, flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_ns(&writer, 2U, 3U) != ENC_OK) return writer.status;
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
    return write_sequence_header(data, capacity, size, 8U, 8U, 1U, 1U, 1U);
}

EncStatus enc_m3_sequence_header(void *data, size_t capacity, size_t *size,
                                 size_t width, size_t height) {
    return write_sequence_header(
        data, capacity, size, width, height, 0U, 1U, 1U);
}

EncStatus enc_m4_sequence_header(void *data, size_t capacity, size_t *size) {
    return write_sequence_header(data, capacity, size, 8U, 8U, 0U, 0U, 1U);
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
    if (enc_bit_writer_uvlc(&writer, 0U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 0U) != ENC_OK) {
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

EncStatus enc_m3_frame_header_q(void *data, size_t capacity, size_t *size,
                                unsigned int qindex) {
    return enc_m3_frame_header_q_for_size(
        data, capacity, size, 64U, 64U, qindex);
}

EncStatus enc_m3_frame_header_q_for_size(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int qindex) {
    EncBitWriter writer;
    static const uint8_t prefix_flags[] = {1U, 0U, 0U, 0U, 1U};
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        width == 0U || width > 16384U ||
        height == 0U || height > 16384U ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }

    *size = 0U;
    enc_bit_writer_init(&writer, data, capacity);
    if (enc_bit_writer_uvlc(&writer, 0U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 0U) != ENC_OK) return writer.status;
    for (index = 0U; index < sizeof(prefix_flags); ++index) {
        if (enc_bit_writer_write(
                &writer, prefix_flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
    }
    if ((width > 64U &&
         enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK) ||
        (height > 64U &&
         enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK)) {
        return writer.status;
    }
    if (enc_bit_writer_write(&writer, qindex, 8U) != ENC_OK ||
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

EncStatus enc_m3_frame_header(void *data, size_t capacity, size_t *size) {
    return enc_m3_frame_header_q(data, capacity, size, 100U);
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
    if (enc_bit_writer_uvlc(&writer, 0U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 0U) != ENC_OK) return writer.status;
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
    return magnitude >= 8U
        ? enc_range_writer_coeff_remainder(
              writer, magnitude - 8U, 0U) : ENC_OK;
}

typedef struct {
    EncRangeWriter *writer;
    EncSyntaxCdfs *cdfs;
    size_t width;
    size_t height;
    const int16_t *levels;
    size_t q_context;
    size_t level_index;
    uint8_t partition_above[224U];
    uint8_t partition_left[16U];
    uint8_t coeff_above[224U];
    uint8_t coeff_left[16U];
} EncM3TileState;

static size_t m3_context_bit(
    uint8_t value, size_t dimension, size_t extension) {
    size_t shift = 0U;
    while (dimension > 1U) {
        dimension >>= 1U;
        ++shift;
    }
    shift = shift > extension ? shift - extension : 0U;
    return ((size_t)value >> shift) & 1U;
}

static size_t m3_or_low_levels(const uint8_t *values, size_t count) {
    size_t result = 0U;
    size_t index;
    for (index = 0U; index < count; ++index) result |= values[index] & 7U;
    return result;
}

static void m3_coeff_context(
    EncM3TileState *state, size_t x, size_t y,
    size_t block_width, size_t block_height,
    size_t *skip_context, size_t *sign_context) {
    size_t column = x / 4U;
    size_t row = (y / 4U) % 16U;
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    size_t columns = column + 2U <= mi_columns ? 2U : mi_columns - column;
    size_t global_row = y / 4U;
    size_t rows =
        global_row + 2U <= mi_rows ? 2U : mi_rows - global_row;
    size_t top = m3_or_low_levels(state->coeff_above + column, columns);
    size_t side = m3_or_low_levels(state->coeff_left + row, rows);
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

static void m3_commit_coeff_context(
    EncM3TileState *state, size_t x, size_t y, int16_t level) {
    size_t column = x / 4U;
    size_t global_row = y / 4U;
    size_t row = global_row % 16U;
    size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-(int)level) : (unsigned int)level;
    uint8_t value = (uint8_t)(magnitude > 7U ? 7U : magnitude);
    size_t index;
    value = (uint8_t)(value | (level < 0 ? 8U : level > 0 ? 16U : 0U));
    for (index = column; index < column + 2U && index < mi_columns; ++index) {
        state->coeff_above[index] = value;
    }
    for (index = row;
         index < row + 2U && global_row + index - row < mi_rows;
         ++index) {
        state->coeff_left[index] = value;
    }
}

static EncStatus write_m3_leaf_tail(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height);

static EncStatus write_m3_leaf(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height) {
    size_t block_size_group =
        block_width == block_height
            ? (block_width == 16U ? 4U : 2U) : 3U;
    size_t tx_size_group =
        block_width == block_height
            ? (block_width == 16U ? 3U : 1U) : 2U;
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
    return write_m3_leaf_tail(
        state, block_x, block_y, block_width, block_height);
}

static EncStatus write_m3_large_leaf(
        EncM3TileState *state, size_t block_x, size_t block_y,
        size_t block_width, size_t block_height) {
        size_t block_size_group =
            block_width == block_height
                ? (block_width == 16U ? 4U : 2U) : 3U;
        size_t tx_size_group =
            block_width == block_height
                ? (block_width == 16U ? 3U : 1U) : 2U;
        size_t tx_y;
        size_t tx_x;
        EncStatus status = write_m3_mode(
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
        if ((block_width != 8U || block_height != 8U) &&
            enc_range_writer_symbol(
                state->writer,
                block_width == block_height ? 0U :
                block_width == 8U ? 1U : 2U,
                enc_syntax_tx_partition_type_cdf(
                    state->cdfs, 0U, 0U,
                    block_width == block_height ? 3U :
                    block_width == 8U ? 1U : 2U),
                7U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        for (tx_y = 0U; tx_y < block_height; tx_y += 8U) {
            for (tx_x = 0U; tx_x < block_width; tx_x += 8U) {
                int16_t level = state->levels[state->level_index++];
                size_t skip_context;
                size_t sign_context;
                m3_coeff_context(
                    state, block_x + tx_x, block_y + tx_y,
                    block_width, block_height, &skip_context, &sign_context);
                status = write_m3_dc_coeff(
                    state->writer, state->cdfs, level,
                    skip_context, sign_context, state->q_context);
                if (status != ENC_OK) return status;
                m3_commit_coeff_context(
                    state, block_x + tx_x, block_y + tx_y, level);
            }
        }
        return ENC_OK;
    }

    static int m3_forced_axis(
        size_t mi_rows, size_t mi_columns, size_t row, size_t column,
        size_t width, size_t height) {
        int has_rows = row + height / 2U < mi_rows;
        int has_columns = column + width / 2U < mi_columns;
        if (has_rows && has_columns) return 0;
        if (width == height) {
            return has_rows && !has_columns ? 2 : 1;
        }
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

    static int m3_partial_boundary_axis(
        size_t mi_rows, size_t mi_columns, size_t row, size_t column,
        size_t width, size_t height) {
        int has_rows = row + height / 2U < mi_rows;
        int has_columns = column + width / 2U < mi_columns;
        if (height > width && has_rows && !has_columns) return 2;
        if (width > height && !has_rows && has_columns) return 1;
        return 0;
    }

    static void m3_partition_contexts(
        EncM3TileState *state, size_t row, size_t column,
        size_t width, size_t height, int axis,
        size_t *split_context, size_t *rect_context,
        size_t *extension_context) {
        size_t split_adjust =
            width >= 16U || height >= 16U
                ? (width >= 8U || height >= 8U ? 3U : 2U) : 1U;
        size_t rect_adjust;
        size_t left_index = row % 16U;
        size_t left_smaller =
            m3_context_bit(state->partition_left[left_index], height, 1U);
        size_t above_smaller =
            m3_context_bit(state->partition_above[column], width, 1U);
        size_t first;
        size_t second;
        if (width == height) {
            split_adjust = width == 16U ? 3U :
                           width == 8U ? 2U :
                           width == 4U ? 1U : 0U;
            rect_adjust = width == 16U ? 6U : width == 8U ? 3U : 0U;
        } else if (height > width) {
            split_adjust = height == 16U ? 3U : height == 8U ? 2U : 1U;
            rect_adjust = width == 8U ? 4U : width == 4U ? 1U : 0U;
        } else {
            split_adjust = width == 16U ? 3U : width == 8U ? 2U : 1U;
            rect_adjust = height == 8U ? 5U : height == 4U ? 2U : 0U;
        }
        if (axis == 1) {
            first = m3_context_bit(
                state->partition_left[left_index], height, 2U);
            second = m3_context_bit(
                state->partition_left[
                    (left_index + height / 2U) % 16U], height, 2U);
        } else {
            first = m3_context_bit(
                state->partition_above[column], width, 2U);
            second = m3_context_bit(
                state->partition_above[column + width / 2U], width, 2U);
        }
        *split_context = split_adjust * 4U + left_smaller * 2U + above_smaller;
        *rect_context = rect_adjust * 4U + left_smaller * 2U + above_smaller;
        *extension_context = split_adjust * 4U + first * 2U + second;
    }

    static int m3_extension_allowed(
        size_t width, size_t height, int axis) {
        size_t pixel_width = width * 4U;
        size_t pixel_height = height * 4U;
        if (pixel_width <= 8U && pixel_height <= 8U) return 0;
        if ((pixel_width == 16U && pixel_height == 8U && axis == 1) ||
            (pixel_width == 8U && pixel_height == 16U && axis == 2) ||
            (pixel_width == 32U && pixel_height == 8U && axis == 1) ||
            (pixel_width == 8U && pixel_height == 32U && axis == 2) ||
            (pixel_width == 64U && pixel_height == 16U && axis == 1) ||
            (pixel_width == 16U && pixel_height == 64U && axis == 2)) {
            return 0;
        }
        return 1;
    }

    static void m3_commit_partition(
        EncM3TileState *state, size_t row, size_t column,
        size_t width, size_t height) {
        size_t index;
        size_t left_index = row % 16U;
        for (index = column;
             index < column + width && index < 224U;
             ++index) {
            state->partition_above[index] = (uint8_t)(64U - width);
        }
        for (index = left_index;
             index < left_index + height && index < 16U;
             ++index) {
            state->partition_left[index] = (uint8_t)(64U - height);
        }
    }

    static EncStatus write_m3_large_partition(
        EncM3TileState *state, size_t row, size_t column,
        size_t width, size_t height) {
        size_t mi_columns = ((state->width + 7U) / 8U) * 2U;
        size_t mi_rows = ((state->height + 7U) / 8U) * 2U;
        int forced;
        int boundary;
        int axis;
        size_t split_context;
        size_t rect_context;
        size_t extension_context;
        EncStatus status;
        if (row >= mi_rows || column >= mi_columns) return ENC_OK;
        forced = m3_forced_axis(
            mi_rows, mi_columns, row, column, width, height);
        if (forced != 0) {
            if (forced == 1) {
                status = write_m3_large_partition(
                    state, row, column, width, height / 2U);
                if (status != ENC_OK) return status;
                return write_m3_large_partition(
                    state, row + height / 2U, column, width, height / 2U);
            }
            status = write_m3_large_partition(
                state, row, column, width / 2U, height);
            if (status != ENC_OK) return status;
            return write_m3_large_partition(
                state, row, column + width / 2U, width / 2U, height);
        }
        if (width <= 4U && height <= 4U) {
            m3_partition_contexts(
                state, row, column, width, height, 1,
                &split_context, &rect_context, &extension_context);
            if (enc_range_writer_symbol(
                    state->writer, 0U,
                    enc_syntax_do_split_cdf(state->cdfs, 0U, split_context),
                    2U, 1) != ENC_OK) {
                return enc_range_writer_status(state->writer);
            }
            status = write_m3_large_leaf(
                state, column * 4U, row * 4U, width * 4U, height * 4U);
            if (status == ENC_OK) {
                m3_commit_partition(state, row, column, width, height);
            }
            return status;
        }
        boundary = m3_partial_boundary_axis(
            mi_rows, mi_columns, row, column, width, height);
        axis = height >= width ? 1 : 2;
        m3_partition_contexts(
            state, row, column, width, height, axis,
            &split_context, &rect_context, &extension_context);
        if (boundary == 0 &&
            enc_range_writer_symbol(
                state->writer, 1U,
                enc_syntax_do_split_cdf(state->cdfs, 0U, split_context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        if (enc_range_writer_symbol(
                state->writer, axis == 1 ? 0U : 1U,
                enc_syntax_rect_type_cdf(state->cdfs, 0U, rect_context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        if (m3_extension_allowed(width, height, axis) &&
            enc_range_writer_symbol(
                state->writer, 0U,
                enc_syntax_do_ext_partition_cdf(
                    state->cdfs, 0U, extension_context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->writer);
        }
        if (axis == 1) {
            status = write_m3_large_partition(
                state, row, column, width, height / 2U);
            if (status != ENC_OK) return status;
            return write_m3_large_partition(
                state, row + height / 2U, column, width, height / 2U);
        }
        status = write_m3_large_partition(
            state, row, column, width / 2U, height);
        if (status != ENC_OK) return status;
        return write_m3_large_partition(
            state, row, column + width / 2U, width / 2U, height);
}

static EncStatus write_m3_leaf_tail(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height) {
    size_t block_columns = (state->width + 7U) / 8U;
    size_t tx_columns = block_width / 8U;
    size_t tx_rows = block_height / 8U;
    size_t tx_y;
    size_t tx_x;
    EncStatus status;
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
                skip_context, sign_context, state->q_context);
            if (status != ENC_OK) return status;
        }
    }
    return ENC_OK;
}

static EncStatus write_m3_partition(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_size) {
    size_t half;
    size_t coded_width = (state->width + 7U) & ~(size_t)7U;
    size_t coded_height = (state->height + 7U) & ~(size_t)7U;
    EncStatus status;
    if (block_x >= coded_width || block_y >= coded_height) return ENC_OK;
    if (block_size == 16U &&
        block_x + block_size / 2U >= coded_width &&
        block_y + block_size / 2U < coded_height) {
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
        block_y + block_size / 2U >= coded_height &&
        block_x + block_size / 2U < coded_width) {
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
        (block_x + block_size / 2U >= coded_width ||
         block_y + block_size / 2U >= coded_height)) {
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

EncStatus enc_m3_dc_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    unsigned int qindex) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3TileState state;
    size_t block_columns;
    size_t block_count;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) || levels == 0 ||
        width == 0U || width > 896U ||
        height == 0U || height > 896U ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    block_columns = (width + 7U) / 8U;
    block_count = block_columns * ((height + 7U) / 8U);
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
    state.level_index = 0U;
    {
        size_t index;
        for (index = 0U; index < 224U; ++index) {
            state.partition_above[index] = 0U;
            state.coeff_above[index] = 0U;
        }
        for (index = 0U; index < 16U; ++index) {
            state.partition_left[index] = 0U;
            state.coeff_left[index] = 0U;
        }
    }
    state.q_context = qindex <= 90U ? 0U :
                      qindex <= 140U ? 1U :
                      qindex <= 190U ? 2U : 3U;
    if (width <= 16U && height <= 16U) {
        status = write_m3_partition(&state, 0U, 0U, 64U);
    } else {
        size_t mi_columns = ((width + 7U) / 8U) * 2U;
        size_t mi_rows = ((height + 7U) / 8U) * 2U;
        size_t row;
        size_t column;
        status = ENC_OK;
        for (row = 0U; row < mi_rows && status == ENC_OK; row += 16U) {
            if (row != 0U) {
                size_t index;
                for (index = 0U; index < 16U; ++index) {
                    state.partition_left[index] = 0U;
                    state.coeff_left[index] = 0U;
                }
            }
            for (column = 0U;
                 column < mi_columns && status == ENC_OK;
                 column += 16U) {
                status = write_m3_large_partition(
                    &state, row, column, 16U, 16U);
            }
        }
    }
    if (status != ENC_OK) return status;
    if (width > 16U && state.level_index != level_count) {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

EncStatus enc_m3_dc_grid_tile(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count) {
    return enc_m3_dc_grid_tile_q(
        data, capacity, size, width, height, levels, level_count, 100U);
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

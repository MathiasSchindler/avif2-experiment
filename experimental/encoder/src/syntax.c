#include "encoder/cdf.h"
#include "encoder/m3.h"
#include "encoder/range.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static EncStatus write_m3_mode(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    size_t block_size_group);

static int m3_directional_mode(uint8_t mode) {
    return mode == 1U || mode == 2U;
}

static size_t m3_luma_mode_index(
    uint8_t mode, size_t block_width, size_t block_height,
    uint8_t left_mode, uint8_t above_mode) {
    static const uint8_t defaults[56U] = {
        22U, 50U, 8U, 15U, 29U, 36U, 43U, 57U,
        20U, 24U, 48U, 52U, 6U, 10U, 13U, 17U,
        27U, 31U, 34U, 38U, 41U, 45U, 55U, 59U,
        21U, 23U, 49U, 51U, 7U, 9U, 14U, 16U,
        28U, 30U, 35U, 37U, 42U, 44U, 56U, 58U,
        19U, 25U, 47U, 53U, 5U, 11U, 12U, 18U,
        26U, 32U, 33U, 39U, 40U, 46U, 54U, 60U
    };
    uint8_t target;
    uint8_t neighbor[2U];
    size_t position = 5U;
    size_t index;
    if (mode == 0U) return 0U;
    if (mode >= 3U && mode <= 6U) return mode - 2U;
    target = mode == 1U ? 22U : 50U;
    if (block_width == 8U && block_height == 8U) {
        return mode == 1U ? 5U : 6U;
    }
    neighbor[0U] = left_mode == 1U ? 22U :
                   left_mode == 2U ? 50U : 0U;
    neighbor[1U] = above_mode == 1U ? 22U :
                   above_mode == 2U ? 50U : 0U;
    for (index = 0U; index < 2U; ++index) {
        if (neighbor[index] != 0U &&
            (index == 0U || neighbor[index] != neighbor[0U])) {
            if (neighbor[index] == target) return position;
            ++position;
        }
    }
    for (index = 0U; index < 56U; ++index) {
        if (defaults[index] == neighbor[0U] ||
            defaults[index] == neighbor[1U]) {
            continue;
        }
        if (defaults[index] == target) return position;
        ++position;
    }
    return 64U;
}

static EncStatus write_m3_intra_mode(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    size_t block_width, size_t block_height, size_t block_size_group,
    uint8_t mode, uint8_t left_mode, uint8_t above_mode) {
    size_t context =
        (size_t)m3_directional_mode(left_mode) +
        (size_t)m3_directional_mode(above_mode);
    size_t mode_index = m3_luma_mode_index(
        mode, block_width, block_height, left_mode, above_mode);
    if (mode > 6U || mode_index >= 8U) return ENC_INVALID_ARGUMENT;
    if (enc_range_writer_symbol(
            writer, 0U, enc_syntax_y_mode_set_cdf(cdfs), 4U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, (uint32_t)mode_index,
            enc_syntax_y_mode_index_cdf(cdfs, context), 8U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, 0U,
            enc_syntax_fsc_mode_cdf(cdfs, 0U, block_size_group),
            2U, 1) != ENC_OK ||
        ((mode == 1U || mode == 2U) && enc_range_writer_symbol(
            writer, 0U, enc_syntax_mrl_index_cdf(cdfs, 0U),
            4U, 1) != ENC_OK)) {
        return enc_range_writer_status(writer);
    }
    return ENC_OK;
}

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
    unsigned int enable_intra_ist, unsigned int chroma_format_idc,
    unsigned int enable_ext_partitions,
    const EncQuantizerDeltaConfig *qdelta) {
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
        width == 0U || height == 0U || width > 16384U || height > 16384U ||
        chroma_format_idc > 2U) {
        return ENC_INVALID_ARGUMENT;
    }
    if (qdelta != 0 &&
        (chroma_format_idc == 1U ||
         qdelta->separate_uv_delta_q < 0 ||
         qdelta->separate_uv_delta_q > 1 ||
         qdelta->equal_ac_dc_q < 0 || qdelta->equal_ac_dc_q > 1 ||
         qdelta->base_y_dc_delta_q < -23 ||
         qdelta->base_y_dc_delta_q > 8 ||
         qdelta->base_uv_dc_delta_q < -23 ||
         qdelta->base_uv_dc_delta_q > 8 ||
         qdelta->base_uv_ac_delta_q < -23 ||
         qdelta->base_uv_ac_delta_q > 8 ||
         qdelta->y_dc_delta_q_enabled < 0 ||
         qdelta->y_dc_delta_q_enabled > 1 ||
         qdelta->uv_dc_delta_q_enabled < 0 ||
         qdelta->uv_dc_delta_q_enabled > 1 ||
         qdelta->uv_ac_delta_q_enabled < 0 ||
         qdelta->uv_ac_delta_q_enabled > 1 ||
         (qdelta->equal_ac_dc_q &&
          (qdelta->base_y_dc_delta_q != 0 ||
           qdelta->y_dc_delta_q_enabled ||
           qdelta->base_uv_dc_delta_q != qdelta->base_uv_ac_delta_q ||
           qdelta->uv_dc_delta_q_enabled)))) {
        return ENC_INVALID_ARGUMENT;
    }
    width_bits = dimension_bits(width);
    height_bits = dimension_bits(height);
    level_index =
        width > ENC_M3_FROZEN_PRODUCTION_MAX_DIMENSION ||
        height > ENC_M3_FROZEN_PRODUCTION_MAX_DIMENSION
            ? 4U
            : width * height > 552960U ? 1U : 0U;
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
        enc_bit_writer_write(
            &writer, chroma_format_idc == 2U ? 4U : 0U, 5U) != ENC_OK ||
        enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, level_index, 5U) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, chroma_format_idc) != ENC_OK ||
        enc_bit_writer_uvlc(&writer, 1U) != ENC_OK ||
        enc_bit_writer_write(&writer, width_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, height_bits - 1U, 4U) != ENC_OK ||
        enc_bit_writer_write(&writer, width - 1U, width_bits) != ENC_OK ||
        enc_bit_writer_write(&writer, height - 1U, height_bits) != ENC_OK) {
        return writer.status;
    }
    for (index = 0U; index < sizeof(flags); ++index) {
        if (index == 3U && chroma_format_idc != 1U &&
            enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK) {
            return writer.status;
        }
        if (index == 4U && enable_ext_partitions == 0U) continue;
        if (enc_bit_writer_write(&writer, flags[index], 1U) != ENC_OK) {
            return writer.status;
        }
        if (index == 11U && chroma_format_idc != 1U &&
            enc_bit_writer_write(&writer, 0U, 2U) != ENC_OK) {
            return writer.status;
        }
    }
    if (enc_bit_writer_ns(&writer, 2U, 3U) != ENC_OK) return writer.status;
    for (index = 0U; index < sizeof(transform_filter_flags); ++index) {
        if (index == 5U && chroma_format_idc != 1U &&
            enc_bit_writer_write(&writer, 1U, 1U) != ENC_OK) {
            return writer.status;
        }
        if (index == 6U && chroma_format_idc != 1U &&
            enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK) {
            return writer.status;
        }
        if (index == 8U && chroma_format_idc != 1U &&
            enc_bit_writer_write(
                &writer,
                qdelta != 0 ? (uint32_t)qdelta->separate_uv_delta_q : 0U,
                1U) != ENC_OK) {
            return writer.status;
        }
        if (enc_bit_writer_write(
                &writer,
                index == 8U && qdelta != 0
                    ? (uint32_t)qdelta->equal_ac_dc_q
                    : transform_filter_flags[index],
                1U) != ENC_OK) {
            return writer.status;
        }
        if (index == 8U && chroma_format_idc != 1U &&
            qdelta != 0 && !qdelta->equal_ac_dc_q) {
            if (enc_bit_writer_write(
                    &writer,
                    (uint32_t)(qdelta->base_y_dc_delta_q + 23), 5U) != ENC_OK ||
                enc_bit_writer_write(
                    &writer, (uint32_t)qdelta->y_dc_delta_q_enabled,
                    1U) != ENC_OK ||
                enc_bit_writer_write(
                    &writer,
                    (uint32_t)(qdelta->base_uv_dc_delta_q + 23),
                    5U) != ENC_OK ||
                enc_bit_writer_write(
                    &writer, (uint32_t)qdelta->uv_dc_delta_q_enabled,
                    1U) != ENC_OK) {
                return writer.status;
            }
        }
        if (index == 8U && chroma_format_idc != 1U) {
            uint32_t base = qdelta != 0
                ? (uint32_t)(qdelta->base_uv_ac_delta_q + 23) : 23U;
            uint32_t enabled = qdelta != 0
                ? (uint32_t)qdelta->uv_ac_delta_q_enabled : 0U;
            if (enc_bit_writer_write(&writer, base, 5U) != ENC_OK ||
                enc_bit_writer_write(&writer, enabled, 1U) != ENC_OK) {
                return writer.status;
            }
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
    return write_sequence_header(
        data, capacity, size, 8U, 8U, 1U, 1U, 1U, 1U, 0);
}

EncStatus enc_m3_sequence_header(void *data, size_t capacity, size_t *size,
                                 size_t width, size_t height) {
    return write_sequence_header(
        data, capacity, size, width, height, 0U, 1U, 1U, 1U, 0);
}

EncStatus enc_m3_sequence_header_for_format(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int chroma_format_idc) {
    if (chroma_format_idc != 0U && chroma_format_idc != 2U) {
        return ENC_INVALID_ARGUMENT;
    }
    return write_sequence_header(
        data, capacity, size, width, height, 0U, 1U,
        chroma_format_idc, 1U, 0);
}

EncStatus enc_m3_sequence_header_for_qdelta(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int chroma_format_idc,
    const EncQuantizerDeltaConfig *config) {
    if (config == 0 ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U)) {
        return ENC_INVALID_ARGUMENT;
    }
    return write_sequence_header(
        data, capacity, size, width, height, 0U, 1U,
        chroma_format_idc, 1U, config);
}

EncStatus enc_m4_sequence_header(void *data, size_t capacity, size_t *size) {
    return write_sequence_header(
        data, capacity, size, 8U, 8U, 0U, 0U, 1U, 1U, 0);
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

static EncStatus write_frame_delta(
    EncBitWriter *writer, int value) {
    if (value < -64 || value > 63) return ENC_INVALID_ARGUMENT;
    if (enc_bit_writer_write(writer, value != 0, 1U) != ENC_OK) {
        return writer->status;
    }
    return value == 0
        ? ENC_OK
        : enc_bit_writer_write(writer, (uint32_t)value & 127U, 7U);
}

static EncStatus write_frame_header_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int qindex,
    const EncQuantizerDeltaConfig *config,
    const EncFrameQuantizerDeltas *deltas) {
    EncBitWriter writer;
    static const uint8_t prefix_flags[] = {1U, 0U, 0U, 0U, 1U};
    size_t index;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        width == 0U || width > 16384U ||
        height == 0U || height > 16384U ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    if ((config == 0) != (deltas == 0)) return ENC_INVALID_ARGUMENT;
    if (config != 0 &&
        (config->separate_uv_delta_q < 0 ||
         config->separate_uv_delta_q > 1 ||
         config->equal_ac_dc_q < 0 || config->equal_ac_dc_q > 1 ||
         config->y_dc_delta_q_enabled < 0 ||
         config->y_dc_delta_q_enabled > 1 ||
         config->uv_dc_delta_q_enabled < 0 ||
         config->uv_dc_delta_q_enabled > 1 ||
         config->uv_ac_delta_q_enabled < 0 ||
         config->uv_ac_delta_q_enabled > 1 ||
         deltas->y_dc < -64 || deltas->y_dc > 63 ||
         deltas->u_dc < -64 || deltas->u_dc > 63 ||
         deltas->u_ac < -64 || deltas->u_ac > 63 ||
         deltas->v_dc < -64 || deltas->v_dc > 63 ||
         deltas->v_ac < -64 || deltas->v_ac > 63 ||
         (!config->y_dc_delta_q_enabled && deltas->y_dc != 0) ||
         (config->equal_ac_dc_q &&
          (deltas->u_dc != deltas->u_ac ||
           deltas->v_dc != deltas->v_ac)) ||
         (!config->equal_ac_dc_q &&
          !config->uv_dc_delta_q_enabled &&
          (deltas->u_dc != 0 || deltas->v_dc != 0)) ||
         (!config->uv_ac_delta_q_enabled &&
          (deltas->u_ac != 0 || deltas->v_ac != 0)) ||
         (!config->separate_uv_delta_q &&
          (deltas->u_dc != deltas->v_dc ||
           deltas->u_ac != deltas->v_ac)))) {
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
    if (enc_bit_writer_write(&writer, qindex, 8U) != ENC_OK) {
        return writer.status;
    }
    if (config != 0) {
        int different =
            deltas->u_dc != deltas->v_dc ||
            deltas->u_ac != deltas->v_ac;
        if (config->y_dc_delta_q_enabled &&
            write_frame_delta(&writer, deltas->y_dc) != ENC_OK) {
            return writer.status;
        }
        if (config->uv_dc_delta_q_enabled ||
            config->uv_ac_delta_q_enabled) {
            if (config->separate_uv_delta_q &&
                enc_bit_writer_write(
                    &writer, different != 0, 1U) != ENC_OK) {
                return writer.status;
            }
            if (!config->equal_ac_dc_q &&
                config->uv_dc_delta_q_enabled &&
                write_frame_delta(&writer, deltas->u_dc) != ENC_OK) {
                return writer.status;
            }
            if (config->uv_ac_delta_q_enabled &&
                write_frame_delta(&writer, deltas->u_ac) != ENC_OK) {
                return writer.status;
            }
            if (different) {
                if (!config->equal_ac_dc_q &&
                    config->uv_dc_delta_q_enabled &&
                    write_frame_delta(&writer, deltas->v_dc) != ENC_OK) {
                    return writer.status;
                }
                if (config->uv_ac_delta_q_enabled &&
                    write_frame_delta(&writer, deltas->v_ac) != ENC_OK) {
                    return writer.status;
                }
            }
        }
    }
    if (enc_bit_writer_write(&writer, 0U, 1U) != ENC_OK ||
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

EncStatus enc_m3_frame_header_q_for_size(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int qindex) {
    return write_frame_header_q(
        data, capacity, size, width, height, qindex, 0, 0);
}

EncStatus enc_m3_frame_header_qdelta_for_size(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, unsigned int qindex,
    const EncQuantizerDeltaConfig *config,
    const EncFrameQuantizerDeltas *deltas) {
    return write_frame_header_q(
        data, capacity, size, width, height, qindex, config, deltas);
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

static const uint8_t ac8x8_scan[64U] = {
        0U, 8U, 1U, 16U, 9U, 2U, 24U, 17U, 10U, 3U, 32U, 25U, 18U, 11U,
        4U, 40U, 33U, 26U, 19U, 12U, 5U, 48U, 41U, 34U, 27U, 20U, 13U,
        6U, 56U, 49U, 42U, 35U, 28U, 21U, 14U, 7U, 57U, 50U, 43U, 36U,
        29U, 22U, 15U, 58U, 51U, 44U, 37U, 30U, 23U, 59U, 52U, 45U,
        38U, 31U, 60U, 53U, 46U, 39U, 61U, 54U, 47U, 62U, 55U, 63U
    };

static unsigned int ac8x8_magnitude(const int16_t *levels, int row, int col) {
        int value;
        if (row < 0 || col < 0 || row >= 8 || col >= 8) return 0U;
        value = levels[(size_t)row * 8U + (size_t)col];
        return value < 0 ? (unsigned int)(-value) : (unsigned int)value;
    }

static int ac8x8_is_lf(size_t position) {
        return position / 8U + position % 8U < 4U;
    }

static size_t ac8x8_lf_context(const int16_t *levels, size_t position) {
        int row = (int)(position / 8U);
        int col = (int)(position % 8U);
        unsigned int sum =
            (ac8x8_magnitude(levels, row, col + 1) > 5U ? 5U :
             ac8x8_magnitude(levels, row, col + 1)) +
            (ac8x8_magnitude(levels, row + 1, col) > 5U ? 5U :
             ac8x8_magnitude(levels, row + 1, col)) +
            (ac8x8_magnitude(levels, row + 1, col + 1) > 5U ? 5U :
             ac8x8_magnitude(levels, row + 1, col + 1)) +
            (ac8x8_magnitude(levels, row, col + 2) > 5U ? 5U :
             ac8x8_magnitude(levels, row, col + 2)) +
            (ac8x8_magnitude(levels, row + 2, col) > 5U ? 5U :
             ac8x8_magnitude(levels, row + 2, col));
        size_t context = (sum + 1U) >> 1U;
        if (position == 0U) return context > 8U ? 8U : context;
        if (row + col < 2) return 9U + (context > 6U ? 6U : context);
        return 16U + (context > 4U ? 4U : context);
    }

static size_t ac8x8_base_context(const int16_t *levels, size_t position) {
        int row = (int)(position / 8U);
        int col = (int)(position % 8U);
        unsigned int sum =
            (ac8x8_magnitude(levels, row, col + 1) > 3U ? 3U :
             ac8x8_magnitude(levels, row, col + 1)) +
            (ac8x8_magnitude(levels, row + 1, col) > 3U ? 3U :
             ac8x8_magnitude(levels, row + 1, col)) +
            (ac8x8_magnitude(levels, row + 1, col + 1) > 3U ? 3U :
             ac8x8_magnitude(levels, row + 1, col + 1)) +
            (ac8x8_magnitude(levels, row, col + 2) > 3U ? 3U :
             ac8x8_magnitude(levels, row, col + 2)) +
            (ac8x8_magnitude(levels, row + 2, col) > 3U ? 3U :
             ac8x8_magnitude(levels, row + 2, col));
        size_t context = (sum + 1U) >> 1U;
        int diagonal = row + col;
        if (context > 4U) context = 4U;
        return diagonal < 6 ? context : diagonal < 8 ? 5U + context :
                                                       10U + context;
    }

static size_t ac8x8_br_context(
        const int16_t *levels, size_t position, int low) {
        int row = (int)(position / 8U);
        int col = (int)(position % 8U);
        unsigned int sum =
            (ac8x8_magnitude(levels, row, col + 1) > 5U ? 5U :
             ac8x8_magnitude(levels, row, col + 1)) +
            (ac8x8_magnitude(levels, row + 1, col) > 5U ? 5U :
             ac8x8_magnitude(levels, row + 1, col)) +
            (ac8x8_magnitude(levels, row + 1, col + 1) > 5U ? 5U :
             ac8x8_magnitude(levels, row + 1, col + 1));
        size_t context = (sum + 1U) >> 1U;
        if (context > 6U) context = 6U;
        return low && position != 0U ? context + 7U : context;
    }

static EncStatus write_ac8x8_coeff(
        EncRangeWriter *writer, EncSyntaxCdfs *cdfs, const int16_t *levels,
        size_t skip_context, size_t sign_context, size_t q_context,
        uint16_t *secondary_tx_cdf) {
        static const uint8_t group_start[9U] = {0U, 1U, 2U, 3U, 5U, 9U, 17U, 33U, 65U};
        static const uint8_t offset_bits[9U] = {0U, 0U, 0U, 1U, 2U, 3U, 4U, 5U, 6U};
        int last = -1;
        int scan_index;
        int16_t base_levels[64U] = {0};
        uint32_t high_average = 0U;
        for (scan_index = 63; scan_index >= 0; --scan_index) {
            if (levels[ac8x8_scan[scan_index]] != 0) {
                last = scan_index;
                break;
            }

        }
        if (enc_range_writer_symbol(
                writer, last < 0 ? 1U : 0U,
                enc_syntax_txb_skip_cdf(
                    cdfs, q_context, 0U, 1U, skip_context),
                2U, 1) != ENC_OK || last < 0) {
            return enc_range_writer_status(writer);
        }
        {
            unsigned int eob = (unsigned int)last + 1U;
            unsigned int point = 1U;
            unsigned int extra;
            unsigned int bits;
            while (point + 1U < 9U && group_start[point + 1U] <= eob) ++point;
            if (enc_range_writer_symbol(
                    writer, point - 1U,
                    enc_syntax_eob_pt_64_cdf(cdfs, q_context, 0U),
                    7U, 1) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            bits = offset_bits[point];
            extra = eob - group_start[point];
            if (bits != 0U &&
                enc_range_writer_symbol(
                    writer, extra >> (bits - 1U),
                    enc_syntax_eob_extra_cdf(cdfs, q_context),
                    2U, 1) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            if (bits > 1U &&
                enc_range_writer_literal(
                    writer, extra & ((1U << (bits - 1U)) - 1U),
                    bits - 1U) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            if (eob > 1U &&
                enc_range_writer_symbol(
                    writer, 0U,
                    enc_syntax_intra_tx_type_set1_cdf(cdfs, 1U),
                    7U, 1) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            if (eob > 1U && eob <= 20U && secondary_tx_cdf != 0 &&
                enc_range_writer_symbol(
                    writer, 0U, secondary_tx_cdf, 4U, 1) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
        }
        for (scan_index = last; scan_index >= 0; --scan_index) {
            size_t position = ac8x8_scan[scan_index];
            unsigned int magnitude = ac8x8_magnitude(
                levels, (int)(position / 8U), (int)(position % 8U));
            int low = ac8x8_is_lf(position);
            unsigned int coded;
            if (scan_index == last) {
                size_t context = scan_index == 0 ? 0U :
                    scan_index <= 8 ? 1U : scan_index <= 16 ? 2U : 3U;
                if (low) {
                    unsigned int symbol = magnitude - 1U;
                    if (symbol > 4U) symbol = 4U;
                    if (enc_range_writer_symbol(
                            writer, symbol,
                            enc_syntax_coeff_base_lf_eob_cdf(
                                cdfs, q_context, 1U, context),
                            5U, 1) != ENC_OK) {
                        return enc_range_writer_status(writer);
                    }
                    coded = symbol + 1U;
                    if (coded > 4U) {
                        unsigned int br = magnitude - coded;
                        size_t br_context =
                            ac8x8_br_context(base_levels, position, 1);
                        if (br > 3U) br = 3U;
                        if (enc_range_writer_symbol(
                                writer, br,
                                enc_syntax_coeff_br_lf_cdf(
                                    cdfs, q_context, br_context),
                                4U, 1) != ENC_OK) {
                            return enc_range_writer_status(writer);
                        }
                        coded += br;
                    }
                } else {
                    unsigned int symbol = magnitude - 1U;
                    if (symbol > 2U) symbol = 2U;
                    if (enc_range_writer_symbol(
                            writer, symbol,
                            enc_syntax_coeff_base_eob_cdf(
                                cdfs, q_context, 1U, context),
                            3U, 1) != ENC_OK) {
                        return enc_range_writer_status(writer);
                    }
                    coded = symbol + 1U;
                    if (coded > 2U) {
                        unsigned int br = magnitude - coded;
                        if (br > 3U) br = 3U;
                        if (enc_range_writer_symbol(
                                writer, br,
                                enc_syntax_coeff_br_cdf(cdfs, q_context, 0U),
                                4U, 1) != ENC_OK) {
                            return enc_range_writer_status(writer);
                        }
                        coded += br;
                    }
                }
            } else if (low) {
                size_t context = ac8x8_lf_context(
                    base_levels, position);
                unsigned int symbol = magnitude > 5U ? 5U : magnitude;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_lf_cdf(
                            cdfs, q_context, 1U, context, 0U),
                        6U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
                coded = symbol;
                if (symbol > 4U) {
                    unsigned int br = magnitude - coded;
                    size_t br_context = ac8x8_br_context(
                        base_levels, position, 1);
                    if (br > 3U) br = 3U;
                    if (enc_range_writer_symbol(
                            writer, br,
                            enc_syntax_coeff_br_lf_cdf(
                                cdfs, q_context, br_context),
                            4U, 1) != ENC_OK) {
                        return enc_range_writer_status(writer);
                    }
                    coded += br;
                }
            } else {
                size_t context = ac8x8_base_context(
                    base_levels, position);
                unsigned int symbol = magnitude > 3U ? 3U : magnitude;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_cdf(
                            cdfs, q_context, 1U, context, 0U),
                        4U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
                coded = symbol;
                if (symbol > 2U) {
                    unsigned int br = magnitude - coded;
                    size_t br_context = ac8x8_br_context(
                        base_levels, position, 0);
                    if (br > 3U) br = 3U;
                    if (enc_range_writer_symbol(
                            writer, br,
                            enc_syntax_coeff_br_cdf(
                                cdfs, q_context, br_context),
                            4U, 1) != ENC_OK) {
                        return enc_range_writer_status(writer);
                    }
                    coded += br;
                }
            }
            base_levels[position] = (int16_t)coded;
        }
        for (scan_index = last; scan_index >= 0; --scan_index) {
            size_t position = ac8x8_scan[scan_index];
            int level = levels[position];
            unsigned int magnitude =
                level < 0 ? (unsigned int)(-level) : (unsigned int)level;
            unsigned int threshold = ac8x8_is_lf(position) ? 8U : 6U;
            if (magnitude == 0U) continue;
            if (position == 0U) {
                if (enc_range_writer_symbol(
                        writer, level < 0 ? 1U : 0U,
                        enc_syntax_dc_sign_cdf(
                            cdfs, q_context, 0U, 0U, sign_context),
                        2U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
            } else if (enc_range_writer_bypass(
                           writer, level < 0 ? 1U : 0U) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            if ((unsigned int)base_levels[position] >= threshold) {
                uint32_t remainder = magnitude - base_levels[position];
                if (enc_range_writer_coeff_remainder(
                        writer, remainder, high_average) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
                high_average = (high_average + remainder) >> 1U;
            }
        }
        return ENC_OK;
}

EncStatus enc_ac8x8_coeff_rate(
    const int16_t levels[64U], unsigned int qindex,
    size_t skip_context, size_t sign_context, size_t *tell_bits) {
    uint8_t data[512U];
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncStatus status;
    if (levels == 0 || tell_bits == 0 || qindex == 0U || qindex > 255U ||
        skip_context >= 10U || sign_context >= 3U) {
        return ENC_INVALID_ARGUMENT;
    }
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    enc_range_writer_init(&writer, data, sizeof(data));
    status = write_ac8x8_coeff(
        &writer, &cdfs, levels, skip_context, sign_context,
        qindex <= 90U ? 0U : qindex <= 140U ? 1U :
        qindex <= 190U ? 2U : 3U, 0);
    if (status != ENC_OK) return status;
    *tell_bits = enc_range_writer_tell(&writer);
    return ENC_OK;
}

static EncStatus write_m3_chroma_dc_coeff(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    int16_t level, size_t tx_size, size_t eob_class, size_t skip_context,
    size_t sign_context, size_t q_context, int is_v) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-(int)level) : (unsigned int)level;
    size_t eob_symbols = 0U;
    uint16_t *eob_cdf;
    uint16_t *skip_cdf = is_v != 0
        ? enc_syntax_v_txb_skip_cdf(cdfs, q_context, skip_context)
        : enc_syntax_txb_skip_cdf(
              cdfs, q_context, 0U, tx_size, skip_context);
    if (enc_range_writer_symbol(
            writer, magnitude == 0U ? 1U : 0U,
            skip_cdf, 2U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    if (magnitude == 0U) return ENC_OK;
    eob_cdf = enc_syntax_chroma_eob_pt_cdf(
        cdfs, q_context, eob_class, &eob_symbols);
    if (enc_range_writer_symbol(
            writer, 0U, eob_cdf, eob_symbols, 1) != ENC_OK ||
        enc_range_writer_symbol(
            writer, magnitude > 5U ? 4U : magnitude - 1U,
            enc_syntax_coeff_base_lf_eob_uv_cdf(
                cdfs, q_context, 0U),
            5U, 1) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    (void)sign_context;
    if (enc_range_writer_bypass(
            writer, level < 0 ? 1U : 0U) != ENC_OK) {
        return enc_range_writer_status(writer);
    }
    return magnitude >= 5U
        ? enc_range_writer_coeff_remainder(
              writer, magnitude - 5U, 0U) : ENC_OK;
}

static size_t m3_chroma_scan(
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

static unsigned int m3_chroma_magnitude(
    const int16_t *levels, size_t width, size_t height,
    int row, int column) {
    int level;
    if (row < 0 || column < 0 ||
        (size_t)row >= height || (size_t)column >= width) {
        return 0U;
    }
    level = levels[(size_t)row * width + (size_t)column];
    return level < 0 ? (unsigned int)(-level) : (unsigned int)level;
}

static size_t m3_chroma_base_context(
    const int16_t *levels, size_t width, size_t height,
    size_t position, int is_v, unsigned int limit) {
    int row = (int)(position / width);
    int column = (int)(position % width);
    unsigned int sum =
        (m3_chroma_magnitude(levels, width, height, row, column + 1) > limit
            ? limit
            : m3_chroma_magnitude(
                  levels, width, height, row, column + 1)) +
        (m3_chroma_magnitude(levels, width, height, row + 1, column) > limit
            ? limit
            : m3_chroma_magnitude(
                  levels, width, height, row + 1, column)) +
        (m3_chroma_magnitude(
             levels, width, height, row + 1, column + 1) > limit
            ? limit
            : m3_chroma_magnitude(
                  levels, width, height, row + 1, column + 1));
    size_t context = (sum + 1U) >> 1U;
    if (context > 3U) context = 3U;
    return context + (is_v != 0 ? 4U : 0U);
}

static size_t m3_chroma_br_context(
    const int16_t *levels, size_t width, size_t height, size_t position) {
    int row = (int)(position / width);
    int column = (int)(position % width);
    unsigned int sum =
        m3_chroma_magnitude(levels, width, height, row, column + 1) +
        m3_chroma_magnitude(levels, width, height, row + 1, column) +
        m3_chroma_magnitude(levels, width, height, row + 1, column + 1);
    size_t context = (sum + 1U) >> 1U;
    return context > 3U ? 3U : context;
}

static EncStatus write_m3_chroma_coeff(
    EncRangeWriter *writer, EncSyntaxCdfs *cdfs,
    const int16_t *levels, size_t width, size_t height,
    size_t tx_size, size_t eob_class, size_t skip_context,
    size_t q_context, int is_v) {
    static const uint16_t group_start[10U] = {
        0U, 1U, 2U, 3U, 5U, 9U, 17U, 33U, 65U, 129U
    };
    static const uint8_t offset_bits[10U] = {
        0U, 0U, 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U
    };
    size_t capacity = width * height;
    int last = -1;
    int scan_index;
    int16_t base_levels[256U] = {0};
    uint32_t high_average = 0U;
    uint16_t *skip_cdf = is_v != 0
        ? enc_syntax_v_txb_skip_cdf(cdfs, q_context, skip_context)
        : enc_syntax_txb_skip_cdf(
              cdfs, q_context, 0U, tx_size, skip_context);
    if (capacity > 256U) return ENC_INVALID_ARGUMENT;
    for (scan_index = (int)capacity - 1; scan_index >= 0; --scan_index) {
        if (levels[m3_chroma_scan(
                width, height, (size_t)scan_index)] != 0) {
            last = scan_index;
            break;
        }
    }
    if (last <= 0) {
        return write_m3_chroma_dc_coeff(
            writer, cdfs, levels[0U], tx_size, eob_class,
            skip_context, 0U, q_context, is_v);
    }
    if (enc_range_writer_symbol(
            writer, last < 0 ? 1U : 0U, skip_cdf, 2U, 1) != ENC_OK ||
        last < 0) {
        return enc_range_writer_status(writer);
    }
    {
        unsigned int eob = (unsigned int)last + 1U;
        unsigned int point = 1U;
        unsigned int bits;
        unsigned int extra;
        unsigned int point_symbol;
        size_t symbols = 0U;
        uint16_t *eob_cdf = enc_syntax_chroma_eob_pt_cdf(
            cdfs, q_context, eob_class, &symbols);
        while (point + 1U < 10U && group_start[point + 1U] <= eob) ++point;
        point_symbol = point - 1U;
        if (point_symbol >= symbols) point_symbol = (unsigned int)symbols - 1U;
        if (eob_cdf == 0 || point == 0U ||
            enc_range_writer_symbol(
                writer, point_symbol, eob_cdf, symbols, 1) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        if (eob_class == 4U && point_symbol == symbols - 1U &&
            enc_range_writer_literal(
                writer, point - 1U - point_symbol, 1U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        bits = offset_bits[point];
        extra = eob - group_start[point];
        if (bits != 0U &&
            enc_range_writer_symbol(
                writer, extra >> (bits - 1U),
                enc_syntax_eob_extra_cdf(cdfs, q_context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        if (bits > 1U &&
            enc_range_writer_literal(
                writer, extra & ((1U << (bits - 1U)) - 1U),
                bits - 1U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
    }
    for (scan_index = last; scan_index >= 0; --scan_index) {
        size_t position = m3_chroma_scan(
            width, height, (size_t)scan_index);
        unsigned int magnitude = m3_chroma_magnitude(
            levels, width, height,
            (int)(position / width), (int)(position % width));
        int low = position == 0U;
        unsigned int coded;
        if (scan_index == last) {
            size_t context = scan_index == 0 ? 0U :
                (size_t)scan_index <= capacity / 8U ? 1U :
                (size_t)scan_index <= capacity / 4U ? 2U : 3U;
            unsigned int symbol = magnitude - 1U;
            if (low) {
                if (symbol > 4U) symbol = 4U;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_lf_eob_uv_cdf(
                            cdfs, q_context, context),
                        5U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
            } else {
                if (symbol > 2U) symbol = 2U;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_eob_uv_cdf(
                            cdfs, q_context, context),
                        3U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
            }
            coded = symbol + 1U;
        } else {
            size_t context = m3_chroma_base_context(
                base_levels, width, height, position, is_v, low ? 5U : 3U);
            unsigned int symbol = magnitude;
            if (low) {
                if (symbol > 5U) symbol = 5U;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_lf_uv_cdf(
                            cdfs, q_context, context),
                        6U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
            } else {
                if (symbol > 3U) symbol = 3U;
                if (enc_range_writer_symbol(
                        writer, symbol,
                        enc_syntax_coeff_base_uv_cdf(
                            cdfs, q_context, context),
                        4U, 1) != ENC_OK) {
                    return enc_range_writer_status(writer);
                }
            }
            coded = symbol;
        }
        if (!low && coded > 2U) {
            unsigned int br = magnitude - coded;
            size_t context = scan_index == last ? 0U :
                m3_chroma_br_context(
                    base_levels, width, height, position);
            if (br > 3U) br = 3U;
            if (enc_range_writer_symbol(
                    writer, br,
                    enc_syntax_coeff_br_uv_cdf(cdfs, q_context, context),
                    4U, 1) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            coded += br;
        }
        base_levels[position] = (int16_t)coded;
    }
    for (scan_index = last; scan_index >= 0; --scan_index) {
        size_t position = m3_chroma_scan(
            width, height, (size_t)scan_index);
        int level = levels[position];
        unsigned int magnitude =
            level < 0 ? (unsigned int)(-level) : (unsigned int)level;
        unsigned int threshold = position == 0U ? 5U : 6U;
        if (magnitude == 0U) continue;
        if (enc_range_writer_bypass(
                writer, level < 0 ? 1U : 0U) != ENC_OK) {
            return enc_range_writer_status(writer);
        }
        if ((unsigned int)base_levels[position] >= threshold) {
            uint32_t remainder = magnitude - base_levels[position];
            if (enc_range_writer_coeff_remainder(
                    writer, remainder, high_average) != ENC_OK) {
                return enc_range_writer_status(writer);
            }
            high_average = (high_average + remainder) >> 1U;
        }
    }
    return ENC_OK;
}

typedef struct {
    EncRangeWriter *writer;
    EncSyntaxCdfs *cdfs;
    size_t width;
    size_t height;
    const int16_t *levels;
    size_t levels_per_block;
    uint16_t *secondary_tx_cdf;
    size_t q_context;
    size_t level_index;
    const uint8_t *modes;
    size_t mode_count;
    size_t mode_index;
    int has_chroma;
    int16_t u_level;
    int16_t v_level;
    uint8_t partition_above[ENC_M3_MAX_MI_COLUMNS];
    uint8_t partition_left[16U];
    uint8_t coeff_above[ENC_M3_MAX_MI_COLUMNS];
    uint8_t coeff_left[16U];
    uint8_t mode_above[ENC_M3_MAX_MI_COLUMNS];
    uint8_t mode_left[16U];
} EncM3TileState;

static uint16_t m3_leaf_mode(
    EncM3TileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height) {
    size_t column = block_x / 4U;
    size_t row = (block_y / 4U) % 16U;
    uint8_t mode = state->modes == 0 ? 0U :
        state->mode_count == 1U ? state->modes[0U] :
        state->mode_index < state->mode_count
            ? state->modes[state->mode_index] : 255U;
    uint8_t left_mode = block_x == 0U ? 0U : state->mode_left[row];
    uint8_t above_mode = block_y == 0U ? 0U : state->mode_above[column];
    size_t width_units = block_width / 4U;
    size_t height_units = block_height / 4U;
    size_t index;
    ++state->mode_index;
    if (mode == 3U) mode = block_y == 0U ? 0U : 1U;
    if (mode == 4U) mode = block_x == 0U ? 0U : 2U;
    if (mode >= 5U && mode <= 8U) mode -= 2U;
    if (mode > 6U) return UINT16_MAX;
    for (index = 0U;
         index < width_units && column + index < ENC_M3_MAX_MI_COLUMNS;
         ++index) {
        state->mode_above[column + index] = mode;
    }
    for (index = 0U; index < height_units && row + index < 16U; ++index) {
        state->mode_left[row + index] = mode;
    }
    return (uint16_t)(mode | ((uint16_t)left_mode << 3U) |
                      ((uint16_t)above_mode << 6U));
}

static const int16_t *m3_block_levels(
    const EncM3TileState *state, size_t block) {
    return state->levels + block * state->levels_per_block;
}

static uint8_t m3_cul_level(const int16_t *levels, size_t count) {
    unsigned int sum = 0U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        int level = levels[index];
        sum += level < 0 ? (unsigned int)(-level) : (unsigned int)level;
        if (sum > 7U) sum = 7U;
    }
    if (levels[0] < 0) sum |= 8U;
    else if (levels[0] > 0) sum += 16U;
    return (uint8_t)sum;
}

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
    EncM3TileState *state, size_t x, size_t y, uint8_t value) {
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
    uint16_t packed_mode = m3_leaf_mode(
        state, block_x, block_y, block_width, block_height);
    uint8_t mode = (uint8_t)(packed_mode & 7U);
    EncStatus status;
    if (packed_mode == UINT16_MAX) return ENC_INVALID_ARGUMENT;
    status = write_m3_intra_mode(
        state->writer, state->cdfs, block_width, block_height,
        block_size_group, mode,
        (uint8_t)((packed_mode >> 3U) & 7U),
        (uint8_t)((packed_mode >> 6U) & 7U));
    if (status != ENC_OK) return status;
    if (enc_range_writer_symbol(
            state->writer,
            block_width != 8U || block_height != 8U ? 1U : 0U,
            enc_syntax_tx_do_partition_cdf(
                state->cdfs, 0U, 0U, tx_size_group),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(state->writer);
    }
    status = write_m3_leaf_tail(
        state, block_x, block_y, block_width, block_height);
    if (status != ENC_OK || !state->has_chroma) return status;
    if (enc_range_writer_symbol(
            state->writer, 0U, enc_syntax_is_cfl_cdf(state->cdfs, 0U),
            2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            state->writer, 0U, enc_syntax_uv_mode_cdf(state->cdfs, 0U),
            8U, 1) != ENC_OK ||
        write_m3_chroma_dc_coeff(
            state->writer, state->cdfs, state->u_level,
            block_width == 8U ? 0U : 1U,
            block_width == 8U ? 0U : 2U,
            6U, 0U, state->q_context, 0) != ENC_OK ||
        write_m3_chroma_dc_coeff(
            state->writer, state->cdfs, state->v_level,
            block_width == 8U ? 0U : 1U,
            block_width == 8U ? 0U : 2U,
            (block_width == 8U ? 0U : 3U) +
                (state->u_level != 0 ? 6U : 0U),
            0U, state->q_context, 1) != ENC_OK) {
        return enc_range_writer_status(state->writer);
    }
    return ENC_OK;
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
        uint16_t packed_mode = m3_leaf_mode(
            state, block_x, block_y, block_width, block_height);
        uint8_t mode = (uint8_t)(packed_mode & 7U);
        EncStatus status;
        if (packed_mode == UINT16_MAX) return ENC_INVALID_ARGUMENT;
        status = write_m3_intra_mode(
            state->writer, state->cdfs, block_width, block_height,
            block_size_group, mode,
            (uint8_t)((packed_mode >> 3U) & 7U),
            (uint8_t)((packed_mode >> 6U) & 7U));
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
                const int16_t *block_levels =
                    m3_block_levels(state, state->level_index++);
                size_t skip_context;
                size_t sign_context;
                m3_coeff_context(
                    state, block_x + tx_x, block_y + tx_y,
                    block_width, block_height, &skip_context, &sign_context);
                status = state->levels_per_block == 1U
                    ? write_m3_dc_coeff(
                          state->writer, state->cdfs, block_levels[0],
                          skip_context, sign_context, state->q_context)
                    : write_ac8x8_coeff(
                          state->writer, state->cdfs, block_levels,
                          skip_context, sign_context, state->q_context,
                          state->secondary_tx_cdf);
                if (status != ENC_OK) return status;
                m3_commit_coeff_context(
                    state, block_x + tx_x, block_y + tx_y,
                    m3_cul_level(block_levels, state->levels_per_block));
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
            rect_adjust = width == 8U ? 4U :
                          width == 4U || width == 2U ? 1U : 0U;
        } else {
            split_adjust = width == 16U ? 3U : width == 8U ? 2U : 1U;
            rect_adjust = height == 8U ? 5U :
                          height == 4U || height == 2U ? 2U : 0U;
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
             index < column + width && index < ENC_M3_MAX_MI_COLUMNS;
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
                const int16_t *above =
                    m3_block_levels(state, block - block_columns);
                uint8_t cul = m3_cul_level(above, state->levels_per_block);
                size_t magnitude = cul & 7U;
                skip_context += magnitude > 4U ? 4U : magnitude;
                score += (cul & 8U) != 0U ? -1 :
                         (cul & 16U) != 0U ? 1 : 0;
            }
            if (column != 0U) {
                const int16_t *left = m3_block_levels(state, block - 1U);
                uint8_t cul = m3_cul_level(left, state->levels_per_block);
                size_t magnitude = cul & 7U;
                skip_context += magnitude > 4U ? 4U : magnitude;
                score += (cul & 8U) != 0U ? -1 :
                         (cul & 16U) != 0U ? 1 : 0;
            }
            if (block_width != 8U || block_height != 8U) {
                skip_context = (skip_context + 3U) >> 1U;
            }
            if (score < 0) sign_context = 1U;
            if (score > 0) sign_context = 2U;
            status = state->levels_per_block == 1U
                ? write_m3_dc_coeff(
                      state->writer, state->cdfs,
                      m3_block_levels(state, block)[0],
                      skip_context, sign_context, state->q_context)
                : write_ac8x8_coeff(
                      state->writer, state->cdfs,
                      m3_block_levels(state, block),
                      skip_context, sign_context, state->q_context,
                      state->secondary_tx_cdf);
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

static EncStatus enc_m3_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    const uint8_t *modes, size_t mode_count,
    unsigned int qindex, size_t levels_per_block) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3TileState state;
    size_t block_columns;
    size_t block_count;
    EncStatus status;
    uint16_t secondary_tx_cdf[8U];
    if (size == 0 || (data == 0 && capacity != 0U) || levels == 0 ||
        width == 0U || width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height == 0U || height > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        qindex == 0U || qindex > 255U ||
        ((modes == 0) != (mode_count == 0U)) ||
        (levels_per_block != 1U && levels_per_block != 64U)) {
        return ENC_INVALID_ARGUMENT;
    }
    block_columns = (width + 7U) / 8U;
    block_count = block_columns * ((height + 7U) / 8U);
    if (level_count != block_count * levels_per_block) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    if (levels_per_block == 64U) {
        static const uint16_t thresholds[3U] = {2357U, 9504U, 16641U};
        status = enc_cdf_materialize(
            secondary_tx_cdf, 8U, thresholds, 4U, 5U, 0U);
        if (status != ENC_OK) return status;
    }
    enc_range_writer_init(&writer, data, capacity);
    state.writer = &writer;
    state.cdfs = &cdfs;
    state.width = width;
    state.height = height;
    state.levels = levels;
    state.levels_per_block = levels_per_block;
    state.secondary_tx_cdf =
        levels_per_block == 64U ? secondary_tx_cdf : 0;
    state.level_index = 0U;
    state.modes = modes;
    state.mode_count = mode_count;
    state.mode_index = 0U;
    state.has_chroma = 0;
    state.u_level = 0;
    state.v_level = 0;
    {
        size_t index;
        for (index = 0U; index < ENC_M3_MAX_MI_COLUMNS; ++index) {
            state.partition_above[index] = 0U;
            state.coeff_above[index] = 0U;
            state.mode_above[index] = 0U;
        }
        for (index = 0U; index < 16U; ++index) {
            state.partition_left[index] = 0U;
            state.coeff_left[index] = 0U;
            state.mode_left[index] = 0U;
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
                    state.mode_left[index] = 0U;
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
    if (width > 16U && state.level_index != block_count) {
        return ENC_INVALID_ARGUMENT;
    }
    if (modes != 0 && mode_count != 1U && state.mode_index != mode_count) {
        return ENC_INVALID_ARGUMENT;
    }

    if (enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

EncStatus enc_m3_dc_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    unsigned int qindex) {
    return enc_m3_grid_tile_q(
        data, capacity, size, width, height, levels, level_count,
        0, 0U, qindex, 1U);
}

EncStatus enc_m3_intra_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    const uint8_t *luma_modes, size_t luma_mode_count,
    unsigned int qindex) {
    return enc_m3_grid_tile_q(
        data, capacity, size, width, height, levels, level_count,
        luma_modes, luma_mode_count, qindex, 1U);
}

EncStatus enc_ac8x8_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count,
    unsigned int qindex) {
    return enc_m3_grid_tile_q(
        data, capacity, size, width, height, levels, level_count,
        0, 0U, qindex, 64U);
}

EncStatus enc_m3_dc_grid_tile(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *levels, size_t level_count) {
    return enc_m3_dc_grid_tile_q(
        data, capacity, size, width, height, levels, level_count, 100U);
}

EncStatus enc_m3_dc_i420_i444_8x8_tile_q(
    void *data, size_t capacity, size_t *size,
    int16_t y_level, int16_t u_level, int16_t v_level,
    unsigned int chroma_format_idc, unsigned int qindex) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    size_t q_context;
    size_t chroma_tx_size;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    q_context = qindex <= 90U ? 0U :
                qindex <= 140U ? 1U :
                qindex <= 190U ? 2U : 3U;
    chroma_tx_size = chroma_format_idc == 2U ? 1U : 0U;
    enc_range_writer_init(&writer, data, capacity);
    if (enc_range_writer_symbol(
            &writer, 0U, enc_syntax_do_split_cdf(&cdfs, 0U, 0U),
            2U, 1) != ENC_OK ||
        write_m3_mode(&writer, &cdfs, 2U) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U,
            enc_syntax_tx_do_partition_cdf(&cdfs, 0U, 0U, 1U),
            2U, 1) != ENC_OK ||
        write_m3_dc_coeff(
            &writer, &cdfs, y_level, 0U, 0U, q_context) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U, enc_syntax_is_cfl_cdf(&cdfs, 0U),
            2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            &writer, 0U, enc_syntax_uv_mode_cdf(&cdfs, 0U),
            8U, 1) != ENC_OK ||
        write_m3_chroma_dc_coeff(
            &writer, &cdfs, u_level, chroma_tx_size,
            chroma_tx_size == 0U ? 0U : 2U,
            6U, 0U, q_context, 0) != ENC_OK ||
        write_m3_chroma_dc_coeff(
            &writer, &cdfs, v_level, chroma_tx_size,
            chroma_tx_size == 0U ? 0U : 2U,
            u_level != 0 ? 6U : 0U, 0U, q_context, 1) != ENC_OK ||
        enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

typedef struct {
    EncM3TileState base;
    const int16_t *plane_levels[3U];
    size_t level_index;
    size_t chroma_level_index;
    size_t chroma_coefficient_offset[2U];
    int chroma_ac;
    const uint8_t *chroma_modes;
    size_t chroma_mode_count;
    size_t chroma_mode_index;
    uint8_t luma_mode;
    unsigned int chroma_format_idc;
    uint8_t chroma_above[2U][ENC_M3_MAX_MI_COLUMNS];
    uint8_t chroma_left[2U][16U];
} EncM3ColorTileState;

static size_t m3_uv_mode_symbol(uint8_t luma_mode, uint8_t chroma_mode) {
    if (luma_mode >= 5U && luma_mode <= 8U) luma_mode -= 2U;
    if (chroma_mode >= 5U && chroma_mode <= 8U) chroma_mode -= 2U;
    if (!m3_directional_mode(luma_mode)) {
        return chroma_mode == 0U ? 0U :
               chroma_mode >= 3U ? chroma_mode - 2U :
               chroma_mode == 1U ? 5U : 6U;
    }
    if (chroma_mode == luma_mode) return 0U;
    if (chroma_mode == 0U) return 1U;
    if (chroma_mode >= 3U && chroma_mode <= 6U) return chroma_mode - 1U;
    return 6U;
}

static void m3_chroma_coeff_context(
    EncM3ColorTileState *state, size_t plane, size_t x, size_t y,
    size_t tx_width, size_t tx_height,
    size_t *skip_context, size_t *sign_context) {
    size_t width_units = tx_width / 4U;
    size_t height_units = tx_height / 4U;
    size_t column = x / 4U;
    size_t row = (y / 4U) % 16U;
    size_t top = 0U;
    size_t side = 0U;
    int score = 0;
    size_t index;
    for (index = 0U; index < width_units; ++index) {
        uint8_t above = state->chroma_above[plane][column + index];
        top |= above & 7U;
        score += (above & 16U) != 0U ? 1 :
                 (above & 8U) != 0U ? -1 : 0;
    }
    for (index = 0U; index < height_units; ++index) {
        uint8_t left = state->chroma_left[plane][row + index];
        side |= left & 7U;
        score += (left & 16U) != 0U ? 1 :
                 (left & 8U) != 0U ? -1 : 0;
    }
    *skip_context = (top != 0U ? 1U : 0U) +
                    (side != 0U ? 1U : 0U);
    *sign_context = score < 0 ? 1U : score > 0 ? 2U : 0U;
}

static void m3_commit_chroma_coeff_context(
    EncM3ColorTileState *state, size_t plane, size_t x, size_t y,
    size_t tx_width, size_t tx_height,
    const int16_t *levels, size_t level_count) {
    size_t width_units = tx_width / 4U;
    size_t height_units = tx_height / 4U;
    size_t column = x / 4U;
    size_t row = (y / 4U) % 16U;
    uint8_t value = m3_cul_level(levels, level_count);
    size_t index;
    for (index = 0U; index < width_units; ++index) {
        state->chroma_above[plane][column + index] = value;
    }
    for (index = 0U; index < height_units; ++index) {
        state->chroma_left[plane][row + index] = value;
    }
}

static EncStatus write_m3_color_leaf(
    EncM3ColorTileState *state, size_t block_x, size_t block_y,
    size_t block_width, size_t block_height) {
    size_t subsampling = state->chroma_format_idc == 0U ? 1U : 0U;
    size_t chroma_x = block_x >> subsampling;
    size_t chroma_y = block_y >> subsampling;
    size_t chroma_tx_width = block_width >> subsampling;
    size_t chroma_tx_height = block_height >> subsampling;
    size_t chroma_tx_max = chroma_tx_width > chroma_tx_height
        ? chroma_tx_width : chroma_tx_height;
    size_t chroma_tx_index = chroma_tx_max == 4U ? 0U :
                             chroma_tx_max == 8U ? 1U : 2U;
    size_t chroma_eob_class =
        chroma_tx_width * chroma_tx_height == 16U ? 0U :
        chroma_tx_width * chroma_tx_height == 32U ? 1U :
        chroma_tx_width * chroma_tx_height == 64U ? 2U :
        chroma_tx_width * chroma_tx_height == 128U ? 3U : 4U;
    size_t chroma_index = state->chroma_level_index++;
    size_t coefficient_count = chroma_tx_width * chroma_tx_height;
    const int16_t *u_levels = state->chroma_ac
        ? state->plane_levels[1U] + state->chroma_coefficient_offset[0U]
        : state->plane_levels[1U] + chroma_index;
    const int16_t *v_levels = state->chroma_ac
        ? state->plane_levels[2U] + state->chroma_coefficient_offset[1U]
        : state->plane_levels[2U] + chroma_index;
    size_t u_skip;
    size_t u_sign;
    size_t v_skip;
    size_t v_sign;
    uint8_t luma_mode = state->luma_mode == 3U
        ? (block_y == 0U ? 0U : 1U)
        : state->luma_mode == 4U
            ? (block_x == 0U ? 0U : 2U) : state->luma_mode;
    uint8_t chroma_mode = state->chroma_modes == 0 ? 0U :
        state->chroma_mode_count == 1U ? state->chroma_modes[0U] :
        state->chroma_mode_index < state->chroma_mode_count
            ? state->chroma_modes[state->chroma_mode_index] : 255U;
    EncStatus status;
    ++state->chroma_mode_index;
    if (luma_mode >= 5U && luma_mode <= 8U) luma_mode -= 2U;
    if (chroma_mode > 2U && (chroma_mode < 5U || chroma_mode > 8U)) {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_range_writer_symbol(
            state->base.writer, 0U,
            enc_syntax_is_cfl_cdf(state->base.cdfs, 0U),
            2U, 1) != ENC_OK ||
        enc_range_writer_symbol(
            state->base.writer,
            (uint32_t)m3_uv_mode_symbol(luma_mode, chroma_mode),
            enc_syntax_uv_mode_cdf(
                state->base.cdfs,
                m3_directional_mode(luma_mode) ? 1U : 0U),
            8U, 1) != ENC_OK) {
        return enc_range_writer_status(state->base.writer);
    }
    m3_chroma_coeff_context(
        state, 0U, chroma_x, chroma_y,
        chroma_tx_width, chroma_tx_height,
        &u_skip, &u_sign);
    status = state->chroma_ac
        ? write_m3_chroma_coeff(
              state->base.writer, state->base.cdfs, u_levels,
              chroma_tx_width, chroma_tx_height, chroma_tx_index,
              chroma_eob_class, u_skip + 6U, state->base.q_context, 0)
        : write_m3_chroma_dc_coeff(
              state->base.writer, state->base.cdfs, u_levels[0U],
              chroma_tx_index, chroma_eob_class,
              u_skip + 6U, u_sign, state->base.q_context, 0);
    if (status != ENC_OK) return status;
    m3_commit_chroma_coeff_context(
        state, 0U, chroma_x, chroma_y,
        chroma_tx_width, chroma_tx_height,
        u_levels, state->chroma_ac ? coefficient_count : 1U);
    m3_chroma_coeff_context(
        state, 1U, chroma_x, chroma_y,
        chroma_tx_width, chroma_tx_height,
        &v_skip, &v_sign);
    status = state->chroma_ac
        ? write_m3_chroma_coeff(
              state->base.writer, state->base.cdfs, v_levels,
              chroma_tx_width, chroma_tx_height, chroma_tx_index,
              chroma_eob_class,
              v_skip + (m3_cul_level(u_levels, coefficient_count) != 0U
                  ? 6U : 0U),
              state->base.q_context, 1)
        : write_m3_chroma_dc_coeff(
              state->base.writer, state->base.cdfs, v_levels[0U],
              chroma_tx_index, chroma_eob_class,
              v_skip + (u_levels[0U] != 0 ? 6U : 0U),
              v_sign, state->base.q_context, 1);
    if (status != ENC_OK) return status;
    m3_commit_chroma_coeff_context(
        state, 1U, chroma_x, chroma_y,
        chroma_tx_width, chroma_tx_height,
        v_levels, state->chroma_ac ? coefficient_count : 1U);
    if (state->chroma_ac) {
        state->chroma_coefficient_offset[0U] += coefficient_count;
        state->chroma_coefficient_offset[1U] += coefficient_count;
    }
    return ENC_OK;
}

static EncStatus write_m3_color_partition(
    EncM3ColorTileState *state, size_t row, size_t column,
    size_t width, size_t height) {
    size_t mi_columns = ((state->base.width + 7U) / 8U) * 2U;
    size_t mi_rows = ((state->base.height + 7U) / 8U) * 2U;
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
            status = write_m3_color_partition(
                state, row, column, width, height / 2U);
            if (status != ENC_OK) return status;
            return write_m3_color_partition(
                state, row + height / 2U, column, width, height / 2U);
        }
        status = write_m3_color_partition(
            state, row, column, width / 2U, height);
        if (status != ENC_OK) return status;
        return write_m3_color_partition(
            state, row, column + width / 2U, width / 2U, height);
    }
    if (width <= 4U && height <= 4U) {
        m3_partition_contexts(
            &state->base, row, column, width, height, 1,
            &split_context, &rect_context, &extension_context);
        if ((width != 2U || height != 2U) &&
            enc_range_writer_symbol(
                state->base.writer, 0U,
                enc_syntax_do_split_cdf(
                    state->base.cdfs, 1U, split_context),
                2U, 1) != ENC_OK) {
            return enc_range_writer_status(state->base.writer);
        }
        status = write_m3_color_leaf(
            state, column * 4U, row * 4U, width * 4U, height * 4U);
        if (status == ENC_OK) {
            m3_commit_partition(&state->base, row, column, width, height);
        }
        return status;
    }
    boundary = m3_partial_boundary_axis(
        mi_rows, mi_columns, row, column, width, height);
    axis = height >= width ? 1 : 2;
    m3_partition_contexts(
        &state->base, row, column, width, height, axis,
        &split_context, &rect_context, &extension_context);
    if (boundary == 0 &&
        enc_range_writer_symbol(
            state->base.writer, 1U,
            enc_syntax_do_split_cdf(
                state->base.cdfs, 1U, split_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(state->base.writer);
    }
    if (enc_range_writer_symbol(
            state->base.writer, axis == 1 ? 0U : 1U,
            enc_syntax_rect_type_cdf(
                state->base.cdfs, 1U, rect_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(state->base.writer);
    }
    if (m3_extension_allowed(width, height, axis) &&
        enc_range_writer_symbol(
            state->base.writer, 0U,
            enc_syntax_do_ext_partition_cdf(
                state->base.cdfs, 1U, extension_context),
            2U, 1) != ENC_OK) {
        return enc_range_writer_status(state->base.writer);
    }
    if (axis == 1) {
        status = write_m3_color_partition(
            state, row, column, width, height / 2U);
        if (status != ENC_OK) return status;
        return write_m3_color_partition(
            state, row + height / 2U, column, width, height / 2U);
    }
    status = write_m3_color_partition(
        state, row, column, width / 2U, height);
    if (status != ENC_OK) return status;
    return write_m3_color_partition(
        state, row, column + width / 2U, width / 2U, height);
}

static EncStatus enc_m3_yuv_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, const int16_t *u_levels,
    const int16_t *v_levels, size_t level_count,
    const uint8_t *luma_modes, size_t luma_mode_count,
    const uint8_t *chroma_modes, size_t chroma_mode_count,
    unsigned int chroma_format_idc, unsigned int qindex,
    size_t luma_levels_per_block, int chroma_ac,
    size_t chroma_level_count) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3ColorTileState state;
    EncM3TileState luma;
    size_t block_count;
    size_t index;
    size_t row;
    size_t column;
    EncStatus status;
    uint16_t secondary_tx_cdf[8U];
    if (size == 0 || (data == 0 && capacity != 0U) ||
        y_levels == 0 || u_levels == 0 || v_levels == 0 ||
        width == 0U || height == 0U ||
        width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        ((luma_modes == 0) != (luma_mode_count == 0U)) ||
        ((chroma_modes == 0) != (chroma_mode_count == 0U)) ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    block_count = ((width + 7U) / 8U) * ((height + 7U) / 8U);
    if (level_count != block_count) return ENC_INVALID_ARGUMENT;
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    if (luma_levels_per_block == 64U) {
        static const uint16_t thresholds[3U] = {2357U, 9504U, 16641U};
        status = enc_cdf_materialize(
            secondary_tx_cdf, 8U, thresholds, 4U, 5U, 0U);
        if (status != ENC_OK) return status;
    } else if (luma_levels_per_block != 1U) {
        return ENC_INVALID_ARGUMENT;
    }
    enc_range_writer_init(&writer, data, capacity);
    if (width == 8U && height == 8U) {
        uint8_t luma_mode = luma_modes == 0 ? 0U : luma_modes[0U];
        uint8_t chroma_mode = chroma_modes == 0 ? 0U : chroma_modes[0U];
        size_t q_context = qindex <= 90U ? 0U :
                           qindex <= 140U ? 1U :
                           qindex <= 190U ? 2U : 3U;
        size_t chroma_tx_size = chroma_format_idc == 2U ? 1U : 0U;
        size_t chroma_width = chroma_format_idc == 2U ? 8U : 4U;
        size_t chroma_height = chroma_format_idc == 2U ? 8U : 4U;
        size_t chroma_count = chroma_width * chroma_height;
        size_t chroma_eob_class = chroma_count == 16U ? 0U : 2U;
        size_t chroma_index;
        int u_has_eob = 0;
        if (chroma_ac && chroma_level_count != chroma_count) {
            return ENC_INVALID_ARGUMENT;
        }
        if (chroma_ac) {
            for (chroma_index = 0U;
                 chroma_index < chroma_count; ++chroma_index) {
                u_has_eob |= u_levels[chroma_index] != 0;
            }
        }
        if (enc_range_writer_symbol(
                &writer, 0U, enc_syntax_do_split_cdf(&cdfs, 0U, 0U),
                2U, 1) != ENC_OK ||
            write_m3_intra_mode(
                &writer, &cdfs, 8U, 8U, 2U,
                luma_mode, 0U, 0U) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, 0U,
                enc_syntax_tx_do_partition_cdf(&cdfs, 0U, 0U, 1U),
                2U, 1) != ENC_OK ||
            (luma_levels_per_block == 1U
                ? write_m3_dc_coeff(
                      &writer, &cdfs, y_levels[0U], 0U, 0U, q_context)
                : write_ac8x8_coeff(
                      &writer, &cdfs, y_levels, 0U, 0U, q_context,
                      secondary_tx_cdf)) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, 0U, enc_syntax_is_cfl_cdf(&cdfs, 0U),
                2U, 1) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, (uint32_t)m3_uv_mode_symbol(
                    luma_mode, chroma_mode),
                enc_syntax_uv_mode_cdf(
                    &cdfs, m3_directional_mode(luma_mode) ? 1U : 0U),
                8U, 1) != ENC_OK ||
            (chroma_ac
                ? write_m3_chroma_coeff(
                      &writer, &cdfs, u_levels,
                      chroma_width, chroma_height, chroma_tx_size,
                      chroma_eob_class, 6U, q_context, 0)
                : write_m3_chroma_dc_coeff(
                      &writer, &cdfs, u_levels[0U], chroma_tx_size,
                      chroma_eob_class, 6U, 0U, q_context, 0)) != ENC_OK ||
            (chroma_ac
                ? write_m3_chroma_coeff(
                      &writer, &cdfs, v_levels,
                      chroma_width, chroma_height, chroma_tx_size,
                      chroma_eob_class,
                      u_has_eob ? 6U : 0U, q_context, 1)
                : write_m3_chroma_dc_coeff(
                      &writer, &cdfs, v_levels[0U], chroma_tx_size,
                      chroma_eob_class,
                      u_levels[0U] != 0 ? 6U : 0U,
                      0U, q_context, 1)) != ENC_OK ||
            enc_range_writer_finish(&writer) != ENC_OK) {
            return enc_range_writer_status(&writer);
        }
        *size = enc_range_writer_bytes_used(&writer);
        return ENC_OK;
    }
    state.base.writer = &writer;
    state.base.cdfs = &cdfs;
    state.base.width = width;
    state.base.height = height;
    state.base.levels = y_levels;
    state.base.levels_per_block = luma_levels_per_block;
    state.base.secondary_tx_cdf =
        luma_levels_per_block == 64U ? secondary_tx_cdf : 0;
    state.base.q_context = qindex <= 90U ? 0U :
                           qindex <= 140U ? 1U :
                           qindex <= 190U ? 2U : 3U;
    state.base.level_index = 0U;
    state.base.modes = 0;
    state.base.mode_count = 0U;
    state.base.mode_index = 0U;
    state.plane_levels[0U] = y_levels;
    state.plane_levels[1U] = u_levels;
    state.plane_levels[2U] = v_levels;
    state.level_index = 0U;
    state.chroma_level_index = 0U;
    state.chroma_coefficient_offset[0U] = 0U;
    state.chroma_coefficient_offset[1U] = 0U;
    state.chroma_ac = chroma_ac;
    state.chroma_modes = chroma_modes;
    state.chroma_mode_count = chroma_mode_count;
    state.chroma_mode_index = 0U;
    state.luma_mode = luma_modes == 0 ? 0U : luma_modes[0U];
    state.chroma_format_idc = chroma_format_idc;
    luma.writer = &writer;
    luma.cdfs = &cdfs;
    luma.width = width;
    luma.height = height;
    luma.levels = y_levels;
    luma.levels_per_block = luma_levels_per_block;
    luma.secondary_tx_cdf =
        luma_levels_per_block == 64U ? secondary_tx_cdf : 0;
    luma.q_context = state.base.q_context;
    luma.level_index = 0U;
    luma.modes = luma_modes;
    luma.mode_count = luma_mode_count;
    luma.mode_index = 0U;
    luma.has_chroma = 2;
    luma.u_level = 0;
    luma.v_level = 0;
    for (index = 0U; index < ENC_M3_MAX_MI_COLUMNS; ++index) {
        luma.partition_above[index] = 0U;
        luma.coeff_above[index] = 0U;
        luma.mode_above[index] = 0U;
        state.base.partition_above[index] = 0U;
        state.base.coeff_above[index] = 0U;
        state.base.mode_above[index] = 0U;
        state.chroma_above[0U][index] = 0U;
        state.chroma_above[1U][index] = 0U;
    }
    for (index = 0U; index < 16U; ++index) {
        luma.partition_left[index] = 0U;
        luma.coeff_left[index] = 0U;
        luma.mode_left[index] = 0U;
        state.base.partition_left[index] = 0U;
        state.base.coeff_left[index] = 0U;
        state.base.mode_left[index] = 0U;
        state.chroma_left[0U][index] = 0U;
        state.chroma_left[1U][index] = 0U;
    }
    status = ENC_OK;
    for (row = 0U;
         row < ((height + 7U) / 8U) * 2U && status == ENC_OK;
         row += 16U) {
        if (row != 0U) {
            for (index = 0U; index < 16U; ++index) {
                luma.partition_left[index] = 0U;
                luma.coeff_left[index] = 0U;
                luma.mode_left[index] = 0U;
                state.base.partition_left[index] = 0U;
                state.base.coeff_left[index] = 0U;
                state.base.mode_left[index] = 0U;
                state.chroma_left[0U][index] = 0U;
                state.chroma_left[1U][index] = 0U;
            }
        }
        for (column = 0U;
             column < ((width + 7U) / 8U) * 2U && status == ENC_OK;
             column += 16U) {
            size_t chroma_columns = ((width + 7U) / 8U) * 2U;
            size_t chroma_rows = ((height + 7U) / 8U) * 2U;
            status = write_m3_large_partition(
                &luma, row, column, 16U, 16U);
            if (status == ENC_OK) {
                if (chroma_columns - column > 8U &&
                    chroma_rows - row > 8U) {
                    status = write_m3_color_partition(
                        &state, row, column, 16U, 8U);
                    if (status == ENC_OK) {
                        status = write_m3_color_partition(
                            &state, row + 8U, column, 16U, 8U);
                    }
                } else {
                    status = write_m3_color_partition(
                        &state, row, column, 16U, 16U);
                }
            }
        }
    }
    if (status != ENC_OK) return status;
    if (luma.level_index != level_count) return ENC_INVALID_ARGUMENT;
    if (luma_modes != 0 && luma_mode_count != 1U &&
        luma.mode_index != luma_mode_count) {
        return ENC_INVALID_ARGUMENT;
    }
    if (state.chroma_level_index !=
        (width <= 8U && height <= 8U
            ? 1U : ((width + 15U) / 16U) * ((height + 15U) / 16U))) {
        return ENC_INVALID_ARGUMENT;
    }
    if (chroma_ac) {
        if (state.chroma_coefficient_offset[0U] != chroma_level_count ||
            state.chroma_coefficient_offset[1U] != chroma_level_count) {
            return ENC_INVALID_ARGUMENT;
        }
    }
    if (chroma_modes != 0 && chroma_mode_count != 1U &&
        state.chroma_mode_index != chroma_mode_count) {
        return ENC_INVALID_ARGUMENT;
    }
    if (enc_range_writer_finish(&writer) != ENC_OK)
        return enc_range_writer_status(&writer);
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

EncStatus enc_m3_dc_yuv_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, const int16_t *u_levels,
    const int16_t *v_levels, size_t level_count,
    unsigned int chroma_format_idc, unsigned int qindex) {
    return enc_m3_yuv_grid_tile_q(
        data, capacity, size, width, height,
        y_levels, u_levels, v_levels, level_count,
        0, 0U, 0, 0U, chroma_format_idc, qindex, 1U, 0, 0U);
}

EncStatus enc_ac8x8_yuv_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, const int16_t *u_levels,
    const int16_t *v_levels, size_t level_count,
    unsigned int chroma_format_idc, unsigned int qindex) {
    return enc_m3_yuv_grid_tile_q(
        data, capacity, size, width, height,
        y_levels, u_levels, v_levels, level_count,
        0, 0U, 0, 0U, chroma_format_idc, qindex, 64U, 0, 0U);
}

EncStatus enc_ac8x8_yuv_chroma_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, const int16_t *u_levels,
    const int16_t *v_levels, size_t level_count, size_t chroma_level_count,
    unsigned int chroma_format_idc, unsigned int qindex) {
    return enc_m3_yuv_grid_tile_q(
        data, capacity, size, width, height,
        y_levels, u_levels, v_levels, level_count,
        0, 0U, 0, 0U, chroma_format_idc, qindex,
        64U, 1, chroma_level_count);
}

EncStatus enc_m3_intra_yuv_grid_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, const int16_t *u_levels,
    const int16_t *v_levels, size_t level_count,
    const uint8_t *luma_modes, size_t luma_mode_count,
    const uint8_t *chroma_modes, size_t chroma_mode_count,
    unsigned int chroma_format_idc, unsigned int qindex) {
    return enc_m3_yuv_grid_tile_q(
        data, capacity, size, width, height,
        y_levels, u_levels, v_levels, level_count,
        luma_modes, luma_mode_count, chroma_modes, chroma_mode_count,
        chroma_format_idc, qindex, 1U, 0, 0U);
}

EncStatus enc_m3_dc_i420_single_leaf_tile_q(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height,
    const int16_t *y_levels, size_t y_level_count,
    int16_t u_level, int16_t v_level, unsigned int qindex) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3TileState state;
    size_t coded_width;
    size_t coded_height;
    size_t index;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) || y_levels == 0 ||
        width == 0U || width > 16U || height == 0U || height > 16U ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    coded_width = (width + 7U) & ~(size_t)7U;
    coded_height = (height + 7U) & ~(size_t)7U;
    if (coded_width != coded_height ||
        y_level_count != (coded_width / 8U) * (coded_height / 8U)) {
        return ENC_UNSUPPORTED;
    }
    *size = 0U;
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    enc_range_writer_init(&writer, data, capacity);
    state.writer = &writer;
    state.cdfs = &cdfs;
    state.width = width;
    state.height = height;
    state.levels = y_levels;
    state.levels_per_block = 1U;
    state.secondary_tx_cdf = 0;
    state.q_context = qindex <= 90U ? 0U :
                      qindex <= 140U ? 1U :
                      qindex <= 190U ? 2U : 3U;
    state.level_index = 0U;
    state.modes = 0;
    state.mode_count = 0U;
    state.mode_index = 0U;
    state.has_chroma = 1;
    state.u_level = u_level;
    state.v_level = v_level;
    for (index = 0U; index < ENC_M3_MAX_MI_COLUMNS; ++index) {
        state.partition_above[index] = 0U;
        state.coeff_above[index] = 0U;
        state.mode_above[index] = 0U;
    }
    for (index = 0U; index < 16U; ++index) {
        state.partition_left[index] = 0U;
        state.coeff_left[index] = 0U;
        state.mode_left[index] = 0U;
    }
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

#include "encoder/range.h"

#define ENC_RANGE_TOP 32768U
#define ENC_RANGE_PROB_SHIFT 7U

static unsigned int range_floor_log2(uint32_t value) {
    unsigned int result = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

static EncStatus range_output_byte(EncRangeWriter *writer, uint8_t value) {
    if (writer->position >= writer->capacity) {
        writer->status = ENC_OUT_OF_SPACE;
        return writer->status;
    }
    writer->data[writer->position++] = value;
    return ENC_OK;
}

static void range_carry_out(EncRangeWriter *writer, uint32_t value) {
    uint32_t carry;
    uint8_t fill;
    if (writer->status != ENC_OK) {
        return;
    }
    if (writer->symbols_out == SIZE_MAX) {
        writer->status = ENC_OVERFLOW;
        return;
    }
    ++writer->symbols_out;
    if (value == 0xffU) {
        if (writer->pending_ff == SIZE_MAX) {
            writer->status = ENC_OVERFLOW;
        } else {
            ++writer->pending_ff;
        }
        return;
    }
    carry = value >> 8U;
    if (writer->pending_byte >= 0 &&
        range_output_byte(writer,
                          (uint8_t)((uint32_t)writer->pending_byte + carry)) !=
            ENC_OK) {
        return;
    }
    fill = (uint8_t)(0xffU + carry);
    while (writer->pending_ff != 0U) {
        if (range_output_byte(writer, fill) != ENC_OK) {
            return;
        }
        --writer->pending_ff;
    }
    writer->pending_byte = (int)(value & 0xffU);
}

static void range_normalize(EncRangeWriter *writer, uint64_t low,
                            uint32_t range) {
    unsigned int shift;
    int count;
    int available;
    uint64_t mask;
    if (range == 0U) {
        writer->status = ENC_INVALID_ARGUMENT;
        return;
    }
    shift = 15U - range_floor_log2(range);
    count = writer->count;
    available = count + (int)shift;
    if (available >= 0) {
        count += 16;
        mask = (UINT64_C(1) << (unsigned int)count) - 1U;
        if (available >= 8) {
            range_carry_out(writer, (uint32_t)(low >> (unsigned int)count));
            low &= mask;
            count -= 8;
            mask >>= 8U;
        }
        range_carry_out(writer, (uint32_t)(low >> (unsigned int)count));
        available = count + (int)shift - 24;
        low &= mask;
    }
    writer->low = low << shift;
    writer->range = range << shift;
    writer->count = available;
}

static uint32_t range_probability_scale(uint16_t inverse_probability,
                                        uint32_t range, size_t symbol,
                                        size_t symbol_count) {
    uint32_t reduced_range = range >> 8U;
    uint32_t reduced_probability =
        ((uint32_t)inverse_probability >> ENC_RANGE_PROB_SHIFT) << 4U;
    uint32_t increment =
        (uint32_t)(16U * (symbol_count - symbol - 1U) / symbol_count);
    reduced_probability += increment;
    return ((reduced_range * reduced_probability) >> 7U) << 3U;
}

static EncStatus range_validate_cdf(EncRangeWriter *writer,
                                    const uint16_t *inverse_cdf,
                                    size_t symbol_count) {
    size_t index;
    if (inverse_cdf == 0 || symbol_count < 2U || symbol_count > 16U ||
        inverse_cdf[symbol_count - 1U] != 0U) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    for (index = 1U; index < symbol_count; ++index) {
        if (inverse_cdf[index - 1U] < inverse_cdf[index]) {
            writer->status = ENC_INVALID_ARGUMENT;
            return writer->status;
        }
    }
    return ENC_OK;
}

void enc_range_writer_init(EncRangeWriter *writer, void *data, size_t capacity) {
    if (writer == 0) {
        return;
    }
    writer->data = (uint8_t *)data;
    writer->capacity = capacity;
    writer->position = 0U;
    writer->symbols_out = 0U;
    writer->low = 0U;
    writer->range = ENC_RANGE_TOP;
    writer->count = -9;
    writer->pending_byte = -1;
    writer->pending_ff = 0U;
    writer->status =
        (data == 0 && capacity != 0U) ? ENC_INVALID_ARGUMENT : ENC_OK;
    writer->finished = 0;
}

EncStatus enc_range_writer_bypass(EncRangeWriter *writer, unsigned int bit) {
    uint32_t width;
    uint64_t low;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (writer->finished || bit > 1U) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    width = writer->range >> 1U;
    low = writer->low;
    if (bit != 0U) {
        low += writer->range - width;
    }
    range_normalize(writer, low, width);
    return writer->status;
}

EncStatus enc_range_writer_literal(EncRangeWriter *writer, uint32_t value,
                                   unsigned int bit_count) {
    unsigned int index;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (bit_count > 32U) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    if (writer->finished) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    for (index = 0U; index < bit_count; ++index) {
        EncStatus status = enc_range_writer_bypass(
            writer, (value >> (bit_count - index - 1U)) & 1U);
        if (status != ENC_OK) {
            return status;
        }
    }
    return ENC_OK;
}

EncStatus enc_range_cdf_update(uint16_t *inverse_cdf, size_t symbol_count,
                               uint32_t symbol) {
    size_t interval;
    unsigned int rate;
    uint32_t target = ENC_RANGE_TOP;
    size_t index;
    if (inverse_cdf == 0 || symbol_count < 2U || symbol_count > 16U ||
        symbol >= symbol_count ||
        inverse_cdf[symbol_count - 1U] != 0U ||
        inverse_cdf[symbol_count] > 32U) {
        return ENC_INVALID_ARGUMENT;
    }
    for (index = 1U; index < symbol_count; ++index) {
        if (inverse_cdf[index - 1U] < inverse_cdf[index]) {
            return ENC_INVALID_ARGUMENT;
        }
    }
    interval = inverse_cdf[symbol_count] > 31U ? 2U
        : inverse_cdf[symbol_count] > 15U ? 1U : 0U;
    rate = 2U + inverse_cdf[symbol_count + 1U + interval];
    if (rate > 15U) {
        return ENC_INVALID_ARGUMENT;
    }
    for (index = 0U; index + 1U < symbol_count; ++index) {
        if (index == symbol) {
            target = 0U;
        }
        if (target < inverse_cdf[index]) {
            inverse_cdf[index] = (uint16_t)(inverse_cdf[index] -
                ((inverse_cdf[index] - target) >> rate));
        } else {
            inverse_cdf[index] = (uint16_t)(inverse_cdf[index] +
                ((target - inverse_cdf[index]) >> rate));
        }
    }
    if (inverse_cdf[symbol_count] < 32U) {
        ++inverse_cdf[symbol_count];
    }
    return ENC_OK;
}

EncStatus enc_range_writer_symbol(EncRangeWriter *writer, uint32_t symbol,
                                  uint16_t *inverse_cdf,
                                  size_t symbol_count, int update_cdf) {
    uint32_t upper;
    uint32_t lower;
    uint64_t low;
    EncStatus status;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK) {
        return writer->status;
    }
    if (writer->finished || symbol >= symbol_count) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    status = range_validate_cdf(writer, inverse_cdf, symbol_count);
    if (status != ENC_OK) {
        return status;
    }
    if (update_cdf) {
        size_t interval = inverse_cdf[symbol_count] > 31U ? 2U
            : inverse_cdf[symbol_count] > 15U ? 1U : 0U;
        if (inverse_cdf[symbol_count] > 32U ||
            2U + inverse_cdf[symbol_count + 1U + interval] > 15U) {
            writer->status = ENC_INVALID_ARGUMENT;
            return writer->status;
        }
    }
    upper = symbol == 0U ? writer->range
        : range_probability_scale(inverse_cdf[symbol - 1U], writer->range,
                                  symbol - 1U, symbol_count);
    lower = range_probability_scale(inverse_cdf[symbol], writer->range,
                                    symbol, symbol_count);
    if (upper <= lower) {
        writer->status = ENC_INVALID_ARGUMENT;
        return writer->status;
    }
    low = writer->low + writer->range - upper;
    range_normalize(writer, low, upper - lower);
    if (writer->status == ENC_OK && update_cdf) {
        writer->status =
            enc_range_cdf_update(inverse_cdf, symbol_count, symbol);
    }
    return writer->status;
}

size_t enc_range_writer_tell(const EncRangeWriter *writer) {
    size_t base;
    int tail;
    if (writer == 0 || writer->symbols_out > (SIZE_MAX - 10U) / 8U) {
        return 0U;
    }
    if (writer->finished) {
        return writer->position <= SIZE_MAX / 8U
            ? writer->position * 8U : 0U;
    }
    base = writer->symbols_out * 8U;
    tail = writer->count + 10;
    if (tail < 0 || (size_t)tail > SIZE_MAX - base) {
        return 0U;
    }
    return base + (size_t)tail;
}

size_t enc_range_writer_bytes_used(const EncRangeWriter *writer) {
    return writer != 0 ? writer->position : 0U;
}

EncStatus enc_range_writer_finish(EncRangeWriter *writer) {
    uint64_t mask;
    uint64_t end;
    uint64_t low;
    uint32_t range;
    int count;
    int bits;
    uint64_t remainder_mask;
    size_t tell;
    size_t trailing_position;
    size_t trailing_byte;
    unsigned int trailing_offset;
    if (writer == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (writer->status != ENC_OK || writer->finished) {
        return writer->status;
    }
    tell = enc_range_writer_tell(writer);
    if (tell == 0U) {
        writer->status = ENC_OVERFLOW;
        return writer->status;
    }
    trailing_position = tell - 1U;
    trailing_byte = trailing_position >> 3U;
    trailing_offset = (unsigned int)(trailing_position & 7U);
    low = writer->low;
    range = writer->range;
    count = writer->count;
    bits = 9;
    mask = 0x7fffU;
    end = (low + mask) & ~mask;
    while ((end | mask) >= low + range) {
        ++bits;
        mask >>= 1U;
        end = (low + mask) & ~mask;
    }
    bits += count;
    if (bits > 0) {
        remainder_mask =
            (UINT64_C(1) << (unsigned int)(count + 16)) - 1U;
        do {
            range_carry_out(
                writer, (uint32_t)(end >> (unsigned int)(count + 16)));
            end &= remainder_mask;
            bits -= 8;
            count -= 8;
            remainder_mask >>= 8U;
        } while (bits > 0 && writer->status == ENC_OK);
    }
    if (writer->status == ENC_OK &&
        (writer->pending_byte >= 0 || writer->pending_ff != 0U)) {
        range_carry_out(writer, 0U);
    }
    if (writer->status == ENC_OK) {
        while (writer->position <= trailing_byte) {
            if (range_output_byte(writer, 0U) != ENC_OK) {
                return writer->status;
            }
        }
        writer->data[trailing_byte] &=
            (uint8_t)(0xffU << (7U - trailing_offset));
        writer->data[trailing_byte] |=
            (uint8_t)(1U << (7U - trailing_offset));
        writer->finished = 1;
    }
    return writer->status;
}

EncStatus enc_range_writer_status(const EncRangeWriter *writer) {
    return writer != 0 ? writer->status : ENC_INVALID_ARGUMENT;
}

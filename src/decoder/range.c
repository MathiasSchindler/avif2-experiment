#include "range.h"
#include "shared/bit_math.h"

#define AV2H_RANGE_TOP 32768U
#define AV2H_RANGE_PROB_SHIFT 7U

static int range_byte_at(const Av2hRangeDecoder *decoder,
                         size_t position,
                         uint8_t *value) {
    size_t index;
    for (index = 0U; index < decoder->span_count; ++index) {
        if (position < decoder->spans[index].size) {
            *value = decoder->spans[index].data[position];
            return 1;
        }
        position -= decoder->spans[index].size;
    }
    return 0;
}

static uint32_t range_input_bits(Av2hRangeDecoder *decoder,
                                 unsigned int bit_count) {
    uint32_t value = 0U;
    unsigned int index;
    for (index = 0U; index < bit_count; ++index) {
        size_t position = decoder->bit_position++;
        uint8_t byte = 0U;
        if (range_byte_at(decoder, position >> 3U, &byte)) {
            value |= (uint32_t)((byte >> (7U - (position & 7U))) & 1U)
                     << (bit_count - index - 1U);
        }
    }
    return value;
}

static void range_check_overflow(Av2hRangeDecoder *decoder) {
    if (decoder->status == AV2H_OK && decoder->tell > decoder->size * 8U) {
        decoder->status = AV2H_TRUNCATED;
    }
}

static void range_normalize(Av2hRangeDecoder *decoder) {
    unsigned int shift;
    unsigned int input_bits;
    uint32_t input;
    if (decoder->range == 0U) {
        decoder->status = AV2H_INVALID_DATA;
        return;
    }
    shift = 15U - av2h_floor_log2_u32(decoder->range);
    decoder->range <<= shift;
    input_bits = decoder->available_bits > 0
        ? (unsigned int)(decoder->available_bits < (int64_t)shift
            ? decoder->available_bits : (int64_t)shift)
        : 0U;
    input = range_input_bits(decoder, input_bits);
    decoder->value = (input << (shift - input_bits)) ^
                     (((decoder->value + 1U) << shift) - 1U);
    decoder->available_bits -= shift;
    decoder->tell += shift;
    range_check_overflow(decoder);
}

static uint32_t range_probability_scale(uint16_t inverse_probability,
                                        uint32_t range,
                                        size_t symbol,
                                        size_t symbol_count) {
    uint32_t reduced_range = range >> 8U;
    uint32_t reduced_probability =
        ((uint32_t)inverse_probability >> AV2H_RANGE_PROB_SHIFT) << 4U;
    uint32_t increment =
        (uint32_t)(16U * (symbol_count - symbol - 1U) / symbol_count);
    reduced_probability += increment;
    return ((reduced_range * reduced_probability) >> 7U) << 3U;
}

Av2hStatus av2h_range_init(Av2hRangeDecoder *decoder,
                            const Av2hSpan *spans,
                            size_t span_count) {
    size_t size = 0U;
    size_t index;
    unsigned int initial_bits;
    uint32_t initial_value;
    if (decoder == 0 || spans == 0 || span_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < span_count; ++index) {
        if (spans[index].data == 0 && spans[index].size != 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        if (!av2h_size_add(size, spans[index].size, &size)) {
            return AV2H_OVERFLOW;
        }
    }
    if (size > (size_t)(UINT64_MAX >> 1U) / 8U) return AV2H_OVERFLOW;
    decoder->spans = spans;
    decoder->span_count = span_count;
    decoder->size = size;
    decoder->bit_position = 0U;
    decoder->range = AV2H_RANGE_TOP;
    decoder->available_bits = (int64_t)(size * 8U) - 15;
    decoder->tell = 1U;
    decoder->status = AV2H_OK;
    decoder->diagnostics = 0;
    initial_bits = size * 8U < 15U ? (unsigned int)(size * 8U) : 15U;
    initial_value = range_input_bits(decoder, initial_bits);
    decoder->value = (AV2H_RANGE_TOP - 1U) ^
                     (initial_value << (15U - initial_bits));
    range_check_overflow(decoder);
    return decoder->status;
}

uint32_t av2h_range_read_symbol(Av2hRangeDecoder *decoder,
                                const uint16_t *inverse_cdf,
                                size_t symbol_count) {
    uint32_t upper;
    uint32_t lower;
    size_t symbol;
    size_t index;
    if (decoder == 0 || inverse_cdf == 0 ||
        symbol_count < 2U || symbol_count > 16U) {
        if (decoder != 0) decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    if (decoder->status != AV2H_OK) return 0U;
    if (inverse_cdf[symbol_count - 1U] != 0U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    for (index = 1U; index < symbol_count; ++index) {
        if (inverse_cdf[index - 1U] < inverse_cdf[index]) {
            decoder->status = AV2H_INVALID_ARGUMENT;
            return 0U;
        }
    }
    upper = decoder->range;
    symbol = 0U;
    for (;;) {
        lower = range_probability_scale(inverse_cdf[symbol], decoder->range,
                                        symbol, symbol_count);
        if (decoder->value >= lower) break;
        upper = lower;
        ++symbol;
        if (symbol >= symbol_count) {
            decoder->status = AV2H_INVALID_DATA;
            return 0U;
        }
    }
    decoder->range = upper - lower;
    decoder->value -= lower;
    range_normalize(decoder);
    return (uint32_t)symbol;
}

uint32_t av2h_range_read_adaptive(Av2hRangeDecoder *decoder,
                                  uint16_t *inverse_cdf,
                                  size_t symbol_count) {
    uint32_t symbol;
    size_t interval;
    unsigned int rate;
    uint32_t target = AV2H_RANGE_TOP;
    size_t index;
    symbol = av2h_range_read_symbol(decoder, inverse_cdf, symbol_count);
    if (decoder == 0 || decoder->status != AV2H_OK) return 0U;
    if (inverse_cdf[symbol_count] > 32U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    interval = inverse_cdf[symbol_count] > 31U ? 2U
        : inverse_cdf[symbol_count] > 15U ? 1U : 0U;
    rate = 2U + inverse_cdf[symbol_count + 1U + interval];
    if (rate > 15U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    for (index = 0U; index + 1U < symbol_count; ++index) {
        if (index == symbol) target = 0U;
        if (target < inverse_cdf[index]) {
            inverse_cdf[index] = (uint16_t)(inverse_cdf[index] -
                ((inverse_cdf[index] - target) >> rate));
        } else {
            inverse_cdf[index] = (uint16_t)(inverse_cdf[index] +
                ((target - inverse_cdf[index]) >> rate));
        }
    }
    if (inverse_cdf[symbol_count] < 32U) ++inverse_cdf[symbol_count];
    return symbol;
}

uint32_t av2h_range_read_bypass(Av2hRangeDecoder *decoder,
                                unsigned int bit_count) {
    uint32_t result = 0U;
    unsigned int index;
    if (decoder == 0 || bit_count > 32U) {
        if (decoder != 0) decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    if (decoder->status != AV2H_OK) return 0U;
    for (index = 0U; index < bit_count; ++index) {
        uint32_t split = decoder->range >> 1U;
        uint32_t input;
        result <<= 1U;
        if (decoder->value < split) {
            result |= 1U;
        } else {
            decoder->value -= split;
        }
        input = decoder->available_bits > 0
            ? range_input_bits(decoder, 1U) : 0U;
        decoder->value = input ^ ((decoder->value << 1U) | 1U);
        --decoder->available_bits;
        ++decoder->tell;
        range_check_overflow(decoder);
        if (decoder->status != AV2H_OK) return 0U;
    }
    return result;
}

size_t av2h_range_tell(const Av2hRangeDecoder *decoder) {
    return decoder != 0 ? decoder->tell : 0U;
}

Av2hStatus av2h_range_status(const Av2hRangeDecoder *decoder) {
    return decoder != 0 ? decoder->status : AV2H_INVALID_ARGUMENT;
}
#include "range.h"

#include "shared/bit_math.h"

#include <stddef.h>

#define AV2H_RANGE_TOP 32768U
#define AV2H_RANGE_PROB_SHIFT 7U

static int range_byte_at(const Av2hRangeDecoder *decoder, size_t position, uint8_t *value) {
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

static uint32_t range_spanning_bits(const Av2hRangeDecoder *decoder, size_t position,
                                    unsigned int offset, unsigned int bit_count) {
    unsigned char gathered[5U] = {0U};
    unsigned int byte_count = (offset + bit_count + 7U) >> 3U;
    unsigned int index;
    for (index = 0U; index < byte_count; ++index)
        (void)range_byte_at(decoder, position + index, &gathered[index]);
    return av2h_read_bits_msb(gathered, offset, bit_count);
}

static uint32_t range_input_bits(Av2hRangeDecoder *decoder, unsigned int bit_count) {
    uint32_t value;
    size_t position = decoder->bit_position >> 3U;
    unsigned int offset = (unsigned int)(decoder->bit_position & 7U);
    unsigned int byte_count = (offset + bit_count + 7U) >> 3U;
    if (bit_count == 0U)
        return 0U;
    if (decoder->span_count == 1U && position <= decoder->spans[0].size &&
        byte_count <= decoder->spans[0].size - position) {
        value = av2h_read_bits_msb(decoder->spans[0].data + position, offset, bit_count);
    } else {
        value = range_spanning_bits(decoder, position, offset, bit_count);
    }
    decoder->bit_position += bit_count;
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
    input_bits =
        decoder->available_bits > 0
            ? (unsigned int)(decoder->available_bits < (int64_t)shift ? decoder->available_bits
                                                                      : (int64_t)shift)
            : 0U;
    input = range_input_bits(decoder, input_bits);
    decoder->value = (input << (shift - input_bits)) ^ (((decoder->value + 1U) << shift) - 1U);
    decoder->available_bits -= shift;
    decoder->tell += shift;
    range_check_overflow(decoder);
}

static uint32_t range_probability_scale(uint16_t inverse_probability, uint32_t range, size_t symbol,
                                        size_t symbol_count) {
    uint32_t reduced_range = range >> 8U;
    uint32_t reduced_probability = ((uint32_t)inverse_probability >> AV2H_RANGE_PROB_SHIFT) << 4U;
    uint32_t increment = (uint32_t)(16U * (symbol_count - symbol - 1U) / symbol_count);
    reduced_probability += increment;
    return ((reduced_range * reduced_probability) >> 7U) << 3U;
}

Av2hStatus av2h_range_init(Av2hRangeDecoder *decoder, const Av2hSpan *spans, size_t span_count) {
    size_t size = 0U;
    size_t index;
    unsigned int initial_bits;
    uint32_t initial_value;
    if (decoder == NULL || spans == NULL || span_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (index = 0U; index < span_count; ++index) {
        if (spans[index].data == NULL && spans[index].size != 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        if (!av2h_size_add(size, spans[index].size, &size)) {
            return AV2H_OVERFLOW;
        }
    }
    if (size > (size_t)(UINT64_MAX >> 1U) / 8U)
        return AV2H_OVERFLOW;
    decoder->spans = spans;
    decoder->span_count = span_count;
    decoder->size = size;
    decoder->bit_position = 0U;
    decoder->range = AV2H_RANGE_TOP;
    decoder->available_bits = (int64_t)(size * 8U) - 15;
    decoder->tell = 1U;
    decoder->status = AV2H_OK;
    decoder->diagnostics = NULL;
    initial_bits = size * 8U < 15U ? (unsigned int)(size * 8U) : 15U;
    initial_value = range_input_bits(decoder, initial_bits);
    decoder->value = (AV2H_RANGE_TOP - 1U) ^ (initial_value << (15U - initial_bits));
    range_check_overflow(decoder);
    return decoder->status;
}

uint32_t av2h_range_read_symbol(Av2hRangeDecoder *decoder, const uint16_t *inverse_cdf,
                                size_t symbol_count) {
    uint32_t upper;
    uint32_t lower;
    uint16_t previous;
    size_t symbol;
    size_t index;
    if (decoder == NULL || inverse_cdf == NULL || symbol_count < 2U || symbol_count > 16U) {
        if (decoder != NULL)
            decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    if (decoder->status != AV2H_OK)
        return 0U;
    if (inverse_cdf[symbol_count - 1U] != 0U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    upper = decoder->range;
    symbol = 0U;
    if (symbol_count == 2U) {
        lower = range_probability_scale(inverse_cdf[0], decoder->range, 0U, 2U);
        if (decoder->value < lower) {
            symbol = 1U;
            upper = lower;
            lower = 0U;
        }
    } else {
        previous = inverse_cdf[0];
        for (;;) {
            uint16_t current;
            lower = range_probability_scale(previous, decoder->range, symbol, symbol_count);
            if (decoder->value >= lower)
                break;
            upper = lower;
            ++symbol;
            if (symbol >= symbol_count) {
                decoder->status = AV2H_INVALID_DATA;
                return 0U;
            }
            current = inverse_cdf[symbol];
            if (previous < current) {
                decoder->status = AV2H_INVALID_ARGUMENT;
                return 0U;
            }
            previous = current;
        }
        /* Validate the unsearched tail before committing any entropy state. */
        for (index = symbol + 1U; index < symbol_count; ++index) {
            uint16_t current = inverse_cdf[index];
            if (previous < current) {
                decoder->status = AV2H_INVALID_ARGUMENT;
                return 0U;
            }
            previous = current;
        }
    }
    decoder->range = upper - lower;
    decoder->value -= lower;
    range_normalize(decoder);
    return (uint32_t)symbol;
}

uint32_t av2h_range_read_adaptive(Av2hRangeDecoder *decoder, uint16_t *inverse_cdf,
                                  size_t symbol_count) {
    uint32_t symbol;
    size_t interval;
    unsigned int rate;
    uint32_t target = AV2H_RANGE_TOP;
    size_t index;
    symbol = av2h_range_read_symbol(decoder, inverse_cdf, symbol_count);
    if (decoder == NULL || decoder->status != AV2H_OK)
        return 0U;
    if (inverse_cdf[symbol_count] > 32U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    interval = inverse_cdf[symbol_count] > 31U ? 2U : inverse_cdf[symbol_count] > 15U ? 1U : 0U;
    rate = 2U + inverse_cdf[symbol_count + 1U + interval];
    if (rate > 15U) {
        decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    for (index = 0U; index + 1U < symbol_count; ++index) {
        if (index == symbol)
            target = 0U;
        if (target < inverse_cdf[index]) {
            inverse_cdf[index] =
                (uint16_t)(inverse_cdf[index] - ((inverse_cdf[index] - target) >> rate));
        } else {
            inverse_cdf[index] =
                (uint16_t)(inverse_cdf[index] + ((target - inverse_cdf[index]) >> rate));
        }
    }
    if (inverse_cdf[symbol_count] < 32U)
        ++inverse_cdf[symbol_count];
    return symbol;
}

uint32_t av2h_range_read_bypass(Av2hRangeDecoder *decoder, unsigned int bit_count) {
    uint32_t result = 0U;
    uint32_t input;
    uint32_t value;
    uint32_t split;
    unsigned int steps = bit_count;
    unsigned int input_bits;
    size_t total_bits;
    unsigned int index;
    if (decoder == NULL || bit_count > 32U) {
        if (decoder != NULL)
            decoder->status = AV2H_INVALID_ARGUMENT;
        return 0U;
    }
    if (decoder->status != AV2H_OK)
        return 0U;
    if (bit_count == 0U)
        return 0U;
    total_bits = decoder->size * 8U;
    /* Include exactly the first failing step to retain scalar truncation state. */
    if (decoder->tell > total_bits)
        steps = 1U;
    else if (steps > total_bits - decoder->tell)
        steps = (unsigned int)(total_bits - decoder->tell) + 1U;
    input_bits =
        decoder->available_bits > 0
            ? (unsigned int)(decoder->available_bits < (int64_t)steps ? decoder->available_bits
                                                                      : (int64_t)steps)
            : 0U;
    input = range_input_bits(decoder, input_bits);
    if (input_bits != 0U)
        input <<= 32U - input_bits;
    value = decoder->value;
    split = decoder->range >> 1U;
    for (index = 0U; index < steps; ++index) {
        result <<= 1U;
        if (value < split) {
            result |= 1U;
        } else {
            value -= split;
        }
        value = (input >> 31U) ^ ((value << 1U) | 1U);
        input <<= 1U;
    }
    decoder->value = value;
    decoder->available_bits -= steps;
    decoder->tell += steps;
    range_check_overflow(decoder);
    return decoder->status == AV2H_OK ? result : 0U;
}

size_t av2h_range_tell(const Av2hRangeDecoder *decoder) {
    return decoder != NULL ? decoder->tell : 0U;
}

Av2hStatus av2h_range_status(const Av2hRangeDecoder *decoder) {
    return decoder != NULL ? decoder->status : AV2H_INVALID_ARGUMENT;
}
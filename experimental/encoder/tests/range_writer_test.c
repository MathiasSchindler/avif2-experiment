#include "encoder/cdf.h"
#include "encoder/range.h"

#include <stdio.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_BUFFER_SIZE 8192U

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

typedef struct {
    const uint8_t *first;
    size_t first_size;
    const uint8_t *second;
    size_t second_size;
    size_t bit_position;
    uint32_t value;
    uint32_t range;
    int max_bits;
    int valid;
} ModelDecoder;

static unsigned int model_floor_log2(uint32_t value) {
    unsigned int result = 0U;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

static uint32_t model_probability_scale(uint16_t inverse_probability,
                                        uint32_t range, size_t symbol,
                                        size_t symbol_count) {
    uint32_t probability =
        ((uint32_t)inverse_probability >> 7U) << 4U;
    probability +=
        (uint32_t)(16U * (symbol_count - symbol - 1U) / symbol_count);
    return ((((range >> 8U) * probability) >> 7U) << 3U);
}

static unsigned int model_bit(const ModelDecoder *decoder,
                              size_t bit_position) {
    size_t byte_position = bit_position >> 3U;
    uint8_t byte;
    if (byte_position < decoder->first_size) {
        byte = decoder->first[byte_position];
    } else {
        byte_position -= decoder->first_size;
        if (byte_position >= decoder->second_size) {
            return 0U;
        }
        byte = decoder->second[byte_position];
    }
    return (byte >> (7U - (unsigned int)(bit_position & 7U))) & 1U;
}

static uint32_t model_read_bits(ModelDecoder *decoder, unsigned int count) {
    uint32_t value = 0U;
    unsigned int index;
    for (index = 0U; index < count; ++index) {
        value = (value << 1U) | model_bit(decoder, decoder->bit_position);
        ++decoder->bit_position;
    }
    return value;
}

static void model_decoder_init(ModelDecoder *decoder, const uint8_t *data,
                               size_t size, size_t split) {
    unsigned int initial_bits =
        (unsigned int)(size * 8U < 15U ? size * 8U : 15U);
    decoder->first = data;
    decoder->first_size = split;
    decoder->second = data + split;
    decoder->second_size = size - split;
    decoder->bit_position = 0U;
    decoder->value = 0U;
    decoder->range = 32768U;
    decoder->max_bits = (int)(size * 8U) - 15;
    decoder->valid = 1;
    decoder->value = model_read_bits(decoder, initial_bits);
    decoder->value <<= 15U - initial_bits;
    decoder->value ^= 32767U;
}

static unsigned int model_normalize(ModelDecoder *decoder, uint32_t value,
                                    uint32_t range) {
    unsigned int bits = 15U - model_floor_log2(range);
    unsigned int num_bits = 0U;
    uint32_t new_data;
    uint32_t mask;
    if (decoder->max_bits > 0) {
        num_bits = (unsigned int)decoder->max_bits;
        if (num_bits > bits) {
            num_bits = bits;
        }
    }
    new_data = model_read_bits(decoder, num_bits);
    mask = bits == 0U ? 0U : (UINT32_C(1) << bits) - 1U;
    decoder->value =
        (value << bits) | ((new_data << (bits - num_bits)) ^ mask);
    decoder->range = range << bits;
    decoder->max_bits -= (int)bits;
    return bits;
}

static uint32_t model_read_symbol(ModelDecoder *decoder,
                                  const uint16_t *inverse_cdf,
                                  size_t symbol_count,
                                  unsigned int *cost) {
    uint32_t current = decoder->range;
    uint32_t previous;
    uint32_t symbol = 0U;
    do {
        previous = current;
        current = model_probability_scale(inverse_cdf[symbol], decoder->range,
                                          symbol, symbol_count);
        if (decoder->value >= current) {
            break;
        }
        ++symbol;
    } while (symbol < symbol_count);
    if (symbol >= symbol_count || previous <= current) {
        decoder->valid = 0;
        *cost = 0U;
        return 0U;
    }
    *cost = model_normalize(decoder, decoder->value - current,
                            previous - current);
    return symbol;
}

static unsigned int model_symbol_cost(uint32_t range,
                                      const uint16_t *inverse_cdf,
                                      size_t symbol_count, uint32_t symbol) {
    uint32_t upper = symbol == 0U ? range
        : model_probability_scale(inverse_cdf[symbol - 1U], range,
                                  symbol - 1U, symbol_count);
    uint32_t lower = model_probability_scale(inverse_cdf[symbol], range,
                                             symbol, symbol_count);
    return 15U - model_floor_log2(upper - lower);
}

static unsigned int model_read_bypass(ModelDecoder *decoder) {
    uint32_t current = decoder->range >> 1U;
    unsigned int symbol = decoder->value < current;
    uint32_t value = decoder->value;
    if (symbol == 0U) {
        value -= current;
    }
    (void)model_normalize(decoder, value, current);
    return symbol;
}

static int model_exit_is_legal(const ModelDecoder *decoder) {
    long trailing_position;
    long padding_end;
    long lookback = decoder->max_bits + 15;
    size_t total_bits = (decoder->first_size + decoder->second_size) * 8U;
    size_t position;
    if (!decoder->valid || decoder->max_bits < -14) {
        return 0;
    }
    if (lookback > 15) {
        lookback = 15;
    }
    trailing_position = (long)decoder->bit_position - lookback;
    padding_end = (long)decoder->bit_position;
    if (decoder->max_bits > 0) {
        padding_end += decoder->max_bits;
    }
    if (trailing_position < 0 || padding_end != (long)total_bits ||
        model_bit(decoder, (size_t)trailing_position) != 1U) {
        return 0;
    }
    for (position = (size_t)trailing_position + 1U;
         position < (size_t)padding_end; ++position) {
        if (model_bit(decoder, position) != 0U) {
            return 0;
        }
    }
    return 1;
}

static void make_inverse_cdf(uint16_t *cdf, size_t symbol_count) {
    size_t index;
    for (index = 0U; index < symbol_count; ++index) {
        cdf[index] =
            (uint16_t)(((symbol_count - index - 1U) * 32768U) / symbol_count);
    }
}

static void copy_u16(uint16_t *destination, const uint16_t *source,
                     size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        destination[index] = source[index];
    }
}

static void model_update_cdf(uint16_t *cdf, size_t symbol_count,
                             uint32_t symbol) {
    size_t interval = cdf[symbol_count] > 31U ? 2U
        : cdf[symbol_count] > 15U ? 1U : 0U;
    unsigned int rate = 2U + cdf[symbol_count + 1U + interval];
    uint32_t target = 32768U;
    size_t index;
    for (index = 0U; index + 1U < symbol_count; ++index) {
        if (index == symbol) {
            target = 0U;
        }
        if (target < cdf[index]) {
            cdf[index] =
                (uint16_t)(cdf[index] - ((cdf[index] - target) >> rate));
        } else {
            cdf[index] =
                (uint16_t)(cdf[index] + ((target - cdf[index]) >> rate));
        }
    }
    if (cdf[symbol_count] < 32U) {
        ++cdf[symbol_count];
    }
}

static void check_decode_symbols(const uint8_t *data, size_t size,
                                 const uint32_t *symbols,
                                 size_t transcript_size,
                                 const uint16_t *initial_cdf,
                                 size_t symbol_count, int adaptive) {
    size_t split;
    for (split = 0U; split <= size; ++split) {
        uint16_t cdf[ENC_CDF_MAX_SIZE];
        ModelDecoder decoder;
        size_t index;
        copy_u16(cdf, initial_cdf, symbol_count + (adaptive ? 4U : 0U));
        model_decoder_init(&decoder, data, size, split);
        for (index = 0U; index < transcript_size; ++index) {
            unsigned int cost;
            CHECK(model_read_symbol(&decoder, cdf, symbol_count, &cost) ==
                  symbols[index]);
            if (adaptive) {
                model_update_cdf(cdf, symbol_count, symbols[index]);
            }
        }
        CHECK(model_exit_is_legal(&decoder));
    }
}

static void test_all_alphabets_and_symbols(void) {
    uint8_t data[TEST_BUFFER_SIZE];
    uint16_t cdf[ENC_CDF_MAX_SYMBOLS];
    uint32_t symbols[ENC_CDF_MAX_SYMBOLS * 2U];
    size_t symbol_count;
    for (symbol_count = 2U; symbol_count <= ENC_CDF_MAX_SYMBOLS;
         ++symbol_count) {
        EncRangeWriter writer;
        size_t index;
        size_t previous_tell = 1U;
        make_inverse_cdf(cdf, symbol_count);
        enc_range_writer_init(&writer, data, sizeof(data));
        for (index = 0U; index < symbol_count * 2U; ++index) {
            unsigned int cost;
            symbols[index] = index < symbol_count
                ? (uint32_t)index
                : (uint32_t)(symbol_count * 2U - index - 1U);
            cost = model_symbol_cost(writer.range, cdf, symbol_count,
                                     symbols[index]);
            CHECK(enc_range_writer_symbol(&writer, symbols[index], cdf,
                                          symbol_count, 0) == ENC_OK);
            CHECK(enc_range_writer_tell(&writer) == previous_tell + cost);
            CHECK(enc_range_writer_tell(&writer) >= previous_tell);
            previous_tell = enc_range_writer_tell(&writer);
        }
        CHECK(enc_range_writer_finish(&writer) == ENC_OK);
        CHECK(enc_range_writer_tell(&writer) ==
              enc_range_writer_bytes_used(&writer) * 8U);
        check_decode_symbols(data, enc_range_writer_bytes_used(&writer),
                             symbols, symbol_count * 2U, cdf, symbol_count, 0);
    }
}

static int parameter_adjustment(unsigned int digit) {
    static const int adjustments[5] = { 0, -1, -2, 1, 1 };
    return adjustments[digit];
}

static void test_materialization_and_adaptation(void) {
    static const uint8_t counts[] = { 0U, 15U, 16U, 31U, 32U };
    uint16_t thresholds[ENC_CDF_MAX_SYMBOLS];
    uint16_t actual[ENC_CDF_MAX_SIZE];
    uint16_t expected[ENC_CDF_MAX_SIZE];
    size_t symbol_count;
    for (symbol_count = 2U; symbol_count <= ENC_CDF_MAX_SYMBOLS;
         ++symbol_count) {
        unsigned int parameter;
        size_t index;
        for (index = 0U; index + 1U < symbol_count; ++index) {
            thresholds[index] =
                (uint16_t)(((index + 1U) * 32768U) / symbol_count);
        }
        for (parameter = 0U; parameter < 125U; ++parameter) {
            size_t count_index;
            unsigned int value = parameter;
            int offsets[3];
            offsets[0] = parameter_adjustment(value / 25U);
            offsets[1] = parameter_adjustment((value / 5U) % 5U);
            offsets[2] = parameter_adjustment(value % 5U);
            for (count_index = 0U; count_index < ARRAY_SIZE(counts);
                 ++count_index) {
                unsigned int base = symbol_count < 4U ? 2U : 3U;
                uint32_t symbol;
                CHECK(enc_cdf_materialize(actual, ARRAY_SIZE(actual),
                                          thresholds, symbol_count,
                                          (uint8_t)parameter,
                                          counts[count_index]) == ENC_OK);
                for (index = 0U; index + 1U < symbol_count; ++index) {
                    CHECK(actual[index] ==
                          (uint16_t)(32768U - thresholds[index]));
                }
                CHECK(actual[symbol_count - 1U] == 0U);
                CHECK(actual[symbol_count] == counts[count_index]);
                CHECK(actual[symbol_count + 1U] ==
                      (uint16_t)(base + offsets[0]));
                CHECK(actual[symbol_count + 2U] ==
                      (uint16_t)(base + 1U + offsets[1]));
                CHECK(actual[symbol_count + 3U] ==
                      (uint16_t)(base + 2U + offsets[2]));
                for (symbol = 0U; symbol < symbol_count; ++symbol) {
                    copy_u16(expected, actual, symbol_count + 4U);
                    model_update_cdf(expected, symbol_count, symbol);
                    CHECK(enc_range_cdf_update(actual, symbol_count, symbol) ==
                          ENC_OK);
                    for (index = 0U; index < symbol_count + 4U; ++index) {
                        CHECK(actual[index] == expected[index]);
                    }
                    CHECK(enc_cdf_materialize(actual, ARRAY_SIZE(actual),
                                              thresholds, symbol_count,
                                              (uint8_t)parameter,
                                              counts[count_index]) == ENC_OK);
                }
            }
        }
    }
}

static void test_adaptive_round_trip(void) {
    static const uint8_t counts[] = { 0U, 15U, 16U, 31U, 32U };
    uint8_t data[TEST_BUFFER_SIZE];
    uint16_t encoder_cdf[ENC_CDF_MAX_SIZE];
    uint16_t initial_cdf[ENC_CDF_MAX_SIZE];
    uint16_t expected_cdf[ENC_CDF_MAX_SIZE];
    uint32_t symbols[ENC_CDF_MAX_SYMBOLS * 2U];
    size_t symbol_count;
    for (symbol_count = 2U; symbol_count <= ENC_CDF_MAX_SYMBOLS;
         ++symbol_count) {
        size_t count_index;
        for (count_index = 0U; count_index < ARRAY_SIZE(counts);
             ++count_index) {
            EncRangeWriter writer;
            size_t index;
            make_inverse_cdf(initial_cdf, symbol_count);
            initial_cdf[symbol_count] = counts[count_index];
            initial_cdf[symbol_count + 1U] =
                (uint16_t)(symbol_count < 4U ? 2U : 3U);
            initial_cdf[symbol_count + 2U] =
                (uint16_t)(initial_cdf[symbol_count + 1U] + 1U);
            initial_cdf[symbol_count + 3U] =
                (uint16_t)(initial_cdf[symbol_count + 1U] + 2U);
            copy_u16(encoder_cdf, initial_cdf, symbol_count + 4U);
            copy_u16(expected_cdf, initial_cdf, symbol_count + 4U);
            enc_range_writer_init(&writer, data, sizeof(data));
            for (index = 0U; index < symbol_count * 2U; ++index) {
                symbols[index] = index < symbol_count
                    ? (uint32_t)index
                    : (uint32_t)(symbol_count * 2U - index - 1U);
                CHECK(enc_range_writer_symbol(
                          &writer, symbols[index], encoder_cdf,
                          symbol_count, 1) == ENC_OK);
                model_update_cdf(
                    expected_cdf, symbol_count, symbols[index]);
            }
            for (index = 0U; index < symbol_count + 4U; ++index) {
                CHECK(encoder_cdf[index] == expected_cdf[index]);
            }
            CHECK(enc_range_writer_finish(&writer) == ENC_OK);
            check_decode_symbols(
                data, enc_range_writer_bytes_used(&writer), symbols,
                symbol_count * 2U, initial_cdf, symbol_count, 1);
        }
    }
}

static uint32_t literal_pattern(unsigned int bit_count, unsigned int variant) {
    uint32_t mask = bit_count == 32U
        ? UINT32_MAX
        : bit_count == 0U ? 0U : (UINT32_C(1) << bit_count) - 1U;
    static const uint32_t patterns[] = {
        0U, UINT32_MAX, UINT32_C(0xaaaaaaaa), UINT32_C(0x13579bdf)
    };
    return patterns[variant] & mask;
}

static void test_bypass_and_literals(void) {
    uint8_t data[TEST_BUFFER_SIZE];
    unsigned int bit_count;
    for (bit_count = 0U; bit_count <= 32U; ++bit_count) {
        unsigned int variant;
        for (variant = 0U; variant < 4U; ++variant) {
            EncRangeWriter writer;
            ModelDecoder decoder;
            uint32_t value = literal_pattern(bit_count, variant);
            unsigned int index;
            size_t previous_tell = 1U;
            enc_range_writer_init(&writer, data, sizeof(data));
            CHECK(enc_range_writer_literal(&writer, value, bit_count) ==
                  ENC_OK);
            CHECK(enc_range_writer_tell(&writer) ==
                  previous_tell + bit_count);
            CHECK(enc_range_writer_finish(&writer) == ENC_OK);
            model_decoder_init(&decoder, data,
                               enc_range_writer_bytes_used(&writer),
                               enc_range_writer_bytes_used(&writer) / 2U);
            for (index = 0U; index < bit_count; ++index) {
                unsigned int expected =
                    (value >> (bit_count - index - 1U)) & 1U;
                CHECK(model_read_bypass(&decoder) == expected);
            }
            CHECK(model_exit_is_legal(&decoder));
        }
    }
}

static EncStatus encode_stress(uint8_t *data, size_t capacity,
                               size_t *bytes_used, int *saw_ff,
                               int *saw_carry) {
    uint16_t cdf[16];
    EncRangeWriter writer;
    uint32_t state = UINT32_C(0x31415926);
    size_t index;
    make_inverse_cdf(cdf, ARRAY_SIZE(cdf));
    enc_range_writer_init(&writer, data, capacity);
    for (index = 0U; index < 6000U && writer.status == ENC_OK; ++index) {
        size_t old_position = writer.position;
        int old_pending = writer.pending_byte;
        state = state * UINT32_C(1664525) + UINT32_C(1013904223);
        if ((state & 3U) == 0U) {
            (void)enc_range_writer_literal(&writer, state,
                                           (unsigned int)(state % 33U));
        } else {
            (void)enc_range_writer_symbol(&writer, state % 16U, cdf, 16U, 0);
        }
        if (writer.pending_ff != 0U) {
            *saw_ff = 1;
        }
        if (old_pending >= 0 && writer.position > old_position &&
            data != 0 && data[old_position] == (uint8_t)(old_pending + 1)) {
            *saw_carry = 1;
        }
    }
    if (writer.status == ENC_OK) {
        (void)enc_range_writer_finish(&writer);
    }
    *bytes_used = enc_range_writer_bytes_used(&writer);
    return enc_range_writer_status(&writer);
}

static void check_decode_stress(const uint8_t *data, size_t size) {
    size_t splits[5] = { 0U, 1U, size / 2U, size - 1U, size };
    size_t split_index;
    for (split_index = 0U; split_index < ARRAY_SIZE(splits); ++split_index) {
        uint16_t cdf[16];
        ModelDecoder decoder;
        uint32_t state = UINT32_C(0x31415926);
        size_t index;
        make_inverse_cdf(cdf, ARRAY_SIZE(cdf));
        model_decoder_init(&decoder, data, size, splits[split_index]);
        for (index = 0U; index < 6000U; ++index) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            if ((state & 3U) == 0U) {
                unsigned int bit_count = (unsigned int)(state % 33U);
                unsigned int bit_index;
                for (bit_index = 0U; bit_index < bit_count; ++bit_index) {
                    CHECK(model_read_bypass(&decoder) ==
                          ((state >> (bit_count - bit_index - 1U)) & 1U));
                }
            } else {
                unsigned int cost;
                CHECK(model_read_symbol(&decoder, cdf, ARRAY_SIZE(cdf),
                                        &cost) == state % 16U);
            }
        }
        CHECK(model_exit_is_legal(&decoder));
    }
}

static void test_capacity_carry_and_determinism(void) {
    uint8_t first[TEST_BUFFER_SIZE];
    uint8_t second[TEST_BUFFER_SIZE];
    size_t exact_size;
    size_t second_size;
    size_t short_size;
    int saw_ff = 0;
    int saw_carry = 0;
    int ignored_ff = 0;
    int ignored_carry = 0;
    size_t index;
    CHECK(encode_stress(first, sizeof(first), &exact_size, &saw_ff,
                        &saw_carry) == ENC_OK);
    CHECK(exact_size > 32U);
    CHECK(saw_ff);
    CHECK(saw_carry);
    check_decode_stress(first, exact_size);
    CHECK(encode_stress(second, exact_size, &second_size, &ignored_ff,
                        &ignored_carry) == ENC_OK);
    CHECK(second_size == exact_size);
    for (index = 0U; index < exact_size; ++index) {
        CHECK(first[index] == second[index]);
    }
    CHECK(encode_stress(second, exact_size - 1U, &short_size, &ignored_ff,
                        &ignored_carry) == ENC_OUT_OF_SPACE);
    CHECK(short_size == exact_size - 1U);
    CHECK(encode_stress(second, 1U, &short_size, &ignored_ff,
                        &ignored_carry) == ENC_OUT_OF_SPACE);
    CHECK(short_size == 1U);
    CHECK(encode_stress(0, 0U, &short_size, &ignored_ff,
                        &ignored_carry) == ENC_OUT_OF_SPACE);
    CHECK(short_size == 0U);
}

static void test_finish_and_failures(void) {
    uint8_t data[16] = { 0U };
    uint16_t cdf[] = { 16384U, 0U, 0U, 1U, 2U, 3U };
    uint16_t bad_order[] = { 8192U, 16384U, 0U };
    uint16_t bad_count[] = { 16384U, 0U, 33U, 0U, 0U, 0U };
    EncRangeWriter writer;
    ModelDecoder decoder;

    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
    CHECK(enc_range_writer_bytes_used(&writer) == 1U);
    CHECK(data[0] == 0x80U);
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
    model_decoder_init(&decoder, data, 1U, 0U);
    CHECK(model_exit_is_legal(&decoder));
    CHECK(enc_range_writer_bypass(&writer, 0U) == ENC_INVALID_ARGUMENT);

    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_literal(&writer, 0U, 33U) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, 0, 0U);
    CHECK(enc_range_writer_finish(&writer) == ENC_OUT_OF_SPACE);
    CHECK(enc_range_writer_literal(&writer, 0U, 33U) ==
          ENC_OUT_OF_SPACE);
    writer.status = ENC_OVERFLOW;
    CHECK(enc_range_writer_literal(&writer, 0U, 33U) == ENC_OVERFLOW);
    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_bypass(&writer, 2U) == ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_symbol(&writer, 2U, cdf, 2U, 0) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_symbol(&writer, 0U, bad_order, 3U, 0) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, data, sizeof(data));
    CHECK(enc_range_writer_symbol(&writer, 0U, bad_count, 2U, 1) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, 0, 1U);
    CHECK(enc_range_writer_status(&writer) == ENC_INVALID_ARGUMENT);
    CHECK(enc_range_writer_tell(0) == 0U);
    CHECK(enc_range_writer_bytes_used(0) == 0U);
}

int main(void) {
    test_all_alphabets_and_symbols();
    test_materialization_and_adaptation();
    test_adaptive_round_trip();
    test_bypass_and_literals();
    test_capacity_carry_and_determinism();
    test_finish_and_failures();
    if (failures != 0) {
        fprintf(stderr, "%d range writer checks failed\n", failures);
        return 1;
    }
    puts("range writer exhaustive unit: pass");
    return 0;
}

#include "decoder/range.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void decode_transcript(const Av2hSpan *spans, size_t span_count) {
    static const uint16_t cdf2[] = { 16384U, 0U };
    static const uint16_t cdf3[] = { 24576U, 8192U, 0U };
    static const uint16_t cdf5[] = {
        30000U, 22000U, 14000U, 6000U, 0U
    };
    static const size_t tells[] = { 4U, 9U, 10U, 12U, 13U, 15U, 25U, 26U };
    Av2hRangeDecoder decoder;
    CHECK(av2h_range_init(&decoder, spans, span_count) == AV2H_OK);
    CHECK(av2h_range_tell(&decoder) == 1U);
    CHECK(av2h_range_read_symbol(&decoder, cdf3, 3U) == 0U);
    CHECK(av2h_range_tell(&decoder) == tells[0]);
    CHECK(av2h_range_read_bypass(&decoder, 5U) == 21U);
    CHECK(av2h_range_tell(&decoder) == tells[1]);
    CHECK(av2h_range_read_symbol(&decoder, cdf2, 2U) == 1U);
    CHECK(av2h_range_tell(&decoder) == tells[2]);
    CHECK(av2h_range_read_symbol(&decoder, cdf5, 5U) == 4U);
    CHECK(av2h_range_tell(&decoder) == tells[3]);
    CHECK(av2h_range_read_bypass(&decoder, 1U) == 0U);
    CHECK(av2h_range_tell(&decoder) == tells[4]);
    CHECK(av2h_range_read_symbol(&decoder, cdf3, 3U) == 2U);
    CHECK(av2h_range_tell(&decoder) == tells[5]);
    CHECK(av2h_range_read_bypass(&decoder, 10U) == 0x2d3U);
    CHECK(av2h_range_tell(&decoder) == tells[6]);
    CHECK(av2h_range_read_symbol(&decoder, cdf2, 2U) == 0U);
    CHECK(av2h_range_tell(&decoder) == tells[7]);
    CHECK(av2h_range_status(&decoder) == AV2H_OK);
}

static void test_oracle_transcript(void) {
    static const unsigned char bytes[] = { 0x2bU, 0x79U, 0x82U, 0x40U };
    Av2hSpan contiguous = { bytes, sizeof(bytes) };
    size_t split;
    decode_transcript(&contiguous, 1U);
    for (split = 0U; split <= sizeof(bytes); ++split) {
        Av2hSpan spans[2] = {
            { bytes, split },
            { bytes + split, sizeof(bytes) - split }
        };
        decode_transcript(spans, 2U);
    }
}

static void test_adaptive_transcript(void) {
    static const unsigned char bytes[] = { 0x38U, 0x79U, 0x80U };
    static const uint32_t symbols[] = { 0U, 2U, 1U, 1U, 0U, 2U, 2U, 1U };
    static const uint16_t states[][4] = {
        { 18432U, 6144U, 0U, 1U },
        { 22016U, 12800U, 0U, 2U },
        { 24704U, 9600U, 0U, 3U },
        { 26720U, 7200U, 0U, 4U },
        { 20040U, 5400U, 0U, 5U },
        { 23222U, 12242U, 0U, 6U },
        { 25608U, 17373U, 0U, 7U },
        { 27398U, 13030U, 0U, 8U }
    };
    static const size_t tells[] = { 4U, 6U, 8U, 9U, 11U, 14U, 15U, 17U };
    uint16_t cdf[] = { 24576U, 8192U, 0U, 0U, 0U, 1U, 2U };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hRangeDecoder decoder;
    size_t index;
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    for (index = 0U; index < sizeof(symbols) / sizeof(symbols[0]); ++index) {
        CHECK(av2h_range_read_adaptive(&decoder, cdf, 3U) == symbols[index]);
        CHECK(cdf[0] == states[index][0]);
        CHECK(cdf[1] == states[index][1]);
        CHECK(cdf[2] == states[index][2]);
        CHECK(cdf[3] == states[index][3]);
        CHECK(av2h_range_tell(&decoder) == tells[index]);
    }
    CHECK(av2h_range_status(&decoder) == AV2H_OK);
}

static uint32_t next_random(uint32_t *state) {
    *state ^= *state << 13U;
    *state ^= *state >> 17U;
    *state ^= *state << 5U;
    return *state;
}

static uint64_t hash_value(uint64_t hash, uint64_t value) {
    unsigned int byte;
    for (byte = 0U; byte < 8U; ++byte) {
        hash ^= (unsigned char)(value >> (byte * 8U));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_state(uint64_t hash, const Av2hRangeDecoder *decoder) {
    hash = hash_value(hash, decoder->size);
    hash = hash_value(hash, decoder->bit_position);
    hash = hash_value(hash, decoder->range);
    hash = hash_value(hash, decoder->value);
    hash = hash_value(hash, (uint64_t)decoder->available_bits);
    hash = hash_value(hash, decoder->tell);
    return hash_value(hash, decoder->status);
}

static void check_state(const Av2hRangeDecoder *actual, const Av2hRangeDecoder *expected) {
    CHECK(actual->size == expected->size);
    CHECK(actual->bit_position == expected->bit_position);
    CHECK(actual->range == expected->range);
    CHECK(actual->value == expected->value);
    CHECK(actual->available_bits == expected->available_bits);
    CHECK(actual->tell == expected->tell);
    CHECK(actual->status == expected->status);
    CHECK(actual->diagnostics == expected->diagnostics);
}

static void test_refill_state_transcripts(void) {
    uint32_t random = UINT32_C(0xa3e17b29);
    uint64_t hash = UINT64_C(14695981039346656037);
    unsigned int trial;
    for (trial = 0U; trial < 520U; ++trial) {
        size_t size = trial / 8U;
        unsigned char *bytes = size != 0U ? malloc(size) : NULL;
        unsigned char scattered[128U];
        Av2hSpan byte_spans[66U] = {{NULL, 0U}};
        Av2hSpan split_spans[2U];
        Av2hSpan contiguous;
        Av2hRangeDecoder decoders[3U];
        size_t split = next_random(&random) % (size + 1U);
        size_t index;
        unsigned int step;
        CHECK(bytes != NULL || size == 0U);
        if (bytes == NULL && size != 0U)
            return;
        for (index = 0U; index < size; ++index) {
            uint32_t value = next_random(&random);
            bytes[index] = trial % 8U == 0U ? 0U :
                           trial % 8U == 1U ? 255U : (unsigned char)value;
            scattered[index * 2U] = bytes[index];
            scattered[index * 2U + 1U] = 0xa5U;
            byte_spans[index + 1U] = (Av2hSpan){scattered + index * 2U, 1U};
        }
        contiguous = (Av2hSpan){bytes, size};
        split_spans[0] = (Av2hSpan){bytes, split};
        split_spans[1] = (Av2hSpan){bytes != NULL ? bytes + split : NULL, size - split};
        (void)av2h_range_init(&decoders[0], &contiguous, 1U);
        (void)av2h_range_init(&decoders[1], split_spans, 2U);
        (void)av2h_range_init(&decoders[2], byte_spans, size + 2U);
        check_state(&decoders[1], &decoders[0]);
        check_state(&decoders[2], &decoders[0]);
        hash = hash_state(hash, &decoders[0]);
        for (step = 0U; step < 32U; ++step) {
            uint16_t cdfs[3U][20U];
            size_t count = 2U + next_random(&random) % 15U;
            unsigned int bits = next_random(&random) % 33U;
            uint32_t expected = 0U;
            unsigned int variant;
            for (index = 0U; index < count; ++index) {
                cdfs[0][index] = trial % 8U == 2U ? 0U :
                    (uint16_t)(32768U * (count - index - 1U) / count);
            }
            cdfs[0][count] = (uint16_t)(next_random(&random) % 33U);
            cdfs[0][count + 1U] = 0U;
            cdfs[0][count + 2U] = 1U;
            cdfs[0][count + 3U] = 2U;
            if (step >= 24U) {
                if (trial % 4U == 0U)
                    cdfs[0][count - 1U] = 1U;
                else if (trial % 4U == 1U) {
                    cdfs[0][count - 2U] = 0U;
                    cdfs[0][count - 1U] = 1U;
                } else if (trial % 4U == 2U)
                    cdfs[0][count] = 33U;
                else
                    bits = 33U;
            }
            for (variant = 1U; variant < 3U; ++variant)
                av2h_memory_copy(cdfs[variant], cdfs[0], (count + 4U) * sizeof(uint16_t));
            for (variant = 0U; variant < 3U; ++variant) {
                uint32_t result;
                if (step % 3U == 0U)
                    result = av2h_range_read_bypass(&decoders[variant], bits);
                else if (step % 3U == 1U)
                    result = av2h_range_read_symbol(&decoders[variant], cdfs[variant], count);
                else
                    result = av2h_range_read_adaptive(&decoders[variant], cdfs[variant], count);
                if (variant == 0U) {
                    expected = result;
                    hash = hash_value(hash, result);
                    hash = hash_state(hash, &decoders[variant]);
                    for (index = 0U; index < count + 4U; ++index)
                        hash = hash_value(hash, cdfs[variant][index]);
                } else {
                    CHECK(result == expected);
                    check_state(&decoders[variant], &decoders[0]);
                    CHECK(av2h_memory_compare(cdfs[variant], cdfs[0],
                                              (count + 4U) * sizeof(uint16_t)) == 0);
                }
            }
        }
        free(bytes);
    }
    printf("range refill: 49920 operations, %016" PRIx64 "\n", hash);
    CHECK(hash == UINT64_C(0x6b158c47ce691433));
}

static void test_malformed_cdf_state(void) {
    static const unsigned char bytes[8U] = {0U};
    Av2hSpan span = {bytes, sizeof(bytes)};
    size_t count;
    for (count = 2U; count <= 16U; ++count) {
        size_t violation;
        for (violation = 0U; violation + 1U < count; ++violation) {
            unsigned int value;
            for (value = 0U; value < 2U; ++value) {
                Av2hRangeDecoder decoder;
                Av2hRangeDecoder before;
                uint16_t cdf[16U];
                size_t index;
                CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
                decoder.value = value != 0U ? 0U : decoder.value;
                for (index = 0U; index < count; ++index)
                    cdf[index] = (uint16_t)(32768U * (count - index - 1U) / count);
                cdf[violation] = 0U;
                cdf[violation + 1U] = 1U;
                before = decoder;
                CHECK(av2h_range_read_symbol(&decoder, cdf, count) == 0U);
                CHECK(decoder.status == AV2H_INVALID_ARGUMENT);
                before.status = AV2H_INVALID_ARGUMENT;
                check_state(&decoder, &before);
            }
        }
    }
}

static void test_failures(void) {
    static const unsigned char bytes[] = { 0x2bU, 0x79U, 0x82U, 0x40U };
    static const uint16_t cdf2[] = { 16384U, 0U };
    static const uint16_t invalid_cdf[] = { 8192U, 16384U, 0U };
    size_t size;
    for (size = 0U; size < sizeof(bytes); ++size) {
        Av2hSpan span = { bytes, size };
        Av2hRangeDecoder decoder;
        Av2hStatus status = av2h_range_init(&decoder, &span, 1U);
        if (status == AV2H_OK) {
            (void)av2h_range_read_symbol(&decoder, cdf2, 2U);
            (void)av2h_range_read_bypass(&decoder, 32U);
        }
        CHECK(av2h_range_status(&decoder) == AV2H_TRUNCATED);
    }
    {
        Av2hSpan span = { bytes, sizeof(bytes) };
        Av2hRangeDecoder decoder;
        CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
        CHECK(av2h_range_read_symbol(&decoder, invalid_cdf, 3U) == 0U);
        CHECK(av2h_range_status(&decoder) == AV2H_INVALID_ARGUMENT);
    }
}

int main(void) {
    test_oracle_transcript();
    test_adaptive_transcript();
    test_refill_state_transcripts();
    test_malformed_cdf_state();
    test_failures();
    if (failures != 0) {
        fprintf(stderr, "%d range checks failed\n", failures);
        return 1;
    }
    puts("range unit: pass");
    return 0;
}
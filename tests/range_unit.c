#include "decoder/range.h"

#include <stdio.h>

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
    test_failures();
    if (failures != 0) {
        fprintf(stderr, "%d range checks failed\n", failures);
        return 1;
    }
    puts("range unit: pass");
    return 0;
}
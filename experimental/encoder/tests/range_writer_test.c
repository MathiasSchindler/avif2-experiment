#include "encoder/range.h"

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

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

static void test_mixed_transcript(void) {
    static uint16_t cdf2[] = { 16384U, 0U };
    static uint16_t cdf3[] = { 24576U, 8192U, 0U };
    static uint16_t cdf5[] = { 30000U, 22000U, 14000U, 6000U, 0U };
    static const uint8_t expected[] = { 0x2bU, 0x79U, 0x82U, 0x40U };
    static const size_t tells[] = { 4U, 9U, 10U, 12U, 13U, 15U, 25U, 26U };
    uint8_t storage[16] = { 0U };
    EncRangeWriter writer;

    enc_range_writer_init(&writer, storage, sizeof(storage));
    CHECK(enc_range_writer_tell(&writer) == 1U);
    CHECK(enc_range_writer_symbol(&writer, 0U, cdf3, 3U, 0) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[0]);
    CHECK(enc_range_writer_literal(&writer, 21U, 5U) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[1]);
    CHECK(enc_range_writer_symbol(&writer, 1U, cdf2, 2U, 0) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[2]);
    CHECK(enc_range_writer_symbol(&writer, 4U, cdf5, 5U, 0) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[3]);
    CHECK(enc_range_writer_bypass(&writer, 0U) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[4]);
    CHECK(enc_range_writer_symbol(&writer, 2U, cdf3, 3U, 0) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[5]);
    CHECK(enc_range_writer_literal(&writer, 0x2d3U, 10U) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[6]);
    CHECK(enc_range_writer_symbol(&writer, 0U, cdf2, 2U, 0) == ENC_OK);
    CHECK(enc_range_writer_tell(&writer) == tells[7]);
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
    CHECK(enc_range_writer_bytes_used(&writer) == sizeof(expected));
    CHECK(bytes_equal(storage, expected, sizeof(expected)));
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
}

static void test_adaptive_transcript(void) {
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
    static const uint8_t expected[] = { 0x38U, 0x79U, 0x80U };
    uint16_t cdf[] = { 24576U, 8192U, 0U, 0U, 0U, 1U, 2U };
    uint8_t storage[16] = { 0U };
    EncRangeWriter writer;
    size_t index;

    enc_range_writer_init(&writer, storage, sizeof(storage));
    for (index = 0U; index < sizeof(symbols) / sizeof(symbols[0]); ++index) {
        CHECK(enc_range_writer_symbol(&writer, symbols[index], cdf, 3U, 1) ==
              ENC_OK);
        CHECK(cdf[0] == states[index][0]);
        CHECK(cdf[1] == states[index][1]);
        CHECK(cdf[2] == states[index][2]);
        CHECK(cdf[3] == states[index][3]);
    }
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
    CHECK(enc_range_writer_bytes_used(&writer) == sizeof(expected));
    CHECK(bytes_equal(storage, expected, sizeof(expected)));
}

static void test_failures(void) {
    uint8_t storage[2] = { 0U };
    uint16_t cdf[] = { 16384U, 0U, 0U, 1U, 2U, 3U };
    uint16_t invalid[] = { 8192U, 16384U, 0U };
    EncRangeWriter writer;

    enc_range_writer_init(&writer, storage, sizeof(storage));
    CHECK(enc_range_writer_literal(&writer, 0U, 33U) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, storage, sizeof(storage));
    CHECK(enc_range_writer_symbol(&writer, 2U, cdf, 2U, 0) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, storage, sizeof(storage));
    CHECK(enc_range_writer_symbol(&writer, 0U, invalid, 3U, 0) ==
          ENC_INVALID_ARGUMENT);
    enc_range_writer_init(&writer, storage, 0U);
    CHECK(enc_range_writer_literal(&writer, 0xffffffffU, 32U) == ENC_OK);
    CHECK(enc_range_writer_finish(&writer) == ENC_OUT_OF_SPACE);
    enc_range_writer_init(&writer, 0, 1U);
    CHECK(enc_range_writer_status(&writer) == ENC_INVALID_ARGUMENT);
}

static void test_empty_flush(void) {
    static const uint8_t expected[] = { 0x80U };
    uint8_t storage[1] = { 0U };
    EncRangeWriter writer;

    enc_range_writer_init(&writer, storage, sizeof(storage));
    CHECK(enc_range_writer_finish(&writer) == ENC_OK);
    CHECK(enc_range_writer_bytes_used(&writer) == sizeof(expected));
    CHECK(bytes_equal(storage, expected, sizeof(expected)));
}

int main(void) {
    test_mixed_transcript();
    test_adaptive_transcript();
    test_failures();
    test_empty_flush();
    if (failures != 0) {
        fprintf(stderr, "%d range writer checks failed\n", failures);
        return 1;
    }
    puts("range writer unit: pass");
    return 0;
}

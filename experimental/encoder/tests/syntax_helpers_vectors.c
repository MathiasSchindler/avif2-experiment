#include "encoder/syntax.h"
#include "encoder/writer.h"

#include <stdio.h>

static int emit_hex(const char *name, const uint8_t *data, size_t size) {
    size_t index;
    if (printf("%s ", name) < 0) return 0;
    for (index = 0U; index < size; ++index) {
        if (printf("%02x", data[index]) < 0) return 0;
    }
    return printf("\n") >= 0;
}

static EncStatus encode_small_raw(
    uint8_t *data, size_t capacity, size_t *used) {
    EncBitWriter writer;
    enc_bit_writer_init(&writer, data, capacity);
    if (enc_bit_writer_uvlc(&writer, UINT32_C(0xfffffffe)) != ENC_OK ||
        enc_bit_writer_svlc(&writer, -32768) != ENC_OK ||
        enc_bit_writer_signed_literal(&writer, -8, 3U) != ENC_OK ||
        enc_bit_writer_ns(&writer, 4U, 5U) != ENC_OK ||
        enc_bit_writer_subexp(&writer, 63U, 64U, 3U) != ENC_OK ||
        enc_bit_writer_unsigned_subexp_with_ref(
            &writer, 0U, 64U, 63U, 3U) != ENC_OK ||
        enc_bit_writer_trailing_bits(&writer) != ENC_OK) {
        return writer.status;
    }
    if (used != 0) *used = enc_bit_writer_bytes_used(&writer);
    return ENC_OK;
}

static EncStatus encode_small_range(
    uint8_t *data, size_t capacity, size_t *used) {
    EncRangeWriter writer;
    enc_range_writer_init(&writer, data, capacity);
    if (enc_range_writer_signed_literal(&writer, -8, 3U) != ENC_OK ||
        enc_range_writer_ns(&writer, 32U, 33U) != ENC_OK ||
        enc_range_writer_coeff_remainder(
            &writer, UINT32_MAX, UINT32_MAX) != ENC_OK ||
        enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    if (used != 0) *used = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

static int raw_vectors(void) {
    static const uint32_t uvlc_values[] = {
        0U, 1U, 2U, 3U, 14U, 15U, 16U, 65534U,
        UINT32_C(0x7ffffffe), UINT32_C(0xfffffffe)
    };
    static const int32_t svlc_values[] = {
        INT32_C(-2147483647), -32768, -2, -1, 0, 1, 2, 32767,
        INT32_MAX
    };
    static uint8_t data[262144U];
    EncBitWriter writer;
    size_t index;
    uint32_t count;
    uint32_t value;
    enc_bit_writer_init(&writer, data, sizeof(data));
    for (index = 0U;
         index < sizeof(uvlc_values) / sizeof(uvlc_values[0]); ++index) {
        if (enc_bit_writer_uvlc(&writer, uvlc_values[index]) != ENC_OK) {
            return 0;
        }
    }
    for (index = 0U;
         index < sizeof(svlc_values) / sizeof(svlc_values[0]); ++index) {
        if (enc_bit_writer_svlc(&writer, svlc_values[index]) != ENC_OK) {
            return 0;
        }
    }
    for (value = 0U; value < 16U; ++value) {
        if (enc_bit_writer_signed_literal(
                &writer, (int32_t)value - 8, 3U) != ENC_OK) {
            return 0;
        }
    }
    for (count = 1U; count <= 65U; ++count) {
        for (value = 0U; value < count; ++value) {
            if (enc_bit_writer_ns(&writer, value, count) != ENC_OK) {
                return 0;
            }
        }
    }
    for (count = 1U; count <= 64U; ++count) {
        uint32_t reference;
        for (value = 0U; value < count; ++value) {
            if (enc_bit_writer_subexp(&writer, value, count, 3U) != ENC_OK) {
                return 0;
            }
        }
        for (reference = 0U; reference < count; ++reference) {
            for (value = 0U; value < count; ++value) {
                if (enc_bit_writer_unsigned_subexp_with_ref(
                        &writer, value, count, reference, 3U) != ENC_OK) {
                    return 0;
                }
            }
        }
    }
    for (value = 0U; value < 17U; ++value) {
        if (enc_bit_writer_signed_subexp_with_ref(
                &writer, (int32_t)value - 8, -8, 9, 3, 3U) != ENC_OK) {
            return 0;
        }
    }
    if (enc_bit_writer_trailing_bits(&writer) != ENC_OK) return 0;
    return emit_hex(
        "RAW", data, enc_bit_writer_bytes_used(&writer));
}

static int range_vectors(void) {
    static uint16_t eob_base[] = {26000U, 19000U, 12000U, 6000U, 0U};
    static uint16_t non_eob_base[] = {24000U, 15000U, 7000U, 0U};
    static uint16_t br[] = {24000U, 15000U, 7000U, 0U};
    static const uint32_t predictions[] = {0U, 1U, 3U, 4U, 15U, 64U};
    static const uint32_t remainders[] = {
        0U, 1U, 9U, 10U, 17U, 18U, 33U, 34U, 255U, 65535U,
        UINT32_MAX
    };
    uint8_t data[4096U];
    EncRangeWriter writer;
    size_t p;
    size_t r;
    uint32_t count;
    uint32_t value;
    enc_range_writer_init(&writer, data, sizeof(data));
    for (value = 0U; value < 16U; ++value) {
        if (enc_range_writer_signed_literal(
                &writer, (int32_t)value - 8, 3U) != ENC_OK) {
            return 0;
        }
    }
    for (count = 1U; count <= 33U; ++count) {
        for (value = 0U; value < count; ++value) {
            if (enc_range_writer_ns(&writer, value, count) != ENC_OK) {
                return 0;
            }
        }
    }
    for (p = 0U; p < sizeof(predictions) / sizeof(predictions[0]); ++p) {
        for (r = 0U; r < sizeof(remainders) / sizeof(remainders[0]); ++r) {
            if (enc_range_writer_coeff_remainder(
                    &writer, remainders[r], predictions[p]) != ENC_OK) {
                return 0;
            }
        }
    }
    for (value = 1U; value <= 20U; ++value) {
        if (enc_range_writer_coeff_value(
                &writer, value, 1, 4U, 3U, value / 2U,
                eob_base, br, 0, 0) != ENC_OK) {
            return 0;
        }
    }
    for (value = 0U; value <= 20U; ++value) {
        if (enc_range_writer_coeff_value(
                &writer, value, 0, 2U, 3U, value / 2U,
                non_eob_base, br, 0, 0) != ENC_OK) {
            return 0;
        }
    }
    if (enc_range_writer_finish(&writer) != ENC_OK) return 0;
    return emit_hex(
        "RANGE", data, enc_range_writer_bytes_used(&writer));
}

static int failure_vectors(void) {
    uint8_t bytes[128U];
    uint8_t byte = 0xa5U;
    EncBitWriter bits;
    EncRangeWriter range;
    uint16_t base[] = {16384U, 0U};
    uint16_t br[] = {16384U, 0U};
    size_t required;
    size_t capacity;
    size_t index;
    enc_bit_writer_init(&bits, &byte, 0U);
    if (enc_bit_writer_uvlc(&bits, 0U) != ENC_OUT_OF_SPACE ||
        enc_bit_writer_ns(&bits, 0U, 1U) != ENC_OUT_OF_SPACE ||
        byte != 0xa5U) return 0;
    enc_bit_writer_init(&bits, &byte, 1U);
    if (enc_bit_writer_uvlc(&bits, UINT32_MAX) != ENC_INVALID_ARGUMENT ||
        enc_bit_writer_ns(&bits, 0U, 1U) != ENC_INVALID_ARGUMENT ||
        enc_bit_writer_svlc(&bits, INT32_MIN) != ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_bit_writer_init(&bits, &byte, 1U);
    if (enc_bit_writer_subexp(&bits, 0U, 1U, 32U) !=
            ENC_INVALID_ARGUMENT ||
        enc_bit_writer_signed_literal(&bits, -9, 3U) !=
            ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, sizeof(byte));
    if (enc_range_writer_signed_literal(&range, 8, 3U) !=
            ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, sizeof(byte));
    if (enc_range_writer_ns(&range, 1U, 1U) != ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, 0U);
    if (enc_range_writer_finish(&range) != ENC_OUT_OF_SPACE ||
        enc_range_writer_ns(&range, 0U, 1U) != ENC_OUT_OF_SPACE) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, sizeof(byte));
    if (enc_range_writer_literal(&range, 0U, 33U) !=
            ENC_INVALID_ARGUMENT ||
        enc_range_writer_ns(&range, 0U, 1U) != ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, sizeof(byte));
    if (enc_range_writer_coeff_value(
            &range, 0U, 1, 1U, 1U, 0U, base, br, 0, 0) !=
            ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, sizeof(byte));
    if (enc_range_writer_coeff_value(
            &range, 1U, 1, UINT32_MAX, UINT32_MAX, 0U,
            base, br, 0, 0) != ENC_INVALID_ARGUMENT) {
        return 0;
    }
    enc_range_writer_init(&range, &byte, 0U);
    {
        EncStatus status = enc_range_writer_coeff_remainder(
            &range, UINT32_MAX, UINT32_MAX);
        if (status == ENC_OK) {
            status = enc_range_writer_finish(&range);
        }
        if (status != ENC_OUT_OF_SPACE) return 0;
    }
    if (encode_small_raw(bytes, sizeof(bytes), &required) != ENC_OK) return 0;
    for (capacity = 0U; capacity < required; ++capacity) {
        for (index = 0U; index < sizeof(bytes); ++index) bytes[index] = 0xa5U;
        if (encode_small_raw(bytes, capacity, 0) != ENC_OUT_OF_SPACE) return 0;
        for (index = capacity; index < sizeof(bytes); ++index) {
            if (bytes[index] != 0xa5U) return 0;
        }
    }
    if (encode_small_range(bytes, sizeof(bytes), &required) != ENC_OK) return 0;
    for (capacity = 0U; capacity < required; ++capacity) {
        for (index = 0U; index < sizeof(bytes); ++index) bytes[index] = 0xa5U;
        if (encode_small_range(bytes, capacity, 0) != ENC_OUT_OF_SPACE) {
            return 0;
        }
        for (index = capacity; index < sizeof(bytes); ++index) {
            if (bytes[index] != 0xa5U) return 0;
        }
    }
    return 1;
}

int main(void) {
    if (!failure_vectors() || !raw_vectors() || !range_vectors()) {
        return 1;
    }
    return 0;
}

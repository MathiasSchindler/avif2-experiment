#include "encoder/m4.h"
#include "encoder/syntax.h"

static int bytes_equal(
    const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static unsigned int magnitude(int16_t level) {
    return level < 0 ? (unsigned int)(-(int)level) : (unsigned int)level;
}

int main(void) {
    static const uint8_t rows[8U] = {
        128U, 128U, 128U, 129U, 129U, 129U, 130U, 130U
    };
    uint8_t input[64U];
    uint8_t reconstruction[64U];
    uint8_t first[ENC_M4_MAX_STREAM_SIZE];
    uint8_t second[ENC_M4_MAX_STREAM_SIZE];
    EncM4Result first_result;
    EncM4Result second_result;
    size_t y;
    size_t x;
    unsigned int value;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            input[y * 8U + x] = rows[y];
        }
    }
    if (enc_m4_ac_i400(
            input, 8U, first, sizeof(first), reconstruction,
            sizeof(reconstruction), &first_result) != ENC_OK ||
        first_result.dc_level != 3 ||
        first_result.ac_level != -2 ||
        first_result.squared_error != 0U ||
        !bytes_equal(input, reconstruction, sizeof(input))) {
        return 1;
    }
    if (enc_m4_ac_i400(
            input, 8U, second, sizeof(second), reconstruction,
            sizeof(reconstruction), &second_result) != ENC_OK ||
        first_result.stream_size != second_result.stream_size ||
        !bytes_equal(first, second, first_result.stream_size)) {
        return 2;
    }
    if (enc_m4_ac_i400(
            input, 8U, second, first_result.stream_size - 1U,
            reconstruction, sizeof(reconstruction), &second_result) !=
            ENC_OUT_OF_SPACE ||
        second_result.stream_size != 0U) {
        return 3;
    }
    if (enc_m4_ac_i400(
            input, 7U, second, sizeof(second), reconstruction,
            sizeof(reconstruction), &second_result) != ENC_INVALID_ARGUMENT ||
        enc_m4_ac_i400(
            input, 8U, second, sizeof(second), reconstruction,
            63U, &second_result) != ENC_INVALID_ARGUMENT ||
        enc_m4_ac_tile(
            second, sizeof(second), &x, 0, -2) != ENC_INVALID_ARGUMENT) {
        return 4;
    }
    for (value = 0U; value <= 255U; ++value) {
        for (x = 0U; x < sizeof(input); ++x) {
            input[x] = (uint8_t)value;
        }
        if (enc_m4_ac_i400(
                input, 8U, first, sizeof(first), reconstruction,
                sizeof(reconstruction), &first_result) != ENC_OK ||
            first_result.stream_size == 0U ||
            magnitude(first_result.dc_level) < ENC_M4_MIN_LEVEL_MAGNITUDE ||
            magnitude(first_result.dc_level) > ENC_M4_MAX_LEVEL_MAGNITUDE ||
            magnitude(first_result.ac_level) < ENC_M4_MIN_LEVEL_MAGNITUDE ||
            magnitude(first_result.ac_level) > ENC_M4_MAX_LEVEL_MAGNITUDE) {
            return 5;
        }
    }
    if (enc_m4_ac_tile(
            second, sizeof(second), &x,
            (int16_t)(ENC_M4_MAX_LEVEL_MAGNITUDE + 1U), -2) !=
            ENC_INVALID_ARGUMENT ||
        enc_m4_ac_tile(
            second, sizeof(second), &x, 2,
            (int16_t)-(int)(ENC_M4_MAX_LEVEL_MAGNITUDE + 1U)) !=
            ENC_INVALID_ARGUMENT) {
        return 6;
    }
    return 0;
}

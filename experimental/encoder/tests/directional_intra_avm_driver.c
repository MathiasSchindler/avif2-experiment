#include <stddef.h>
#include <stdint.h>

void avm_v_predictor_8x8_c(
    uint8_t *destination, ptrdiff_t stride,
    const uint8_t *above, const uint8_t *left);
void avm_h_predictor_8x8_c(
    uint8_t *destination, ptrdiff_t stride,
    const uint8_t *above, const uint8_t *left);
void avm_v_predictor_16x16_c(
    uint8_t *destination, ptrdiff_t stride,
    const uint8_t *above, const uint8_t *left);
void avm_h_predictor_16x16_c(
    uint8_t *destination, ptrdiff_t stride,
    const uint8_t *above, const uint8_t *left);

static int check_size(size_t size) {
    uint8_t above[16U];
    uint8_t left[16U];
    uint8_t output[16U * 16U];
    size_t row;
    size_t column;
    for (column = 0U; column < size; ++column) {
        above[column] = (uint8_t)(17U + column);
    }
    for (row = 0U; row < size; ++row) {
        left[row] = (uint8_t)(91U + row);
    }
    if (size == 8U) {
        avm_v_predictor_8x8_c(output, 16, above, left);
    } else {
        avm_v_predictor_16x16_c(output, 16, above, left);
    }
    for (row = 0U; row < size; ++row) {
        for (column = 0U; column < size; ++column) {
            if (output[row * 16U + column] != above[column]) return 1;
        }
    }
    if (size == 8U) {
        avm_h_predictor_8x8_c(output, 16, above, left);
    } else {
        avm_h_predictor_16x16_c(output, 16, above, left);
    }
    for (row = 0U; row < size; ++row) {
        for (column = 0U; column < size; ++column) {
            if (output[row * 16U + column] != left[row]) return 2;
        }
    }
    return 0;
}

int main(void) {
    int result = check_size(8U);
    return result != 0 ? result : check_size(16U);
}

#include "decoder/reconstruct.h"

static int log2_size(int value) {
    int result = 0;
    while ((1 << result) < value) ++result;
    return result;
}

static int round_shift(int value, int bits) {
    return (value + (1 << (bits - 1))) >> bits;
}

static uint8_t expected(
    uint8_t intra_mode, int row, int column, int width, int height,
    const uint8_t *above, const uint8_t *left) {
    int top = above[column];
    int side = left[row];
    if (intra_mode == AV2H_INTRA_PAETH) {
        int corner = above[-1];
        int base = top + side - corner;
        int ds = base > side ? base - side : side - base;
        int dt = base > top ? base - top : top - base;
        int dc = base > corner ? base - corner : corner - base;
        return (uint8_t)(ds <= dt && ds <= dc ? side :
                         dt <= dc ? top : corner);
    } else {
        int scale = round_shift(log2_size(width) + log2_size(height) - 4, 2);
        int sw = 32 >> (((column << 1) >> scale) < 6 ?
                        ((column << 1) >> scale) : 6);
        int sh = 32 >> (((row << 1) >> scale) < 6 ?
                        ((row << 1) >> scale) : 6);
        int horizontal = above[width] +
            round_shift((side - above[width]) * (width - 1 - column),
                        log2_size(width));
        int vertical = left[height] +
            round_shift((top - left[height]) * (height - 1 - row),
                        log2_size(height));
        horizontal += round_shift((side - horizontal) * sw, 6);
        vertical += round_shift((top - vertical) * sh, 6);
        return (uint8_t)(
            intra_mode == AV2H_INTRA_SMOOTH_VERTICAL ? vertical :
            intra_mode == AV2H_INTRA_SMOOTH_HORIZONTAL ? horizontal :
            round_shift(vertical + horizontal, 1));
    }
}

static int check(uint8_t intra_mode, uint16_t width, uint16_t height) {
    uint8_t above_storage[18U];
    uint8_t left[17U];
    uint8_t output[16U * 16U];
    Av2hFirstLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    above[-1] = 53U;
    for (column = 0U; column <= width; ++column) {
        above[column] = (uint8_t)(17U + column);
    }
    for (row = 0U; row <= height; ++row) {
        left[row] = (uint8_t)(91U + row);
    }
    references.above = above;
    references.above_count = (size_t)width + 1U;
    references.left = left;
    references.left_count = (size_t)height + 1U;
    references.above_left = 53U;
    references.has_above_left = 1U;
    mode.mode = intra_mode;
    if (av2h_luma_predict(
            &mode, width, height, &references, output, 16U) != AV2H_OK) {
        return 1;
    }
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            if (output[(size_t)row * 16U + column] != expected(
                    intra_mode, row, column, width, height, above, left)) {
                return 2;
            }
        }
    }
    return 0;
}

#define CHECK_SIZE(w, h) do { \
    if (check(AV2H_INTRA_SMOOTH, w, h)) return 1; \
    if (check(AV2H_INTRA_SMOOTH_VERTICAL, w, h)) return 2; \
    if (check(AV2H_INTRA_SMOOTH_HORIZONTAL, w, h)) return 3; \
    if (check(AV2H_INTRA_PAETH, w, h)) return 4; \
} while (0)

int main(void) {
    CHECK_SIZE(4U, 4U);
    CHECK_SIZE(4U, 8U);
    CHECK_SIZE(8U, 4U);
    CHECK_SIZE(8U, 8U);
    CHECK_SIZE(8U, 16U);
    CHECK_SIZE(16U, 8U);
    CHECK_SIZE(16U, 16U);
    return 0;
}

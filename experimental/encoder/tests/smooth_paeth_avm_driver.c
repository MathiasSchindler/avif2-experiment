#include <stddef.h>
#include <stdint.h>

typedef void (*Predictor)(uint16_t *, ptrdiff_t, const uint16_t *,
                          const uint16_t *, int);

#define DECLARE_MODE(name, w, h) \
    void avm_highbd_##name##_predictor_##w##x##h##_c( \
        uint16_t *, ptrdiff_t, const uint16_t *, const uint16_t *, int);
#define DECLARE_SIZE(w, h) \
    DECLARE_MODE(smooth, w, h) \
    DECLARE_MODE(smooth_v, w, h) \
    DECLARE_MODE(smooth_h, w, h) \
    DECLARE_MODE(paeth, w, h)
DECLARE_SIZE(4, 4)
DECLARE_SIZE(4, 8)
DECLARE_SIZE(8, 4)
DECLARE_SIZE(8, 8)
DECLARE_SIZE(8, 16)
DECLARE_SIZE(16, 8)
DECLARE_SIZE(16, 16)

static int log2_size(int value) {
    int result = 0;
    while ((1 << result) < value) ++result;
    return result;
}

static int round_shift(int value, int bits) {
    return (value + (1 << (bits - 1))) >> bits;
}

static uint16_t expected(int mode, int row, int column, int width, int height,
                         const uint16_t *above, const uint16_t *left) {
    int top = above[column];
    int side = left[row];
    if (mode == 3) {
        int corner = above[-1];
        int base = top + side - corner;
        int ds = base > side ? base - side : side - base;
        int dt = base > top ? base - top : top - base;
        int dc = base > corner ? base - corner : corner - base;
        return (uint16_t)(ds <= dt && ds <= dc ? side :
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
        return (uint16_t)(mode == 1 ? vertical : mode == 2 ? horizontal :
                          round_shift(vertical + horizontal, 1));
    }
}

static int check(Predictor predictor, int mode, int width, int height) {
    uint16_t above_storage[18];
    uint16_t left[17];
    uint16_t output[16 * 16];
    uint16_t *above = &above_storage[1];
    int row;
    int column;
    above[-1] = 53;
    for (column = 0; column <= width; ++column) above[column] = 17 + column;
    for (row = 0; row <= height; ++row) left[row] = 91 + row;
    predictor(output, 16, above, left, 8);
    for (row = 0; row < height; ++row) {
        for (column = 0; column < width; ++column) {
            if (output[row * 16 + column] !=
                expected(mode, row, column, width, height, above, left)) {
                return 1;
            }
        }
    }
    return 0;
}

#define CHECK_SIZE(w, h) do { \
    if (check(avm_highbd_smooth_predictor_##w##x##h##_c, 0, w, h)) return 1; \
    if (check(avm_highbd_smooth_v_predictor_##w##x##h##_c, 1, w, h)) return 2; \
    if (check(avm_highbd_smooth_h_predictor_##w##x##h##_c, 2, w, h)) return 3; \
    if (check(avm_highbd_paeth_predictor_##w##x##h##_c, 3, w, h)) return 4; \
} while (0)

int main(void) {
    CHECK_SIZE(4, 4);
    CHECK_SIZE(4, 8);
    CHECK_SIZE(8, 4);
    CHECK_SIZE(8, 8);
    CHECK_SIZE(8, 16);
    CHECK_SIZE(16, 8);
    CHECK_SIZE(16, 16);
    return 0;
}

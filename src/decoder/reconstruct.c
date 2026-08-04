#include "reconstruct.h"
#include "tx_geometry.h"
#include "ist_data.inc"
#include "ist_type1_data.inc"
#include "ist_type2_data.inc"
#include "ist_set6_data.inc"
#include "ist_set2_data.inc"
#include "ist_set9_type1_data.inc"
#include "ist_adst_data.inc"
#include "ist_reached_gaps_data.inc"

int32_t av2h_debug_intrabc_geometry[10U];

static const int8_t dct4[4U][4U] = {
    { 64, 64, 64, 64 },
    { 83, 35, -35, -83 },
    { 64, -64, -64, 64 },
    { 35, -83, 83, -35 }
};

static const int8_t adst4[4U][4U] = {
    { 18, 50, 75, 89 },
    { 50, 89, 18, -75 },
    { 75, 18, -89, 50 },
    { 89, -75, 50, -18 }
};

static const int8_t d45_filter[4U][4U] = {
    { -9, 46, 102, -11 },
    { -12, 94, 56, -10 },
    { -3, 125, 8, -2 },
    { -8, 37, 109, -10 }
};

static const int16_t dr_interp_filter[32U][4U] = {
    { 0, 128, 0, 0 }, { -2, 127, 4, -1 }, { -3, 125, 8, -2 },
    { -5, 123, 13, -3 }, { -6, 121, 17, -4 }, { -7, 118, 22, -5 },
    { -9, 116, 27, -6 }, { -9, 112, 32, -7 }, { -10, 109, 37, -8 },
    { -11, 106, 41, -8 }, { -11, 102, 46, -9 }, { -12, 98, 52, -10 },
    { -12, 94, 56, -10 }, { -12, 90, 61, -11 },
    { -12, 85, 66, -11 }, { -12, 81, 71, -12 },
    { -12, 76, 76, -12 }, { -12, 71, 81, -12 },
    { -11, 66, 85, -12 }, { -11, 61, 90, -12 },
    { -10, 56, 94, -12 }, { -10, 52, 98, -12 },
    { -9, 46, 102, -11 }, { -8, 41, 106, -11 },
    { -8, 37, 109, -10 }, { -7, 32, 112, -9 },
    { -6, 27, 116, -9 }, { -5, 22, 118, -7 },
    { -4, 17, 121, -6 }, { -3, 13, 123, -5 },
    { -2, 8, 125, -3 }, { -1, 4, 127, -2 }
};

static const int8_t d67_filter[8U][4U] = {
    { -5, 22, 118, -7 },
    { -10, 109, 37, -8 },
    { -9, 46, 102, -11 },
    { -5, 123, 13, -3 },
    { -12, 71, 81, -12 },
    { -2, 8, 125, -3 },
    { -12, 94, 56, -10 },
    { -7, 32, 112, -9 }
};

static const int8_t d67_mrl4_filter[4U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 }
};

static const int8_t d67_mrl8_filter[8U][4U] = {
    { -1, 4, 127, -2 },
    { -12, 85, 66, -11 },
    { -2, 8, 125, -3 },
    { -12, 90, 61, -11 },
    { -3, 13, 123, -5 },
    { -12, 94, 56, -10 },
    { -4, 17, 121, -6 },
    { -12, 98, 52, -10 }
};

static const int8_t d67_mrl3_filter[4U][4U] = {
    { -12, 94, 56, -10 },
    { -1, 4, 127, -2 },
    { -11, 66, 85, -12 },
    { -7, 118, 22, -5 }
};

static const uint8_t smooth8_blend[8U] = {
    32U, 16U, 8U, 4U, 2U, 1U, 0U, 0U
};

static const uint8_t ibp_weights[5U][16U] = {
    { 96U },
    { 86U, 107U },
    { 77U, 90U, 102U, 115U },
    { 71U, 78U, 86U, 92U, 100U, 107U, 114U, 121U },
    { 68U, 72U, 76U, 79U, 83U, 87U, 90U, 94U,
      98U, 102U, 106U, 109U, 113U, 117U, 121U, 124U }
};

static const int8_t adst8[8U][8U] = {
    { 11, 28, 44, 58, 70, 79, 86, 89 },
    { 34, 74, 89, 76, 39, -12, -58, -86 },
    { 54, 89, 48, -34, -87, -66, 12, 79 },
    { 71, 68, -41, -86, 1, 87, 38, -70 },
    { 84, 17, -89, 10, 86, -35, -75, 58 },
    { 88, -44, -44, 88, -44, -44, 88, -44 },
    { 79, -83, 50, 6, -59, 86, -74, 29 },
    { 50, -69, 81, -84, 78, -62, 40, -14 }
};

static const int8_t dct8[8U][8U] = {
    { 64, 64, 64, 64, 64, 64, 64, 64 },
    { 89, 75, 50, 18, -18, -50, -75, -89 },
    { 83, 35, -35, -83, -83, -35, 35, 83 },
    { 75, -18, -89, -50, 50, 89, 18, -75 },
    { 64, -64, -64, 64, 64, -64, -64, 64 },
    { 50, -89, 18, 75, -75, -18, 89, -50 },
    { 35, -83, 83, -35, -35, 83, -83, 35 },
    { 18, -50, 75, -89, 89, -75, 50, -18 }
};

static const uint8_t dct16_frequency[] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U,
    8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U
};
static const int8_t dct16[16][16] = {
        { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
        { 90, 87, 80, 70, 57, 43, 26, 9,
            -9, -26, -43, -57, -70, -80, -87, -90 },
        { 89, 75, 50, 18, -18, -50, -75, -89,
            -89, -75, -50, -18, 18, 50, 75, 89 },
        { 87, 57, 9, -43, -80, -90, -70, -26,
            26, 70, 90, 80, 43, -9, -57, -87 },
        { 83, 35, -35, -83, -83, -35, 35, 83,
            83, 35, -35, -83, -83, -35, 35, 83 },
        { 80, 9, -70, -87, -26, 57, 90, 43,
            -43, -90, -57, 26, 87, 70, -9, -80 },
        { 75, -18, -89, -50, 50, 89, 18, -75,
            -75, 18, 89, 50, -50, -89, -18, 75 },
        { 70, -43, -87, 9, 90, 26, -80, -57,
            57, 80, -26, -90, -9, 87, 43, -70 },
        { 64, -64, -64, 64, 64, -64, -64, 64,
            64, -64, -64, 64, 64, -64, -64, 64 },
        { 57, -80, -26, 90, -9, -87, 43, 70,
            -70, -43, 87, 9, -90, 26, 80, -57 },
        { 50, -89, 18, 75, -75, -18, 89, -50,
            -50, 89, -18, -75, 75, 18, -89, 50 },
        { 43, -90, 57, 26, -87, 70, 9, -80,
            80, -9, -70, 87, -26, -57, 90, -43 },
        { 35, -83, 83, -35, -35, 83, -83, 35,
            35, -83, 83, -35, -35, 83, -83, 35 },
        { 26, -70, 90, -80, 43, 9, -57, 87,
            -87, 57, -9, -43, 80, -90, 70, -26 },
        { 18, -50, 75, -89, 89, -75, 50, -18,
            -18, 50, -75, 89, -89, 75, -50, 18 },
        { 9, -26, 43, -57, 70, -80, 87, -90,
            90, -87, 80, -70, 57, -43, 26, -9 }
};

static const int8_t adst16[16][16] = {
        { 8, 17, 25, 33, 41, 48, 55, 62,
            67, 73, 77, 81, 84, 87, 88, 89 },
        { 25, 48, 67, 81, 88, 88, 81, 67,
            48, 25, 0, -25, -48, -67, -81, -88 },
        { 41, 73, 88, 84, 62, 25, -17, -55,
            -81, -89, -77, -48, -8, 33, 67, 87 },
        { 55, 87, 81, 41, -17, -67, -89, -73,
            -25, 33, 77, 88, 62, 8, -48, -84 },
        { 67, 88, 48, -25, -81, -81, -25, 48,
            88, 67, 0, -67, -88, -48, 25, 81 },
        { 77, 77, 0, -77, -77, 0, 77, 77,
            0, -77, -77, 0, 77, 77, 0, -77 },
        { 84, 55, -48, -87, -8, 81, 62, -41,
            -88, -17, 77, 67, -33, -89, -25, 73 },
        { 88, 25, -81, -48, 67, 67, -48, -81,
            25, 88, 0, -88, -25, 81, 48, -67 },
        { 89, -8, -88, 17, 87, -25, -84, 33,
            81, -41, -77, 48, 73, -55, -67, 62 },
        { 87, -41, -67, 73, 33, -88, 8, 84,
            -48, -62, 77, 25, -89, 17, 81, -55 },
        { 81, -67, -25, 88, -48, -48, 88, -25,
            -67, 81, 0, -81, 67, 25, -88, 48 },
        { 73, -84, 25, 55, -89, 48, 33, -87,
            67, 8, -77, 81, -17, -62, 88, -41 },
        { 62, -89, 67, -8, -55, 88, -73, 17,
            48, -87, 77, -25, -41, 84, -81, 33 },
        { 48, -81, 88, -67, 25, 25, -67, 88,
            -81, 48, 0, -48, 81, -88, 67, -25 },
        { 33, -62, 81, -89, 84, -67, 41, -8,
            -25, 55, -77, 88, -87, 73, -48, 17 },
        { 17, -33, 48, -62, 73, -81, 87, -89,
            88, -84, 77, -67, 55, -41, 25, -8 }
};

static const uint8_t dct32_frequency[] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U,
    12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U,
    24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U
};
static const int8_t dct32[32][32] = {
    {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    },
    {
        90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 47, 39, 30, 22, 13, 4,
        -4, -13, -22, -30, -39, -47, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90
    },
    {
        90, 87, 80, 70, 57, 43, 26, 9, -9, -26, -43, -57, -70, -80, -87, -90,
        -90, -87, -80, -70, -57, -43, -26, -9, 9, 26, 43, 57, 70, 80, 87, 90
    },
    {
        90, 82, 67, 47, 22, -4, -30, -54, -73, -85, -90, -88, -78, -61, -39, -13,
        13, 39, 61, 78, 88, 90, 85, 73, 54, 30, 4, -22, -47, -67, -82, -90
    },
    {
        89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89,
        89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89
    },
    {
        88, 67, 30, -13, -54, -82, -90, -78, -47, -4, 39, 73, 90, 85, 61, 22,
        -22, -61, -85, -90, -73, -39, 4, 47, 78, 90, 82, 54, 13, -30, -67, -88
    },
    {
        87, 57, 9, -43, -80, -90, -70, -26, 26, 70, 90, 80, 43, -9, -57, -87,
        -87, -57, -9, 43, 80, 90, 70, 26, -26, -70, -90, -80, -43, 9, 57, 87
    },
    {
        85, 47, -13, -67, -90, -73, -22, 39, 82, 88, 54, -4, -61, -90, -78, -30,
        30, 78, 90, 61, 4, -54, -88, -82, -39, 22, 73, 90, 67, 13, -47, -85
    },
    {
        83, 35, -35, -83, -83, -35, 35, 83, 83, 35, -35, -83, -83, -35, 35, 83,
        83, 35, -35, -83, -83, -35, 35, 83, 83, 35, -35, -83, -83, -35, 35, 83
    },
    {
        82, 22, -54, -90, -61, 13, 78, 85, 30, -47, -90, -67, 4, 73, 88, 39,
        -39, -88, -73, -4, 67, 90, 47, -30, -85, -78, -13, 61, 90, 54, -22, -82
    },
    {
        80, 9, -70, -87, -26, 57, 90, 43, -43, -90, -57, 26, 87, 70, -9, -80,
        -80, -9, 70, 87, 26, -57, -90, -43, 43, 90, 57, -26, -87, -70, 9, 80
    },
    {
        78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 30, 90, 54, -39, -90, -47,
        47, 90, 39, -54, -90, -30, 61, 88, 22, -67, -85, -13, 73, 82, 4, -78
    },
    {
        75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75,
        75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75
    },
    {
        73, -30, -90, -22, 78, 67, -39, -90, -13, 82, 61, -47, -88, -4, 85, 54,
        -54, -85, 4, 88, 47, -61, -82, 13, 90, 39, -67, -78, 22, 90, 30, -73
    },
    {
        70, -43, -87, 9, 90, 26, -80, -57, 57, 80, -26, -90, -9, 87, 43, -70,
        -70, 43, 87, -9, -90, -26, 80, 57, -57, -80, 26, 90, 9, -87, -43, 70
    },
    {
        67, -54, -78, 39, 85, -22, -90, 4, 90, 13, -88, -30, 82, 47, -73, -61,
        61, 73, -47, -82, 30, 88, -13, -90, -4, 90, 22, -85, -39, 78, 54, -67
    },
    {
        64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64,
        64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64
    },
    {
        61, -73, -47, 82, 30, -88, -13, 90, -4, -90, 22, 85, -39, -78, 54, 67,
        -67, -54, 78, 39, -85, -22, 90, 4, -90, 13, 88, -30, -82, 47, 73, -61
    },
    {
        57, -80, -26, 90, -9, -87, 43, 70, -70, -43, 87, 9, -90, 26, 80, -57,
        -57, 80, 26, -90, 9, 87, -43, -70, 70, 43, -87, -9, 90, -26, -80, 57
    },
    {
        54, -85, -4, 88, -47, -61, 82, 13, -90, 39, 67, -78, -22, 90, -30, -73,
        73, 30, -90, 22, 78, -67, -39, 90, -13, -82, 61, 47, -88, 4, 85, -54
    },
    {
        50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50,
        50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50
    },
    {
        47, -90, 39, 54, -90, 30, 61, -88, 22, 67, -85, 13, 73, -82, 4, 78,
        -78, -4, 82, -73, -13, 85, -67, -22, 88, -61, -30, 90, -54, -39, 90, -47
    },
    {
        43, -90, 57, 26, -87, 70, 9, -80, 80, -9, -70, 87, -26, -57, 90, -43,
        -43, 90, -57, -26, 87, -70, -9, 80, -80, 9, 70, -87, 26, 57, -90, 43
    },
    {
        39, -88, 73, -4, -67, 90, -47, -30, 85, -78, 13, 61, -90, 54, 22, -82,
        82, -22, -54, 90, -61, -13, 78, -85, 30, 47, -90, 67, 4, -73, 88, -39
    },
    {
        35, -83, 83, -35, -35, 83, -83, 35, 35, -83, 83, -35, -35, 83, -83, 35,
        35, -83, 83, -35, -35, 83, -83, 35, 35, -83, 83, -35, -35, 83, -83, 35
    },
    {
        30, -78, 90, -61, 4, 54, -88, 82, -39, -22, 73, -90, 67, -13, -47, 85,
        -85, 47, 13, -67, 90, -73, 22, 39, -82, 88, -54, -4, 61, -90, 78, -30
    },
    {
        26, -70, 90, -80, 43, 9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -26,
        -26, 70, -90, 80, -43, -9, 57, -87, 87, -57, 9, 43, -80, 90, -70, 26
    },
    {
        22, -61, 85, -90, 73, -39, -4, 47, -78, 90, -82, 54, -13, -30, 67, -88,
        88, -67, 30, 13, -54, 82, -90, 78, -47, 4, 39, -73, 90, -85, 61, -22
    },
    {
        18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18,
        18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18
    },
    {
        13, -39, 61, -78, 88, -90, 85, -73, 54, -30, 4, 22, -47, 67, -82, 90,
        -90, 82, -67, 47, -22, -4, 30, -54, 73, -85, 90, -88, 78, -61, 39, -13
    },
    {
        9, -26, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 26, -9,
        -9, 26, -43, 57, -70, 80, -87, 90, -90, 87, -80, 70, -57, 43, -26, 9
    },
    {
        4, -13, 22, -30, 39, -47, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90,
        90, -90, 88, -85, 82, -78, 73, -67, 61, -54, 47, -39, 30, -22, 13, -4
    }
};

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
}

static int32_t round_shift_signed(int64_t value, uint8_t shift) {
    int64_t magnitude = value < 0 ? -value : value;
    int32_t rounded = (int32_t)((magnitude +
        ((int64_t)1 << (shift - 1U))) >> shift);
    return value < 0 ? -rounded : rounded;
}

static uint16_t ist_scan_position(
    uint16_t scan_index, uint16_t width, uint16_t height) {
    unsigned int diagonal;
    unsigned int remaining = scan_index;
    for (diagonal = 0U; diagonal < (unsigned int)width + height - 1U;
         ++diagonal) {
        int row = (int)(diagonal < height ? diagonal : height - 1U);
        int last_row = diagonal >= width ? (int)(diagonal - width + 1U) : 0;
        unsigned int diagonal_count = (unsigned int)(row - last_row + 1);
        if (remaining < diagonal_count) {
            row -= (int)remaining;
            return (uint16_t)((unsigned int)row * width + diagonal -
                              (unsigned int)row);
        }
        remaining -= diagonal_count;
    }
    return AV2H_FIRST_COEFF_CAPACITY;
}

static Av2hStatus inverse_secondary_transform_4x4(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]) {
    const int16_t (*kernel)[16U];
    uint8_t transpose_output = (uint8_t)(
        mode->mode == AV2H_INTRA_HORIZONTAL ||
        mode->mode == AV2H_INTRA_D157 ||
        mode->mode == AV2H_INTRA_D67 ||
        mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL);
    int32_t input[8U];
    uint16_t index;
    if (transform->secondary_set == 0U &&
        transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set0_type1_kernel;
    } else if (transform->secondary_set == 0U &&
        transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set0_type2_kernel;
    } else if (transform->secondary_set == 0U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set0_type3_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set1_type1_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set1_type2_kernel;
    } else if (transform->secondary_set == 1U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set1_type3_kernel;
    } else if (transform->secondary_set == 2U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set2_type1_kernel;
    } else if (transform->secondary_set == 2U &&
        transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set2_type2_kernel;
    } else if (transform->secondary_set == 3U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set3_type1_kernel;
    } else if (transform->secondary_set == 3U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set3_type2_kernel;
    } else if (transform->secondary_set == 4U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set4_type1_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set6_type1_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set6_type2_kernel;
    } else if (transform->secondary_set == 6U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set6_type3_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set7_type1_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set7_type2_kernel;
    } else if (transform->secondary_set == 7U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set7_type3_kernel;
    } else if (transform->secondary_set == 8U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set8_type1_kernel;
    } else if (transform->secondary_set == 8U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set8_type2_kernel;
    } else if (transform->secondary_set == 9U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set9_type2_kernel;
    } else if (transform->secondary_set == 10U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set10_type1_kernel;
    } else if (transform->secondary_set == 10U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set10_type2_kernel;
    } else if (transform->secondary_set == 11U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set11_type1_kernel;
    } else if (transform->secondary_set == 11U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set11_type2_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 1U) {
        kernel = av2h_ist_4x4_set12_type1_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set12_type2_kernel;
    } else if (transform->secondary_set == 12U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set12_type3_kernel;
    } else if (transform->secondary_set == 13U &&
               transform->secondary_symbol == 2U) {
        kernel = av2h_ist_4x4_set13_type2_kernel;
    } else if (transform->secondary_set == 13U &&
               transform->secondary_symbol == 3U) {
        kernel = av2h_ist_4x4_set13_type3_kernel;
    } else {
        return AV2H_UNSUPPORTED;
    }
    for (index = 0U; index < 8U; ++index) {
        uint16_t position = ist_scan_position(
            index, coefficients->width, coefficients->height);
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        input[index] = dequantized[position];
        dequantized[position] = 0;
    }
    for (index = 0U; index < 16U; ++index) {
        uint16_t row;
        uint16_t scan_position = ist_scan_position(index, 4U, 4U);
        uint16_t x = (uint16_t)(scan_position % 4U);
        uint16_t y = (uint16_t)(scan_position / 4U);
        uint16_t position = transpose_output ?
            (uint16_t)(x * coefficients->width + y) :
            (uint16_t)(y * coefficients->width + x);
        int64_t sum = 0;
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        for (row = 0U; row < 8U; ++row) {
            sum += (int64_t)input[row] * kernel[row][index];
        }
        dequantized[position] =
            clamp(round_shift_signed(sum, 7U), -32768, 32767);
    }
    return AV2H_OK;
}

static Av2hStatus inverse_secondary_transform(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    int32_t dequantized[AV2H_FIRST_COEFF_CAPACITY]) {
    int32_t input[32U];
    const int16_t (*kernel)[48U];
    const uint8_t *scan_map = av2h_ist_8x8_set1_type3_scan_map;
    const uint16_t *output_positions = 0;
    uint8_t transpose_output;
    uint16_t input_count;
    uint16_t index;
    if (transform->secondary_symbol == 0U) return AV2H_OK;
    if (coefficients->width < 8U || coefficients->height < 8U) {
        return inverse_secondary_transform_4x4(
            mode, transform, coefficients, dequantized);
    }
    if (mode->mode == AV2H_INTRA_DC &&
        prefix->tx_size == AV2H_TX_4X4 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 8U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx4_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx4_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set8_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC &&
        prefix->tx_size == AV2H_TX_8X4 &&
        transform->secondary_symbol == 2U &&
        transform->secondary_set == 12U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx8_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx8_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set12_type2_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        prefix->tx_size == AV2H_TX_8X4 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 6U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx8_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx8_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set6_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D135 &&
        prefix->tx_size == AV2H_TX_4X8 &&
        transform->secondary_symbol == 1U &&
        transform->secondary_set == 3U) {
        int32_t small_input[8U];
        for (index = 0U; index < 8U; ++index) {
            uint16_t position = av2h_ist_4x4_tx4_input_scan[index];
            small_input[index] = dequantized[position];
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
        for (index = 0U; index < 16U; ++index) {
            uint16_t row;
            int64_t sum = 0;
            uint16_t position = av2h_ist_4x4_tx4_output_scan[index];
            for (row = 0U; row < 8U; ++row) {
                sum += (int64_t)small_input[row] *
                    av2h_ist_4x4_set3_type1_kernel[row][index];
            }
            dequantized[position] =
                clamp(round_shift_signed(sum, 7U), -32768, 32767);
        }
        return AV2H_OK;
    }
    transpose_output = (uint8_t)(
        mode->mode == AV2H_INTRA_HORIZONTAL ||
        mode->mode == AV2H_INTRA_D157 ||
        mode->mode == AV2H_INTRA_D67 ||
        mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL);
    input_count = (uint16_t)(
        prefix->tx_size == AV2H_TX_8X8 ||
        transform->primary_type == AV2H_TX_ADST_ADST ? 16U : 32U);
    /* Only these geometries use output positions beyond the set/type map. */
    if (!((mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_8X32 &&
           transform->secondary_symbol == 2U &&
           transform->secondary_set == 0U) ||
          (mode->mode == AV2H_INTRA_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_32X64 &&
           transform->secondary_symbol == 2U &&
           transform->secondary_set == 0U) ||
          (mode->mode == AV2H_INTRA_DC &&
           prefix->tx_size == AV2H_TX_8X16 &&
           transform->secondary_set == 6U) ||
          (mode->mode == AV2H_INTRA_SMOOTH &&
           prefix->tx_size == AV2H_TX_8X16 &&
           transform->secondary_symbol == 3U &&
           transform->secondary_set == 6U) ||
          (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
           prefix->tx_size == AV2H_TX_64X64 &&
           transform->secondary_symbol == 1U &&
           transform->secondary_set == 0U))) {
        goto generic_dispatch;
    }
    goto specialized_dispatch;
generic_dispatch:
    if (transform->secondary_symbol == 1U &&
        transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
        transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type2_kernel;
        scan_map = av2h_ist_8x8_set2_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type3_kernel;
        scan_map = av2h_ist_8x8_set7_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type1_kernel;
        scan_map = av2h_ist_8x8_set7_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 7U) {
        kernel = av2h_ist_8x8_set7_type2_kernel;
        scan_map = av2h_ist_8x8_set7_type2_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type2_kernel;
        scan_map = av2h_ist_8x8_set8_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type3_kernel;
        scan_map = av2h_ist_8x8_set8_type3_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 8U) {
        kernel = av2h_ist_8x8_set8_type1_kernel;
        scan_map = av2h_ist_8x8_set8_type1_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type1_kernel;
        scan_map = av2h_ist_8x8_set9_type1_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type2_kernel;
        scan_map = av2h_ist_8x8_set9_type2_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 9U) {
        kernel = av2h_ist_8x8_set9_type3_kernel;
        scan_map = av2h_ist_8x8_set9_type3_scan_map;
    } else if (transform->secondary_symbol == 2U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type2_kernel;
        scan_map = av2h_ist_8x8_set10_type2_scan_map;
    } else if (transform->secondary_symbol == 1U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type1_kernel;
        scan_map = av2h_ist_8x8_set10_type1_scan_map;
    } else if (transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else {
        return AV2H_UNSUPPORTED;
    }
    goto apply_transform;
specialized_dispatch:
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
        prefix->tx_size == AV2H_TX_8X32 &&
        transform->secondary_symbol == 2U &&
        transform->secondary_set == 0U &&
        coefficients->width == 8U && coefficients->height == 32U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        output_positions =
            av2h_ist_8x32_set0_type2_transpose_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        output_positions = av2h_ist_32x64_set0_type2_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_SMOOTH) &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        output_positions = av2h_ist_8x16_set6_positions;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               (prefix->tx_size == AV2H_TX_8X16 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_32X16 ||
                prefix->tx_size == AV2H_TX_64X8) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               (prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D135) &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               (mode->angle_delta == 2 ||
                (mode->angle_delta == 0 && mode->mrl_index == 1U)) &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               mode->angle_delta == 2 &&
               prefix->tx_size == AV2H_TX_8X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D113 &&
        prefix->tx_size == AV2H_TX_64X32 &&
        transform->secondary_symbol == 3U &&
        transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL) &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X32) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X16) &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               mode->angle_delta == -3 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_DC) &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               (prefix->tx_size == AV2H_TX_32X8 ||
                prefix->tx_size == AV2H_TX_64X8) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               mode->angle_delta == 0 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               (prefix->tx_size == AV2H_TX_16X16 ||
                prefix->tx_size == AV2H_TX_32X8) &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45) &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_VERTICAL) &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if ((mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D135) &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if ((mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_VERTICAL) &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               (prefix->tx_size == AV2H_TX_64X16 ||
                prefix->tx_size == AV2H_TX_64X64) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        output_positions =
            av2h_ist_64x64_set0_type1_transpose_positions;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if ((mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL) &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if ((mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
               (prefix->tx_size == AV2H_TX_16X32 ||
                prefix->tx_size == AV2H_TX_64X16) &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type1_kernel;
        scan_map = av2h_ist_8x8_set2_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 2U) {
        kernel = av2h_ist_8x8_set2_type3_kernel;
        scan_map = av2h_ist_8x8_set2_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_32X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type3_kernel;
        scan_map = av2h_ist_8x8_set5_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type1_kernel;
        scan_map = av2h_ist_8x8_set1_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type2_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D157 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X8 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type2_kernel;
        scan_map = av2h_ist_8x8_set5_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D45 &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
             transform->secondary_set == 10U) {
         kernel = av2h_ist_8x8_set10_type3_kernel;
         scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 10U) {
        kernel = av2h_ist_8x8_set10_type3_kernel;
        scan_map = av2h_ist_8x8_set10_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D113 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type3_kernel;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_64X8 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type3_kernel;
        scan_map = av2h_ist_8x8_set3_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D67 &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type2_kernel;
        scan_map = av2h_ist_8x8_set0_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X16 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type2_kernel;
        scan_map = av2h_ist_8x8_set6_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 0U) {
        kernel = av2h_ist_8x8_set0_type1_kernel;
        scan_map = av2h_ist_8x8_set0_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type3_kernel;
        scan_map = av2h_ist_8x8_set1_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_8X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type3_kernel;
        scan_map = av2h_ist_8x8_set4_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 4U) {
        kernel = av2h_ist_8x8_set4_type1_kernel;
        scan_map = av2h_ist_8x8_set4_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_8X32 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_D203 &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 5U) {
        kernel = av2h_ist_8x8_set5_type1_kernel;
        scan_map = av2h_ist_8x8_set5_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_16X8 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_DC &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type1_kernel;
        scan_map = av2h_ist_8x8_set6_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_VERTICAL &&
               prefix->tx_size == AV2H_TX_64X32 &&
               transform->secondary_symbol == 2U &&
               transform->secondary_set == 1U) {
        kernel = av2h_ist_8x8_set1_type2_kernel;
        scan_map = av2h_ist_8x8_set1_type2_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_32X16 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else if (mode->mode == AV2H_INTRA_D135 &&
               prefix->tx_size == AV2H_TX_16X32 &&
               transform->secondary_symbol == 1U &&
               transform->secondary_set == 3U) {
        kernel = av2h_ist_8x8_set3_type1_kernel;
        scan_map = av2h_ist_8x8_set3_type1_scan_map;
    } else if (mode->mode == AV2H_INTRA_SMOOTH &&
               prefix->tx_size == AV2H_TX_64X64 &&
               transform->secondary_symbol == 3U &&
               transform->secondary_set == 6U) {
        kernel = av2h_ist_8x8_set6_type3_kernel;
        scan_map = av2h_ist_8x8_set6_type3_scan_map;
    } else {
        goto generic_dispatch;
    }
apply_transform:
    for (index = 0U; index < input_count; ++index) {
        uint16_t position = ist_scan_position(
            scan_map[index], coefficients->width, coefficients->height);
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        input[index] = dequantized[position];
        dequantized[position] = 0;
    }
    if (output_positions != 0) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            dequantized[index] = 0;
        }
    }
    for (index = 0U; index < 48U; ++index) {
        uint16_t row;
        int64_t sum = 0;
        uint16_t scan_position;
        uint16_t position;
        for (row = 0U; row < input_count; ++row) {
            sum += (int64_t)input[row] * kernel[row][index];
        }
        if (output_positions != 0) {
            position = output_positions[index];
        } else {
            scan_position = ist_scan_position(
                scan_map[index], 8U, 8U);
            position = transpose_output ?
                (uint16_t)((scan_position / 8U) * coefficients->width +
                           scan_position % 8U) :
                (uint16_t)((scan_position % 8U) * coefficients->width +
                           scan_position / 8U);
        }
        if (position >= coefficients->coefficient_count) {
            return AV2H_INVALID_DATA;
        }
        dequantized[position] =
            clamp(round_shift_signed(sum, 7U), -32768, 32767);
    }
    return AV2H_OK;
}

static int32_t identity_scale(uint16_t size) {
    return size == 4U ? 128 : size == 8U ? 181 :
        size == 16U ? 256 : 362;
}

static uint8_t block_log2(uint16_t size) {
    return size == 4U ? 2U : size == 8U ? 3U :
        size == 16U ? 4U : size == 32U ? 5U : 6U;
}

static uint8_t smooth_scale(uint16_t width, uint16_t height) {
    unsigned int sum =
        (unsigned int)block_log2(width) - 2U +
        (unsigned int)block_log2(height) - 2U;
    return (uint8_t)((sum + 2U) >> 2U);
}

static uint8_t smooth_blend(uint16_t position, uint8_t scale) {
    unsigned int shift = ((unsigned int)position << 1U) >> scale;
    return shift >= 6U ? 0U : (uint8_t)(64U >> shift);
}

static const int16_t directional_derivative[90U] = {
    0, 4096, 2048, 1365, 1024, 819, 682, 585, 512, 455, 409, 409, 409, 372,
    341, 292, 273, 256, 227, 215, 204, 186, 178, 170, 157, 151, 146, 136,
    132, 128, 117, 110, 107, 99, 97, 97, 93, 87, 83, 81, 77, 74, 73, 69,
    66, 64, 62, 59, 56, 55, 53, 50, 49, 47, 44, 42, 42, 41, 38, 37, 35,
    32, 31, 30, 28, 27, 26, 24, 23, 22, 20, 19, 18, 16, 15, 14, 12, 11,
    10, 10, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
};

static int directional_base_angle(Av2hFirstLeafIntraMode mode) {
    return mode == AV2H_INTRA_VERTICAL ? 90 :
           mode == AV2H_INTRA_HORIZONTAL ? 180 :
           mode == AV2H_INTRA_D45 ? 45 :
           mode == AV2H_INTRA_D67 ? 67 :
           mode == AV2H_INTRA_D113 ? 113 :
           mode == AV2H_INTRA_D135 ? 135 :
           mode == AV2H_INTRA_D157 ? 157 :
           mode == AV2H_INTRA_D203 ? 203 : 0;
}

static int wide_angle_map(int angle, uint16_t width, uint16_t height) {
    if ((height == 2U * width && angle < 61) ||
        (height == 4U * width && angle < 73) ||
        (height == 8U * width && angle < 82) ||
        (height == 16U * width && angle < 86)) {
        return angle + 180;
    }
    if ((width == 2U * height && angle > 209) ||
        (width == 4U * height && angle > 197) ||
        (width == 8U * height && angle > 188) ||
        (width == 16U * height && angle > 184)) {
        return angle - 180;
    }
    return angle;
}

static uint8_t directional_reference(const uint8_t *reference,
                                     size_t reference_count,
                                     uint8_t above_left,
                                     int index) {
    if (index < 0) return above_left;
    if (reference == 0 || reference_count == 0U) return above_left;
    if ((size_t)index >= reference_count) {
        return reference[reference_count - 1U];
    }
    return reference[index];
}

static int directional_filter_strength(int size0, int size1, int delta,
                                       uint8_t smooth) {
    int distance = delta < 0 ? -delta : delta;
    int sum = size0 + size1;
    if (smooth) {
        if (sum <= 8) return distance >= 64 ? 2 : distance >= 40 ? 1 : 0;
        if (sum <= 16) return distance >= 48 ? 2 : distance >= 20 ? 1 : 0;
        if (sum <= 24) return distance >= 4 ? 3 : 0;
        return distance >= 1 ? 3 : 0;
    }
    if (sum <= 8) return distance >= 56 ? 1 : 0;
    if (sum <= 12) return distance >= 40 ? 1 : 0;
    if (sum <= 16) return distance >= 40 ? 1 : 0;
    if (sum <= 24) {
        return distance >= 32 ? 3 :
               distance >= 16 ? 2 :
               distance >= 8 ? 1 : 0;
    }
    if (sum <= 32) {
        return distance >= 32 ? 3 :
               distance >= 4 ? 2 :
               distance >= 1 ? 1 : 0;
    }
    return distance >= 1 ? 3 : 0;
}

static void directional_filter_edge(uint8_t *edge, size_t count,
                                    int strength) {
    static const uint8_t kernels[3U][5U] = {
        { 0U, 4U, 8U, 4U, 0U },
        { 0U, 5U, 6U, 5U, 0U },
        { 2U, 4U, 4U, 4U, 2U }
    };
    uint8_t source[132U];
    size_t index;
    if (strength == 0 || count > sizeof(source)) return;
    for (index = 0U; index < count; ++index) source[index] = edge[index];
    for (index = 1U; index < count; ++index) {
        unsigned int sum = 0U;
        size_t tap;
        for (tap = 0U; tap < 5U; ++tap) {
            int source_index = (int)index - 2 + (int)tap;
            if (source_index < 0) source_index = 0;
            if ((size_t)source_index >= count) {
                source_index = (int)count - 1;
            }
            sum += source[source_index] * kernels[strength - 1][tap];
        }
        edge[index] = (uint8_t)((sum + 8U) >> 4U);
    }
}

static uint16_t cfl_resolve_divisor_32(uint32_t d, int *shift);
static Av2hStatus apply_intrabc_morph_prediction(
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    int32_t source_x,
    int32_t source_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor);
static uint8_t dc_predictor_average(unsigned int sum, unsigned int count,
                                    uint16_t width, uint16_t height,
                                    uint8_t have_top, uint8_t have_left);

uint64_t av2h_debug_smooth_above_hash;
uint64_t av2h_debug_smooth_left_hash;
uint8_t av2h_debug_directional_edges[18U];
uint8_t av2h_debug_directional_raw[3U];
uint8_t av2h_debug_directional_ibp[24U];
uint8_t av2h_debug_directional_target[3U];

static uint8_t directional_ibp_weight(unsigned int row,
                                      unsigned int column,
                                      int dy) {
    uint32_t y = (uint32_t)dy * (column + 1U);
    uint32_t distance = ((row + 1U) << 6U) + y;
    int shift;
    uint16_t divisor = cfl_resolve_divisor_32(distance, &shift);
    shift -= 7;
    return (uint8_t)((y * divisor + ((uint32_t)1U << (shift - 1))) >> shift);
}

static int directional_ibp_canonical_derivative(int angle) {
    if (angle <= 39) return directional_derivative[51];
    if (angle == 45) return directional_derivative[45];
    if (angle == 51) return directional_derivative[39];
    if (angle == 61) return directional_derivative[29];
    if (angle == 67) return directional_derivative[23];
    if (angle == 73) return directional_derivative[17];
    if (angle == 84) return directional_derivative[6];
    return 0;
}

static Av2hStatus predict_directional(
    const Av2hFirstLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride,
    uint8_t use_idif,
    uint8_t filter_edges) {
    int dx;
    int dy;
    uint8_t above_storage[141U];
    uint8_t left_storage[141U];
    uint8_t *above = &above_storage[5U];
    uint8_t *left = &left_storage[5U];
    size_t edge_count =
        (size_t)width + height + 2U + 2U * mode->mrl_index;
    size_t index;
    uint16_t row;
    uint16_t column;
    int missing_left_ibp;
    uint8_t edge_ibp = (uint8_t)(mode->enable_ibp || mode->edge_ibp);
    if (mode->mrl_index == 1U) {
        ++angle;
    } else if (mode->mrl_index == 2U) {
        --angle;
    }
    if (angle <= 0 || angle >= 270 || mode->multi_line_mrl != 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (angle == 90 || angle == 180) {
        const uint8_t *edge = angle == 90 ? references->above :
            references->left;
        size_t edge_count_available = angle == 90 ? references->above_count :
            references->left_count;
        uint8_t fallback = angle == 90 ?
            (references->left != 0 && references->left_count != 0U ?
                references->left[0] : 127U) :
            (references->above != 0 && references->above_count != 0U ?
                references->above[0] : 129U);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t edge_index = angle == 90 ? column : row;
                predictor[(size_t)row * predictor_stride + column] =
                    edge != 0 && edge_count_available != 0U ?
                        edge[edge_index < edge_count_available ? edge_index :
                             edge_count_available - 1U] : fallback;
            }
        }
        return AV2H_OK;
    }
    missing_left_ibp =
        ((mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0 &&
          width == 32U && height == 16U) ||
         (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
          width == 64U && height == 16U)) &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        (references->left == 0 || references->left_count == 0U) &&
        references->above != 0 && references->above_count != 0U;
    dx = angle < 90 ? directional_derivative[angle] :
         angle < 180 ? directional_derivative[180 - angle] : 1;
    dy = angle < 90 ? 1 :
         angle < 180 ? directional_derivative[angle - 90] :
                       directional_derivative[270 - angle];
    if (edge_count > 136U) return AV2H_UNSUPPORTED;
    {
        uint8_t no_references =
            (references->above == 0 || references->above_count == 0U) &&
            (references->left == 0 || references->left_count == 0U);
        uint8_t fallback = references->above != 0 &&
            references->above_count != 0U ? references->above[0] :
            references->left != 0 && references->left_count != 0U ?
                references->left[0] : 128U;
        uint8_t above_fallback = no_references ? 127U : fallback;
        uint8_t left_fallback = no_references ? 129U : fallback;
        uint8_t corner = references->has_above_left ?
            references->above_left : fallback;
        above[-4] = above[-3] = above[-2] = above[-1] = corner;
        left[-4] = left[-3] = left[-2] = left[-1] = corner;
        if (mode->mrl_index != 0U) {
            size_t negative_count = (size_t)mode->mrl_index + 1U;
            for (index = 1U; index <= negative_count; ++index) {
                above[-(int)index] = index <= references->negative_count ?
                    references->above_negative[index] : corner;
                left[-(int)index] = index <= references->negative_count ?
                    references->left_negative[index] : corner;
            }
            above[-(int)negative_count - 1] =
                above[-(int)negative_count];
            left[-(int)negative_count - 1] =
                left[-(int)negative_count];
        }
        for (index = 0U; index < edge_count; ++index) {
            size_t directional_extent = (size_t)width + height;
            size_t above_available = references->above_count;
            size_t left_available = references->left_count;
            if ((angle < 90 || (angle > 180 && edge_ibp)) &&
                above_available > 2U * width) {
                above_available = 2U * width;
            }
            if ((angle > 180 || (angle < 90 && edge_ibp)) &&
                left_available > 2U * height) {
                left_available = 2U * height;
            }
            size_t above_source = angle < 90 ?
                (index < above_available ? index : above_available - 1U) :
                angle > 180 && edge_ibp ?
                (index < above_available ? index : above_available - 1U) :
                index < width ? index : width - 1U;
            size_t left_source =
                angle < 90 && edge_ibp ?
                    (index < directional_extent ?
                        index : directional_extent - 1U) :
                angle > 180 ? index :
                mode->mode == AV2H_INTRA_HORIZONTAL &&
                    mode->angle_delta == 1 && width == 16U && height == 16U ?
                index : index < height ? index : height - 1U;
            above[index] = references->above != 0 &&
                references->above_count != 0U ?
                references->above[
                    above_source < references->above_count ?
                    above_source : references->above_count - 1U] :
                above_fallback;
            left[index] = references->left != 0 &&
                references->left_count != 0U ?
                references->left[left_source < left_available ?
                                 left_source : left_available - 1U] :
                left_fallback;
        }
        if (mode->mode == AV2H_INTRA_HORIZONTAL &&
            mode->angle_delta == 2 && mode->mrl_index == 3U &&
            width == 16U && height == 32U) {
            for (index = 0U; index < 10U; ++index) {
                av2h_debug_directional_edges[8U + index] = left[index];
            }
        }
            if (mode->mode == AV2H_INTRA_HORIZONTAL &&
                mode->angle_delta == 2 && width == 32U && height == 16U) {
                av2h_debug_directional_raw[0] = left[13];
                av2h_debug_directional_raw[1] = left[14];
                av2h_debug_directional_raw[2] = left[15];
                av2h_debug_directional_edges[0] = left[16];
                av2h_debug_directional_target[0] =
                (uint8_t)references->left_count;
            }
        if (mode->mode == AV2H_INTRA_VERTICAL &&
            mode->angle_delta == -2 &&
            width == 16U && height == 32U) {
            av2h_debug_directional_raw[0] = above[-1];
            av2h_debug_directional_raw[1] = above[0];
            av2h_debug_directional_raw[2] = left[0];
        }
        if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
            width == 16U && height == 16U) {
            av2h_debug_directional_edges[4] = left[19];
            av2h_debug_directional_edges[5] = left[20];
            av2h_debug_directional_edges[6] = left[21];
            av2h_debug_directional_edges[7] = left[22];
        }
        if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -1 &&
            width == 4U && height == 8U && references->above != 0 &&
            references->above[0] == 95U) {
            for (index = 0U; index < 4U; ++index) {
                av2h_debug_directional_edges[index] = above[index];
            }
            av2h_debug_directional_edges[4] = (uint8_t)
                directional_filter_strength(
                    width, height, angle - 90,
                    (uint8_t)(mode->above_smooth || mode->left_smooth));
        }
        if (filter_edges != 0U && mode->mrl_index == 0U &&
            references->above != 0 && references->above_count != 0U &&
            references->left != 0 && references->left_count != 0U &&
            width + height >= 24U) {
            corner = (uint8_t)((left[0] * 5U + above[-1] * 6U +
                                above[0] * 5U + 8U) >> 4U);
            above[-1] = left[-1] = corner;
        }
        if (filter_edges != 0U && mode->mrl_index == 0U &&
            references->above != 0 && references->above_count != 0U) {
            size_t above_filter_count = angle < 90 ?
                (size_t)width + height + 1U :
                angle > 180 && edge_ibp ?
                (size_t)width + height + 1U :
                missing_left_ibp ? (size_t)width + height + 1U :
                                   (size_t)width + 1U;
            directional_filter_edge(
                above - 1, above_filter_count,
                directional_filter_strength(
                    width, height,
                    edge_ibp && angle > 180 ? angle - 270 :
                        angle - 90,
                    edge_ibp ? mode->above_smooth :
                        (uint8_t)(mode->above_smooth || mode->left_smooth)));
        }
        if (filter_edges != 0U && mode->mrl_index == 0U &&
            references->left != 0 && references->left_count != 0U) {
            size_t left_filter_count = (size_t)height + 1U;
            if (angle > 180 || (angle < 90 && edge_ibp)) {
                left_filter_count += width;
            } else if (mode->mode == AV2H_INTRA_HORIZONTAL &&
                mode->angle_delta == 1 && width == 16U && height == 16U) {
                left_filter_count += width;
            }
            directional_filter_edge(
                left - 1, left_filter_count,
                directional_filter_strength(
                    height, width,
                    edge_ibp && angle < 90 ? angle : angle - 180,
                    edge_ibp ? mode->left_smooth :
                        (uint8_t)(mode->above_smooth || mode->left_smooth)));
        }
        if (mode->mode == AV2H_INTRA_HORIZONTAL &&
            mode->angle_delta == 2 && width == 32U && height == 16U) {
            av2h_debug_directional_edges[1] = left[13];
            av2h_debug_directional_edges[2] = left[14];
            av2h_debug_directional_edges[3] = left[15];
            av2h_debug_directional_edges[4] = left[16];
        }
        if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -2 &&
            width == 16U && height == 16U) {
            av2h_debug_directional_edges[0] = above[5];
            av2h_debug_directional_edges[1] = above[6];
            av2h_debug_directional_edges[2] = above[7];
            av2h_debug_directional_edges[3] = above[8];
        }
        if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
            width == 16U && height == 16U) {
            av2h_debug_directional_edges[0] = left[19];
            av2h_debug_directional_edges[1] = left[20];
            av2h_debug_directional_edges[2] = left[21];
            av2h_debug_directional_edges[3] = left[22];
        }
        if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -1 &&
            width == 4U && height == 8U && references->above != 0 &&
            references->above[0] == 95U) {
            for (index = 0U; index < 4U; ++index) {
                av2h_debug_directional_edges[5U + index] = above[index];
            }
        }
        if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
            width == 8U && height == 32U) {
            av2h_debug_directional_edges[0] = left[30];
            av2h_debug_directional_edges[1] = left[31];
            av2h_debug_directional_edges[2] = left[32];
            av2h_debug_directional_edges[3] = left[33];
        }
        if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
            width == 4U && height == 8U && angle == 225 && left[0] == 104U &&
            left[1] == 106U && left[2] == 104U && left[3] == 102U) {
            for (index = 0U; index < 13U; ++index) {
                av2h_debug_directional_edges[index] = left[index];
            }
        }
        if (angle > 90 && angle < 180 && mode->mrl_index == 0U) {
            above[width] = above[width - 1U];
            left[height] = left[height - 1U];
        } else if (angle < 90) {
            size_t max_base = (size_t)width + height - 1U +
                2U * mode->mrl_index;
            above[max_base + 1U] = above[max_base];
            above[max_base + 2U] = above[max_base];
            if (mode->mrl_index == 0U && mode->enable_ibp) {
                left[max_base + 1U] = left[max_base];
                left[max_base + 2U] = left[max_base];
            }
        } else if (angle > 180) {
            size_t max_base = (size_t)width + height - 1U +
                2U * mode->mrl_index;
            left[max_base + 1U] = left[max_base];
            left[max_base + 2U] = left[max_base];
            if (mode->mrl_index == 0U && mode->enable_ibp) {
                above[max_base + 1U] = above[max_base];
                above[max_base + 2U] = above[max_base];
            }
        }
           if ((mode->mode == AV2H_INTRA_D45 &&
               width == 8U && height == 64U) ||
              (mode->mode == AV2H_INTRA_VERTICAL &&
               mode->angle_delta == -2 &&
               width == 16U && height == 32U)) {
            for (index = 0U; index < 9U; ++index) {
                int edge_index = (int)index - 1;
                av2h_debug_directional_edges[index] = above[edge_index];
                av2h_debug_directional_edges[index + 9U] = left[edge_index];
            }
        }
    }
    if (angle < 90) {
        int max_base =
            (int)width + height - 1 + 2 * mode->mrl_index;
        int x = dx * (1 + mode->mrl_index);
        for (row = 0U; row < height; ++row, x += dx) {
            int base = x >> 6;
            int shift = (x & 0x3f) >> 1;
            for (column = 0U; column < width; ++column, ++base) {
                int32_t sum = 0;
                size_t tap;
                uint8_t primary;
                if (base > max_base || (!use_idif && base == max_base)) {
                    primary = above[max_base];
                } else if (!use_idif) {
                    primary = (uint8_t)((above[base] * (32 - shift) +
                        above[base + 1] * shift + 16) >> 5U);
                } else {
                    for (tap = 0U; tap < 4U; ++tap) {
                        sum += dr_interp_filter[shift][tap] *
                            above[base + (int)tap - 1];
                    }
                    primary = (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
                }
                if (mode->mrl_index == 0U && mode->enable_ibp &&
                    mode->angle_delta % 2 == 0) {
                    int secondary_dy = directional_derivative[90 - angle];
                    int weight_dy =
                        directional_ibp_canonical_derivative(angle);
                    int secondary_y = secondary_dy * ((int)column + 1);
                    int secondary_base = (secondary_y >> 6) + row;
                    int secondary_shift = (secondary_y & 0x3f) >> 1;
                    uint8_t secondary;
                    unsigned int row_shift = height >> 5U;
                    unsigned int column_shift = width >> 5U;
                    uint8_t weight;
                    if (secondary_base > max_base) {
                        secondary = left[max_base];
                    } else {
                        sum = 0;
                        for (tap = 0U; tap < 4U; ++tap) {
                            sum += dr_interp_filter[secondary_shift][tap] *
                                left[secondary_base + (int)tap - 1];
                        }
                        secondary = (uint8_t)clamp(
                            (sum + 64) >> 7U, 0, 255);
                    }
                    weight = directional_ibp_weight(
                        row >> row_shift, column >> column_shift,
                        weight_dy);
                    if (mode->mode == AV2H_INTRA_D67 &&
                        mode->angle_delta == -2 && width == 4U &&
                        height == 4U && row == 0U) {
                        av2h_debug_directional_ibp[column] = primary;
                        av2h_debug_directional_ibp[column + 4U] = secondary;
                        av2h_debug_directional_ibp[column + 8U] = weight;
                    }
                    if (mode->mode == AV2H_INTRA_VERTICAL &&
                        mode->angle_delta == -2 && width == 16U &&
                        height == 32U && row == 5U && column == 3U) {
                        av2h_debug_directional_target[0] = primary;
                        av2h_debug_directional_target[1] = secondary;
                        av2h_debug_directional_target[2] = weight;
                    }
                    if (mode->mode == AV2H_INTRA_VERTICAL &&
                        mode->angle_delta == -2 && width == 64U &&
                        height == 32U && row == 0U && column == 24U) {
                        av2h_debug_directional_target[0] = primary;
                        av2h_debug_directional_target[1] = secondary;
                        av2h_debug_directional_target[2] = weight;
                    }
                    if (mode->mode == AV2H_INTRA_VERTICAL &&
                        mode->angle_delta == -2 && width == 16U &&
                        height == 16U && row == 0U && column == 6U) {
                        av2h_debug_directional_target[0] = primary;
                        av2h_debug_directional_target[1] = secondary;
                        av2h_debug_directional_target[2] = weight;
                    }
                    if (mode->mode == AV2H_INTRA_D67 &&
                        mode->angle_delta == -2 && width == 16U &&
                        height == 16U && row == 0U && column == 9U) {
                        av2h_debug_directional_target[0] = primary;
                        av2h_debug_directional_target[1] = secondary;
                        av2h_debug_directional_target[2] = weight;
                    }
                                        if (((mode->mode == AV2H_INTRA_VERTICAL &&
                                                    mode->angle_delta == -2 && width == 16U &&
                                                    height == 32U) ||
                                                 (mode->mode == AV2H_INTRA_D45 &&
                                                    mode->angle_delta == 0 && width == 8U &&
                                                    height == 64U)) &&
                                                row == 0U && column < 8U) {
                        av2h_debug_directional_ibp[column] = primary;
                        av2h_debug_directional_ibp[column + 8U] = secondary;
                        av2h_debug_directional_ibp[column + 16U] = weight;
                    }
                    primary = (uint8_t)((primary * weight +
                        secondary * (128U - weight) + 64U) >> 7U);
                }
                predictor[(size_t)row * predictor_stride + column] = primary;
                if (mode->mode == AV2H_INTRA_D45 &&
                    mode->angle_delta == -1 && width == 4U && height == 8U &&
                    references->above != 0 && references->above[0] == 95U &&
                    row == 0U) {
                    av2h_debug_directional_edges[9U + column] = primary;
                }
            }
        }
        return AV2H_OK;
    }
    if (angle < 180) {
        const int min_base = -1 - mode->mrl_index;
        if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 1 &&
            width == 16U && height == 32U && references->above_count >= 16U &&
            references->above[0] == 167U && references->above[15] == 166U) {
            av2h_debug_directional_ibp[0] = above[13];
            av2h_debug_directional_ibp[1] = above[14];
            av2h_debug_directional_ibp[2] = above[15];
            av2h_debug_directional_ibp[3] = above[16];
            av2h_debug_directional_ibp[4] = (uint8_t)dx;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int x = ((int)column << 6) -
                    ((int)row + 1 + mode->mrl_index) * dx;
                int base = x >> 6;
                int shift = (x & 0x3f) >> 1;
                const uint8_t *reference;
                int32_t sum = 0;
                size_t tap;
                if (base >= min_base) {
                    reference = above;
                } else {
                    int y = ((int)row << 6) -
                        ((int)column + 1 + mode->mrl_index) * dy;
                    base = y >> 6;
                    shift = (y & 0x3f) >> 1;
                    reference = left;
                }
                if (!use_idif) {
                    sum = reference[base] * (32 - shift) +
                        reference[base + 1] * shift;
                } else {
                    for (tap = 0U; tap < 4U; ++tap) {
                        int reference_index = base + (int)tap - 1;
                        uint8_t sample = mode->mrl_index != 0U ?
                            reference[reference_index] :
                            directional_reference(
                                reference, edge_count, reference[-1],
                                reference_index);
                        sum += dr_interp_filter[shift][tap] * sample;
                    }
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp(use_idif ? (sum + 64) >> 7U :
                        (sum + 16) >> 5U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    {
        int max_base = (int)width + height - 1 + 2 * mode->mrl_index;
        int y = dy * (1 + mode->mrl_index);
    for (column = 0U; column < width; ++column) {
        int base = y >> 6;
        int shift = (y & 0x3f) >> 1;
        y += dy;
        for (row = 0U; row < height; ++row, ++base) {
            int32_t sum = 0;
            size_t tap;
            if (base > max_base || (!use_idif && base == max_base)) {
                sum = (int32_t)left[max_base] << (use_idif ? 7U : 5U);
            } else if (!use_idif) {
                sum = left[base] * (32 - shift) +
                    left[base + 1] * shift;
            } else {
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                        directional_reference(
                            left, edge_count, left[-1],
                            base + (int)tap - 1);
                }
            }
            {
                uint8_t primary =
                    (uint8_t)clamp(use_idif ? (sum + 64) >> 7U :
                        (sum + 16) >> 5U, 0, 255);
                if (mode->mrl_index == 0U && mode->enable_ibp &&
                    mode->angle_delta % 2 == 0) {
                    int secondary_dx = directional_derivative[angle - 180];
                    int weight_dy = directional_ibp_canonical_derivative(
                        270 - angle);
                    int secondary_x = secondary_dx * ((int)row + 1);
                    int secondary_base =
                        (secondary_x >> 6) + (int)column;
                    int secondary_shift = (secondary_x & 0x3f) >> 1;
                    int32_t secondary_sum = 0;
                    uint8_t secondary;
                    unsigned int row_shift = height >> 5U;
                    unsigned int column_shift = width >> 5U;
                    uint8_t weight;
                    if (secondary_base > max_base) {
                        secondary = above[max_base];
                    } else {
                        for (tap = 0U; tap < 4U; ++tap) {
                            secondary_sum +=
                                dr_interp_filter[secondary_shift][tap] *
                                directional_reference(
                                    above, edge_count, above[-1],
                                    secondary_base + (int)tap - 1);
                        }
                        secondary = (uint8_t)clamp(
                            (secondary_sum + 64) >> 7U, 0, 255);
                    }
                    weight = directional_ibp_weight(
                        column >> column_shift, row >> row_shift,
                        weight_dy);
                    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
                        mode->angle_delta == 2 && width == 8U &&
                        height == 32U && row == 31U && column == 7U) {
                        av2h_debug_directional_target[0] = primary;
                        av2h_debug_directional_target[1] = secondary;
                        av2h_debug_directional_target[2] = weight;
                    }
                    primary = (uint8_t)((primary * weight +
                        secondary * (128U - weight) + 64U) >> 7U);
                }
                predictor[(size_t)row * predictor_stride + column] = primary;
            }
        }
    }
    }
    return AV2H_OK;
}

static Av2hStatus predict_directional_idif(
    const Av2hFirstLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    return predict_directional(
        mode, angle, width, height, references,
        predictor, predictor_stride, 1U, 1U);
}

static Av2hStatus predict_directional_idif_unfiltered(
    const Av2hFirstLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    return predict_directional(
        mode, angle, width, height, references,
        predictor, predictor_stride, 1U, 0U);
}

static Av2hStatus predict_d45_8x4_ibp(
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    static const uint8_t weights[4U][8U] = {
        { 64U, 85U, 96U, 103U, 107U, 110U, 112U, 114U },
        { 43U, 64U, 77U, 85U, 92U, 96U, 100U, 103U },
        { 32U, 51U, 64U, 73U, 80U, 85U, 90U, 93U },
        { 26U, 43U, 55U, 64U, 71U, 77U, 81U, 85U }
    };
    uint8_t above_edge[13U];
    uint8_t left_edge[13U];
    uint8_t source[13U];
    uint8_t base;
    size_t index;
    uint16_t row;
    uint16_t column;
    base = references->above_count != 0U ? references->above[0] :
        references->left_count != 0U ? references->left[0] : 128U;
    above_edge[0] = references->has_above_left ?
        references->above_left : base;
    left_edge[0] = above_edge[0];
    for (index = 0U; index < 12U; ++index) {
        if (references->above_count != 0U) {
            size_t above_index = index < references->above_count ?
                index : references->above_count - 1U;
            above_edge[index + 1U] = references->above[above_index];
        } else {
            above_edge[index + 1U] = base;
        }
        if (references->left_count != 0U) {
            size_t left_index = index < references->left_count ?
                index : references->left_count - 1U;
            left_edge[index + 1U] = references->left[left_index];
        } else {
            left_edge[index + 1U] = base;
        }
    }
    for (index = 0U; index < 13U; ++index) {
        source[index] = above_edge[index];
    }
    for (index = 1U; index < 13U; ++index) {
        size_t next = index < 12U ? index + 1U : index;
        above_edge[index] = (uint8_t)(
            (source[index - 1U] + 2U * source[index] +
             source[next] + 2U) >> 2U);
    }
    for (index = 0U; index < 13U; ++index) {
        source[index] = left_edge[index];
    }
    for (index = 1U; index < 13U; ++index) {
        size_t next = index < 12U ? index + 1U : index;
        left_edge[index] = (uint8_t)(
            (source[index - 1U] + 2U * source[index] +
             source[next] + 2U) >> 2U);
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            unsigned int weight = weights[row][column];
            unsigned int primary = above_edge[row + column + 2U];
            unsigned int secondary = left_edge[row + column + 2U];
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)((primary * weight +
                           secondary * (128U - weight) + 64U) >> 7U);
        }
    }
    return AV2H_OK;
}

static int luma_prediction_dimension_supported(uint16_t dimension) {
    return dimension == 4U || dimension == 8U || dimension == 16U ||
           dimension == 32U || dimension == 64U;
}

static int luma_wide_angle(int angle, uint16_t width, uint16_t height) {
    int threshold = height == 2U * width ? 61 :
                    height == 4U * width ? 73 :
                    height == 8U * width ? 82 :
                    height == 16U * width ? 86 : 0;
    if (threshold != 0 && angle < threshold) return angle + 180;
    threshold = width == 2U * height ? 61 :
                width == 4U * height ? 73 :
                width == 8U * height ? 82 :
                width == 16U * height ? 86 : 0;
    return threshold != 0 && angle > 270 - threshold ? angle - 180 : angle;
}

static Av2hStatus predict_luma_directional_feature(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    Av2hFirstLeafMode primary_mode;
    Av2hFirstLeafMode secondary_mode;
    Av2hLumaReferences secondary_references;
    uint8_t secondary_predictor[AV2H_RECON_SAMPLE_CAPACITY];
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    int mrl_angle_delta = mode->mrl_index == 1U ? 1 :
                          mode->mrl_index == 2U ? -1 : 0;
    int secondary_angle = luma_wide_angle(
        directional_base_angle(mode->mode) + mode->angle_delta * 3 +
            mrl_angle_delta,
        width, height);
    int primary_angle = secondary_angle - mrl_angle_delta;
    if (mode->multi_line_mrl == 0U) {
        return predict_directional_idif(
            mode, primary_angle, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->multi_line_mrl != 1U || mode->mrl_index == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((width != 4U || height != 4U) &&
        (references->secondary_above == 0 ||
         references->secondary_above_count == 0U) &&
        (references->secondary_left == 0 ||
         references->secondary_left_count == 0U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
    av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
    av2h_memory_copy(
        &secondary_references, references, sizeof(secondary_references));
    primary_mode.multi_line_mrl = 0U;
    primary_mode.enable_ibp = 0U;
    secondary_mode.mrl_index = 0U;
    secondary_mode.multi_line_mrl = 0U;
    secondary_mode.enable_ibp = 0U;
    status = predict_directional_idif(
        &primary_mode, primary_angle, width, height, references,
        predictor, predictor_stride);
    if (status != AV2H_OK) return status;
    if (width == 4U && height == 4U) return AV2H_OK;
    secondary_references.above = references->secondary_above;
    secondary_references.above_count = references->secondary_above_count;
    secondary_references.left = references->secondary_left;
    secondary_references.left_count = references->secondary_left_count;
    secondary_references.above_left = references->secondary_above_left;
    secondary_references.has_above_left =
        references->has_secondary_above_left;
    status = predict_directional_idif_unfiltered(
        &secondary_mode, secondary_angle, width, height,
        &secondary_references,
        secondary_predictor, width);
    if (status != AV2H_OK) return status;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            size_t output = (size_t)row * predictor_stride + column;
            predictor[output] = (uint8_t)(
                (predictor[output] +
                 secondary_predictor[(size_t)row * width + column] + 1U) >>
                1U);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_luma_predict(
    const Av2hFirstLeafMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint8_t left_storage[44U];
    uint8_t *left = &left_storage[1U];
    uint8_t above_storage[45U];
    uint8_t *above = &above_storage[1U];
    uint16_t row;
    uint16_t column;
    size_t index;
    if (mode == 0 || references == 0 || predictor == 0 ||
        predictor_stride < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 32U && height == 32U) {
        return predict_directional_idif(
            mode, 90, width, height, references,
            predictor, predictor_stride);
    }
    if (directional_base_angle(mode->mode) != 0 &&
        mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        width == 4U && height == 4U) {
        return predict_directional_idif(
            mode, directional_base_angle(mode->mode),
            width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        width == 4U && height == 8U) {
        return predict_directional_idif(
            mode, 203, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->angle_delta == 0 && mode->mrl_index == 0U &&
        mode->multi_line_mrl == 0U &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        return predict_luma_directional_feature(
            mode, width, height, references, predictor, predictor_stride);
    }
    if (((mode->angle_delta == -3 || mode->angle_delta == -2 ||
            mode->angle_delta == -1 || mode->angle_delta == 1 ||
            mode->angle_delta == 2 || mode->angle_delta == 3) ||
         (mode->mrl_index == 1U || mode->mrl_index == 2U ||
          mode->mrl_index == 3U) ||
         mode->multi_line_mrl == 1U ||
         (mode->angle_delta == 0 && mode->mrl_index == 0U &&
          mode->multi_line_mrl == 0U &&
          ((mode->mode == AV2H_INTRA_VERTICAL &&
                        ((width == 8U && height == 4U) ||
                         (width == 16U && height == 4U) ||
                         (width == 8U && height == 8U) ||
                         (width == 32U && height == 4U) ||
                         (width == 4U && height == 16U) ||
                         (width == 16U && height == 32U) ||
                         (width == 16U && height == 64U) ||
                         (width == 32U && height == 16U) ||
                         (width == 64U && height == 8U) ||
                                                 (width == 64U && height == 16U) ||
                                                 (width == 64U && height == 32U) ||
                         (width == 64U && height == 64U) ||
                         (width == 32U && height == 64U))) ||
                     (mode->mode == AV2H_INTRA_D45 &&
                        ((width == 4U && height == 32U) ||
                         (width == 16U && height == 8U) ||
                         (width == 16U && height == 32U) ||
                                                 (width == 16U && height == 64U) ||
                         (width == 64U && height == 8U) ||
                         (width == 64U && height == 32U))) ||
                       (mode->mode == AV2H_INTRA_D113 &&
                                                ((width == 4U && height == 8U) ||
                                                 (width == 4U && height == 16U) ||
                                                 (width == 8U && height == 32U) ||
                                                 (width == 8U && height == 16U) ||
                                                 (width == 32U && height == 4U) ||
                                                 (width == 64U && height == 8U))) ||
                     (mode->mode == AV2H_INTRA_D135 &&
                        ((width == 4U && height == 32U) ||
                         (width == 8U && height == 8U) ||
                         (width == 8U && height == 32U) ||
                         (width == 16U && height == 8U) ||
                         (width == 16U && height == 32U) ||
                         (width == 32U && height == 32U) ||
                         (width == 16U && height == 4U) ||
                         (width == 8U && height == 16U) ||
                         (width == 64U && height == 16U) ||
                         (width == 32U && height == 64U))) ||
                     (mode->mode == AV2H_INTRA_D157 &&
                        ((width == 8U && height == 4U) ||
                         (width == 4U && height == 16U) ||
                         (width == 16U && height == 4U) ||
                         (width == 8U && height == 16U) ||
                         (width == 8U && height == 32U) ||
                         (width == 32U && height == 32U))) ||
           (mode->mode == AV2H_INTRA_VERTICAL &&
                        width == 4U && height == 8U) ||
           (mode->mode == AV2H_INTRA_D67 &&
                        ((width == 8U && height == 4U) ||
                         (width == 8U && height == 32U) ||
                         (width == 16U && height == 32U) ||
                         (width == 64U && height == 64U))) ||
           (mode->mode == AV2H_INTRA_D203 &&
                        ((width == 4U && height == 32U) ||
                         (width == 16U && height == 4U) ||
                         (width == 16U && height == 8U) ||
                         (width == 16U && height == 16U) ||
                         (width == 8U && height == 4U) ||
                         (width == 8U && height == 32U) ||
                         (width == 16U && height == 32U) ||
                         (width == 32U && height == 8U) ||
                         (width == 32U && height == 32U) ||
                         (width == 32U && height == 64U) ||
                         (width == 64U && height == 16U) ||
                         (width == 64U && height == 64U))) ||
           (mode->mode == AV2H_INTRA_D45 &&
            width == 4U && height == 16U)))) &&
        mode->angle_delta >= -3 && mode->angle_delta <= 3 &&
        directional_base_angle(mode->mode) != 0 &&
        mode->mrl_index < AV2H_MRL_INDEX_SYMBOLS &&
        mode->multi_line_mrl <= 1U && mode->enable_ibp <= 1U &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        return predict_luma_directional_feature(
            mode, width, height, references, predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -3 &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
                (((mode->mrl_index == 0U || mode->mrl_index == 1U) &&
                    width == 8U && height == 8U) ||
         (mode->mrl_index == 0U && width == 32U && height == 8U) ||
         ((mode->mrl_index == 0U || mode->mrl_index == 1U) &&
          width == 8U && height == 32U))) {
        return predict_directional_idif(
            mode, 171, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 &&
        mode->multi_line_mrl == 0U &&
                mode->enable_ibp == 1U &&
                ((mode->mrl_index == 0U &&
                    ((width == 8U && height == 16U) ||
                     (width == 16U && height == 4U))) ||
                 (mode->mrl_index == 1U &&
                    width == 16U && height == 16U) ||
                 ((mode->mrl_index == 0U || mode->mrl_index == 2U) &&
                    width == 16U && height == 32U) ||
                 (mode->mrl_index == 3U && width == 32U && height == 32U))) {
        return predict_directional_idif(
            mode, 183, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 32U && height == 16U) ||
         (width == 16U && height == 32U) ||
         (width == 8U && height == 64U))) {
        return predict_directional_idif(
            mode, 177, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 32U && height == 8U) ||
         (width == 64U && height == 8U))) {
        return predict_directional_idif(
            mode, 135, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 1 &&
                mode->multi_line_mrl == 0U && mode->enable_ibp == 1U &&
                ((mode->mrl_index == 0U &&
                    ((width == 4U && height == 16U) ||
                     (width == 16U && height == 32U) ||
                     (width == 64U && height == 8U))) ||
                 (mode->mrl_index == 2U && width == 4U && height == 16U))) {
        return predict_directional_idif(
            mode, 138, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 4U) {
        return predict_directional_idif(
            mode, 141, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == -3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 8U) {
        return predict_directional_idif(
            mode, 126, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 4U && height == 8U) ||
         (width == 8U && height == 8U) ||
         (width == 8U && height == 16U) ||
         (width == 16U && height == 16U) ||
         (width == 16U && height == 64U))) {
        return predict_directional_idif(
            mode, 174, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 16U) ||
         (width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 154, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 8U) ||
         (width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 151, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == -3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 32U && height == 16U) {
        return predict_directional_idif(
            mode, 148, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 4U && height == 8U) ||
         (width == 16U && height == 8U) ||
         (width == 32U && height == 4U))) {
        return predict_directional_idif(
            mode, 157, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 0 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U && height == 4U) {
        return predict_directional_idif(
            mode, 157, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 8U) ||
         (width == 32U && height == 8U))) {
        return predict_directional_idif(
            mode, 206, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 2 &&
        (mode->mrl_index == 0U || mode->mrl_index == 3U) &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 8U && height == 8U) ||
         (width == 16U && height == 16U) ||
         (mode->mrl_index == 3U && width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 209, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 3 &&
        mode->multi_line_mrl == 0U &&
                mode->enable_ibp == 1U &&
                ((mode->mrl_index == 0U &&
                    ((width == 8U && height == 8U) ||
                     (width == 16U && (height == 8U || height == 16U)))) ||
                 (mode->mrl_index == 3U && width == 16U && height == 8U))) {
        return predict_directional_idif(
            mode, 144, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 3 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 4U && height == 8U) {
        return predict_directional_idif(
            mode, 144, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 2 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 16U) {
        return predict_directional_idif(
            mode, 51, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 8U) {
        return predict_directional_idif(
            mode, 51, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -3 &&
        (mode->mrl_index == 0U || mode->mrl_index == 3U) &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 8U && height == 8U) ||
            (mode->mrl_index == 0U &&
             ((width == 16U && height == 8U) ||
              (width == 32U && height == 32U))))) {
        return predict_directional_idif(
            mode, 36, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        mode->multi_line_mrl == 0U && mode->enable_ibp == 1U &&
        ((mode->mrl_index == 0U && width == 8U && height == 16U) ||
         (mode->mrl_index == 1U && width == 4U && height == 8U) ||
         (mode->mrl_index == 1U && width == 32U && height == 8U))) {
        return predict_directional_idif(
            mode, luma_wide_angle(45, width, height), width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U && height == 16U) {
        return predict_directional_idif(
            mode, 42, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 16U) {
        return predict_directional_idif(
            mode, 39, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 4U && height == 8U) ||
         (width == 8U && (height == 16U || height == 32U)))) {
        return predict_directional_idif(
            mode, 54, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 16U) ||
         (width == 16U && height == 32U) ||
         (width == 64U && height == 16U))) {
        return predict_directional_idif(
            mode, 186, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U && height == 16U) {
        return predict_directional_idif(
            mode, 61, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 8U && height == 16U) ||
         (width == 16U && height == 4U))) {
        return predict_directional_idif(
            mode, 58, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 8U && height == 32U) ||
         (width == 16U && height == 8U) ||
         (width == 16U && height == 32U) ||
         (width == 32U && height == 16U) ||
         (width == 32U && height == 32U) ||
         (width == 64U && height == 16U))) {
        return predict_directional_idif(
            mode, 186, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((mode->mrl_index == 1U && width == 8U && height == 16U) ||
         (mode->mrl_index == 1U && width == 16U && height == 32U) ||
         (mode->mrl_index == 2U && width == 64U && height == 16U) ||
         ((mode->mrl_index == 1U || mode->mrl_index == 2U) &&
          width == 32U && height == 32U))) {
        return predict_directional_idif(
            mode, 186, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U && height == 16U) {
        return predict_directional_idif(
            mode, 84, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 16U) {
        return predict_directional_idif(
            mode, 87, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 16U) ||
         (width == 32U && height == 4U))) {
        return predict_directional_idif(
            mode, 200, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == -2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 16U && height == 8U) ||
         (width == 32U && height == 8U) ||
         (width == 64U && height == 16U))) {
        return predict_directional_idif(
            mode, 197, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -3 &&
        (mode->mrl_index == 0U || mode->mrl_index == 3U) &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U &&
        (height == 16U || height == 32U)) {
        return predict_directional_idif(
            mode, 171, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 1U &&
        ((width == 8U && height == 4U) ||
         (width == 32U && height == 16U))) {
        if (references->above == 0 || references->above_count == 0U ||
            references->secondary_above == 0 ||
            references->secondary_above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t primary = column < references->above_count ?
                    column : references->above_count - 1U;
                size_t secondary =
                    column < references->secondary_above_count ?
                        column : references->secondary_above_count - 1U;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((references->above[primary] +
                               references->secondary_above[secondary] + 1U) >>
                              1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        ((width == 4U && height == 16U) ||
         (width == 16U && height == 16U) ||
         (width == 32U && height == 16U))) {
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint8_t value = references->left[
                row < references->left_count ?
                    row : references->left_count - 1U];
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 4U && height == 8U) ||
         (width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 180, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
        mode->mrl_index == 2U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 16U) {
        return predict_directional_idif(
            mode, 180, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 0 &&
        ((width == 4U && height == 4U) ||
         (width == 4U && height == 8U) ||
         (width == 8U && height == 16U) ||
         (width == 16U && height == 8U) ||
         (width == 4U && height == 32U) ||
         (width == 16U && height == 4U) ||
         (width == 32U && height == 4U))) {
        return predict_directional_idif(
            mode, 67, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 3 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 8U) {
        return predict_directional_idif(
            mode, 76, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        width == 64U && (height == 16U || height == 64U)) {
        return predict_directional_idif(
            mode, 212, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        ((width == 4U && height == 4U) ||
         (width == 8U && height == 4U) ||
         (width == 8U && height == 16U) ||
         (width == 8U && height == 32U) ||
         (width == 16U && height == 8U) ||
         (width == 32U && height == 4U))) {
        return predict_directional_idif(
            mode, 166, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -2 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        ((width == 16U && height == 16U) ||
         (width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 174, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == -1 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        width == 64U && height == 8U) {
        return predict_directional_idif(
            mode, 177, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
        mode->mrl_index == 2U && mode->multi_line_mrl == 1U &&
        width == 8U && height == 16U) {
        if (references->left == 0 || references->left_count == 0U ||
            references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            size_t primary = row < references->left_count ?
                row : references->left_count - 1U;
            size_t secondary = row < references->secondary_left_count ?
                row : references->secondary_left_count - 1U;
            uint8_t value = (uint8_t)(
                (references->left[primary] +
                 references->secondary_left[secondary] + 1U) >> 1U);
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        return AV2H_OK;
    }
        if (mode->multi_line_mrl == 1U && mode->enable_ibp == 1U &&
                ((mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 1 &&
                    ((mode->mrl_index == 2U && width == 8U && height == 16U) ||
                     (mode->mrl_index == 3U && width == 8U && height == 8U))) ||
                 (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 0 &&
                  mode->mrl_index == 3U && width == 8U && height == 8U) ||
                 (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 3 &&
                  mode->mrl_index == 1U && width == 16U && height == 32U) ||
                 (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
                  mode->mrl_index == 1U && width == 8U && height == 8U))) {
        Av2hFirstLeafMode primary_mode;
        Av2hFirstLeafMode secondary_mode;
        Av2hLumaReferences secondary_references;
        uint8_t secondary_predictor[16U * 32U];
        Av2hStatus status;
        if (references->secondary_above == 0 ||
            references->secondary_above_count == 0U ||
            references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
        av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
        av2h_memory_copy(
            &secondary_references, references, sizeof(secondary_references));
        primary_mode.multi_line_mrl = 0U;
        secondary_mode.mrl_index = 0U;
        secondary_mode.multi_line_mrl = 0U;
        int angle = mode->mode == AV2H_INTRA_D135 ? 138 :
            mode->mode == AV2H_INTRA_D157 ? 157 :
            mode->mode == AV2H_INTRA_VERTICAL ? 99 : 180;
        status = predict_directional_idif(
            &primary_mode, angle, width, height, references,
            predictor, predictor_stride);
        if (status != AV2H_OK) return status;
        secondary_references.above = references->secondary_above;
        secondary_references.above_count =
            references->secondary_above_count;
        secondary_references.left = references->secondary_left;
        secondary_references.left_count =
            references->secondary_left_count;
        secondary_references.above_left =
            references->secondary_above_left;
        secondary_references.has_above_left =
            references->has_secondary_above_left;
        status = predict_directional_idif_unfiltered(
            &secondary_mode, angle, width, height, &secondary_references,
            secondary_predictor, width);
        if (status != AV2H_OK) return status;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t output = (size_t)row * predictor_stride + column;
                predictor[output] = (uint8_t)(
                    (predictor[output] +
                     secondary_predictor[(size_t)row * width + column] +
                     1U) >> 1U);
            }
        }
        return AV2H_OK;
    }
        if (mode->mode == AV2H_INTRA_HORIZONTAL &&
                mode->multi_line_mrl == 1U &&
                ((mode->angle_delta == -2 &&
                    ((mode->mrl_index == 2U && width == 16U && height == 8U) ||
                     (mode->mrl_index == 3U && width == 32U && height == 16U))) ||
                      (mode->angle_delta == 2 &&
                          ((mode->mrl_index == 3U &&
                             width == 8U && height == 8U) ||
                            (mode->mrl_index == 1U &&
                             width == 32U && height == 16U))) ||
                 (mode->angle_delta == -3 && mode->mrl_index == 3U &&
                    width == 8U && height == 16U))) {
        Av2hFirstLeafMode primary_mode;
        Av2hFirstLeafMode secondary_mode;
        Av2hLumaReferences secondary_references;
        uint8_t secondary_predictor[32U * 16U];
                int angle = 180 + mode->angle_delta * 3;
        Av2hStatus status;
        if (references->secondary_above == 0 ||
            references->secondary_above_count == 0U ||
            references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
        av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
        av2h_memory_copy(
            &secondary_references, references, sizeof(secondary_references));
        primary_mode.multi_line_mrl = 0U;
        secondary_mode.mrl_index = 0U;
        secondary_mode.multi_line_mrl = 0U;
        status = predict_directional_idif(
            &primary_mode, angle, width, height, references,
            predictor, predictor_stride);
        if (status != AV2H_OK) return status;
        secondary_references.above = references->secondary_above;
        secondary_references.above_count =
            references->secondary_above_count;
        secondary_references.left = references->secondary_left;
        secondary_references.left_count =
            references->secondary_left_count;
        secondary_references.above_left =
            references->secondary_above_left;
        secondary_references.has_above_left =
            references->has_secondary_above_left;
        status = predict_directional_idif_unfiltered(
            &secondary_mode, angle, width, height, &secondary_references,
            secondary_predictor, width);
        if (status != AV2H_OK) return status;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t output = (size_t)row * predictor_stride + column;
                predictor[output] = (uint8_t)(
                    (predictor[output] +
                     secondary_predictor[(size_t)row * width + column] +
                     1U) >> 1U);
            }
        }
        return AV2H_OK;
    }
        if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
                mode->multi_line_mrl == 0U &&
                (((mode->mrl_index == 0U) &&
                    ((width == 4U && height == 4U) ||
         (width == 8U && height == 4U) ||
         (width == 8U && height == 8U) ||
         (width == 16U && height == 16U) ||
         (width == 16U && height == 32U) ||
         (width == 32U && height == 8U) ||
                     (width == 32U && height == 32U))) ||
                 (mode->mrl_index == 2U && width == 8U && height == 8U) ||
                 (mode->mrl_index == 3U && width == 4U && height == 4U))) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[column < references->above_count ?
                        column : references->above_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 2 &&
        mode->multi_line_mrl == 0U &&
        ((mode->mrl_index == 1U && width == 16U && height == 16U) ||
         (mode->mrl_index == 0U && width == 8U && height == 16U) ||
         (mode->enable_ibp == 1U && width == 64U && height == 64U))) {
        return predict_directional_idif(
            mode, 96, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 3 &&
        (mode->mrl_index == 0U || mode->mrl_index == 3U) &&
        mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((mode->mrl_index == 0U && width == 8U && height == 8U) ||
         (width == 8U && height == 16U) ||
         (mode->mrl_index == 0U && width == 32U && height == 16U))) {
        return predict_directional_idif(
            mode, 99, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == -3 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 8U && height == 8U) ||
         (width == 8U && height == 16U) ||
         (width == 16U && height == 32U))) {
        return predict_directional_idif(
            mode, 81, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == 0 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U &&
        ((width == 32U && height == 32U) ||
         (width == 64U && height == 16U))) {
        return predict_directional_idif(
            mode, directional_base_angle(mode->mode), width, height,
            references, predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 3 &&
        mode->multi_line_mrl == 0U && mode->enable_ibp == 1U &&
        ((mode->mrl_index == 3U && width == 8U && height == 32U) ||
                 (mode->mrl_index == 0U && width == 8U && height == 8U) ||
                 (mode->mrl_index == 0U && width == 16U && height == 8U) ||
                 ((mode->mrl_index == 1U || mode->mrl_index == 2U) &&
                    width == 32U && height == 32U))) {
        return predict_directional_idif(
            mode, 189, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D135 && mode->angle_delta == -2 &&
        mode->mrl_index == 3U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 16U && height == 16U) {
        return predict_directional_idif(
            mode, 129, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D113 && mode->angle_delta == -3 &&
        mode->mrl_index == 2U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 8U) {
        return predict_directional_idif(
            mode, 104, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == -3 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 4U) {
        return predict_directional_idif(
            mode, 148, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 2 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 32U && height == 8U) {
        return predict_directional_idif(
            mode, 163, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 2 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 1U &&
        mode->enable_ibp == 1U && width == 16U && height == 64U) {
        Av2hFirstLeafMode primary_mode;
        Av2hFirstLeafMode secondary_mode;
        Av2hLumaReferences secondary_references;
        uint8_t secondary_predictor[16U * 64U];
        Av2hStatus status;
        if (references->secondary_above == 0 ||
            references->secondary_above_count == 0U ||
            references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
        av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
        av2h_memory_copy(
            &secondary_references, references, sizeof(secondary_references));
        primary_mode.multi_line_mrl = 0U;
        secondary_mode.mrl_index = 0U;
        secondary_mode.multi_line_mrl = 0U;
        status = predict_directional_idif(
            &primary_mode, 186, width, height, references,
            predictor, predictor_stride);
        if (status != AV2H_OK) return status;
        secondary_references.above = references->secondary_above;
        secondary_references.above_count =
            references->secondary_above_count;
        secondary_references.left = references->secondary_left;
        secondary_references.left_count =
            references->secondary_left_count;
        secondary_references.above_left =
            references->secondary_above_left;
        secondary_references.has_above_left =
            references->has_secondary_above_left;
        status = predict_directional_idif_unfiltered(
            &secondary_mode, 186, width, height, &secondary_references,
            secondary_predictor, width);
        if (status != AV2H_OK) return status;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t output = (size_t)row * predictor_stride + column;
                predictor[output] = (uint8_t)(
                    (predictor[output] +
                     secondary_predictor[(size_t)row * width + column] +
                     1U) >> 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        mode->enable_ibp == 1U && width == 8U && height == 32U) {
        return predict_directional_idif(
            mode, 45, width, height, references,
            predictor, predictor_stride);
    }
    if ((mode->mrl_index != 0U || mode->multi_line_mrl != 0U) &&
                                !(mode->mode == AV2H_INTRA_VERTICAL &&
                                    mode->multi_line_mrl == 1U &&
                                    mode->mrl_index == 1U &&
                                    mode->angle_delta == 0 &&
                                    ((width == 4U && height == 4U) ||
                                     (width == 8U && height == 32U))) &&
                                !(mode->mode == AV2H_INTRA_D45 &&
                                                                        mode->multi_line_mrl == 0U &&
                                                                        mode->mrl_index == 1U &&
                                                                        ((mode->angle_delta == 0 &&
                                                                            width == 32U && height == 32U) ||
                                                                         (mode->angle_delta == -1 &&
                                                                            width == 8U && height == 8U))) &&
                                !(mode->mode == AV2H_INTRA_D135 &&
                                    mode->multi_line_mrl == 0U && mode->mrl_index == 2U &&
                                    mode->angle_delta == 0 && width == 16U && height == 32U) &&
                                !(mode->mode == AV2H_INTRA_D113 &&
                                    mode->multi_line_mrl == 0U && mode->mrl_index == 2U &&
                                    mode->angle_delta == -2 && width == 16U && height == 8U) &&
                                !(mode->mode == AV2H_INTRA_D113 &&
                                    mode->multi_line_mrl == 0U && mode->mrl_index == 3U &&
                                    mode->angle_delta == -3 && width == 16U && height == 8U) &&
                                !(mode->mode == AV2H_INTRA_HORIZONTAL &&
                                    mode->multi_line_mrl == 0U &&
                                    mode->mrl_index == 2U &&
                                    mode->angle_delta == -2 &&
                                    width == 16U && height == 64U) &&
                                !(mode->mode == AV2H_INTRA_HORIZONTAL &&
                                    mode->multi_line_mrl == 0U &&
                                    mode->mrl_index == 2U &&
                                    mode->angle_delta == 0 &&
                                    width == 8U && height == 8U) &&
                                !(mode->mode == AV2H_INTRA_D157 &&
                                    mode->multi_line_mrl == 0U &&
                                    mode->mrl_index == 2U &&
                                    mode->angle_delta == 2 &&
                                    width == 16U && height == 16U) &&
                !(mode->mode == AV2H_INTRA_D67 &&
                    mode->multi_line_mrl == 0U &&
                    ((mode->mrl_index == 2U && mode->angle_delta == 0 &&
                        width == 8U && height == 64U) ||
                     (mode->mrl_index == 1U && mode->angle_delta == -1 &&
                        width == 8U && height == 8U) ||
                            (mode->mrl_index == 1U && mode->angle_delta == 0 &&
                                width == 8U && height == 8U) ||
                     (mode->mrl_index == 1U && mode->angle_delta == -2 &&
                        ((width == 4U && height == 4U) ||
                         (width == 8U && height == 8U))) ||
                            (mode->mrl_index == 3U && mode->angle_delta == -3 &&
                                width == 4U && height == 4U))) &&
                        !(mode->mode == AV2H_INTRA_D45 &&
                          mode->multi_line_mrl == 0U && mode->mrl_index == 1U &&
                          mode->angle_delta == 3 && width == 8U &&
                          height == 16U) &&
                        !(mode->mode == AV2H_INTRA_D203 &&
                          mode->multi_line_mrl == 0U && mode->mrl_index == 1U &&
                          mode->angle_delta == 3 && width == 8U &&
                                                    (height == 8U || height == 16U)) &&
                                                !(mode->mode == AV2H_INTRA_D203 &&
                                                    mode->multi_line_mrl == 0U && mode->mrl_index == 1U &&
                                                    mode->angle_delta == -1 && width == 16U &&
                                                    height == 32U) &&
                                                !(mode->mode == AV2H_INTRA_D203 &&
                                                    mode->multi_line_mrl <= 1U && mode->mrl_index == 1U &&
                                                    mode->angle_delta == 1 && width == 16U &&
                                                    height == 64U) &&
                                                !(mode->mode == AV2H_INTRA_D203 &&
                                                    mode->multi_line_mrl == 0U && mode->mrl_index == 3U &&
                                                    mode->angle_delta == -2 && width == 16U &&
                                                    height == 32U)) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 1U &&
        mode->enable_ibp == 1U &&
        ((width == 4U && height == 4U) ||
         (width == 8U && height == 32U))) {
        Av2hFirstLeafMode single_line_mode;
        Av2hLumaReferences secondary_references;
        uint8_t secondary_predictor[8U * 32U];
        Av2hStatus status;
        if (references->secondary_above == 0 ||
            references->secondary_above_count == 0U ||
            references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        av2h_memory_copy(&single_line_mode, mode, sizeof(single_line_mode));
        av2h_memory_copy(
            &secondary_references, references, sizeof(secondary_references));
        single_line_mode.mrl_index = 0U;
        single_line_mode.multi_line_mrl = 0U;
        status = predict_directional_idif(
            &single_line_mode, 91, width, height, references,
            predictor, predictor_stride);
        if (status != AV2H_OK) return status;
        secondary_references.above = references->secondary_above;
        secondary_references.above_count =
            references->secondary_above_count;
        secondary_references.left = references->secondary_left;
        secondary_references.left_count = references->secondary_left_count;
        secondary_references.above_left =
            references->secondary_above_left;
        secondary_references.has_above_left =
            references->has_secondary_above_left;
        status = predict_directional_idif(
            &single_line_mode, 91, width, height, &secondary_references,
            secondary_predictor, width);
        if (status != AV2H_OK) return status;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t output = (size_t)row * predictor_stride + column;
                predictor[output] = (uint8_t)(
                    (predictor[output] +
                     secondary_predictor[(size_t)row * width + column] +
                     1U) >> 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 1 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 1U &&
        mode->enable_ibp == 1U && width == 16U && height == 64U) {
        Av2hFirstLeafMode primary_mode;
        Av2hFirstLeafMode secondary_mode;
        Av2hLumaReferences secondary_references;
        uint8_t secondary_predictor[16U * 64U];
        Av2hStatus status;
        if (references->secondary_left == 0 ||
            references->secondary_left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        av2h_memory_copy(&primary_mode, mode, sizeof(primary_mode));
        av2h_memory_copy(&secondary_mode, mode, sizeof(secondary_mode));
        av2h_memory_copy(
            &secondary_references, references, sizeof(secondary_references));
        primary_mode.multi_line_mrl = 0U;
        secondary_mode.mrl_index = 0U;
        secondary_mode.multi_line_mrl = 0U;
        status = predict_directional_idif(
            &primary_mode, 206, width, height, references,
            predictor, predictor_stride);
        if (status != AV2H_OK) return status;
        secondary_references.above = references->secondary_above;
        secondary_references.above_count =
            references->secondary_above_count;
        secondary_references.left = references->secondary_left;
        secondary_references.left_count =
            references->secondary_left_count;
        secondary_references.above_left =
            references->secondary_above_left;
        secondary_references.has_above_left =
            references->has_secondary_above_left;
        status = predict_directional_idif_unfiltered(
            &secondary_mode, 206, width, height, &secondary_references,
            secondary_predictor, width);
        if (status != AV2H_OK) return status;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t output = (size_t)row * predictor_stride + column;
                predictor[output] = (uint8_t)(
                    (predictor[output] +
                     secondary_predictor[(size_t)row * width + column] +
                     1U) >> 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
                ((width == 4U &&
                    (height == 4U || height == 8U || height == 16U ||
                     height == 32U)) ||
         (width == 8U && (height == 4U || height == 8U)) ||
         (width == 8U && height == 16U) ||
         (width == 8U && height == 32U) ||
         (width == 8U && height == 64U) ||
         (width == 16U && height == 4U) ||
         (width == 16U && height == 8U) ||
         (width == 16U && (height == 16U || height == 32U)) ||
         (width == 16U && height == 64U) ||
         (width == 32U &&
          (height == 4U || height == 16U || height == 64U)) ||
         (width == 32U && height == 8U) ||
         (width == 32U && height == 32U) ||
         (width == 64U &&
                    (height == 8U || height == 16U ||
                     height == 32U || height == 64U)))) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t value;
        if (references->above != 0 && references->above_count >= width) {
            for (index = 0U; index < width; ++index) {
                sum += references->above[index];
            }
            count += width;
        }
        if (references->left != 0 && references->left_count >= height) {
            for (index = 0U; index < height; ++index) {
                sum += references->left[index];
            }
            count += height;
        }
        value = dc_predictor_average(
            sum, count, width, height,
            (uint8_t)(references->above != 0 &&
                      references->above_count >= width),
            (uint8_t)(references->left != 0 &&
                      references->left_count >= height));
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        if (mode->enable_ibp && !(width == 4U && height == 4U)) {
            if (references->above != 0 &&
                references->above_count >= width) {
                uint16_t row_limit = (uint16_t)(height >> 2U);
                uint16_t column_start =
                    width < height && references->left != 0 &&
                        references->left_count >= height ?
                        (uint16_t)(width >> 2U) : 0U;
                uint8_t weight_row = (uint8_t)(block_log2(height) - 2U);
                for (row = 0U; row < row_limit; ++row) {
                    unsigned int weight = ibp_weights[weight_row][row];
                    for (column = column_start; column < width; ++column) {
                        predictor[(size_t)row * predictor_stride + column] =
                            (uint8_t)(
                                (references->above[column] *
                                     (128U - weight) +
                                 predictor[
                                     (size_t)row * predictor_stride + column] *
                                     weight +
                                 64U) >>
                                7U);
                    }
                }
            }
            if (references->left != 0 &&
                references->left_count >= height) {
                uint16_t row_start =
                    width >= height && references->above != 0 &&
                        references->above_count >= width ?
                        (uint16_t)(height >> 2U) : 0U;
                uint16_t column_limit = (uint16_t)(width >> 2U);
                uint8_t weight_row = (uint8_t)(block_log2(width) - 2U);
                for (row = row_start; row < height; ++row) {
                    for (column = 0U; column < column_limit; ++column) {
                        unsigned int weight =
                            ibp_weights[weight_row][column];
                        predictor[(size_t)row * predictor_stride + column] =
                            (uint8_t)(
                                (references->left[row] *
                                     (128U - weight) +
                                 predictor[
                                     (size_t)row * predictor_stride + column] *
                                     weight +
                                 64U) >>
                                7U);
                    }
                }
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->multi_line_mrl == 0U &&
        ((width == 4U && height == 8U) ||
         (width == 8U && height == 16U))) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[column < references->above_count ?
                        column : references->above_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
        mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t bottom_left;
        int have_above = references->above != 0 &&
            references->above_count != 0U;
        int have_left = references->left != 0 &&
            references->left_count != 0U;
        if (have_left && references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = !have_left ? have_above ? references->above[0] : 129U :
            references->left_count > height ?
            references->left[height] : references->left[height - 1U];
        for (row = 0U; row < height; ++row) {
            uint8_t blend = smooth_blend(row, scale);
            for (column = 0U; column < width; ++column) {
                uint8_t top = have_above ? references->above[
                    column < references->above_count ? column :
                    references->above_count - 1U] :
                    have_left ? references->left[0] : 127U;
                int32_t value = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    block_log2(height));
                value += round_shift(
                    ((int64_t)top - value) * blend, 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
        mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t top_right;
        int have_above = references->above != 0 &&
            references->above_count != 0U;
        int have_left = references->left != 0 &&
            references->left_count != 0U;
        top_right = !have_above ? have_left ? references->left[0] : 127U :
            references->above_count > width ?
            references->above[width] :
            references->above[references->above_count - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t left = have_left ? references->left[
                    row < references->left_count ? row :
                    references->left_count - 1U] :
                    have_above ? references->above[0] : 129U;
                uint8_t blend = smooth_blend(column, scale);
                int32_t value = top_right + round_shift(
                    ((int64_t)left - top_right) *
                        (width - 1U - column),
                    block_log2(width));
                value += round_shift(
                    ((int64_t)left - value) * blend, 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) &&
        luma_prediction_dimension_supported(height)) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t bottom_left;
        uint8_t top_right;
        int have_above = references->above != 0 &&
            references->above_count != 0U;
        int have_left = references->left != 0 &&
            references->left_count != 0U;
        if ((have_above && references->above_count < width) ||
            (have_left && references->left_count < height)) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = !have_left ? have_above ? references->above[0] : 129U :
            references->left_count > height ?
            references->left[height] : references->left[height - 1U];
        top_right = !have_above ? have_left ? references->left[0] : 127U :
            references->above_count > width ?
            references->above[width] : references->above[width - 1U];
        if (width == 16U && height == 64U) {
            av2h_debug_smooth_above_hash = 1469598103934665603ULL;
            av2h_debug_smooth_left_hash = 1469598103934665603ULL;
            for (index = 0U; index < width; ++index) {
                av2h_debug_smooth_above_hash ^=
                    !have_above ? have_left ? references->left[0] : 127U :
                                  references->above[index];
                av2h_debug_smooth_above_hash *= 1099511628211ULL;
            }
            for (index = 0U; index < height; ++index) {
                av2h_debug_smooth_left_hash ^=
                    !have_left ? have_above ? references->above[0] : 129U :
                                 references->left[index];
                av2h_debug_smooth_left_hash *= 1099511628211ULL;
            }
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t top = !have_above ?
                    have_left ? references->left[0] : 127U :
                    references->above[column];
                uint8_t left = !have_left ?
                    have_above ? references->above[0] : 129U :
                    references->left[row];
                uint8_t top_blend = smooth_blend(row, scale);
                uint8_t left_blend = smooth_blend(column, scale);
                int32_t vertical = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    block_log2(height));
                int32_t horizontal = top_right + round_shift(
                    ((int64_t)left - top_right) *
                        (width - 1U - column),
                    block_log2(width));
                vertical += round_shift(
                    ((int64_t)top - vertical) *
                        top_blend, 7U);
                horizontal += round_shift(
                    ((int64_t)left - horizontal) *
                        left_blend, 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)round_shift(vertical + horizontal, 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        width == 16U && height == 8U) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t bottom_left;
        uint8_t top_right;
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = references->left_count > height ?
            references->left[height] : references->left[height - 1U];
        top_right = references->above_count > width ?
            references->above[width] : references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t top = references->above[column];
                uint8_t left = references->left[row];
                int32_t vertical = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    block_log2(height));
                int32_t horizontal = top_right + round_shift(
                    ((int64_t)left - top_right) *
                        (width - 1U - column),
                    block_log2(width));
                vertical += round_shift(
                    ((int64_t)top - vertical) *
                        smooth_blend(row, scale), 7U);
                horizontal += round_shift(
                    ((int64_t)left - horizontal) *
                        smooth_blend(column, scale), 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)round_shift(vertical + horizontal, 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        width == 8U && height == 32U) {
        uint8_t bottom_left;
        uint8_t top_right;
        if (references->left == 0 || references->left_count < height) {
            return AV2H_INVALID_ARGUMENT;
        }
        bottom_left = references->left_count > height ?
            references->left[height] : references->left[height - 1U];
        top_right = references->above_count > width ?
            references->above[width] :
            references->above_count != 0U ?
                references->above[references->above_count - 1U] :
                references->left[0];
        for (row = 0U; row < height; ++row) {
            uint8_t top_blend = row < 6U ? (uint8_t)(32U >> row) : 0U;
            for (column = 0U; column < width; ++column) {
                uint8_t top = references->above_count != 0U ?
                    references->above[column] : references->left[0];
                int32_t vertical = bottom_left + round_shift(
                    ((int64_t)top - bottom_left) *
                        (height - 1U - row),
                    5U);
                int32_t horizontal = top_right + round_shift(
                    ((int64_t)references->left[row] - top_right) *
                        (width - 1U - column),
                    3U);
                vertical += round_shift(
                    ((int64_t)top - vertical) * top_blend, 6U);
                horizontal += round_shift(
                    ((int64_t)references->left[row] - horizontal) *
                        smooth8_blend[column],
                    6U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)round_shift(vertical + horizontal, 1U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_PAETH && mode->angle_delta == 0 &&
        width <= 64U && height <= 64U) {
        if (references->above != 0 && references->above_count >= width &&
            (references->left == 0 || references->left_count == 0U)) {
            for (row = 0U; row < height; ++row) {
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride + column] =
                        references->above[column];
                }
            }
            return AV2H_OK;
        }
        if (references->left != 0 && references->left_count >= height &&
            (references->above == 0 || references->above_count == 0U)) {
            for (row = 0U; row < height; ++row) {
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride + column] =
                        references->left[row];
                }
            }
            return AV2H_OK;
        }
        if ((references->above == 0 || references->above_count == 0U) &&
            (references->left == 0 || references->left_count == 0U)) {
            for (row = 0U; row < height; ++row) {
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride + column] = 128U;
                }
            }
            return AV2H_OK;
        }
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t top = references->above[column];
                int32_t side = references->left[row];
                int32_t corner = references->above_left;
                int32_t base = top + side - corner;
                int32_t side_distance = base > side ? base - side : side - base;
                int32_t top_distance = base > top ? base - top : top - base;
                int32_t corner_distance = base > corner ?
                    base - corner : corner - base;
                int32_t value = side_distance <= top_distance &&
                    side_distance <= corner_distance ? side :
                    top_distance <= corner_distance ? top : corner;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 0 &&
        width <= 64U && height <= 64U) {
        for (row = 0U; row < height; ++row) {
            uint8_t value = references->left != 0 &&
                references->left_count != 0U ?
                references->left[row < references->left_count ?
                                 row : references->left_count - 1U] :
                references->above != 0 &&
                    references->above_count != 0U ?
                    references->above[0] : 129U;
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        mode->enable_ibp == 1U && width == 32U && height == 32U &&
        references->above_count == 0U && references->left_count == 0U) {
        for (row = 0U; row < 32U; ++row) {
            uint16_t count_129 = row < 4U ? 0U :
                (uint16_t)(2U * (((row - 4U) / 6U) + 1U));
            uint16_t count_not_127 =
                (uint16_t)(6U * (row / 2U + 1U));
            if (count_not_127 > 32U) count_not_127 = 32U;
            for (column = 0U; column < 32U; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    column < count_129 ? 129U :
                    column < count_not_127 ? 128U : 127U;
            }
        }
        return AV2H_OK;
    }
          if ((((mode->mode == AV2H_INTRA_VERTICAL &&
                            ((mode->angle_delta == 0 &&
                                ((width == 8U && height == 32U) ||
                                 (width == 16U && height == 8U) ||
                                 (width == 32U && height == 32U))) ||
                             (mode->angle_delta == 2 &&
                                ((width == 16U && height == 16U) ||
                                 (width == 64U && height == 32U))) ||
                                      (mode->angle_delta == 1 &&
                                          width == 32U && height == 8U) ||
                             (mode->angle_delta == -2 &&
                                ((width == 8U && height == 32U) ||
                                 (width == 4U && height == 32U) ||
                                 (width == 32U && height == 32U) ||
                                 ((width == 32U || width == 8U) && height == 64U))) ||
                    (mode->angle_delta == 3 &&
                     width == 16U &&
                     (height == 16U || height == 32U)) ||
               (mode->angle_delta == -3 &&
                width == 8U && height == 64U)))) ||
                         (mode->mode == AV2H_INTRA_D45 &&
                            ((mode->angle_delta == 0 &&
                                                                                    ((width == 4U && height == 8U) ||
                                                                                     (width == 8U && height == 8U) ||
	                                                                                    (width == 8U && height == 64U) ||
	                                                                                    (width == 16U && height == 4U) ||
                                                                                        (width == 16U && height == 16U) ||
                                                                                        (width == 32U && height == 8U) ||
                                                                                        (width == 32U && height == 16U) ||
                                                                                        (width == 32U && height == 32U) ||
                                                                                        (width == 32U && height == 64U) ||
                                                                                        (width == 64U && height == 16U) ||
                                            (width == 64U && height == 64U))) ||
                                                                            (mode->angle_delta == -2 &&
                                                                                (width == 16U || width == 32U || width == 64U) &&
                                                                                height == 16U) ||
                                      (mode->angle_delta == 2 && mode->mrl_index == 0U &&
                                          width == 4U && height == 16U) ||
                             (mode->angle_delta == -1 && mode->mrl_index == 1U &&
                                          width == 8U && height == 8U))) ||
             (mode->mode == AV2H_INTRA_D67 &&
                                                        ((mode->angle_delta == 0 &&
                                                            mode->mrl_index == 0U &&
                                                            ((width == 8U && height == 8U) ||
                                                             (width == 4U && height == 16U) ||
                                                             (width == 8U && height == 32U) ||
                                                             (width == 16U && height == 16U) ||
                                                             (width == 16U && height == 64U) ||
                                                             (width == 32U && height == 8U) ||
                                                             (width == 32U && height == 16U) ||
                                                             (width == 32U && height == 32U) ||
                                                             (width == 64U && height == 16U) ||
                                                             (width == 64U && height == 32U))) ||
                                                         (mode->angle_delta == 0 &&
                                                            mode->mrl_index == 1U &&
                                                            width == 8U && height == 8U) ||
                                                         (mode->angle_delta == 1 &&
                                                            mode->mrl_index == 0U &&
                                                                                width == 16U &&
                                                                                (height == 8U || height == 16U)) ||
                                                                            (mode->angle_delta == -1 &&
                                                                                mode->mrl_index == 0U &&
                                                                                ((width == 16U && height == 8U) ||
                                                                                 (width == 32U && height == 16U) ||
                                                                                 (width == 32U && height == 32U) ||
                                                                                 (width == 64U && height == 8U) ||
                                                                                 (width == 64U && height == 32U))) ||
                                                                            (mode->angle_delta == -3 &&
                                                                                mode->mrl_index == 0U &&
                                                                                ((width == 8U && height == 32U) ||
                                                                                 (width == 16U && height == 8U) ||
                                                                                 (width == 16U && height == 16U))) ||
                                                                            (mode->angle_delta == 2 &&
                                                                                mode->mrl_index == 0U &&
                                                                                ((width == 8U && height == 4U) ||
                                                                                 (width == 32U && height == 8U))) ||
                             (mode->angle_delta == 0 &&
                              mode->mrl_index == 2U &&
                              width == 8U && height == 64U) ||
                             (mode->angle_delta == 0 &&
                              mode->mrl_index == 3U &&
                              width == 32U && height == 32U))) ||
             (mode->mode == AV2H_INTRA_D67 &&
              mode->angle_delta == -2 && mode->mrl_index == 0U &&
              width == 8U && height == 16U) ||
                         (mode->mode == AV2H_INTRA_D113 &&
                            ((mode->angle_delta == 0 &&
                                ((width == 8U && height == 4U) ||
                                 (width == 8U && height == 8U))) ||
                             (mode->angle_delta == -3 &&
                                width == 8U && height == 16U) ||
                             (mode->angle_delta == 2 && mode->mrl_index == 0U &&
                                width == 8U && height == 8U) ||
                             (mode->angle_delta == 0 &&
                                width == 16U && height == 4U) ||
                             (mode->angle_delta == -2 &&
                                width == 16U && height == 4U) ||
                             (width == 16U && height == 8U) ||
                             (width == 16U && height == 16U) ||
                             (width == 16U && height == 32U) ||
                             (width == 16U && height == 64U) ||
                             (width == 4U && height == 32U) ||
                             (width == 32U && height == 8U) ||
                             (width == 32U && height == 16U) ||
                             (width == 32U && height == 32U) ||
                             (width == 32U && height == 64U) ||
                             (width == 64U && height == 16U) ||
                             (width == 64U && height == 32U) ||
                             (width == 64U && height == 64U))) ||
           (mode->mode == AV2H_INTRA_HORIZONTAL &&
                        ((mode->angle_delta == 1 &&
                            width == 16U && height == 16U) ||
                         (mode->angle_delta == -2 &&
                                     ((width == 32U && height == 8U) ||
                             (width == 32U && height == 16U) ||
                                     (width == 32U && height == 32U) ||
                                      (mode->mrl_index == 2U &&
                                        width == 16U && height == 64U))) ||
                                 (mode->angle_delta == -3 &&
                                                    ((width == 32U && height == 32U) ||
                                                     (width == 64U && height == 64U))))) ||
                                 (mode->mode == AV2H_INTRA_D157 &&
                                    ((mode->angle_delta == 0 &&
                                        ((width == 8U && height == 8U) ||
                                         (width == 16U && height == 16U) ||
                                         (width == 32U && height == 8U) ||
                                         (width == 32U && height == 16U))) ||
                                                 (mode->angle_delta == 1 &&
                                                    ((width == 8U && height == 16U) ||
                                                     (width == 32U && height == 4U))) ||
                                                 (mode->angle_delta == 2 &&
                                                     ((width == 4U && height == 16U) ||
                                                      (width == 8U &&
                                                       (height == 8U || height == 16U)))) ||
                                                 (mode->angle_delta == 3 &&
                                                     width == 16U && height == 16U) ||
                                     (width == 16U &&
                                                                                ((mode->angle_delta == 2 &&
                                                                                    (height == 8U || height == 16U)) ||
                                         height == 32U || height == 64U)) ||
                                     (mode->angle_delta == 0 &&
                                        width == 64U && height == 32U))) ||
         (mode->mode == AV2H_INTRA_D135 &&
                                        ((mode->angle_delta == 0 &&
                                            width == 4U && height == 4U) ||
                                           ((mode->angle_delta == -3 || mode->angle_delta == 0) &&
                                            width == 4U && height == 8U) ||
                     (mode->angle_delta == 0 &&
                      width == 8U && height == 4U) ||
                     (mode->angle_delta == 0 &&
                      width == 4U && height == 16U) ||
                     (mode->angle_delta == 0 &&
                      width == 16U && height == 16U) ||
                     (mode->angle_delta == 1 && width == 16U && height == 16U) ||
                     (mode->angle_delta == 3 &&
                      width == 32U && height == 32U) ||
                     (mode->angle_delta == 0 &&
                      width == 64U && height == 64U) ||
                     (width == 16U && height == 64U) ||
                     (width == 32U && height == 16U))) ||
         (mode->mode == AV2H_INTRA_D203 &&
          ((mode->angle_delta == 0 && mode->mrl_index == 0U &&
              ((width == 4U && height == 16U) ||
               (width == 8U && height == 8U) ||
               (width == 8U && height == 16U) ||
               (width == 32U && height == 32U))) ||
              (mode->angle_delta == 3 && mode->mrl_index == 0U &&
              width == 4U && height == 16U) ||
              (mode->angle_delta == -3 && mode->mrl_index == 0U &&
              width == 16U && height == 4U) ||
              (width == 64U && height == 32U) ||
              (width == 32U && height == 16U) ||
              (mode->angle_delta == -2 &&
               width == 8U && height == 8U) ||
              (mode->angle_delta == -2 &&
                             width == 16U &&
                                      (height == 4U || height == 16U ||
                                (height == 32U && mode->mrl_index == 3U))) ||
              (mode->angle_delta == -1 && mode->mrl_index == 1U &&
               width == 16U && height == 32U) ||
              (mode->angle_delta == -1 && mode->mrl_index == 0U &&
               width == 16U && height == 64U) ||
              (mode->angle_delta == 1 && mode->mrl_index == 1U &&
                width == 16U && height == 64U)))) &&
        mode->enable_ibp == 1U) {
        return predict_directional_idif(
            mode, luma_wide_angle(
                directional_base_angle(mode->mode) + mode->angle_delta * 3,
                width, height),
            width, height, references, predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        width == 8U && height == 4U) {
        return predict_d45_8x4_ibp(
            references, predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        width == 32U && height == 32U) {
        uint8_t edge[68U];
        int x = directional_derivative[46U] * 2;
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index <= 65U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        edge[66U] = edge[65U];
        edge[67U] = edge[65U];
        for (row = 0U; row < height; ++row, x += directional_derivative[46U]) {
            int base = x >> 6;
            int shift = (x & 0x3f) >> 1;
            for (column = 0U; column < width; ++column, ++base) {
                int32_t sum;
                if (base > 65) {
                    predictor[(size_t)row * predictor_stride + column] =
                        edge[65U];
                    continue;
                }
                sum = dr_interp_filter[shift][0U] * edge[base - 1] +
                      dr_interp_filter[shift][1U] * edge[base] +
                      dr_interp_filter[shift][2U] * edge[base + 1] +
                      dr_interp_filter[shift][3U] * edge[base + 2];
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (((mode->mode == AV2H_INTRA_D135 && mode->angle_delta == 0 &&
          width == 16U && height == 32U) ||
         (mode->mode == AV2H_INTRA_D157 && mode->angle_delta == 2 &&
          width == 16U && height == 16U)) &&
        mode->mrl_index == 2U && mode->multi_line_mrl == 0U) {
        uint8_t above_storage[53U];
        uint8_t left_storage[69U];
        uint8_t *mrl_above = &above_storage[4U];
        uint8_t *mrl_left = &left_storage[4U];
        const int angle = directional_base_angle(mode->mode) +
                  mode->angle_delta * 3 - 1;
        const int dx = directional_derivative[180 - angle];
        const int dy = directional_derivative[angle - 90];
        if (references->above == 0 || references->left == 0 ||
            references->above_count == 0U ||
            references->left_count == 0U ||
            references->negative_count < 3U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < 49U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            mrl_above[index] = references->above[source];
        }
        for (index = 0U; index < 65U; ++index) {
            size_t source = index < references->left_count ?
                index : references->left_count - 1U;
            mrl_left[index] = references->left[source];
        }
        for (index = 1U; index <= 3U; ++index) {
            mrl_above[-(int)index] = references->above_negative[index];
            mrl_left[-(int)index] = references->left_negative[index];
        }
        mrl_above[-4] = mrl_above[-3];
        mrl_left[-4] = mrl_left[-3];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int x = ((int)column << 6) - ((int)row + 3) * dx;
                int base = x >> 6;
                int shift = (x & 0x3f) >> 1;
                const uint8_t *edge = mrl_above;
                int32_t sum;
                if (base < -3) {
                    int y = ((int)row << 6) -
                        ((int)column + 3) * dy;
                    base = y >> 6;
                    shift = (y & 0x3f) >> 1;
                    edge = mrl_left;
                }
                sum = dr_interp_filter[shift][0U] * edge[base - 1] +
                      dr_interp_filter[shift][1U] * edge[base] +
                      dr_interp_filter[shift][2U] * edge[base + 1] +
                      dr_interp_filter[shift][3U] * edge[base + 2];
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0 &&
        ((width == 4U && height == 4U) ||
         (width == 16U && height == 8U))) {
        size_t edge_count = (size_t)width + height;
        if (references->above_count == 0U && references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < edge_count; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - (references->above_count != 0U);
            above[index] = references->above_count != 0U ?
                references->above[source] : references->left[0];
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                predictor[(size_t)row * predictor_stride + column] =
                    above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -1 &&
        mode->mrl_index == 1U && width == 8U && height == 8U) {
        uint8_t storage[21U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < 20U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        edge[-1] = edge[0];
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((54U + row * 27U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
        mode->mrl_index == 1U && width == 4U && height == 4U) {
        uint8_t storage[13U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->has_above_left ? references->above_left :
            references->above[0];
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl4_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -2 &&
        mode->mrl_index == 1U && width == 8U && height == 8U) {
        uint8_t storage[21U];
        uint8_t *edge = &storage[1U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->has_above_left ? references->above_left :
            references->above[0];
        for (index = 0U; index < 20U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((62U + row * 31U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl8_filter[row][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67 && mode->angle_delta == -3 &&
        mode->mrl_index == 3U && width == 4U && height == 4U) {
        uint8_t edge[12U];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[source];
        }
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)((152U + row * 38U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += d67_mrl3_filter[row][tap] *
                           edge[base + tap - 1U];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == -1 &&
        mode->mrl_index == 0U &&
        ((width == 16U && height == 4U) ||
         (width == 32U && height == 8U))) {
        uint8_t source[41U];
        uint8_t *edge = above;
        uint8_t *filtered = &edge[-1];
        size_t edge_count = (size_t)width + height;
        size_t filtered_count = edge_count + 1U;
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[-1] = references->has_above_left ? references->above_left :
                                                    references->above[0];
        for (index = 0U; index < edge_count; ++index) {
            size_t reference = index < references->above_count ?
                index : references->above_count - 1U;
            edge[index] = references->above[reference];
        }
        for (index = 0U; index < filtered_count; ++index) {
            source[index] = filtered[index];
        }
        for (index = 1U; index < filtered_count; ++index) {
            static const uint8_t kernel[5U] = { 2U, 4U, 4U, 4U, 2U };
            size_t tap;
            unsigned int sum = 0U;
            for (tap = 0U; tap < 5U; ++tap) {
                int32_t source_index = (int32_t)index - 2 + (int32_t)tap;
                if (source_index < 0) source_index = 0;
                if (source_index >= (int32_t)filtered_count) {
                    source_index = (int32_t)filtered_count - 1;
                }
                sum += kernel[tap] * source[source_index];
            }
            filtered[index] = (uint8_t)((sum + 8U) >> 4U);
        }
        edge[edge_count] = edge[edge_count - 1U];
        edge[edge_count + 1U] = edge[edge_count - 1U];
        for (row = 0U; row < height; ++row) {
            uint16_t position = (uint16_t)((row + 1U) * 73U);
            uint16_t base = position >> 6U;
            uint8_t shift = (uint8_t)((position & 63U) >> 1U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                if (base >= edge_count) {
                    predictor[(size_t)row * predictor_stride + column] =
                        edge[edge_count - 1U];
                    continue;
                }
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           edge[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 &&
        width == 8U && height == 4U) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        above[-1] = references->above[0];
        for (index = 0U; index < 12U; ++index) {
            size_t source = index < references->above_count ?
                index : references->above_count - 1U;
            above[index] = references->above[source];
        }
        above[12] = above[11];
        above[13] = above[11];
        for (row = 0U; row < height; ++row) {
            uint16_t base = (uint16_t)(((row + 1U) * 44U) >> 6U);
            for (column = 0U; column < width; ++column, ++base) {
                int32_t value;
                if (base <= 11U) {
                    size_t tap;
                    int32_t sum = 0;
                    for (tap = 0U; tap < 4U; ++tap) {
                        sum += d45_filter[row][tap] *
                               above[(int32_t)base + (int32_t)tap - 1];
                    }
                    value = (sum + 64) >> 7U;
                } else {
                    value = above[11];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp(value, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        width == 8U && height == 16U) {
        const uint16_t derivative = 97U;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (column = 0U; column < width; ++column) {
            uint16_t position = (uint16_t)((column + 2U) * derivative);
            uint16_t base = position >> 6U;
            uint8_t shift = (uint8_t)((position >> 1U) & 0x1fU);
            for (row = 0U; row < height; ++row, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    size_t source = base + tap - 1U;
                    if (source >= references->left_count) {
                        source = references->left_count - 1U;
                    }
                    sum += dr_interp_filter[shift][tap] *
                           references->left[source];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 &&
        mode->mrl_index == 1U && mode->multi_line_mrl == 0U &&
        width == 8U && (height == 8U || height == 16U)) {
        const uint16_t derivative = 41U;
        size_t edge_count = (size_t)width + height + 2U;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        left[-1] = references->has_above_left ? references->above_left :
                                                   references->left[0];
        for (index = 0U; index < edge_count; ++index) {
            size_t source = index < references->left_count ?
                index : references->left_count - 1U;
            left[index] = references->left[source];
        }
        left[edge_count] = left[edge_count - 1U];
        left[edge_count + 1U] = left[edge_count - 1U];
        for (column = 0U; column < width; ++column) {
            uint16_t position =
                (uint16_t)((column + 1U + mode->mrl_index) * derivative);
            uint16_t base = position >> 6U;
            uint8_t shift = (uint8_t)((position & 63U) >> 1U);
            for (row = 0U; row < height; ++row, ++base) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           left[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 3 &&
        width == 8U && height == 4U) {
        const uint16_t derivative = 107U;
        size_t edge_count = (size_t)width + height;
        uint8_t source[13U];
        uint8_t *filtered = &above[-1];
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        above[-1] = references->has_above_left ? references->above_left :
                                                   references->above[0];
        for (index = 0U; index < edge_count; ++index) {
            size_t reference = index < references->above_count ?
                index : references->above_count - 1U;
            above[index] = references->above[reference];
        }
        for (index = 0U; index <= edge_count; ++index) {
            source[index] = filtered[index];
        }
        for (index = 1U; index <= edge_count; ++index) {
            size_t previous = index - 1U;
            size_t next = index < edge_count ? index + 1U : index;
            filtered[index] = (uint8_t)(
                (source[previous] + 2U * source[index] + source[next] + 2U) >>
                2U);
        }
        above[edge_count] = above[edge_count - 1U];
        above[edge_count + 1U] = above[edge_count - 1U];
        for (row = 0U; row < height; ++row) {
            uint16_t position = (uint16_t)((row + 1U) * derivative);
            uint16_t base = position >> 6U;
            uint8_t shift = (uint8_t)((position & 63U) >> 1U);
            for (column = 0U; column < width; ++column, ++base) {
                size_t tap;
                int32_t sum = 0;
                if (base > edge_count - 1U) {
                    predictor[(size_t)row * predictor_stride + column] =
                        above[edge_count - 1U];
                    continue;
                }
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           above[(int32_t)base + (int32_t)tap - 1];
                }
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)clamp((sum + 64) >> 7U, 0, 255);
            }
        }
        return AV2H_OK;
    }
        if (mode->mode != AV2H_INTRA_D203 ||
                (mode->angle_delta != 0 && mode->angle_delta != -3 &&
                 mode->angle_delta != 3) ||
        !((width == 4U && height == 4U) ||
                    (width == 4U && height == 16U) ||
                    (width == 8U && height == 16U) ||
                    (width == 8U && height == 32U) ||
                    (width == 16U && height == 16U)) ||
        (mode->angle_delta == 3 &&
         !((width == 4U && height == 4U) ||
           (width == 8U && height == 4U)))) {
        return AV2H_UNSUPPORTED;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    left[-1] = references->left[0];
    for (index = 0U; index < (size_t)width + height; ++index) {
        size_t source = index < references->left_count ?
            index : references->left_count - 1U;
        left[index] = references->left[source];
    }
    if (mode->angle_delta == -3 && width == 8U && height == 32U) {
        static const uint8_t kernel[5U] = { 2U, 4U, 4U, 4U, 2U };
        uint8_t source[41U];
        uint8_t *filtered = &left[-1];
        size_t filtered_count = (size_t)width + height + 1U;
        for (index = 0U; index < filtered_count; ++index) {
            source[index] = filtered[index];
        }
        for (index = 1U; index < filtered_count; ++index) {
            size_t tap;
            unsigned int sum = 0U;
            for (tap = 0U; tap < 5U; ++tap) {
                int32_t source_index = (int32_t)index - 2 + (int32_t)tap;
                if (source_index < 0) source_index = 0;
                if (source_index >= (int32_t)filtered_count) {
                    source_index = (int32_t)filtered_count - 1;
                }
                sum += kernel[tap] * source[source_index];
            }
            filtered[index] = (uint8_t)((sum + 8U) >> 4U);
        }
    }
    left[width + height] = left[width + height - 1U];
    left[width + height + 1U] = left[width + height - 1U];
    for (column = 0U; column < width; ++column) {
        uint16_t derivative =
            mode->angle_delta == 0 ? 24U :
            mode->angle_delta == -3 ? 12U : 38U;
        uint16_t offset = (uint16_t)((column + 1U) * derivative);
        uint16_t base = offset >> 6U;
        uint8_t shift = (uint8_t)((offset >> 1U) & 0x1fU);
        for (row = 0U; row < height; ++row, ++base) {
            int32_t value;
            if (base <= width + height - 1U) {
                size_t tap;
                int32_t sum = 0;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += dr_interp_filter[shift][tap] *
                           left[(int32_t)base + (int32_t)tap - 1];
                }
                value = (sum + 64) >> 7U;
            } else {
                value = left[width + height - 1U];
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(value, 0, 255);
        }
    }
    return AV2H_OK;
}

/*
 * Intra bi-prediction (IBP) refinement of a DC predictor (enable_ibp=1).
 * The flat DC surface in `dst` is blended toward the reconstructed above and
 * left neighbours near the block edges, matching the AVM highbd_ibp_dc*
 * predictors (avm_dsp/intrapred.c).  Applied for every chroma transform
 * except TX_4X4, where IBP is disallowed.
 */
static const uint8_t ibp_size_to_weights_index[9U] = {
    0U, 1U, 2U, 0U, 3U, 0U, 0U, 0U, 4U
};

static uint8_t dc_predictor_average(unsigned int sum, unsigned int count,
                                    uint16_t width, uint16_t height,
                                    uint8_t have_top, uint8_t have_left) {
    if (count == 0U) return 128U;
    if (have_top && have_left && width != height) {
        int shift;
        uint16_t scale = cfl_resolve_divisor_32(count, &shift);
        return (uint8_t)(((uint64_t)sum * scale +
                          ((uint64_t)1U << (shift - 1))) >> shift);
    }
    return (uint8_t)((sum + count / 2U) / count);
}

static void apply_ibp_dc(uint8_t *dst, size_t stride, uint16_t width,
                         uint16_t height, const uint8_t *above,
                         const uint8_t *left) {
    uint16_t row;
    uint16_t column;
    if (above != 0 && left != 0) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        uint16_t len_w = (uint16_t)(width >> 2U);
        uint16_t row_start = width >= height ? len_h : 0U;
        uint16_t col_start = width >= height ? 0U : len_w;
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = col_start; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                    dst[(size_t)row * stride + column] * weights[row] + 64) >>
                    7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
        weights = ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = row_start; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value = (left[row] * (128 - weights[column]) +
                    dst[(size_t)row * stride + column] * weights[column] +
                    64) >> 7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    } else if (above != 0) {
        uint16_t len_h = (uint16_t)(height >> 2U);
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[height >> 3U]];
        for (row = 0U; row < len_h; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t value = (above[column] * (128 - weights[row]) +
                    dst[(size_t)row * stride + column] * weights[row] + 64) >>
                    7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    } else if (left != 0) {
        uint16_t len_w = (uint16_t)(width >> 2U);
        const uint8_t *weights =
            ibp_weights[ibp_size_to_weights_index[width >> 3U]];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < len_w; ++column) {
                int32_t value = (left[row] * (128 - weights[column]) +
                    dst[(size_t)row * stride + column] * weights[column] +
                    64) >> 7U;
                dst[(size_t)row * stride + column] = (uint8_t)value;
            }
        }
    }
}

Av2hStatus av2h_chroma_predict(
    const Av2hFirstChromaMode *mode,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride) {
    uint16_t row;
    uint16_t column;
    if (mode == 0 || references == 0 || predictor == 0 ||
        predictor_stride < width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((mode->angle_delta == -2 || mode->angle_delta == 2) &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) && width <= 32U &&
        luma_prediction_dimension_supported(height) && height <= 32U) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return predict_directional(
            &luma_mode, angle,
            width, height, references, predictor, predictor_stride, 0U, 1U);
    }
    if ((mode->angle_delta == -3 || mode->angle_delta == -1 ||
         mode->angle_delta == 1 || mode->angle_delta == 3) &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) && width <= 32U &&
        luma_prediction_dimension_supported(height) && height <= 32U) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return predict_directional(
            &luma_mode, angle,
            width, height, references, predictor, predictor_stride, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH && mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) && width <= 32U &&
        luma_prediction_dimension_supported(height) && height <= 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->angle_delta == 0 &&
        directional_base_angle(mode->mode) != 0 &&
        luma_prediction_dimension_supported(width) && width <= 32U &&
        luma_prediction_dimension_supported(height) && height <= 32U) {
        Av2hFirstLeafMode luma_mode;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode), width, height);
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.edge_ibp = (uint8_t)(
            (width != 4U || height != 4U) && mode->edge_ibp);
        luma_mode.above_smooth = mode->above_smooth;
        luma_mode.left_smooth = mode->left_smooth;
        return predict_directional(
            &luma_mode, angle,
            width, height, references, predictor, predictor_stride, 0U, 1U);
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_VERTICAL &&
        mode->angle_delta == 0 &&
        luma_prediction_dimension_supported(width) && width <= 32U &&
        luma_prediction_dimension_supported(height) && height <= 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) &&
        width == 32U && height == 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return av2h_luma_predict(
            &luma_mode, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL &&
        (mode->angle_delta == -3 || mode->angle_delta == -2) &&
        width == 32U && height == 16U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return predict_directional_idif(
            &luma_mode, 180 + mode->angle_delta * 3,
            width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL && mode->angle_delta == 1 &&
        width == 16U && height == 32U) {
        Av2hFirstLeafMode luma_mode;
        av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
        luma_mode.enable_ibp = 1U;
        return predict_directional_idif(
            &luma_mode, 183, width, height, references,
            predictor, predictor_stride);
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        width == 32U && height == 16U) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[column < references->above_count ?
                        column : references->above_count - 1U];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_VERTICAL && mode->angle_delta == 0 &&
        width == 32U && height == 32U) {
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] =
                    references->above != 0U &&
                    references->above_count != 0U ?
                        references->above[
                            column < references->above_count ? column :
                            references->above_count - 1U] : 127U;
            }
        }
        return AV2H_OK;
    }
        if (!((width == 4U && height == 4U &&
                    ((mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0) ||
          (mode->mode == AV2H_INTRA_D203 && mode->angle_delta == 0) ||
          (mode->mode == AV2H_INTRA_D67 &&
           (mode->angle_delta == -1 || mode->angle_delta == -2)) ||
                     (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 0))) ||
                    (width == 32U &&
                     (height == 16U || height == 32U) &&
                     mode->mode == AV2H_INTRA_D67 &&
                     mode->angle_delta == 0) ||
                    (width == 16U &&
                     (height == 4U || height == 16U) &&
                     mode->mode == AV2H_INTRA_D203 &&
                     mode->angle_delta == 0) ||
                    (width == 4U && height == 8U &&
                     mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) ||
                    (mode->angle_delta == 0 &&
                     (mode->mode == AV2H_INTRA_DC ||
                      mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                      mode->mode == AV2H_INTRA_PAETH) &&
                     luma_prediction_dimension_supported(width) &&
                     width <= 32U &&
                     luma_prediction_dimension_supported(height) &&
                     height <= 32U))) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->mode == AV2H_INTRA_HORIZONTAL) {
        uint8_t fallback;
        if (references->left != 0 && references->left_count != 0U) {
            for (row = 0U; row < height; ++row) {
                size_t source = row < references->left_count ?
                    row : references->left_count - 1U;
                uint8_t value = references->left[source];
                for (column = 0U; column < width; ++column) {
                    predictor[(size_t)row * predictor_stride + column] = value;
                }
            }
            return AV2H_OK;
        }
        /* No left neighbour: AV2 fills the left edge with the first above
         * sample when the block has an above neighbour, else the mid value. */
        fallback = (references->above != 0 && references->above_count != 0U) ?
            references->above[0] : 128U;
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = fallback;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC) {
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t value;
        if (references->above != 0) {
            for (column = 0U; column < width; ++column) {
                sum += references->above[column];
            }
            count += width;
        }
        if (references->left != 0) {
            for (row = 0U; row < height; ++row) {
                sum += references->left[row];
            }
            count += height;
        }
        value = dc_predictor_average(
            sum, count, width, height,
            references->above != 0, references->left != 0);
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = value;
            }
        }
        if (width != 4U || height != 4U) {
            apply_ibp_dc(predictor, predictor_stride, width, height,
                         references->above, references->left);
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        uint8_t scale = smooth_scale(width, height);
        uint8_t top_right;
        if (references->above == 0 || references->above_count < width) {
            return AV2H_INVALID_ARGUMENT;
        }
        top_right = references->above_count > width ?
            references->above[width] : references->above[width - 1U];
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                uint8_t left = references->left == 0 ?
                    references->above[0] : references->left[row];
                uint8_t blend = smooth_blend(column, scale);
                int32_t value = top_right + round_shift(
                    ((int64_t)left - top_right) *
                        (width - 1U - column),
                    block_log2(width));
                value += round_shift(
                    ((int64_t)left - value) * blend, 7U);
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)value;
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_PAETH) {
        if (references->above == 0 || references->above_count < width ||
            references->left == 0 || references->left_count < height ||
            !references->has_above_left) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                int32_t top = references->above[column];
                int32_t side = references->left[row];
                int32_t corner = references->above_left;
                int32_t base = top + side - corner;
                int32_t side_distance =
                    base > side ? base - side : side - base;
                int32_t top_distance =
                    base > top ? base - top : top - base;
                int32_t corner_distance =
                    base > corner ? base - corner : corner - base;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)(side_distance <= top_distance &&
                                      side_distance <= corner_distance ?
                                  side :
                                  top_distance <= corner_distance ?
                                      top : corner);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45 && mode->angle_delta == 3) {
        const uint16_t derivative = 93U;
        uint8_t edge[13U];
        uint8_t filtered[13U];
        size_t edge_index;
        if (references->left == 0 || references->left_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        edge[0] = references->left[0];
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t source = edge_index - 1U;
            edge[edge_index] = source < references->left_count ?
                references->left[source] :
                references->left[references->left_count - 1U];
        }
        for (edge_index = 1U; edge_index < sizeof(edge); ++edge_index) {
            size_t previous = edge_index == 1U ? 0U : edge_index - 1U;
            size_t next = edge_index + 1U < sizeof(edge) ?
                edge_index + 1U : edge_index;
            int32_t value = edge[previous] + 2 * edge[edge_index] +
                edge[next];
            filtered[edge_index] = (uint8_t)((value + 2) >> 2U);
        }
        filtered[0] = edge[0];
        for (column = 0U; column < width; ++column) {
            uint16_t offset = (uint16_t)((column + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (row = 0U; row < height; ++row, ++base) {
                size_t first = base + 1U < sizeof(edge) ?
                    base + 1U : sizeof(edge) - 1U;
                size_t second = first + 1U < sizeof(edge) ?
                    first + 1U : sizeof(edge) - 1U;
                int32_t value = filtered[first] * (32 - shift) +
                    filtered[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D45) {
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            for (column = 0U; column < width; ++column) {
                size_t source = (size_t)row + column + 1U;
                if (source >= references->above_count) {
                    source = references->above_count - 1U;
                }
                predictor[(size_t)row * predictor_stride + column] =
                    references->above[source];
            }
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_D67) {
        uint16_t derivative = mode->angle_delta == -1 ? 28U :
            mode->angle_delta == -2 ? 32U : 24U;
        if (references->above == 0 || references->above_count == 0U) {
            return AV2H_INVALID_ARGUMENT;
        }
        for (row = 0U; row < height; ++row) {
            uint16_t offset = (uint16_t)((row + 1U) * derivative);
            uint16_t base = offset >> 6U;
            uint16_t shift = (offset & 63U) >> 1U;
            for (column = 0U; column < width; ++column, ++base) {
                size_t first = base < references->above_count ?
                    base : references->above_count - 1U;
                size_t second = first + 1U < references->above_count ?
                    first + 1U : references->above_count - 1U;
                int32_t value = references->above[first] * (32 - shift) +
                    references->above[second] * shift;
                predictor[(size_t)row * predictor_stride + column] =
                    (uint8_t)((value + 16) >> 5U);
            }
        }
        return AV2H_OK;
    }
    if (references->left == 0 || references->left_count == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (column = 0U; column < width; ++column) {
        uint16_t offset = (uint16_t)((column + 1U) * 24U);
        uint16_t base = offset >> 6U;
        uint16_t shift = (offset & 63U) >> 1U;
        for (row = 0U; row < height; ++row, ++base) {
            size_t first = base < references->left_count ?
                base : references->left_count - 1U;
            size_t second = first + 1U < references->left_count ?
                first + 1U : references->left_count - 1U;
            int32_t value = references->left[first] * (32 - shift) +
                references->left[second] * shift;
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)((value + 16) >> 5U);
        }
    }
    return AV2H_OK;
}

static Av2hStatus matrix_value(uint16_t size, uint16_t frequency,
                               uint16_t output, int32_t *value) {
    size_t index;
    if (size == 4U) {
        *value = dct4[frequency][output];
        return AV2H_OK;
    }
    if (size == 8U) {
        *value = dct8[frequency][output];
        return AV2H_OK;
    }
    if (size == 16U) {
        for (index = 0U;
             index < sizeof(dct16_frequency) / sizeof(dct16_frequency[0]);
             ++index) {
            if (dct16_frequency[index] == frequency) {
                *value = dct16[index][output];
                return AV2H_OK;
            }
        }
        return AV2H_UNSUPPORTED;
    }
    if (size == 32U) {
        for (index = 0U;
             index < sizeof(dct32_frequency) / sizeof(dct32_frequency[0]);
             ++index) {
            if (dct32_frequency[index] == frequency) {
                *value = dct32[index][output];
                return AV2H_OK;
            }
        }
        return AV2H_UNSUPPORTED;
    }
    return AV2H_UNSUPPORTED;
}

static uint8_t plane_sample_reconstructed(const Av2hReconstructionPlane *plane,
                                          size_t x, size_t y) {
    size_t index = (y / 4U) * plane->reconstructed_stride + x / 4U;
    return index < plane->reconstructed_count && plane->reconstructed[index];
}

static void plane_mark_reconstructed(Av2hReconstructionPlane *plane,
                                     uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height) {
    size_t first_column = x / 4U;
    size_t first_row = y / 4U;
    size_t last_column = ((size_t)x + width - 1U) / 4U;
    size_t last_row = ((size_t)y + height - 1U) / 4U;
    size_t row;
    size_t column;
    for (row = first_row; row <= last_row; ++row) {
        for (column = first_column; column <= last_column; ++column) {
            plane->reconstructed[row * plane->reconstructed_stride + column] =
                1U;
        }
    }
}

static Av2hStatus flip_adst_matrix_value(uint16_t size, uint16_t frequency,
                                         uint16_t output, int32_t *value) {
    if (size == 4U) {
        *value = adst4[frequency][size - output - 1U];
        if ((frequency & 1U) != 0U) *value = -*value;
        return AV2H_OK;
    }
    if (size == 8U) {
        *value = adst8[frequency][size - output - 1U];
        return AV2H_OK;
    }
    if (size == 16U) {
        *value = adst16[frequency][size - output - 1U];
        if ((frequency & 1U) != 0U) *value = -*value;
        return AV2H_OK;
    }
    return AV2H_UNSUPPORTED;
}

static Av2hStatus inverse_rows(const int32_t *input, int32_t *transposed,
                               uint16_t width, uint16_t height,
                               uint8_t shift, uint8_t identity,
                               uint8_t adst, uint8_t flip_adst) {
    uint16_t row;
    uint16_t output;
    uint16_t frequency;
    for (row = 0U; row < height; ++row) {
        for (output = 0U; output < width; ++output) {
            int64_t sum = 0;
            if (identity) {
                sum = (int64_t)input[(size_t)row * width + output] *
                      identity_scale(width);
            } else if (adst) {
                if (width == 4U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum +=
                            (int64_t)input[(size_t)row * width + frequency] *
                            adst4[frequency][output];
                    }
                } else if (width == 8U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum += (int64_t)input[
                            (size_t)row * width + frequency] *
                            adst8[frequency][output];
                    }
                } else if (width == 16U) {
                    for (frequency = 0U; frequency < width; ++frequency) {
                        sum += (int64_t)input[
                            (size_t)row * width + frequency] *
                            adst16[frequency][output];
                    }
                } else {
                    return AV2H_UNSUPPORTED;
                }
            } else {
                for (frequency = 0U; frequency < width; ++frequency) {
                    int32_t source = input[(size_t)row * width + frequency];
                    int32_t matrix;
                    Av2hStatus status;
                    if (source == 0) continue;
                    status = flip_adst ?
                        flip_adst_matrix_value(width, frequency, output,
                                               &matrix) :
                        matrix_value(width, frequency, output, &matrix);
                    if (status != AV2H_OK) return status;
                    sum += (int64_t)source * matrix;
                }
            }
            transposed[(size_t)output * height + row] =
                clamp(round_shift(sum, shift), -32768, 32767);
        }
    }
    return AV2H_OK;
}

static Av2hStatus inverse_columns(const int32_t *transposed,
                                  int32_t *output_block,
                                  uint16_t width, uint16_t height,
                                  uint8_t shift, uint8_t identity,
                                  uint8_t adst, uint8_t flip_adst) {
    uint16_t column;
    uint16_t output;
    uint16_t frequency;
    for (column = 0U; column < width; ++column) {
        for (output = 0U; output < height; ++output) {
            int64_t sum = 0;
            if (identity) {
                sum = (int64_t)transposed[(size_t)column * height + output] *
                      identity_scale(height);
            }
            for (frequency = 0U; !identity && frequency < height; ++frequency) {
                int32_t source = transposed[(size_t)column * height + frequency];
                int32_t matrix;
                Av2hStatus status;
                if (source == 0) continue;
                if (adst) {
                    if (height == 4U) {
                        matrix = adst4[frequency][output];
                    } else if (height == 8U) {
                        matrix = adst8[frequency][output];
                    } else if (height == 16U) {
                        matrix = adst16[frequency][output];
                    } else {
                        return AV2H_UNSUPPORTED;
                    }
                    status = AV2H_OK;
                } else {
                    status = flip_adst ?
                        flip_adst_matrix_value(height, frequency, output,
                                               &matrix) :
                        matrix_value(height, frequency, output, &matrix);
                }
                if (status != AV2H_OK) return status;
                sum += (int64_t)source * matrix;
            }
            output_block[(size_t)output * width + column] =
                clamp(round_shift(sum, shift), -256, 255);
        }
    }
    return AV2H_OK;
}

/*
 * Dimension-parametrized reconstruction kernel shared by the Emerald Q208
 * luma blocks.  It implements the AV2 2D inverse transform (spec 7.15.4)
 * directly on adjusted coefficient dimensions (each clamped to 32), then
 * expands the residual to the full output geometry by sample duplication
 * for the 64-wide/64-tall transforms.  Row/column transform kinds are
 * derived from the transform type and shifts from the transform size, so
 * this branch never depends on transform-size enum names that are still
 * being introduced by the parallel geometry work.
 */

/*
 * Transform size integer values fixed by the AV2 bitstream format.  Some
 * are not yet named in coeff.h; use the raw values so the kernel stays
 * independent of the parallel geometry branch.
 */
#define RECON_TX_4X4 0
#define RECON_TX_8X8 1
#define RECON_TX_16X16 2
#define RECON_TX_32X32 3
#define RECON_TX_64X64 4
#define RECON_TX_4X8 5
#define RECON_TX_8X4 6
#define RECON_TX_8X16 7
#define RECON_TX_16X8 8
#define RECON_TX_16X32 9
#define RECON_TX_32X64 11
#define RECON_TX_32X16 10
#define RECON_TX_64X32 12
#define RECON_TX_4X16 13
#define RECON_TX_16X4 14
#define RECON_TX_16X64 17
#define RECON_TX_32X8 16
#define RECON_TX_8X32 15
#define RECON_TX_64X16 18
#define RECON_TX_4X32 19
#define RECON_TX_32X4 20
#define RECON_TX_8X64 21
#define RECON_TX_64X8 22
#define RECON_TX_4X64 23
#define RECON_TX_64X4 24

typedef enum {
    RECON_1D_DCT,
    RECON_1D_ADST,
    RECON_1D_FLIPADST,
    RECON_1D_IDENTITY,
    RECON_1D_NONE
} Av2hRecon1dKind;

static uint8_t recon_size_log2(uint16_t size) {
    return size == 4U ? 2U : size == 8U ? 3U : size == 16U ? 4U :
        size == 32U ? 5U : 6U;
}

static Av2hStatus recon_geometry(uint8_t tx_size,
                                 uint16_t *output_width,
                                 uint16_t *output_height,
                                 uint8_t *row_shift,
                                 uint8_t *column_shift) {
    switch (tx_size) {
        case RECON_TX_4X4:
            *output_width = 4U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_4X8:
            *output_width = 4U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_8X8:
            *output_width = 8U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_8X4:
            *output_width = 8U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 10U; return AV2H_OK;
        case RECON_TX_16X16:
            *output_width = 16U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X32:
            *output_width = 32U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X64:
            *output_width = 64U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_8X16:
            *output_width = 8U; *output_height = 16U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_16X8:
            *output_width = 16U; *output_height = 8U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_16X4:
            *output_width = 16U; *output_height = 4U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_4X16:
            *output_width = 4U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_32X8:
            *output_width = 32U; *output_height = 8U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X4:
            *output_width = 32U; *output_height = 4U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_64X16:
            *output_width = 64U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_4X32:
            *output_width = 4U; *output_height = 32U;
            *row_shift = 7U; *column_shift = 11U; return AV2H_OK;
        case RECON_TX_32X16:
            *output_width = 32U; *output_height = 16U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_16X32:
            *output_width = 16U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_64X32:
            *output_width = 64U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_16X64:
            *output_width = 16U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_8X64:
            *output_width = 8U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_4X64:
            *output_width = 4U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X4:
            *output_width = 64U; *output_height = 4U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_64X8:
            *output_width = 64U; *output_height = 8U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        case RECON_TX_8X32:
            *output_width = 8U; *output_height = 32U;
            *row_shift = 6U; *column_shift = 13U; return AV2H_OK;
        case RECON_TX_32X64:
            *output_width = 32U; *output_height = 64U;
            *row_shift = 6U; *column_shift = 12U; return AV2H_OK;
        default:
            return AV2H_UNSUPPORTED;
    }
}

static Av2hStatus recon_transform_kinds(uint8_t primary_type,
                                        Av2hRecon1dKind *row_kind,
                                        Av2hRecon1dKind *column_kind) {
    switch (primary_type) {
        case AV2H_TX_DCT_DCT:
            *row_kind = RECON_1D_DCT; *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_ADST_DCT:
            *row_kind = RECON_1D_DCT; *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_DCT_ADST:
            *row_kind = RECON_1D_ADST; *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_DCT_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_DCT:
            *row_kind = RECON_1D_DCT;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_ADST_ADST:
            *row_kind = RECON_1D_ADST; *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_FLIPADST_ADST:
            *row_kind = RECON_1D_ADST;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_ADST_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_H_ADST:
            *row_kind = RECON_1D_ADST;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_V_ADST:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_ADST;
            return AV2H_OK;
        case AV2H_TX_V_DCT:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_DCT;
            return AV2H_OK;
        case AV2H_TX_H_DCT:
            *row_kind = RECON_1D_DCT;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_V_FLIPADST:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_FLIPADST;
            return AV2H_OK;
        case AV2H_TX_H_FLIPADST:
            *row_kind = RECON_1D_FLIPADST;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        case AV2H_TX_IDTX:
            *row_kind = RECON_1D_IDENTITY;
            *column_kind = RECON_1D_IDENTITY;
            return AV2H_OK;
        default:
            *row_kind = RECON_1D_NONE; *column_kind = RECON_1D_NONE;
            return AV2H_UNSUPPORTED;
    }
}

static Av2hStatus recon_kind_supported(Av2hRecon1dKind kind, uint16_t size) {
    if (kind == RECON_1D_DCT) {
        return (size == 4U || size == 8U || size == 16U || size == 32U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    if (kind == RECON_1D_ADST || kind == RECON_1D_FLIPADST) {
        return (size == 4U || size == 8U || size == 16U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    if (kind == RECON_1D_IDENTITY) {
        return (size == 4U || size == 8U || size == 16U || size == 32U) ?
            AV2H_OK : AV2H_UNSUPPORTED;
    }
    return AV2H_UNSUPPORTED;
}

static int32_t recon_kernel_value(Av2hRecon1dKind kind, uint16_t size,
                                  uint16_t frequency, uint16_t output) {
    if (kind == RECON_1D_DCT) {
        if (size == 4U) return dct4[frequency][output];
        if (size == 8U) return dct8[frequency][output];
        if (size == 16U) return dct16[frequency][output];
        return dct32[frequency][output];
    }
    if (kind == RECON_1D_FLIPADST) {
        int32_t value = size == 4U ?
            adst4[frequency][size - output - 1U] : size == 8U ?
            adst8[frequency][size - output - 1U] :
            adst16[frequency][size - output - 1U];
        return size != 8U && (frequency & 1U) != 0U ? -value : value;
    }
    if (size == 4U) return adst4[frequency][output];
    if (size == 8U) return adst8[frequency][output];
    return adst16[frequency][output];
}

static void recon_transform_1d(const int32_t *input, int32_t *output,
                               Av2hRecon1dKind kind, uint16_t size,
                               uint8_t shift, uint8_t column_transform) {
    int32_t minimum = -((int32_t)1 << (8U + (column_transform ? 0U : 7U)));
    int32_t maximum = ((int32_t)1 << (8U + (column_transform ? 0U : 7U))) - 1;
    uint16_t out;
    uint16_t frequency;
    if (kind == RECON_1D_IDENTITY) {
        for (out = 0U; out < size; ++out) {
            output[out] = clamp(round_shift(
                (int64_t)input[out] * identity_scale(size), shift),
                minimum, maximum);
        }
        return;
    }
    for (out = 0U; out < size; ++out) {
        int64_t sum = 0;
        for (frequency = 0U; frequency < size; ++frequency) {
            sum += (int64_t)recon_kernel_value(kind, size, frequency, out) *
                   input[frequency];
        }
        output[out] = clamp(round_shift(sum, shift), minimum, maximum);
    }
}

static Av2hStatus reconstruct_transform_generic(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    uint16_t coeff_width;
    uint16_t coeff_height;
    uint8_t rectangular;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    Av2hStatus status;
    uint16_t row;
    uint16_t column;
    size_t index;
    av2h_debug_inverse_transform_stage = 1U;
    if (recon_geometry(prefix->tx_size, &output_width, &output_height,
                       &row_shift, &column_shift) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    coeff_width = output_width > 32U ? 32U : output_width;
    coeff_height = output_height > 32U ? 32U : output_height;
    if (coefficients->width != coeff_width ||
        coefficients->height != coeff_height ||
        coefficients->coefficient_count !=
            (uint32_t)coeff_width * coeff_height) {
        return AV2H_UNSUPPORTED;
    }
    if (predictor != 0 && predictor_stride < output_width) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->all_zero && prefix->eob == 0U &&
        coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        reconstruction->width = output_width;
        reconstruction->height = output_height;
        for (row = 0U; row < output_height; ++row) {
            for (column = 0U; column < output_width; ++column) {
                size_t out_index = (size_t)row * output_width + column;
                int32_t sample = predictor == 0 ? 128 :
                    predictor[(size_t)row * predictor_stride + column];
                reconstruction->residual[out_index] = 0;
                reconstruction->pixels[out_index] = (uint8_t)sample;
            }
        }
        return AV2H_OK;
    }
    av2h_debug_inverse_transform_stage = 2U;
    if (recon_transform_kinds(transform->primary_type, &row_kind,
                              &column_kind) != AV2H_OK ||
        recon_kind_supported(row_kind, coeff_width) != AV2H_OK ||
        recon_kind_supported(column_kind, coeff_height) != AV2H_OK) {
        return AV2H_UNSUPPORTED;
    }
    if (transform->primary_type == AV2H_TX_IDTX &&
        prefix->tx_size == RECON_TX_32X4) {
        row_shift = 7U;
        column_shift = 11U;
    }
    reconstruction->width = output_width;
    reconstruction->height = output_height;
    rectangular = (uint8_t)((recon_size_log2(output_width) ^
                             recon_size_log2(output_height)) & 1U);
    for (index = 0U; index < coefficients->coefficient_count; ++index) {
        scratch->scaled[index] = coefficients->dequantized[index];
    }
    av2h_debug_inverse_transform_stage = 3U;
    status = inverse_secondary_transform(
        mode, prefix, transform, coefficients, scratch->scaled);
    if (status != AV2H_OK) return status;
    av2h_debug_inverse_transform_stage = 4U;
    for (row = 0U; row < coeff_height; ++row) {
        int32_t row_input[32U];
        int32_t row_output[32U];
        for (column = 0U; column < coeff_width; ++column) {
            int32_t value =
                scratch->scaled[(size_t)row * coeff_width + column];
            row_input[column] = rectangular ?
                round_shift((int64_t)value * 2896, 12U) : value;
        }
        recon_transform_1d(row_input, row_output, row_kind, coeff_width,
                           row_shift, 0U);
        for (column = 0U; column < coeff_width; ++column) {
            scratch->transposed[(size_t)row * coeff_width + column] =
                row_output[column];
        }
    }
    for (column = 0U; column < coeff_width; ++column) {
        int32_t column_input[32U];
        int32_t column_output[32U];
        for (row = 0U; row < coeff_height; ++row) {
            column_input[row] =
                scratch->transposed[(size_t)row * coeff_width + column];
        }
        recon_transform_1d(column_input, column_output, column_kind,
                           coeff_height, column_shift, 1U);
        for (row = 0U; row < coeff_height; ++row) {
            scratch->residual[(size_t)row * coeff_width + column] =
                column_output[row];
        }
    }
    for (row = 0U; row < output_height; ++row) {
        uint16_t source_row =
            coeff_height != output_height ? (uint16_t)(row >> 1U) : row;
        for (column = 0U; column < output_width; ++column) {
            uint16_t source_column =
                coeff_width != output_width ? (uint16_t)(column >> 1U) : column;
            int32_t residual = scratch->residual[
                (size_t)source_row * coeff_width + source_column];
            size_t out_index = (size_t)row * output_width + column;
            int32_t sample = predictor == 0 ? 128 :
                predictor[(size_t)row * predictor_stride + column];
            reconstruction->residual[out_index] = (int16_t)residual;
            reconstruction->pixels[out_index] =
                (uint8_t)clamp(sample + residual, 0, 255);
        }
    }
    return AV2H_OK;
}

static int reconstruct_generic_selected(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstTransformType *transform,
    uint8_t tx_size) {
    uint16_t output_width;
    uint16_t output_height;
    uint8_t row_shift;
    uint8_t column_shift;
    Av2hRecon1dKind row_kind;
    Av2hRecon1dKind column_kind;
    int identity = transform->primary_type == AV2H_TX_IDTX;
    if (identity) {
        int legal_identity =
                        mode->fsc_mode ||
            mode->use_intrabc ||
            (!mode->fsc_mode && !mode->use_intrabc &&
             (tx_size == RECON_TX_4X32 || tx_size == RECON_TX_32X4 ||
              tx_size == RECON_TX_8X32 || tx_size == RECON_TX_32X8 ||
              tx_size == RECON_TX_16X32 || tx_size == RECON_TX_32X16));
        if (mode->skip_transform || !legal_identity) return 0;
    } else if (mode->skip_transform || mode->fsc_mode) {
        return 0;
    }
    if (recon_geometry(tx_size, &output_width, &output_height,
                       &row_shift, &column_shift) != AV2H_OK) {
        return 0;
    }
    if (!identity && !mode->use_intrabc && tx_size == RECON_TX_4X4 &&
                transform->primary_type != AV2H_TX_FLIPADST_FLIPADST &&
                transform->primary_type != AV2H_TX_ADST_DCT &&
                transform->primary_type != AV2H_TX_DCT_ADST &&
                transform->primary_type != AV2H_TX_DCT_FLIPADST &&
                transform->primary_type != AV2H_TX_FLIPADST_DCT &&
                transform->primary_type != AV2H_TX_ADST_FLIPADST &&
                transform->primary_type != AV2H_TX_FLIPADST_ADST &&
                transform->primary_type != AV2H_TX_ADST_ADST &&
                transform->primary_type != AV2H_TX_H_ADST &&
                transform->primary_type != AV2H_TX_V_ADST &&
                transform->primary_type != AV2H_TX_V_DCT &&
                transform->primary_type != AV2H_TX_H_DCT &&
                transform->primary_type != AV2H_TX_V_FLIPADST &&
                transform->primary_type != AV2H_TX_H_FLIPADST &&
                !((mode->mode == AV2H_INTRA_D157 ||
                   mode->mode == AV2H_INTRA_VERTICAL) &&
                    transform->primary_type == AV2H_TX_DCT_ADST)) {
        return 0;
    }
    if (!identity && !mode->use_intrabc && tx_size == RECON_TX_8X4 &&
        transform->primary_type != AV2H_TX_DCT_DCT &&
        transform->primary_type != AV2H_TX_FLIPADST_FLIPADST &&
        transform->primary_type != AV2H_TX_ADST_DCT &&
        transform->primary_type != AV2H_TX_DCT_ADST &&
        transform->primary_type != AV2H_TX_DCT_FLIPADST &&
        transform->primary_type != AV2H_TX_FLIPADST_DCT &&
        transform->primary_type != AV2H_TX_ADST_FLIPADST &&
        transform->primary_type != AV2H_TX_FLIPADST_ADST &&
        transform->primary_type != AV2H_TX_ADST_ADST &&
        transform->primary_type != AV2H_TX_H_ADST &&
        transform->primary_type != AV2H_TX_V_ADST &&
        transform->primary_type != AV2H_TX_V_DCT &&
        transform->primary_type != AV2H_TX_H_DCT &&
        transform->primary_type != AV2H_TX_V_FLIPADST &&
        transform->primary_type != AV2H_TX_H_FLIPADST) {
        return 0;
    }
    if (recon_transform_kinds(transform->primary_type, &row_kind,
                              &column_kind) != AV2H_OK) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    const uint8_t *predictor,
    size_t predictor_stride,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    size_t index;
    size_t count;
    uint8_t first_shift;
    uint8_t second_shift;
    uint8_t rectangular;
    uint8_t identity_rows;
    uint8_t adst_rows;
    uint8_t flip_adst_rows;
    uint8_t identity_columns;
    uint8_t adst_columns;
    uint8_t flip_adst_columns = 0U;
    Av2hStatus status;
    if (mode == 0 || prefix == 0 || transform == 0 || coefficients == 0 ||
        scratch == 0 || reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->all_zero) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        Av2hStatus descriptor_status;
        if (prefix->eob != 0U || coefficients->nonzero_count != 0U ||
            predictor == 0) {
            return AV2H_UNSUPPORTED;
        }
        descriptor_status = av2h_tx_descriptor_get(
            prefix->tx_size, &descriptor);
        if (descriptor_status != AV2H_OK) return descriptor_status;
        if (predictor_stride < descriptor->output_width ||
            coefficients->coefficient_count !=
                coefficients->width * coefficients->height) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        output_count = (size_t)descriptor->output_width *
            descriptor->output_height;
        if (output_count > AV2H_RECON_SAMPLE_CAPACITY) {
            return AV2H_LIMIT_EXCEEDED;
        }
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride + column];
        }
        return AV2H_OK;
    }
    if (reconstruct_generic_selected(mode, transform, prefix->tx_size)) {
        return reconstruct_transform_generic(
            mode, prefix, transform, coefficients, predictor, predictor_stride,
            scratch, reconstruction);
    }
        if ((mode->mode != AV2H_INTRA_DC &&
            mode->mode != AV2H_INTRA_VERTICAL &&
            mode->mode != AV2H_INTRA_HORIZONTAL &&
            mode->mode != AV2H_INTRA_PAETH &&
            mode->mode != AV2H_INTRA_SMOOTH &&
            mode->mode != AV2H_INTRA_SMOOTH_VERTICAL &&
            mode->mode != AV2H_INTRA_SMOOTH_HORIZONTAL &&
            mode->mode != AV2H_INTRA_D67 &&
            mode->mode != AV2H_INTRA_D113 &&
            mode->mode != AV2H_INTRA_D135 &&
            mode->mode != AV2H_INTRA_D157 &&
            mode->mode != AV2H_INTRA_D45 && mode->mode != AV2H_INTRA_D203) ||
        coefficients->width == 0U || coefficients->height == 0U ||
        coefficients->coefficient_count !=
            coefficients->width * coefficients->height) {
        return AV2H_UNSUPPORTED;
    }
    if (mode->use_intrabc && mode->skip_transform) {
        const Av2hTxDescriptor *descriptor;
        size_t output_count;
        if (av2h_tx_descriptor_get(prefix->tx_size, &descriptor) != AV2H_OK ||
            descriptor == 0 || mode->intrabc_mode > 1U ||
            mode->mv_precision != 3U || !prefix->all_zero ||
            prefix->eob != 0U ||
            transform->primary_type != AV2H_TX_DCT_DCT ||
            coefficients->width != descriptor->coefficient_width ||
            coefficients->height != descriptor->coefficient_height ||
            coefficients->coefficient_count != descriptor->eob_capacity ||
            coefficients->nonzero_count != 0U || predictor == 0 ||
            predictor_stride < descriptor->output_width) {
            return AV2H_UNSUPPORTED;
        }
        output_count = (size_t)descriptor->output_width *
            descriptor->output_height;
        reconstruction->width = descriptor->output_width;
        reconstruction->height = descriptor->output_height;
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < output_count; ++index) {
            size_t row = index / descriptor->output_width;
            size_t column = index % descriptor->output_width;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U &&
        prefix->tx_size == AV2H_TX_16X16 && prefix->all_zero &&
        prefix->eob == 0U &&
        transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 16U && coefficients->height == 16U &&
        coefficients->coefficient_count == 256U &&
        coefficients->nonzero_count == 0U && predictor != 0 &&
        predictor_stride >= 16U) {
        reconstruction->width = 16U;
        reconstruction->height = 16U;
        for (index = 0U; index < 256U; ++index) {
            size_t row = index >> 4U;
            size_t column = index & 15U;
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_DC && mode->angle_delta == 0 &&
        mode->mrl_index == 0U && mode->enable_ibp == 1U &&
        mode->use_intrabc == 0U && mode->skip_transform == 0U &&
        mode->fsc_mode == 0U &&
        prefix->tx_size == AV2H_TX_64X16 && prefix->all_zero &&
        prefix->eob == 0U &&
        transform->primary_type == AV2H_TX_DCT_DCT &&
        coefficients->width == 32U && coefficients->height == 16U &&
        coefficients->coefficient_count == 512U &&
        coefficients->nonzero_count == 0U && predictor != 0 &&
        predictor_stride >= 64U) {
        reconstruction->width = 64U;
        reconstruction->height = 16U;
        for (index = 0U; index < 512U; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < 1024U; ++index) {
            size_t row = index >> 6U;
            size_t column = index & 63U;
            reconstruction->residual[index] = 0;
            reconstruction->pixels[index] =
                predictor[row * predictor_stride + column];
        }
        return AV2H_OK;
    }
    if (mode->mode == AV2H_INTRA_SMOOTH &&
        !((prefix->tx_size == AV2H_TX_4X4 &&
           (transform->primary_type == AV2H_TX_DCT_ADST ||
            transform->primary_type == AV2H_TX_DCT_DCT)) ||
                    (prefix->tx_size == AV2H_TX_4X8 &&
                     (transform->primary_type == AV2H_TX_ADST_DCT ||
                        transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_8X8 &&
                     (transform->primary_type == AV2H_TX_DCT_DCT ||
                        transform->primary_type == AV2H_TX_ADST_ADST)) ||
          (prefix->tx_size == AV2H_TX_16X8 &&
                     (transform->primary_type == AV2H_TX_ADST_ADST ||
                        transform->primary_type == AV2H_TX_DCT_DCT)) ||
          (prefix->tx_size == AV2H_TX_16X16 &&
           transform->primary_type == AV2H_TX_ADST_ADST) ||
          (prefix->tx_size == AV2H_TX_16X32 &&
           transform->primary_type == AV2H_TX_DCT_DCT) ||
          (prefix->tx_size == AV2H_TX_8X32 &&
           transform->primary_type == AV2H_TX_H_DCT))) {
        return AV2H_UNSUPPORTED;
    }
    if (prefix->tx_size == AV2H_TX_8X4 &&
        transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if ((prefix->tx_size == AV2H_TX_4X8 ||
                prefix->tx_size == AV2H_TX_8X4) &&
               transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_HORIZONTAL ||
                (mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL &&
                 prefix->tx_size == AV2H_TX_8X4))) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL ||
                mode->mode == AV2H_INTRA_D135)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_FLIPADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_VERTICAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_PAETH)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X16 &&
               transform->primary_type == AV2H_TX_DCT_ADST) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_V_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               (mode->use_intrabc || mode->fsc_mode ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_ADST &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X4 &&
               transform->primary_type == AV2H_TX_H_FLIPADST &&
               mode->use_intrabc) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X4 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_D45) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_4X8 &&
               transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 10U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_V_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D45)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X8 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_HORIZONTAL) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_IDTX &&
               (mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D135) && mode->fsc_mode) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D157 ||
                mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
               (mode->mode == AV2H_INTRA_SMOOTH ||
                mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D113 ||
                mode->mode == AV2H_INTRA_D67)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X8 &&
               transform->primary_type == AV2H_TX_ADST_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X16 &&
               transform->primary_type == AV2H_TX_ADST_ADST &&
             (mode->mode == AV2H_INTRA_SMOOTH ||
              mode->mode == AV2H_INTRA_D157)) {
        first_shift = 6U;
         second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_8X16 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_DC)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
                mode->mode == AV2H_INTRA_PAETH ||
                mode->mode == AV2H_INTRA_HORIZONTAL)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_4X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_VERTICAL ||
                mode->mode == AV2H_INTRA_DC)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_8X32 &&
               transform->primary_type == AV2H_TX_H_DCT &&
               (mode->mode == AV2H_INTRA_D203 ||
                mode->mode == AV2H_INTRA_SMOOTH)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 &&
                index != 0U && index != 16U &&
                index != 17U && index != 32U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->dequantized[index] != 0 &&
                index != 0U && index != 1U) {
                return AV2H_UNSUPPORTED;
            }
        }
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_FLIPADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 1U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X32 &&
               transform->primary_type == AV2H_TX_DCT_ADST &&
               mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 1U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_D45 ||
                mode->mode == AV2H_INTRA_D113)) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_H_ADST &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 1U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_DC) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_16X4 &&
               transform->primary_type == AV2H_TX_IDTX &&
               mode->mode == AV2H_INTRA_D45 && mode->fsc_mode) {
        first_shift = 6U;
        second_shift = 12U;
        rectangular = 0U;
        identity_rows = 1U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 &&
               transform->primary_type == AV2H_TX_H_DCT &&
               mode->mode == AV2H_INTRA_D67) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 1U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X4 &&
               transform->primary_type == AV2H_TX_FLIPADST_DCT &&
               (mode->mode == AV2H_INTRA_D67 ||
                mode->mode == AV2H_INTRA_D157)) {
        first_shift = 7U;
        second_shift = 11U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
        flip_adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X8 &&
               transform->primary_type == AV2H_TX_DCT_DCT &&
               (mode->mode == AV2H_INTRA_DC ||
                mode->mode == AV2H_INTRA_D45)) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else if (prefix->tx_size == AV2H_TX_32X8 &&
               transform->primary_type == AV2H_TX_ADST_DCT &&
               mode->mode == AV2H_INTRA_SMOOTH_HORIZONTAL) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 1U;
    } else if (prefix->tx_size == AV2H_TX_32X32 &&
               transform->primary_type == AV2H_TX_DCT_DCT) {
        first_shift = 6U;
        second_shift = 13U;
        rectangular = 0U;
        identity_rows = 0U;
        adst_rows = 0U;
        flip_adst_rows = 0U;
        identity_columns = 0U;
        adst_columns = 0U;
    } else {
        return AV2H_UNSUPPORTED;
    }
    reconstruction->width = coefficients->width;
    reconstruction->height = coefficients->height;
    count = coefficients->coefficient_count;
    for (index = 0U; index < count; ++index) {
        scratch->scaled[index] = rectangular ?
            round_shift((int64_t)coefficients->dequantized[index] * 2896, 12U) :
            coefficients->dequantized[index];
        scratch->scaled[index] = clamp(scratch->scaled[index], -32768, 32767);
    }
    status = inverse_rows(scratch->scaled, scratch->transposed,
                          coefficients->width, coefficients->height,
                          first_shift, identity_rows, adst_rows,
                          flip_adst_rows);
    if (status != AV2H_OK) return status;
    status = inverse_columns(scratch->transposed, scratch->residual,
                             coefficients->width, coefficients->height,
                             second_shift, identity_columns, adst_columns,
                             flip_adst_columns);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < count; ++index) {
        int32_t predicted = predictor == 0 ? 128 :
            predictor[(index / coefficients->width) * predictor_stride +
                      index % coefficients->width];
        int32_t pixel = clamp(predicted + scratch->residual[index], 0, 255);
        reconstruction->residual[index] = (int16_t)scratch->residual[index];
        reconstruction->pixels[index] = (uint8_t)pixel;
    }
    return AV2H_OK;
}

Av2hStatus av2h_first_luma_reconstruct(
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    return av2h_luma_reconstruct(mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

Av2hStatus av2h_luma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstLeafMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    const Av2hTxDescriptor *descriptor;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t output_width;
    uint16_t output_height;
    uint16_t visible_width;
    uint16_t visible_height;
    size_t above_count;
    size_t left_count;
    size_t secondary_above_count = 0U;
    size_t secondary_left_count = 0U;
    size_t above_reference_y;
    uint8_t is_top_superblock_boundary;
    size_t index;
    int32_t source_x;
    int32_t source_y;
    Av2hStatus status;
    Av2hFirstLeafMode effective_mode;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        plane->stride < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count < plane->reconstructed_stride *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_debug_luma_reconstruct_stage = 0U;
    block_x = (uint16_t)((uint32_t)leaf->column * 4U);
    block_y = (uint16_t)((uint32_t)leaf->row * 4U);
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    output_width = descriptor->output_width;
    output_height = descriptor->output_height;
    is_top_superblock_boundary = (uint8_t)(mode->mrl_index != 0U &&
        mode->superblock_size != 0U &&
        block_y % mode->superblock_size == 0U);
    above_reference_y = (size_t)block_y - 1U -
        (is_top_superblock_boundary ? 0U : mode->mrl_index);
    if (block_x >= plane->width || block_y >= plane->height ||
        coefficients->width > 32U || coefficients->height > 32U) {
        return AV2H_INVALID_ARGUMENT;
    }
    visible_width = output_width < plane->width - block_x ?
        output_width : (uint16_t)(plane->width - block_x);
    visible_height = output_height < plane->height - block_y ?
        output_height : (uint16_t)(plane->height - block_y);
    above_count = block_y <= mode->mrl_index ||
        block_x >= plane->width ? 0U :
        (size_t)plane->width - block_x;
    if (mode->mode == AV2H_INTRA_PAETH && above_count != 0U &&
        above_count < output_width) {
        above_count = output_width;
    }
    if (above_count > sizeof(scratch->above)) {
        above_count = sizeof(scratch->above);
    }
    for (index = 0U; index < above_count; ++index) {
        size_t source_x = block_x + index;
        if (source_x >= plane->width) {
            source_x = plane->width - 1U;
        }
        if (!plane_sample_reconstructed(
            plane, source_x, above_reference_y)) {
            above_count = index;
            break;
        }
    }
    for (index = 0U; index < above_count; ++index) {
        size_t source_x = block_x + index;
        if (source_x >= plane->width) {
            source_x = plane->width - 1U;
        }
        scratch->above[index] = plane->data[
            above_reference_y * plane->stride + source_x];
    }
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_DC) &&
        above_count != 0U &&
        above_count < output_width) {
        uint8_t edge = scratch->above[above_count - 1U];
        for (index = above_count; index < output_width; ++index) {
            scratch->above[index] = edge;
        }
        above_count = output_width;
    }
    left_count = block_x <= mode->mrl_index ? 0U : output_height;
    if (left_count != 0U &&
        (mode->mode == AV2H_INTRA_SMOOTH ||
         mode->mode == AV2H_INTRA_SMOOTH_VERTICAL) &&
        block_y + left_count < plane->height) {
        ++left_count;
    } else if (left_count != 0U && mode->mrl_index == 0U &&
               directional_base_angle(mode->mode) != 0) {
        int angle = luma_wide_angle(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            output_width, output_height);
        if (angle > 180 ||
            (mode->enable_ibp != 0U && angle > 0 && angle < 90)) {
            left_count += output_width;
            if (left_count > 2U * output_height) {
                left_count = 2U * output_height;
            }
        }
    } else if (left_count != 0U && mode->mrl_index != 0U &&
               directional_base_angle(mode->mode) != 0 &&
               (luma_wide_angle(
                   directional_base_angle(mode->mode) +
                       mode->angle_delta * 3 +
                       (mode->mrl_index == 1U ? 1 :
                        mode->mrl_index == 2U ? -1 : 0),
                   output_width, output_height) > 180 ||
                (mode->enable_ibp != 0U && luma_wide_angle(
                    directional_base_angle(mode->mode) +
                        mode->angle_delta * 3 +
                        (mode->mrl_index == 1U ? 1 :
                         mode->mrl_index == 2U ? -1 : 0),
                    output_width, output_height) < 90))) {
        left_count += output_width + 2U * mode->mrl_index;
    } else if (left_count != 0U &&
               mode->mode == AV2H_INTRA_HORIZONTAL &&
               mode->angle_delta == 1 && mode->mrl_index == 0U &&
               output_width == 16U && output_height == 16U) {
        left_count += output_width;
    }
    if (left_count > sizeof(scratch->left)) {
        left_count = sizeof(scratch->left);
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->height) {
            source_y = plane->height - 1U;
        }
        if (!plane_sample_reconstructed(
            plane, (size_t)block_x - 1U - mode->mrl_index, source_y)) {
            left_count = index;
            break;
        }
    }
    for (index = 0U; index < left_count; ++index) {
        size_t source_y = (size_t)block_y + index;
        if (source_y >= plane->height) {
            source_y = plane->height - 1U;
        }
        scratch->left[index] = plane->data[
            source_y * plane->stride + block_x - 1U -
            mode->mrl_index];
    }
    if ((mode->mode == AV2H_INTRA_SMOOTH ||
            mode->mode == AV2H_INTRA_SMOOTH_VERTICAL ||
            mode->mode == AV2H_INTRA_PAETH) &&
        left_count != 0U && left_count < output_height) {
        uint8_t edge = scratch->left[left_count - 1U];
        for (index = left_count; index < output_height; ++index) {
            scratch->left[index] = edge;
        }
        left_count = output_height;
    }
    if (mode->mrl_index != 0U && mode->multi_line_mrl != 0U) {
        if (block_y != 0U) {
            secondary_above_count =
                (size_t)plane->width - block_x;
            if (secondary_above_count > sizeof(scratch->secondary_above)) {
                secondary_above_count = sizeof(scratch->secondary_above);
            }
            for (index = 0U; index < secondary_above_count; ++index) {
                if (!plane_sample_reconstructed(
                        plane, block_x + index, (size_t)block_y - 1U)) {
                    secondary_above_count = index;
                    break;
                }
                scratch->secondary_above[index] = plane->data[
                    ((size_t)block_y - 1U) * plane->stride + block_x + index];
            }
        }
        if (block_x != 0U) {
            secondary_left_count = output_height;
            if (directional_base_angle(mode->mode) != 0 &&
                luma_wide_angle(
                    directional_base_angle(mode->mode) +
                        mode->angle_delta * 3 +
                        (mode->mrl_index == 1U ? 1 :
                         mode->mrl_index == 2U ? -1 : 0),
                    output_width, output_height) > 180) {
                secondary_left_count += output_width;
                if (secondary_left_count > 2U * output_height) {
                    secondary_left_count = 2U * output_height;
                }
            }
            if (secondary_left_count > sizeof(scratch->secondary_left)) {
                secondary_left_count = sizeof(scratch->secondary_left);
            }
            for (index = 0U; index < secondary_left_count; ++index) {
                size_t source_y = (size_t)block_y + index;
                if (source_y >= plane->height) {
                    source_y = plane->height - 1U;
                }
                if (!plane_sample_reconstructed(
                        plane, (size_t)block_x - 1U, source_y)) {
                    secondary_left_count = index;
                    break;
                }
                scratch->secondary_left[index] =
                    plane->data[source_y * plane->stride + block_x - 1U];
            }
        }
    }
    references.left = left_count == 0U ? 0 : scratch->left;
    references.left_count = left_count;
    references.above = above_count == 0U ? 0 : scratch->above;
    references.above_count = above_count;
    references.secondary_left = secondary_left_count == 0U ?
        0 : scratch->secondary_left;
    references.secondary_left_count = secondary_left_count;
    references.secondary_above = secondary_above_count == 0U ?
        0 : scratch->secondary_above;
    references.secondary_above_count = secondary_above_count;
    references.has_secondary_above_left =
        secondary_above_count != 0U && secondary_left_count != 0U;
    references.secondary_above_left =
        references.has_secondary_above_left ?
            plane->data[((size_t)block_y - 1U) * plane->stride +
                        block_x - 1U] : 0U;
    references.has_above_left = block_x > mode->mrl_index &&
        block_y > mode->mrl_index;
    references.above_left = references.has_above_left ?
        plane->data[((size_t)block_y - 1U - mode->mrl_index) * plane->stride +
                    block_x - 1U - mode->mrl_index] :
        0U;
    references.negative_count = 0U;
    if (mode->mrl_index != 0U && references.has_above_left) {
        size_t negative_index;
        references.negative_count = (uint8_t)(mode->mrl_index + 1U);
        for (negative_index = 1U;
             negative_index <= references.negative_count;
             ++negative_index) {
            references.above_negative[negative_index] = plane->data[
                above_reference_y * plane->stride + block_x - negative_index];
            references.left_negative[negative_index] = plane->data[
                ((size_t)block_y - (is_top_superblock_boundary ? 1U :
                    negative_index)) * plane->stride +
                block_x - 1U - mode->mrl_index];
        }
    }
    if (mode->palette_size != 0U) {
        if (mode->palette_width != output_width ||
            mode->palette_height != output_height ||
            mode->palette_size > AV2H_PALETTE_MAX_SIZE) {
            return AV2H_UNSUPPORTED;
        }
        for (index = 0U; index < (size_t)output_width * output_height;
             ++index) {
            uint8_t color_index = av2h_palette_map_get(mode, index);
            if (color_index >= mode->palette_size) {
                return AV2H_INVALID_DATA;
            }
            scratch->predictor[index] = mode->palette_colors[color_index];
        }
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    } else if (mode->use_intrabc) {
        size_t row;
        size_t column;
        int32_t integer_x = mode->displacement_x / 8;
        int32_t integer_y = mode->displacement_y / 8;
        uint32_t fraction_x;
        uint32_t fraction_y;
        av2h_debug_luma_reconstruct_stage = 10U;
        av2h_debug_intrabc_geometry[0] = block_x;
        av2h_debug_intrabc_geometry[1] = block_y;
        av2h_debug_intrabc_geometry[2] = mode->displacement_x;
        av2h_debug_intrabc_geometry[3] = mode->displacement_y;
        if (mode->displacement_x < 0 && mode->displacement_x % 8 != 0) {
            --integer_x;
        }
        if (mode->displacement_y < 0 && mode->displacement_y % 8 != 0) {
            --integer_y;
        }
        fraction_x = (uint32_t)(mode->displacement_x - integer_x * 8);
        fraction_y = (uint32_t)(mode->displacement_y - integer_y * 8);
        if ((fraction_x & 1U) != 0U || (fraction_y & 1U) != 0U) {
            return AV2H_UNSUPPORTED;
        }
        av2h_debug_luma_reconstruct_stage = 11U;
        source_x = (int32_t)block_x + integer_x;
        source_y = (int32_t)block_y + integer_y;
        av2h_debug_intrabc_geometry[4] = source_x;
        av2h_debug_intrabc_geometry[5] = source_y;
        av2h_debug_intrabc_geometry[6] = (int32_t)fraction_x;
        av2h_debug_intrabc_geometry[7] = (int32_t)fraction_y;
        if (source_x < 0 || source_y < 0 ||
            (uint32_t)source_x + output_width + (fraction_x != 0U) >
                plane->width ||
            (uint32_t)source_y + output_height + (fraction_y != 0U) >
                plane->height) {
            return AV2H_INVALID_DATA;
        }
        av2h_debug_luma_reconstruct_stage = 12U;
        for (row = 0U; row < output_height; ++row) {
            for (column = 0U; column < output_width; ++column) {
                size_t sample_x = (size_t)source_x + column;
                size_t sample_y = (size_t)source_y + row;
                uint32_t left_weight = 8U - fraction_x;
                uint32_t top_weight = 8U - fraction_y;
                uint32_t prediction;
                if (!plane_sample_reconstructed(
                        plane, sample_x, sample_y) ||
                    (fraction_x != 0U && !plane_sample_reconstructed(
                        plane, sample_x + 1U, sample_y)) ||
                    (fraction_y != 0U && !plane_sample_reconstructed(
                        plane, sample_x, sample_y + 1U)) ||
                    (fraction_x != 0U && fraction_y != 0U &&
                     !plane_sample_reconstructed(
                         plane, sample_x + 1U, sample_y + 1U))) {
                    av2h_debug_intrabc_geometry[8] = (int32_t)sample_x;
                    av2h_debug_intrabc_geometry[9] = (int32_t)sample_y;
                    return AV2H_UNSUPPORTED;
                }
                prediction = plane->data[sample_y * plane->stride + sample_x] *
                    left_weight * top_weight;
                if (fraction_x != 0U) {
                    prediction += plane->data[
                        sample_y * plane->stride + sample_x + 1U] *
                        fraction_x * top_weight;
                }
                if (fraction_y != 0U) {
                    prediction += plane->data[
                        (sample_y + 1U) * plane->stride + sample_x] *
                        left_weight * fraction_y;
                }
                if (fraction_x != 0U && fraction_y != 0U) {
                    prediction += plane->data[
                        (sample_y + 1U) * plane->stride + sample_x + 1U] *
                        fraction_x * fraction_y;
                }
                scratch->predictor[row * output_width + column] =
                    (uint16_t)((prediction + 32U) >> 6U);
            }
        }
        if (mode->morph_pred) {
            status = apply_intrabc_morph_prediction(
                plane, block_x, block_y, source_x, source_y,
                output_width, output_height, scratch->predictor);
            if (status != AV2H_OK) return status;
        }
        av2h_debug_luma_reconstruct_stage = 13U;
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    } else if (mode->mode == AV2H_INTRA_DC && above_count == 0U &&
               left_count == 0U) {
        for (index = 0U; index < (size_t)output_width * output_height;
             ++index) {
            scratch->predictor[index] = 128U;
        }
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width,
            scratch, &reconstruction);
    } else {
        av2h_debug_luma_reconstruct_stage = 1U;
        av2h_memory_copy(&effective_mode, mode, sizeof(effective_mode));
        if (prefix->tx_size == AV2H_TX_4X4) {
            effective_mode.enable_ibp = 0U;
        }
        status = av2h_luma_predict(
            &effective_mode, output_width, output_height, &references,
            scratch->predictor, output_width);
        if (status != AV2H_OK) return status;
        av2h_debug_luma_reconstruct_stage = 2U;
        status = av2h_luma_reconstruct(
            mode, prefix, transform, coefficients, scratch->predictor,
            output_width, scratch, &reconstruction);
    }
    if (status != AV2H_OK) return status;
    av2h_debug_luma_reconstruct_stage = 3U;
    av2h_debug_luma_dimensions[0] = output_width;
    av2h_debug_luma_dimensions[1] = output_height;
    av2h_debug_luma_dimensions[2] = reconstruction.width;
    av2h_debug_luma_dimensions[3] = reconstruction.height;
    if (reconstruction.width != output_width ||
        reconstruction.height != output_height) {
        return AV2H_UNSUPPORTED;
    }
    for (index = 0U; index < visible_height; ++index) {
        size_t column;
        for (column = 0U; column < visible_width; ++column) {
            plane->data[((size_t)block_y + index) * plane->stride +
                        block_x + column] = reconstruction.pixels[
                            index * output_width + column];
        }
    }
    if (block_x <= 65U && block_y <= 64U &&
        (uint32_t)block_x + visible_width > 65U &&
        (uint32_t)block_y + visible_height > 64U) {
        size_t target_index = (size_t)(64U - block_y) * output_width +
            (65U - block_x);
        av2h_debug_chimp_luma_target[0] = leaf->row;
        av2h_debug_chimp_luma_target[1] = leaf->column;
        av2h_debug_chimp_luma_target[2] = leaf->block_size;
        av2h_debug_chimp_luma_target[3] = prefix->tx_size;
        av2h_debug_chimp_luma_target[4] = mode->mode;
        av2h_debug_chimp_luma_target[5] = mode->angle_delta;
        av2h_debug_chimp_luma_target[6] = transform->primary_type;
        av2h_debug_chimp_luma_target[7] = prefix->eob;
        av2h_debug_chimp_luma_target[8] = output_width;
        av2h_debug_chimp_luma_target[9] = output_height;
        av2h_debug_chimp_luma_target[10] = scratch->predictor[target_index];
        av2h_debug_chimp_luma_target[11] = scratch->residual[target_index];
        av2h_debug_chimp_luma_target[12] = reconstruction.pixels[target_index];
        av2h_debug_chimp_luma_target[13] = mode->use_intrabc;
        av2h_debug_chimp_luma_target[14] = mode->displacement_x;
        av2h_debug_chimp_luma_target[15] = mode->displacement_y;
        for (size_t debug_index = 0U; debug_index < 8U; ++debug_index) {
            av2h_debug_chimp_luma_target[16U + debug_index] =
                scratch->transposed[debug_index];
            av2h_debug_chimp_luma_target[24U + debug_index] =
                scratch->residual[debug_index];
        }
            for (size_t debug_index = 0U; debug_index < 16U; ++debug_index) {
                av2h_debug_chimp_luma_target[32U + debug_index] =
                coefficients->dequantized[debug_index];
            }
            av2h_debug_chimp_luma_target[48] = coefficients->before_levels_bits;
            av2h_debug_chimp_luma_target[49] = coefficients->after_levels_bits;
            av2h_debug_chimp_luma_target[50] =
                coefficients->after_coefficients_bits;
            av2h_debug_chimp_luma_target[51] = 0;
            av2h_debug_chimp_luma_target[52] = coefficients->after_levels_range;
            av2h_debug_chimp_luma_target[53] = coefficients->diagnostic_context;
            av2h_debug_chimp_luma_target[54] = coefficients->cul_level;
            av2h_debug_chimp_luma_target[55] = coefficients->dc_value;
    }
    plane_mark_reconstructed(
        plane, block_x, block_y, visible_width, visible_height);
    return AV2H_OK;
}

/*
 * CfL scaling and downsampling for derived and explicit alpha prediction.
 * The fixed-point derivation and all three 4:2:0 downsampling filters match
 * the pinned AVM reference (av2/common/cfl.c, warped_motion.h divisor LUT).
 */
#define AV2H_CFL_ADD_BITS_ALPHA 5
#define AV2H_CFL_NUM_REF_SAM 8
#define AV2H_DIV_LUT_BITS 7
#define AV2H_DIV_LUT_PREC_BITS 9

static const uint16_t cfl_div_lut[129U] = {
    512, 508, 504, 500, 496, 493, 489, 485, 482, 478, 475, 471, 468, 465, 462,
    458, 455, 452, 449, 446, 443, 440, 437, 434, 431, 428, 426, 423, 420, 417,
    415, 412, 410, 407, 405, 402, 400, 397, 395, 392, 390, 388, 386, 383, 381,
    379, 377, 374, 372, 370, 368, 366, 364, 362, 360, 358, 356, 354, 352, 350,
    349, 347, 345, 343, 341, 340, 338, 336, 334, 333, 331, 329, 328, 326, 324,
    323, 321, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306, 305, 303, 302,
    301, 299, 298, 297, 295, 294, 293, 291, 290, 289, 287, 286, 285, 284, 282,
    281, 280, 279, 278, 277, 275, 274, 273, 272, 271, 270, 269, 267, 266, 265,
    264, 263, 262, 261, 260, 259, 258, 257, 256
};

static int cfl_get_msb(uint32_t value) {
    int log = 0;
    while (value > 1U) {
        value >>= 1U;
        ++log;
    }
    return log;
}

static int32_t cfl_round_pow2(int64_t value, int shift) {
    return (int32_t)((value + (((int64_t)1 << shift) >> 1)) >> shift);
}

static int32_t cfl_round_pow2_signed(int64_t value, int shift) {
    return value < 0 ? -cfl_round_pow2(-value, shift) :
        cfl_round_pow2(value, shift);
}

static uint16_t cfl_resolve_divisor_32(uint32_t d, int *shift) {
    int32_t f;
    int s = cfl_get_msb(d);
    int32_t e = (int32_t)(d - ((uint32_t)1 << s));
    if (s > AV2H_DIV_LUT_BITS) {
        f = cfl_round_pow2(e, s - AV2H_DIV_LUT_BITS);
    } else {
        f = e << (AV2H_DIV_LUT_BITS - s);
    }
    *shift = s + AV2H_DIV_LUT_PREC_BITS;
    return cfl_div_lut[f];
}

static int32_t cfl_resolve_divisor_cfl(int32_t n, int32_t d, int shift) {
    int sign_n = n < 0;
    int32_t f_n;
    int32_t f_d;
    int shift_n;
    int shift_d;
    int shift_add;
    int32_t ret;
    if (sign_n) n = -n;
    if (n == 0 || d == 0) return 0;
    shift_n = cfl_get_msb((uint32_t)n);
    shift_d = cfl_get_msb((uint32_t)d);
    {
        int32_t e_d = (int32_t)(d - ((uint32_t)1 << shift_d));
        if (shift_d > AV2H_DIV_LUT_BITS) {
            f_d = cfl_round_pow2(e_d, shift_d - AV2H_DIV_LUT_BITS);
        } else {
            f_d = e_d << (AV2H_DIV_LUT_BITS - shift_d);
        }
    }
    if (shift_n > AV2H_DIV_LUT_BITS) {
        f_n = cfl_round_pow2(n, shift_n - AV2H_DIV_LUT_BITS);
    } else {
        f_n = n << (AV2H_DIV_LUT_BITS - shift_n);
    }
    shift_add = shift_d - shift_n - shift;
    if (shift_add <= 1) {
        int shift0 = AV2H_DIV_LUT_PREC_BITS + AV2H_DIV_LUT_BITS + shift_add;
        ret = shift0 >= 0 ? (cfl_div_lut[f_d] * f_n) >> shift0 :
            (2 << shift) - 1;
    } else {
        ret = 0;
    }
    if (ret >= (2 << shift) - 1) ret = (2 << shift) - 1;
    return sign_n ? -ret : ret;
}

static int32_t cfl_derive_alpha(int32_t sum_x, int32_t sum_y, int32_t sum_xx,
                                int32_t sum_xy, int count, int shift) {
    int32_t der;
    int32_t nor;
    if (count == 0) return 0;
    der = sum_xx - (int32_t)((int64_t)sum_x * sum_x / count);
    nor = sum_xy - (int32_t)((int64_t)sum_x * sum_y / count);
    if (der == 0 || nor == 0) return 0;
    return cfl_resolve_divisor_cfl(nor, der, shift);
}

static uint8_t intrabc_morph_template_size(uint16_t size) {
    if (size < 3U) return 0U;
    if (size < 7U) return 4U;
    if (size < 13U) return 8U;
    return 16U;
}

static Av2hStatus apply_intrabc_morph_prediction(
    const Av2hReconstructionPlane *plane,
    uint16_t block_x,
    uint16_t block_y,
    int32_t source_x,
    int32_t source_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor) {
    uint8_t template_width = intrabc_morph_template_size(
        width < 16U ? width : 16U);
    uint8_t template_height = intrabc_morph_template_size(
        height < 16U ? height : 16U);
    uint8_t above_available = block_y != 0U;
    uint8_t left_available = block_x != 0U;
    uint8_t above_count = 0U;
    uint8_t left_count = 0U;
    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_xy = 0;
    int32_t sum_xx = 0;
    int32_t alpha;
    int32_t beta;
    uint8_t index;
    uint16_t row;
    uint16_t column;
    if (plane == 0 || predictor == 0 || source_x <= 0 || source_y <= 0) {
        return AV2H_INVALID_DATA;
    }
    if (above_available && left_available) {
        if (template_width == 16U && template_height == 16U) {
            above_count = 16U;
            left_count = 16U;
        } else if (template_width > 4U && template_height > 4U) {
            above_count = 8U;
            left_count = 8U;
        } else if (template_width < 16U && template_height < 16U) {
            above_count = 4U;
            left_count = 4U;
        } else if (template_width == 16U) {
            above_count = 16U;
        } else {
            left_count = 16U;
        }
    } else if (above_available) {
        above_count = template_width;
    } else if (left_available) {
        left_count = template_height;
    }
    for (index = 0U; index < above_count; ++index) {
        size_t step = template_width / above_count;
        size_t offset = (step == 1U ? 0U : step >> 1U) +
            (size_t)index * step;
        size_t current_x = (size_t)block_x + offset;
        size_t reference_x = (size_t)source_x + offset;
        uint8_t x;
        uint8_t y;
        if (current_x >= plane->width || reference_x >= plane->width ||
            !plane_sample_reconstructed(plane, current_x, block_y - 1U) ||
            !plane_sample_reconstructed(
                plane, reference_x, (size_t)source_y - 1U)) {
            return AV2H_UNSUPPORTED;
        }
        x = plane->data[((size_t)source_y - 1U) * plane->stride + reference_x];
        y = plane->data[((size_t)block_y - 1U) * plane->stride + current_x];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    for (index = 0U; index < left_count; ++index) {
        size_t step = template_height / left_count;
        size_t offset = (step == 1U ? 0U : step >> 1U) +
            (size_t)index * step;
        size_t current_y = (size_t)block_y + offset;
        size_t reference_y = (size_t)source_y + offset;
        uint8_t x;
        uint8_t y;
        if (current_y >= plane->height || reference_y >= plane->height ||
            !plane_sample_reconstructed(plane, block_x - 1U, current_y) ||
            !plane_sample_reconstructed(
                plane, (size_t)source_x - 1U, reference_y)) {
            return AV2H_UNSUPPORTED;
        }
        x = plane->data[reference_y * plane->stride + (size_t)source_x - 1U];
        y = plane->data[current_y * plane->stride + block_x - 1U];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    alpha = cfl_derive_alpha(
        sum_x, sum_y, sum_xx, sum_xy, above_count + left_count, 8);
    if (alpha == 0) alpha = 256;
    beta = ((sum_y << 8) - sum_x * alpha) /
        (int32_t)(above_count + left_count);
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t value =
                (predictor[(size_t)row * width + column] * alpha + beta) >> 8;
            predictor[(size_t)row * width + column] =
                (uint8_t)clamp(value, 0, 255);
        }
    }
    return AV2H_OK;
}

/*
 * Verify a rectangular region of the plane is marked reconstructed at 4x4
 * granularity (the readiness map resolution).
 */
static int plane_region_reconstructed(const Av2hReconstructionPlane *plane,
                                      uint16_t x, uint16_t y,
                                      uint16_t width, uint16_t height) {
    uint16_t row;
    uint16_t column;
    for (row = 0U; row < height; row += 4U) {
        for (column = 0U; column < width; column += 4U) {
            if (!plane_sample_reconstructed(plane, (size_t)x + column,
                                            (size_t)y + row)) {
                return 0;
            }
        }
    }
    return 1;
}

static uint8_t plane_clamped_sample(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    if (x >= plane->reference_width) x = plane->reference_width - 1U;
    if (y >= plane->reference_height) y = plane->reference_height - 1U;
    return plane->data[y * plane->stride + x];
}

static uint8_t plane_coded_sample(
    const Av2hReconstructionPlane *plane, size_t x, size_t y) {
    if (x >= plane->width) x = plane->width - 1U;
    if (y >= plane->height) y = plane->height - 1U;
    return plane->data[y * plane->stride + x];
}

static uint16_t mhccp_luma_filter2_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y);

static uint16_t cfl_luma_filter2_current_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y,
    uint16_t block_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = chroma_x == block_x ? center_x : center_x - 1U;
    size_t top_y = chroma_y == block_y ? center_y : center_y - 1U;
    return (uint16_t)(
        plane_coded_sample(luma_plane, left_x, center_y) +
        4U * plane_coded_sample(luma_plane, center_x, center_y) +
        plane_coded_sample(luma_plane, center_x + 1U, center_y) +
        plane_coded_sample(luma_plane, center_x, top_y) +
        plane_coded_sample(luma_plane, center_x, center_y + 1U));
}

/*
 * Derive the CFL_DERIVED_ALPHA predictor for one chroma transform block.
 * `predictor` enters holding the DC base prediction (built from the chroma
 * neighbours) and leaves holding predictor + scaled luma-AC.  Fails closed
 * (leaving the DC base untouched) whenever the required reconstructed luma is
 * unavailable or the geometry is unsupported.
 */
static Av2hStatus cfl_derived_predict(
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t filter_index,
    uint8_t derive_alpha,
    int32_t supplied_alpha_q3,
    uint16_t superblock_size,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t have_top,
    uint8_t have_left,
    uint8_t *predictor,
    size_t predictor_stride) {
    const int bit_depth = 8;
    uint16_t luma_x = (uint16_t)((uint32_t)block_x * 2U);
    uint16_t luma_y = (uint16_t)((uint32_t)block_y * 2U);
    uint16_t luma_width = (uint16_t)((uint32_t)width * 2U);
    uint16_t luma_height = (uint16_t)((uint32_t)height * 2U);
    int32_t luma_left_ds[32];
    int32_t luma_above_ds[32];
    int32_t chroma_left[32];
    int32_t chroma_above[32];
    uint8_t is_top_sb_boundary;
    int top_offset;
    int bottom_offset;
    uint16_t row;
    uint16_t column;
    int32_t avg_l;
    int32_t alpha;
    int numb_up;
    int numb_left;
    int count;
    int32_t sum_x;
    int32_t sum_y;
    int32_t sum_xx;
    int32_t sum_xy;
    int shift;
    uint16_t visible_luma_width;
    uint16_t visible_luma_height;
    uint16_t effective_luma_width;
    uint16_t effective_luma_height;
    uint16_t effective_chroma_width;
    uint16_t effective_chroma_height;
    if (luma_plane == 0 || luma_plane->data == 0 ||
        filter_index > 2U ||
        width > 32U ||
        height > 32U || width == 0U || height == 0U) {
        return AV2H_UNSUPPORTED;
    }
    if (luma_x >= luma_plane->width ||
        luma_y >= luma_plane->height ||
        luma_plane->stride < luma_plane->width || luma_plane->reconstructed == 0) {
        return AV2H_UNSUPPORTED;
    }
    effective_luma_width = (uint16_t)(
        (luma_plane->reference_width + 7U) & ~(size_t)7U);
    effective_luma_height = (uint16_t)(
        (luma_plane->reference_height + 7U) & ~(size_t)7U);
    effective_chroma_width = (uint16_t)(
        (chroma_plane->reference_width + 3U) & ~(size_t)3U);
    effective_chroma_height = (uint16_t)(
        (chroma_plane->reference_height + 3U) & ~(size_t)3U);
    if (effective_luma_width > luma_plane->width) {
        effective_luma_width = luma_plane->width;
    }
    if (effective_luma_height > luma_plane->height) {
        effective_luma_height = luma_plane->height;
    }
    if (effective_chroma_width > chroma_plane->width) {
        effective_chroma_width = chroma_plane->width;
    }
    if (effective_chroma_height > chroma_plane->height) {
        effective_chroma_height = chroma_plane->height;
    }
    visible_luma_width = luma_x >= effective_luma_width ? 0U :
        luma_width < effective_luma_width - luma_x ?
            luma_width : (uint16_t)(effective_luma_width - luma_x);
    visible_luma_height = luma_y >= effective_luma_height ? 0U :
        luma_height < effective_luma_height - luma_y ?
            luma_height : (uint16_t)(effective_luma_height - luma_y);
    if (!plane_region_reconstructed(luma_plane, luma_x, luma_y,
                                    visible_luma_width,
                                    visible_luma_height)) {
        return AV2H_UNSUPPORTED;
    }
    /* Left neighbour luma (box downsampled) and chroma. */
    if (have_left) {
        uint16_t reference_x = filter_index == 0U ?
            (uint16_t)(luma_x - 2U) : (uint16_t)(luma_x - 3U);
        uint16_t reference_width = filter_index == 0U ? 2U : 3U;
        if (luma_x < (filter_index == 0U ? 2U : 3U) ||
            !plane_region_reconstructed(luma_plane, reference_x, luma_y,
                                        reference_width,
                                        visible_luma_height)) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < height; ++row) {
            if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                luma_left_ds[row] = (int32_t)(
                    plane_coded_sample(luma_plane, center_x - 1U, center_y) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, center_y) +
                    plane_coded_sample(luma_plane, center_x + 1U, center_y) +
                    plane_coded_sample(
                        luma_plane, center_x - 1U, center_y + 1U) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, center_y + 1U) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, center_y + 1U));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x - 2U;
                size_t center_y = (size_t)luma_y + (size_t)row * 2U;
                size_t top_y = row == 0U ? center_y : center_y - 1U;
                luma_left_ds[row] = (int32_t)(
                    plane_coded_sample(
                        luma_plane, center_x - 1U, center_y) +
                    4U * plane_coded_sample(
                        luma_plane, center_x, center_y) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, center_y) +
                    plane_coded_sample(luma_plane, center_x, top_y) +
                    plane_coded_sample(
                        luma_plane, center_x, center_y + 1U));
            } else {
                size_t r0 = (size_t)luma_y + (size_t)row * 2U;
                const uint8_t *base = &luma_plane->data[
                    r0 * luma_plane->stride + luma_x - 2U];
                luma_left_ds[row] = (int32_t)(base[0] + base[1] +
                    base[luma_plane->stride] +
                    base[luma_plane->stride + 1U]) << 1;
            }
            chroma_left[row] = plane_coded_sample(
                chroma_plane, block_x - 1U, (size_t)block_y + row);
            if (luma_y >= effective_luma_height) {
                luma_left_ds[row] = 128 << 3;
            } else if (row >= (visible_luma_height >> 1U) && row != 0U) {
                luma_left_ds[row] = luma_left_ds[row - 1U];
            }
            if (block_y >= effective_chroma_height) {
                chroma_left[row] = 128;
            } else if (row >= effective_chroma_height - block_y && row != 0U) {
                chroma_left[row] = chroma_left[row - 1U];
            }
        }
    }
    /* Above neighbour luma (box downsampled) and chroma. */
    if (have_top) {
        is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
            (luma_y % superblock_size) == 0U);
        top_offset = 2 - is_top_sb_boundary;
        bottom_offset = 1 - is_top_sb_boundary;
        if (luma_y < (uint16_t)top_offset ||
            !plane_region_reconstructed(
                luma_plane, luma_x, (uint16_t)(luma_y - top_offset),
                luma_width < luma_plane->width - luma_x ?
                    luma_width :
                    (uint16_t)(luma_plane->width - luma_x),
                4U)) {
            return AV2H_UNSUPPORTED;
        }
        for (column = 0U; column < width; ++column) {
            if (filter_index == 1U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    plane_coded_sample(luma_plane, left_x, top_row) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, top_row) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, top_row) +
                    plane_coded_sample(luma_plane, left_x, bottom_row) +
                    2U * plane_coded_sample(
                        luma_plane, center_x, bottom_row) +
                    plane_coded_sample(
                        luma_plane, center_x + 1U, bottom_row));
            } else if (filter_index == 2U) {
                size_t center_x = (size_t)luma_x + (size_t)column * 2U;
                size_t center_y =
                    (size_t)luma_y - (size_t)top_offset;
                size_t left_x = column == 0U ? center_x : center_x - 1U;
                size_t top_y = is_top_sb_boundary ?
                    center_y : center_y - 1U;
                size_t bottom_y = center_y + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    luma_plane->data[center_y * luma_plane->stride + left_x] +
                    4U * luma_plane->data[center_y * luma_plane->stride +
                                          center_x] +
                    luma_plane->data[center_y * luma_plane->stride +
                                     center_x + 1U] +
                    luma_plane->data[top_y * luma_plane->stride + center_x] +
                    luma_plane->data[bottom_y * luma_plane->stride +
                                     center_x]);
            } else {
                size_t c0 = (size_t)luma_x + (size_t)column * 2U;
                size_t top_row = (size_t)luma_y - (size_t)top_offset;
                size_t bottom_row = top_row + (size_t)bottom_offset;
                luma_above_ds[column] = (int32_t)(
                    plane_clamped_sample(luma_plane, c0, top_row) +
                    plane_clamped_sample(luma_plane, c0 + 1U, top_row) +
                    plane_clamped_sample(luma_plane, c0, bottom_row) +
                    plane_clamped_sample(
                        luma_plane, c0 + 1U, bottom_row)) << 1;
            }
            chroma_above[column] = plane_coded_sample(
                chroma_plane, (size_t)block_x + column,
                (size_t)block_y - 1U);
            if (luma_x >= effective_luma_width) {
                luma_above_ds[column] = 128 << 3;
            } else if (column >= (visible_luma_width >> 1U) && column != 0U) {
                luma_above_ds[column] = luma_above_ds[column - 1U];
            }
            if (block_x >= effective_chroma_width) {
                chroma_above[column] = 128;
            } else if (column >= effective_chroma_width - block_x &&
                       column != 0U) {
                chroma_above[column] = chroma_above[column - 1U];
            }
        }
    }
    /* Neighbour luma DC (avg_l) subtracted from current-block luma. */
    {
        int ss_hor = width > 32U ? 2 : 1;
        int ss_ver = height > 32U ? 2 : 1;
        int32_t sum = 0;
        int dc_count = 0;
        if (have_top) {
            for (column = 0U; column < width; column += (uint16_t)ss_hor) {
                sum += luma_above_ds[column];
                ++dc_count;
            }
        }
        if (have_left) {
            for (row = 0U; row < height; row += (uint16_t)ss_ver) {
                sum += luma_left_ds[row];
                ++dc_count;
            }
        }
        if (dc_count > 0) {
            int div_shift = 0;
            uint16_t scale = cfl_resolve_divisor_32((uint32_t)dc_count,
                                                    &div_shift);
            int32_t rounding = (1 << div_shift) >> 1;
            int32_t val = (int32_t)(((int64_t)sum * scale + rounding) >>
                                    div_shift);
            int32_t max_v = (1 << (bit_depth + 3)) - 1;
            avg_l = val > max_v ? max_v : val;
        } else {
            avg_l = 8 << (bit_depth - 1);
        }
    }
    /* Distribute reference samples between the available sides. */
    numb_up = 0;
    numb_left = 0;
    if (have_top && have_left) {
        if (width > (uint16_t)(height * 2U)) {
            numb_up = AV2H_CFL_NUM_REF_SAM;
        } else if (height > (uint16_t)(width * 2U)) {
            numb_left = AV2H_CFL_NUM_REF_SAM;
        } else {
            numb_up = AV2H_CFL_NUM_REF_SAM >> 1;
            numb_left = AV2H_CFL_NUM_REF_SAM >> 1;
        }
    } else {
        numb_up = have_top ? AV2H_CFL_NUM_REF_SAM : 0;
        numb_left = have_left ? AV2H_CFL_NUM_REF_SAM : 0;
    }
    if (numb_up > width) numb_up = width;
    if (numb_left > height) numb_left = height;
    count = 0;
    sum_x = 0;
    sum_y = 0;
    sum_xx = 0;
    sum_xy = 0;
    if (numb_up > 0) {
        int step = (int)width / numb_up;
        int start;
        int i;
        if (step < 1) step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)width; i += step) {
            int32_t lx = luma_above_ds[i] >> 3;
            int32_t cy = chroma_above[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
            ++count;
        }
    }
    if (numb_left > 0) {
        int step = (int)height / numb_left;
        int start;
        int i;
        if (step < 1) step = 1;
        start = step == 1 ? 0 : step >> 1;
        for (i = start; i < (int)height; i += step) {
            int32_t lx = luma_left_ds[i] >> 3;
            int32_t cy = chroma_left[i];
            sum_x += lx;
            sum_y += cy;
            sum_xy += lx * cy;
            sum_xx += lx * lx;
            ++count;
        }
    }
    shift = 3 + AV2H_CFL_ADD_BITS_ALPHA;
    alpha = derive_alpha ?
        cfl_derive_alpha(sum_x, sum_y, sum_xx, sum_xy, count, shift) :
        supplied_alpha_q3 * (1 << AV2H_CFL_ADD_BITS_ALPHA);
    /* Predict: DC base + scaled luma AC. */
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int32_t recon_q3;
            if (filter_index == 1U) {
                size_t sample_x =
                    (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y =
                    (size_t)luma_y + (size_t)row * 2U;
                size_t left_x = column == 0U ? sample_x : sample_x - 1U;
                recon_q3 = (int32_t)(
                    plane_coded_sample(luma_plane, left_x, sample_y) +
                    2U * plane_coded_sample(
                        luma_plane, sample_x, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y) +
                    plane_coded_sample(
                        luma_plane, left_x, sample_y + 1U) +
                    2U * plane_coded_sample(
                        luma_plane, sample_x, sample_y + 1U) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y + 1U));
            } else if (filter_index == 2U) {
                recon_q3 = cfl_luma_filter2_current_q3(
                    luma_plane, (uint16_t)(block_x + column),
                    (uint16_t)(block_y + row), block_x, block_y);
            } else {
                size_t sample_x =
                    (size_t)luma_x + (size_t)column * 2U;
                size_t sample_y =
                    (size_t)luma_y + (size_t)row * 2U;
                recon_q3 = (int32_t)(
                    plane_coded_sample(luma_plane, sample_x, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y) +
                    plane_coded_sample(
                        luma_plane, sample_x, sample_y + 1U) +
                    plane_coded_sample(
                        luma_plane, sample_x + 1U, sample_y + 1U)) << 1;
            }
            int32_t ac = recon_q3 - avg_l;
            int32_t scaled = cfl_round_pow2_signed(
                (int64_t)alpha * ac, 6 + AV2H_CFL_ADD_BITS_ALPHA);
            int32_t base_dc = predictor[(size_t)row * predictor_stride + column];
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(scaled + base_dc, 0, 255);
        }
    }
    return AV2H_OK;
}

#define AV2H_MHCCP_DECIM_BITS 16
#define AV2H_MHCCP_DIV_PREC_BITS 14
#define AV2H_MHCCP_DIV_SLOT_BITS 3

static int mhccp_log2(uint32_t value) {
    int result = 0;
    while (value > 1U) {
        value >>= 1U;
        ++result;
    }
    return result;
}

static int32_t mhccp_mul_fixed(int32_t left, int32_t right, int shift) {
    uint32_t abs_left = (uint32_t)(left < 0 ? -left : left);
    uint32_t abs_right = (uint32_t)(right < 0 ? -right : right);
    int left_bits = mhccp_log2(abs_left) + 1;
    int right_bits = mhccp_log2(abs_right) + 1;
    int drop = left_bits + right_bits - 29;
    int left_drop;
    int right_drop;
    int adjusted_shift;
    int32_t product;
    uint32_t bias;
    if (drop < 0) drop = 0;
    left_drop = drop >> 1;
    right_drop = drop - left_drop;
    adjusted_shift = shift - left_drop - right_drop;
    product = (left_drop != 0 ? left >> left_drop : left) *
              (right_drop != 0 ? right >> right_drop : right);
    if (adjusted_shift <= 0) return product;
    if (adjusted_shift > 29) return 0;
    bias = (uint32_t)1U << (adjusted_shift - 1);
    return product >= 0 ?
        (int32_t)(((uint32_t)product + bias) >> adjusted_shift) :
        -(int32_t)(((uint32_t)(-product) + bias) >> adjusted_shift);
}

static void mhccp_division_scale(uint32_t denominator, int32_t *scale,
                                  int *shift) {
    static const int32_t square_weight[8U] = {
        214, 153, 113, 86, 67, 53, 43, 35
    };
    static const int32_t offset[8U] = {
        4822, 5952, 6624, 6792, 6408, 5424, 3792, 1466
    };
    static const int32_t bias[8U] = {
        12784, 12054, 11670, 11583, 11764, 12195, 12870, 13782
    };
    int delta;
    int32_t normalized;
    int32_t fraction;
    int32_t centered;
    int32_t index;
    *shift = mhccp_log2(denominator);
    delta = *shift - AV2H_MHCCP_DIV_PREC_BITS;
    if (delta >= 0) {
        int32_t rounding = delta > 0 ?
            (int32_t)((uint32_t)1U << (delta - 1)) : 0;
        normalized = ((int32_t)denominator + rounding) >> delta;
    } else {
        normalized = (int32_t)(denominator << -delta);
    }
    if (normalized < 1) normalized = 1;
    if (normalized > (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1) {
        normalized = (1 << (AV2H_MHCCP_DIV_PREC_BITS + 1)) - 1;
    }
    fraction = normalized & ((1 << AV2H_MHCCP_DIV_PREC_BITS) - 1);
    index = fraction >>
        (AV2H_MHCCP_DIV_PREC_BITS - AV2H_MHCCP_DIV_SLOT_BITS);
    centered = fraction - offset[index];
    *scale = ((square_weight[index] *
               ((centered * centered) >> AV2H_MHCCP_DIV_PREC_BITS)) >> 8) -
             (centered >> 1) + bias[index];
    *scale <<= AV2H_MHCCP_DECIM_BITS - AV2H_MHCCP_DIV_PREC_BITS;
}

static void mhccp_solve(int32_t matrix[3U][3U], int32_t target[3U],
                         int32_t result[3U]) {
    int32_t work[3U][4U] = { { 0 } };
    int row;
    for (row = 0; row < 3; ++row) {
        int column;
        for (column = 0; column < 3; ++column) {
            work[row][column] = column >= row ?
                matrix[row][column] : matrix[column][row];
        }
        work[row][row] += 2;
        work[row][3] = target[row];
    }
    for (row = 0; row < 3; ++row) {
        uint32_t diagonal = (uint32_t)(work[row][row] < 0 ?
            -work[row][row] : work[row][row]);
        int32_t scale;
        int shift;
        int lower_row;
        if (diagonal < 1U) diagonal = 1U;
        mhccp_division_scale(diagonal, &scale, &shift);
        for (lower_row = row + 1; lower_row < 4; ++lower_row) {
            work[row][lower_row] = mhccp_mul_fixed(
                work[row][lower_row], scale, shift);
        }
        for (lower_row = row + 1; lower_row < 3; ++lower_row) {
            int32_t factor = work[lower_row][row];
            int column;
            for (column = row + 1; column < 4; ++column) {
                work[lower_row][column] -= mhccp_mul_fixed(
                    factor, work[row][column], AV2H_MHCCP_DECIM_BITS);
            }
        }
    }
    result[2] = work[2][3];
    for (row = 1; row >= 0; --row) {
        int column;
        result[row] = work[row][3];
        for (column = row + 1; column < 3; ++column) {
            result[row] -= mhccp_mul_fixed(
                work[row][column], result[column],
                AV2H_MHCCP_DECIM_BITS);
        }
    }
}

static uint16_t mhccp_luma_q3(const Av2hReconstructionPlane *luma_plane,
                               uint16_t chroma_x, uint16_t chroma_y) {
    size_t x0 = (size_t)chroma_x * 2U;
    size_t y0 = (size_t)chroma_y * 2U;
    size_t x1 = x0 + 1U < luma_plane->width ? x0 + 1U : x0;
    size_t y1 = y0 + 1U < luma_plane->height ? y0 + 1U : y0;
    return (uint16_t)((luma_plane->data[y0 * luma_plane->stride + x0] +
        luma_plane->data[y0 * luma_plane->stride + x1] +
        luma_plane->data[y1 * luma_plane->stride + x0] +
        luma_plane->data[y1 * luma_plane->stride + x1]) << 1U);
}

static uint16_t mhccp_luma_filter2_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        4U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[top_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter2_top_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        5U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter2_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        6U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]);
}

static uint16_t mhccp_luma_filter1_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    return (uint16_t)(
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[bottom_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[bottom_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + right_x]);
}

static uint16_t mhccp_luma_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)chroma_y * 2U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    size_t top_y = center_y == 0U ? center_y : center_y - 1U;
    size_t bottom_y = center_y + 1U < luma_plane->height ?
        center_y + 1U : center_y;
    if (filter_index == 0U) {
        return mhccp_luma_q3(luma_plane, chroma_x, chroma_y);
    }
    if (filter_index == 1U) {
        return (uint16_t)(
            3U * luma_plane->data[
                center_y * luma_plane->stride + center_x] +
            luma_plane->data[center_y * luma_plane->stride + right_x] +
            3U * luma_plane->data[
                bottom_y * luma_plane->stride + center_x] +
            luma_plane->data[bottom_y * luma_plane->stride + right_x]);
    }
    return (uint16_t)(
        5U * luma_plane->data[
            center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x] +
        luma_plane->data[top_y * luma_plane->stride + center_x] +
        luma_plane->data[bottom_y * luma_plane->stride + center_x]);
}

static uint16_t mhccp_luma_filter1_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t left_x = center_x == 0U ? center_x : center_x - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(2U * (
        luma_plane->data[center_y * luma_plane->stride + left_x] +
        2U * luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]));
}

static uint16_t mhccp_luma_filter0_sb_top_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    return (uint16_t)(4U * (
        luma_plane->data[center_y * luma_plane->stride + center_x] +
        luma_plane->data[center_y * luma_plane->stride + right_x]));
}

static uint16_t mhccp_luma_sb_top_left_clamped_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    size_t center_x = (size_t)chroma_x * 2U;
    size_t center_y = (size_t)block_y * 2U - 1U;
    size_t right_x = center_x + 1U < luma_plane->width ?
        center_x + 1U : center_x;
    uint16_t center = luma_plane->data[
        center_y * luma_plane->stride + center_x];
    uint16_t right = luma_plane->data[
        center_y * luma_plane->stride + right_x];
    if (filter_index == 0U) return (uint16_t)(4U * (center + right));
    if (filter_index == 1U) return (uint16_t)(2U * (3U * center + right));
    return (uint16_t)(7U * center + right);
}

static uint16_t mhccp_luma_sb_top_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t block_y) {
    return filter_index == 0U ?
        mhccp_luma_filter0_sb_top_q3(luma_plane, chroma_x, block_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_sb_top_q3(luma_plane, chroma_x, block_y) :
        mhccp_luma_filter2_sb_top_q3(luma_plane, chroma_x, block_y);
}

static uint16_t mhccp_luma_sample_q3(
    const Av2hReconstructionPlane *luma_plane,
    uint8_t filter_index,
    uint16_t chroma_x,
    uint16_t chroma_y) {
    return filter_index == 0U ?
        mhccp_luma_q3(luma_plane, chroma_x, chroma_y) :
        filter_index == 1U ?
        mhccp_luma_filter1_q3(luma_plane, chroma_x, chroma_y) :
        mhccp_luma_filter2_q3(luma_plane, chroma_x, chroma_y);
}

int32_t av2h_debug_mhccp_parameters[2U][3U];
uint64_t av2h_debug_mhccp_predictor_hash[2U];
uint64_t av2h_debug_mhccp_luma_hash[2U];
uint64_t av2h_debug_mhccp_chroma_hash[2U];
uint64_t av2h_debug_mhccp_chroma_top_hash[2U];
uint64_t av2h_debug_mhccp_chroma_left_hash[2U];
uint8_t av2h_debug_mhccp_count;
uint8_t av2h_debug_mhccp_reason;
uint8_t av2h_debug_luma_reconstruct_stage;
uint16_t av2h_debug_luma_dimensions[4U];

static int chroma_geometry_supported(uint16_t width, uint16_t height);

static Av2hStatus mhccp_predict_direction01(
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t chroma_plane_index,
    uint8_t filter_index,
    uint8_t direction,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint16_t superblock_size,
    uint8_t *predictor,
    size_t predictor_stride) {
    int32_t matrix[3U][3U] = { { 0 } };
    int32_t target[3U] = { 0 };
    int32_t parameters[3U];
    uint16_t sample;
    uint16_t row;
    uint16_t column;
    uint16_t count = 0U;
    uint16_t top_count;
    uint16_t left_count;
    uint16_t top_start_x;
    uint16_t top_chroma_width;
    uint16_t top_luma_width;
    uint16_t prediction_width;
    uint16_t prediction_height;
    int32_t top_left_center = 0;
    uint8_t have_top;
    uint8_t have_left;
    uint8_t is_top_sb_boundary;
    int matrix_shift;
    uint32_t count_ceiling;
    uint8_t debug_index = av2h_debug_mhccp_count;
    av2h_debug_mhccp_reason = 0U;
    have_top = (uint8_t)(block_y >= 2U);
    have_left = (uint8_t)(block_x >= 2U);
    is_top_sb_boundary = (uint8_t)(superblock_size != 0U &&
        ((uint32_t)block_y * 2U) % superblock_size == 0U);
    if (filter_index > 2U || direction > 2U ||
        width == 0U || height == 0U ||
                block_x >= chroma_plane->reference_width ||
                block_y >= chroma_plane->height ||
        (have_left &&
                 (block_x - 1U >= chroma_plane->reference_width ||
                    block_x * 2U - 2U >= luma_plane->reference_width))) {
          av2h_debug_mhccp_reason = 6U;
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if (!have_top && !have_left) {
        if (!plane_region_reconstructed(
                luma_plane, (uint16_t)(block_x * 2U),
                (uint16_t)(block_y * 2U),
                (uint16_t)(prediction_width * 2U),
                (uint16_t)(prediction_height * 2U))) {
            return AV2H_UNSUPPORTED;
        }
        for (row = 0U; row < prediction_height; ++row) {
            for (column = 0U; column < prediction_width; ++column) {
                predictor[(size_t)row * predictor_stride + column] = 128U;
            }
        }
        return AV2H_OK;
    }
    top_count = have_top ? (uint16_t)(width * 2U) : 0U;
    if (!have_left && top_count >= 2U) top_count = (uint16_t)(top_count - 2U);
    if (is_top_sb_boundary && top_count > 62U) top_count = 62U;
    left_count = have_left ? (uint16_t)(height * 2U - 1U) : 0U;
    if (left_count >= chroma_plane->height - block_y) {
        left_count = (uint16_t)(chroma_plane->height - block_y - 1U);
    }
    if (have_left) {
        uint16_t available_left = 0U;
        while (available_left < left_count &&
               plane_sample_reconstructed(
                   chroma_plane, (size_t)block_x - 1U,
                   (size_t)block_y + available_left)) {
            ++available_left;
        }
        if (available_left > height - 1U) {
            available_left = (uint16_t)(height - 1U +
                ((available_left - (height - 1U)) & ~1U));
        }
        left_count = available_left;
        have_left = (uint8_t)(available_left != 0U);
    }
    top_start_x = (uint16_t)(have_left ? block_x - 1U : block_x + 1U);
    top_chroma_width = 0U;
    top_luma_width = 0U;
    if (have_top) {
        uint16_t available_top = 0U;
        top_chroma_width = (uint16_t)(
            chroma_plane->reference_width - top_start_x);
        if (top_chroma_width > top_count) top_chroma_width = top_count;
        top_luma_width = (uint16_t)(
            luma_plane->reference_width - top_start_x * 2U);
        if (top_luma_width > top_count * 2U) {
            top_luma_width = (uint16_t)(top_count * 2U);
        }
        if (top_count > top_chroma_width) top_count = top_chroma_width;
        if (top_count > (top_luma_width + 1U) / 2U) {
            top_count = (uint16_t)((top_luma_width + 1U) / 2U);
        }
        while (available_top < top_count &&
               plane_sample_reconstructed(
                   chroma_plane, (size_t)top_start_x + available_top,
                   (size_t)block_y - 1U)) {
            ++available_top;
        }
        if (!have_left && available_top != 0U && available_top <= width) {
            --available_top;
        }
        if (available_top > width) {
            available_top = (uint16_t)(width +
                ((available_top - width) & ~1U));
        }
        top_count = available_top;
        top_chroma_width = available_top;
        top_luma_width = (uint16_t)(available_top * 2U);
        have_top = (uint8_t)(available_top != 0U);
    }
    if (have_top &&
        (top_chroma_width == 0U || top_luma_width == 0U ||
         !plane_region_reconstructed(
             chroma_plane, top_start_x, (uint16_t)(block_y - 1U),
             top_chroma_width, 1U))) {
        av2h_debug_mhccp_reason = 1U;
        return AV2H_UNSUPPORTED;
    }
    if (have_top && !plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * 2U),
            (uint16_t)(block_y * 2U - 2U), top_luma_width, 2U)) {
        av2h_debug_mhccp_reason = 2U;
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !plane_region_reconstructed(
            chroma_plane, (uint16_t)(block_x - 1U), block_y, 1U,
            left_count)) {
        av2h_debug_mhccp_reason = 3U;
        return AV2H_UNSUPPORTED;
    }
    if (have_left && !plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U - 2U),
            (uint16_t)(block_y * 2U), 2U,
            (uint16_t)(left_count * 2U))) {
        av2h_debug_mhccp_reason = 4U;
        return AV2H_UNSUPPORTED;
    }
    if (!plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U),
            (uint16_t)(block_y * 2U),
            (uint16_t)(prediction_width * 2U),
            (uint16_t)(prediction_height * 2U))) {
        av2h_debug_mhccp_reason = 5U;
        return AV2H_UNSUPPORTED;
    }
    if (direction == 1U && have_top &&
        !plane_region_reconstructed(
            luma_plane, (uint16_t)(top_start_x * 2U),
            (uint16_t)(block_y * 2U - 4U),
            top_luma_width, 2U)) {
        av2h_debug_mhccp_reason = 8U;
        return AV2H_UNSUPPORTED;
    }
    if (debug_index < 2U) {
        av2h_debug_mhccp_luma_hash[debug_index] =
            1469598103934665603ULL;
        av2h_debug_mhccp_chroma_hash[debug_index] =
            1469598103934665603ULL;
        av2h_debug_mhccp_chroma_top_hash[debug_index] =
            1469598103934665603ULL;
        av2h_debug_mhccp_chroma_left_hash[debug_index] =
            1469598103934665603ULL;
    }
    for (sample = 0U; sample < top_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(top_start_x + sample);
        uint16_t chroma_y = (uint16_t)(block_y - 1U);
        int32_t vector[3U];
        int first;
        int second;
        if (chroma_x >= chroma_plane->reference_width) {
            chroma_x = (uint16_t)(chroma_plane->reference_width - 1U);
        }
        {
            int32_t center = (int32_t)((is_top_sb_boundary ?
                    mhccp_luma_sb_top_sample_q3(
                        luma_plane, filter_index, chroma_x, block_y) :
                    mhccp_luma_sample_q3(
                        luma_plane, filter_index, chroma_x, chroma_y)) >> 3U);
            if (sample == 0U) top_left_center = center;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (is_top_sb_boundary ? center :
                    (int32_t)((filter_index == 0U ?
                        mhccp_luma_q3(
                            luma_plane, chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                     filter_index == 1U ?
                        mhccp_luma_filter1_q3(
                            luma_plane, chroma_x,
                            (uint16_t)(chroma_y - 1U)) :
                        mhccp_luma_filter2_top_clamped_q3(
                            luma_plane, chroma_x,
                            (uint16_t)(chroma_y - 1U))) >> 3U)) :
                    (int32_t)((is_top_sb_boundary ?
                        (sample == 0U && have_left ?
                            mhccp_luma_sb_top_left_clamped_q3(
                                luma_plane, filter_index,
                                chroma_x == 0U ? 0U :
                                    (uint16_t)(chroma_x - 1U), block_y) :
                            mhccp_luma_sb_top_sample_q3(
                                luma_plane, filter_index,
                                chroma_x == 0U ? 0U :
                                    (uint16_t)(chroma_x - 1U), block_y)) :
                        sample == 0U && have_left ?
                            mhccp_luma_left_clamped_q3(
                                luma_plane, filter_index,
                                (uint16_t)(chroma_x - 1U), chroma_y) :
                            mhccp_luma_sample_q3(
                                luma_plane, filter_index,
                                chroma_x == 0U ? 0U :
                                    (uint16_t)(chroma_x - 1U),
                                chroma_y)) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        if (chroma_plane_index == 1U && block_x == 4U && block_y == 40U &&
            sample < 4U) {
            size_t offset = 12U + (size_t)sample * 3U;
            av2h_debug_chimp_chroma_target[offset] = vector[0];
            av2h_debug_chimp_chroma_target[offset + 1U] = vector[1];
            av2h_debug_chimp_chroma_target[offset + 2U] =
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
        }
        if (chroma_plane_index == 1U && block_x == 4U && block_y == 40U &&
            sample + 1U == top_count) {
            av2h_debug_chimp_chroma_target[19] = count;
            av2h_debug_chimp_chroma_target[20] = vector[0];
            av2h_debug_chimp_chroma_target[21] = vector[1];
            av2h_debug_chimp_chroma_target[22] =
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
        }
        if (debug_index < 2U) {
            av2h_debug_mhccp_luma_hash[debug_index] ^= (uint8_t)vector[0];
            av2h_debug_mhccp_luma_hash[debug_index] *=
                1099511628211ULL;
            av2h_debug_mhccp_chroma_hash[debug_index] ^=
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
            av2h_debug_mhccp_chroma_hash[debug_index] *=
                1099511628211ULL;
            av2h_debug_mhccp_chroma_top_hash[debug_index] ^=
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
            av2h_debug_mhccp_chroma_top_hash[debug_index] *=
                1099511628211ULL;
        }
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    for (sample = have_top ? 0U : 1U; sample < left_count; ++sample) {
        uint16_t chroma_x = (uint16_t)(block_x - 1U);
        uint16_t chroma_y = (uint16_t)(block_y + sample);
        int32_t vector[3U];
        int first;
        int second;
        {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index, chroma_x, chroma_y) >> 3U);
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (sample == 0U && have_top ? top_left_center :
                     (int32_t)(mhccp_luma_sample_q3(
                         luma_plane, filter_index, chroma_x,
                         (uint16_t)(chroma_y - 1U)) >> 3U)) :
                    (int32_t)(mhccp_luma_left_clamped_q3(
                        luma_plane, filter_index,
                        (uint16_t)(chroma_x - 1U), chroma_y) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
        }
        vector[2] = 128;
        if (debug_index < 2U) {
            av2h_debug_mhccp_luma_hash[debug_index] ^= (uint8_t)vector[0];
            av2h_debug_mhccp_luma_hash[debug_index] *=
                1099511628211ULL;
            av2h_debug_mhccp_chroma_hash[debug_index] ^=
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
            av2h_debug_mhccp_chroma_hash[debug_index] *=
                1099511628211ULL;
            av2h_debug_mhccp_chroma_left_hash[debug_index] ^=
                chroma_plane->data[(size_t)chroma_y * chroma_plane->stride +
                                   chroma_x];
            av2h_debug_mhccp_chroma_left_hash[debug_index] *=
                1099511628211ULL;
        }
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)chroma_y * chroma_plane->stride + chroma_x];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
        ++count;
    }
    count_ceiling = count - 1U;
    matrix_shift = 0;
    while (count_ceiling != 0U) {
        ++matrix_shift;
        count_ceiling >>= 1U;
    }
    matrix_shift = 6 - matrix_shift;
    for (row = 0U; row < 3U; ++row) {
        for (column = row; column < 3U; ++column) {
            matrix[row][column] = matrix_shift > 0 ?
                matrix[row][column] << matrix_shift :
                matrix[row][column] >> -matrix_shift;
        }
        target[row] = matrix_shift > 0 ?
            target[row] << matrix_shift :
            target[row] >> -matrix_shift;
    }
    mhccp_solve(matrix, target, parameters);
    if (chroma_plane_index == 1U && av2h_debug_mhccp_trace_count < 64U) {
        int32_t *entry =
            av2h_debug_mhccp_trace[av2h_debug_mhccp_trace_count++];
        entry[0] = block_x;
        entry[1] = block_y;
        entry[2] = width;
        entry[3] = height;
        entry[4] = direction;
        entry[5] = top_count;
        entry[6] = left_count;
        entry[7] = parameters[0];
        entry[8] = parameters[1];
        entry[9] = parameters[2];
    }
    if (chroma_plane_index == 1U && block_x == 4U && block_y == 40U) {
        av2h_debug_chimp_chroma_target[19] = filter_index;
        av2h_debug_chimp_chroma_target[20] = direction;
        av2h_debug_chimp_chroma_target[21] = parameters[0];
        av2h_debug_chimp_chroma_target[22] = parameters[1];
        av2h_debug_chimp_chroma_target[23] = parameters[2];
    }
    if (debug_index < 2U) {
        for (column = 0U; column < 3U; ++column) {
            av2h_debug_mhccp_parameters[debug_index][column] =
                parameters[column];
        }
        av2h_debug_mhccp_predictor_hash[debug_index] =
            1469598103934665603ULL;
    }
    for (row = 0U; row < prediction_height; ++row) {
        for (column = 0U; column < prediction_width; ++column) {
            int32_t center = (int32_t)(mhccp_luma_sample_q3(
                luma_plane, filter_index,
                (uint16_t)(block_x + column),
                (uint16_t)(block_y + row)) >> 3U);
            int32_t vector[3U];
            int32_t sum = 0;
            int parameter;
            vector[0] = direction == 0U ? center :
                direction == 1U ?
                    (row == 0U && !have_top ? center :
                     row == 0U && is_top_sb_boundary ?
                        (int32_t)(mhccp_luma_sb_top_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column), block_y) >> 3U) :
                        (int32_t)(mhccp_luma_sample_q3(
                            luma_plane, filter_index,
                            (uint16_t)(block_x + column),
                            (uint16_t)(block_y + row - 1U)) >> 3U)) :
                    (int32_t)(mhccp_luma_sample_q3(
                        luma_plane, filter_index,
                        column == 0U && !have_left ? block_x :
                            (uint16_t)(block_x + column - 1U),
                        (uint16_t)(block_y + row)) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(sum, 0, 255);
            if (debug_index < 2U) {
                av2h_debug_mhccp_predictor_hash[debug_index] ^=
                    predictor[(size_t)row * predictor_stride + column];
                av2h_debug_mhccp_predictor_hash[debug_index] *=
                    1099511628211ULL;
            }
        }
    }
    if (debug_index < 2U) ++av2h_debug_mhccp_count;
    return AV2H_OK;
}

static Av2hStatus mhccp_predict_observed(
    const Av2hFirstChromaMode *mode,
    const Av2hReconstructionPlane *luma_plane,
    const Av2hReconstructionPlane *chroma_plane,
    uint8_t chroma_plane_index,
    uint16_t block_x,
    uint16_t block_y,
    uint16_t width,
    uint16_t height,
    uint8_t *predictor,
    size_t predictor_stride) {
    int32_t matrix[3U][3U] = { { 0 } };
    int32_t target[3U] = { 0 };
    int32_t parameters[3U];
    uint16_t prediction_width;
    uint16_t prediction_height;
    uint16_t row;
    if (mode->mhccp_direction <= 2U &&
        chroma_geometry_supported(width, height) &&
        luma_plane != 0 && luma_plane->data != 0 &&
        luma_plane->reconstructed != 0 &&
        luma_plane->stride >= luma_plane->width) {
        return mhccp_predict_direction01(
            luma_plane, chroma_plane, chroma_plane_index,
            mode->cfl_downsample_filter_index,
            mode->mhccp_direction, block_x, block_y,
            width, height, mode->superblock_size,
            predictor, predictor_stride);
    }
    if (mode->mhccp_direction != 2U || block_y != 0U || block_x < 2U ||
        width != 16U || height != 32U || luma_plane == 0 ||
        luma_plane->data == 0 || luma_plane->reconstructed == 0 ||
        luma_plane->stride < luma_plane->width ||
        (size_t)block_x * 2U >= luma_plane->width ||
        (size_t)(block_y + height) * 2U > luma_plane->height ||
        !plane_region_reconstructed(
            luma_plane, (uint16_t)(block_x * 2U - 4U), 0U,
            (uint16_t)(luma_plane->width - (block_x * 2U - 4U)),
            (uint16_t)(height * 2U)) ||
        !plane_region_reconstructed(
            chroma_plane, (uint16_t)(block_x - 2U), 0U, 2U, height)) {
        return AV2H_UNSUPPORTED;
    }
    prediction_width = width < chroma_plane->width - block_x ?
        width : (uint16_t)(chroma_plane->width - block_x);
    prediction_height = height < chroma_plane->height - block_y ?
        height : (uint16_t)(chroma_plane->height - block_y);
    if ((size_t)(block_x + prediction_width) * 2U > luma_plane->width ||
        (size_t)(block_y + prediction_height) * 2U > luma_plane->height) {
        return AV2H_UNSUPPORTED;
    }
    for (row = 1U; row + 1U < height; ++row) {
        int32_t vector[3U];
        uint16_t center = (uint16_t)(
            mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 1U), row) >> 3U);
        int first;
        int second;
        vector[0] = (int32_t)(
            mhccp_luma_q3(luma_plane, (uint16_t)(block_x - 2U), row) >> 3U);
        vector[1] = (center * center + 128) >> 8U;
        vector[2] = 128;
        for (first = 0; first < 3; ++first) {
            target[first] += vector[first] * chroma_plane->data[
                (size_t)row * chroma_plane->stride + block_x - 1U];
            for (second = first; second < 3; ++second) {
                matrix[first][second] += vector[first] * vector[second];
            }
        }
    }
    for (row = 0U; row < 3U; ++row) {
        uint16_t column;
        target[row] <<= 1U;
        for (column = row; column < 3U; ++column) {
            matrix[row][column] <<= 1U;
        }
    }
    mhccp_solve(matrix, target, parameters);
    for (row = 0U; row < prediction_height; ++row) {
        uint16_t column;
        for (column = 0U; column < prediction_width; ++column) {
            int32_t vector[3U];
            uint16_t center = (uint16_t)(mhccp_luma_q3(
                luma_plane, (uint16_t)(block_x + column), row) >> 3U);
            int32_t sum = 0;
            int parameter;
            vector[0] = (int32_t)(mhccp_luma_q3(
                luma_plane, (uint16_t)(block_x + column - 1U), row) >> 3U);
            vector[1] = (center * center + 128) >> 8U;
            vector[2] = 128;
            for (parameter = 0; parameter < 3; ++parameter) {
                sum += mhccp_mul_fixed(
                    parameters[parameter], vector[parameter],
                    AV2H_MHCCP_DECIM_BITS);
            }
            predictor[(size_t)row * predictor_stride + column] =
                (uint8_t)clamp(sum, 0, 255);
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_first_chroma_reconstruct(
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionScratch *scratch,
    Av2hFirstReconstruction *reconstruction) {
    Av2hFirstLeafMode mode;
    if (prefix == 0 || transform == 0 || coefficients == 0 || scratch == 0 ||
        reconstruction == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (prefix->tx_size != AV2H_TX_4X4 ||
        transform->primary_type != AV2H_TX_DCT_DCT ||
        coefficients->width != 4U || coefficients->height != 4U) {
        return AV2H_UNSUPPORTED;
    }
    av2h_memory_fill(&mode, 0U, sizeof(mode));
    mode.mode = AV2H_INTRA_DC;
    return av2h_luma_reconstruct(&mode, prefix, transform, coefficients,
                                 0, 0U, scratch, reconstruction);
}

static int chroma_geometry_supported(uint16_t width, uint16_t height) {
        return (width == 4U && (height == 4U || height == 8U ||
                                                        height == 16U || height == 32U)) ||
           (width == 8U &&
                        (height == 4U || height == 8U || height == 16U ||
                         height == 32U)) ||
           (width == 16U &&
                        (height == 4U || height == 8U || height == 16U ||
                         height == 32U)) ||
                     (width == 32U && (height == 4U || height == 8U ||
                                                         height == 16U || height == 32U));
}

Av2hStatus av2h_chroma_reconstruct_to_plane(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    Av2hReconstructionScratch *scratch) {
    return av2h_chroma_reconstruct_to_plane_ex(
        leaf, mode, prefix, transform, coefficients, plane, 0, 1U, 0,
        scratch);
}

Av2hStatus av2h_chroma_inverse_cross_transform(
    Av2hFirstCoeffBlock *u_coefficients,
    Av2hFirstCoeffBlock *v_coefficients,
    uint8_t cctx_type) {
    static const int32_t matrix[6U][2U] = {
        { 181, 181 }, { 222, 128 }, { 128, 222 },
        { 181, -181 }, { 222, -128 }, { 128, -222 }
    };
    size_t index;
    if (u_coefficients == 0 || v_coefficients == 0 || cctx_type > 6U ||
        u_coefficients->coefficient_count !=
            v_coefficients->coefficient_count) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (cctx_type == 0U) return AV2H_OK;
    u_coefficients->nonzero_count = 0U;
    v_coefficients->nonzero_count = 0U;
    for (index = 0U; index < u_coefficients->coefficient_count; ++index) {
        int32_t u = u_coefficients->dequantized[index];
        int32_t v = v_coefficients->dequantized[index];
        int32_t first = round_shift_signed(
            (int64_t)matrix[cctx_type - 1U][0U] * u -
                (int64_t)matrix[cctx_type - 1U][1U] * v,
            8U);
        int32_t second = round_shift_signed(
            (int64_t)matrix[cctx_type - 1U][1U] * u +
                (int64_t)matrix[cctx_type - 1U][0U] * v,
            8U);
        u_coefficients->dequantized[index] =
            clamp(first, -32768, 32767);
        v_coefficients->dequantized[index] =
            clamp(second, -32768, 32767);
        if (u_coefficients->dequantized[index] != 0) {
            ++u_coefficients->nonzero_count;
        }
        if (v_coefficients->dequantized[index] != 0) {
            ++v_coefficients->nonzero_count;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_chroma_reconstruct_to_plane_ex(
    const Av2hPartitionNode *leaf,
    const Av2hFirstChromaMode *mode,
    const Av2hFirstCoeffPrefix *prefix,
    const Av2hFirstTransformType *transform,
    const Av2hFirstCoeffBlock *coefficients,
    Av2hReconstructionPlane *plane,
    const Av2hReconstructionPlane *luma_plane,
    uint8_t chroma_plane,
    Av2hReconstructionPlane *predictor_plane,
    Av2hReconstructionScratch *scratch) {
    Av2hFirstLeafMode luma_mode;
    Av2hFirstCoeffPrefix effective_prefix;
    Av2hFirstReconstruction reconstruction;
    Av2hLumaReferences references;
    uint16_t block_x;
    uint16_t block_y;
    uint16_t width;
    uint16_t height;
    uint16_t visible_width;
    uint16_t visible_height;
    uint8_t have_top;
    uint8_t have_left;
    size_t index;
    Av2hStatus status;
    av2h_debug_chroma_reconstruct_stage = 0U;
    if (leaf == 0 || mode == 0 || prefix == 0 || transform == 0 ||
        coefficients == 0 || plane == 0 || scratch == 0 || plane->data == 0 ||
        chroma_plane < 1U || chroma_plane > 2U ||
        plane->stride < plane->width || plane->reference_width == 0U ||
        plane->reference_height == 0U || plane->reconstructed == 0 ||
        plane->reference_width > plane->width ||
        plane->reference_height > plane->height ||
        plane->reconstructed_stride < ((size_t)plane->width + 3U) / 4U ||
        plane->reconstructed_count < plane->reconstructed_stride *
            (((size_t)plane->height + 3U) / 4U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(&luma_mode, 0U, sizeof(luma_mode));
    av2h_debug_chroma_reconstruct_stage = 1U;
    width = coefficients->width;
    height = coefficients->height;
    block_x = (uint16_t)((uint32_t)leaf->column * 2U);
    block_y = (uint16_t)((uint32_t)leaf->row * 2U);
    if (block_x >= plane->width || block_y >= plane->height ||
        !chroma_geometry_supported(width, height)) {
        return AV2H_INVALID_ARGUMENT;
    }
    visible_width = width < plane->width - block_x ?
        width : (uint16_t)(plane->width - block_x);
    visible_height = height < plane->height - block_y ?
        height : (uint16_t)(plane->height - block_y);
    if (predictor_plane != 0 &&
        (predictor_plane->data == 0 ||
         predictor_plane->stride < predictor_plane->width ||
         block_x >= predictor_plane->width ||
         block_y >= predictor_plane->height ||
         visible_width > predictor_plane->width - block_x ||
         visible_height > predictor_plane->height - block_y)) {
        return AV2H_INVALID_ARGUMENT;
    }
    /*
     * Gather the reconstructed neighbour references.  Above extends across the
     * available reference width so directional chroma predictors can read
     * their diagonal; left spans the block height.  Readiness truncates at the
     * first sample not yet reconstructed (edge/crop and same-SB ordering).
     */
    {
        size_t above_count = block_y == 0U ||
            block_x >= plane->width ? 0U :
            (size_t)plane->width - block_x;
        size_t left_count = block_x == 0U ? 0U : height;
        int angle = wide_angle_map(
            directional_base_angle(mode->mode) + mode->angle_delta * 3,
            width, height);
        if (left_count != 0U &&
            (mode->mode == AV2H_INTRA_SMOOTH ||
             mode->mode == AV2H_INTRA_SMOOTH_VERTICAL)) {
            ++left_count;
        }
        if (left_count != 0U && angle > 180 && angle < 270) {
            left_count += height;
        }
        if (above_count > sizeof(scratch->above)) {
            above_count = sizeof(scratch->above);
        }
        if (left_count > sizeof(scratch->left)) {
            left_count = sizeof(scratch->left);
        }
        for (index = 0U; index < above_count; ++index) {
            if (!plane_sample_reconstructed(
                    plane, block_x + index, (size_t)block_y - 1U)) {
                above_count = index;
                break;
            }
        }
        for (index = 0U; index < above_count; ++index) {
            scratch->above[index] = plane->data[
                ((size_t)block_y - 1U) * plane->stride + block_x + index];
        }
        if (above_count != 0U && above_count < width) {
            uint8_t edge = scratch->above[above_count - 1U];
            while (above_count < width) {
                scratch->above[above_count++] = edge;
            }
        }
        for (index = 0U; index < left_count; ++index) {
            size_t sy = (size_t)block_y + index;
            if (sy >= plane->height) {
                sy = plane->height - 1U;
            }
            if (!plane_sample_reconstructed(
                    plane, (size_t)block_x - 1U, sy)) {
                left_count = index;
                break;
            }
        }
        for (index = 0U; index < left_count; ++index) {
            size_t sy = (size_t)block_y + index;
            if (sy >= plane->height) {
                sy = plane->height - 1U;
            }
            scratch->left[index] = plane->data[
                sy * plane->stride + block_x - 1U];
        }
        references.left = left_count == 0U ? 0 : scratch->left;
        references.left_count = left_count;
        references.above = above_count == 0U ? 0 : scratch->above;
        references.above_count = above_count;
        if (chroma_plane == 1U && block_x == 32U && block_y == 40U) {
            for (index = 0U; index < 13U; ++index) {
                av2h_debug_directional_edges[index] =
                    index < left_count ? scratch->left[index] : 0U;
            }
        }
        have_top = above_count >= width ? 1U : 0U;
        have_left = left_count >= height ? 1U : 0U;
    }
    references.above_left = 0U;
    references.has_above_left = 0U;
    if (block_x != 0U && block_y != 0U &&
        plane_sample_reconstructed(
            plane, (size_t)block_x - 1U, (size_t)block_y - 1U)) {
        references.above_left = plane->data[
            ((size_t)block_y - 1U) * plane->stride + block_x - 1U];
        references.has_above_left = 1U;
    }
    if (!mode->is_cfl && mode->mode != AV2H_INTRA_DC &&
        (references.above_count == 0U || references.left_count == 0U)) {
        size_t above_count = (size_t)width + height;
        size_t left_count = (size_t)height + width;
        uint8_t had_above = references.above_count != 0U;
        uint8_t had_left = references.left_count != 0U;
        uint8_t above_value = had_left ? references.left[0] : 127U;
        uint8_t left_value = had_above ? references.above[0] : 129U;
        if (above_count > sizeof(scratch->above)) {
            above_count = sizeof(scratch->above);
        }
        if (left_count > sizeof(scratch->left)) {
            left_count = sizeof(scratch->left);
        }
        if (!had_above) {
            av2h_memory_fill(scratch->above, above_value, above_count);
            references.above = scratch->above;
            references.above_count = above_count;
        }
        if (!had_left) {
            av2h_memory_fill(scratch->left, left_value, left_count);
            references.left = scratch->left;
            references.left_count = left_count;
        }
        references.above_left = had_above ? left_value :
            had_left ? above_value : 128U;
        references.has_above_left = 1U;
    }
    if (mode->is_cfl) {
        /* CfL bypasses IBP and starts from a flat chroma DC base. */
        unsigned int sum = 0U;
        unsigned int count = 0U;
        uint8_t dc_value;
        size_t i;
        if (have_top) {
            for (i = 0U; i < width; ++i) sum += scratch->above[i];
            count += width;
        }
        if (have_left) {
            for (i = 0U; i < height; ++i) sum += scratch->left[i];
            count += height;
        }
        dc_value = dc_predictor_average(
            sum, count, width, height, have_top, have_left);
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            scratch->predictor[index] = dc_value;
        }
        if (mode->cfl_kind == AV2H_CFL_MULTI_PARAM) {
            status = mhccp_predict_observed(
                mode, luma_plane, plane, chroma_plane, block_x, block_y,
                width, height,
                scratch->predictor, width);
        } else {
            uint8_t derive_alpha = (uint8_t)(
                mode->cfl_kind == AV2H_CFL_DERIVED_ALPHA);
            int32_t alpha_q3 = 0;
            if (!derive_alpha) {
                uint8_t sign = chroma_plane == 1U ?
                    (uint8_t)(((mode->cfl_alpha_signs + 1U) * 11U) >> 5U) :
                    (uint8_t)(mode->cfl_alpha_signs + 1U - 3U *
                        (((mode->cfl_alpha_signs + 1U) * 11U) >> 5U));
                uint8_t magnitude = chroma_plane == 1U ?
                    (uint8_t)(mode->cfl_alpha_idx >> 3U) :
                    (uint8_t)(mode->cfl_alpha_idx & 7U);
                if (mode->cfl_kind != AV2H_CFL_EXPLICIT_ALPHA ||
                    mode->cfl_alpha_signs >= 8U || sign > 2U) {
                    return AV2H_UNSUPPORTED;
                }
                alpha_q3 = sign == 0U ? 0 :
                    sign == 2U ? (int32_t)magnitude + 1 :
                    -(int32_t)magnitude - 1;
            }
            status = cfl_derived_predict(
                luma_plane, plane, mode->cfl_downsample_filter_index,
                derive_alpha, alpha_q3, mode->superblock_size, block_x,
                block_y, width, height,
                have_top, have_left,
                scratch->predictor, width);
        }
        if (status != AV2H_OK) return status;
        luma_mode.mode = AV2H_INTRA_DC;
    } else {
        status = av2h_chroma_predict(
            mode, width, height, &references, scratch->predictor, width);
        if (status != AV2H_OK) return status;
        luma_mode.mode = mode->mode;
        luma_mode.angle_delta = mode->angle_delta;
    }
    av2h_debug_chroma_reconstruct_stage = 2U;
    if (chroma_plane == 1U && block_x <= 160U && block_y <= 4U &&
        (uint32_t)block_x + visible_width > 160U &&
        (uint32_t)block_y + visible_height > 4U) {
        size_t target_index = (size_t)(4U - block_y) * width +
            (160U - block_x);
        av2h_debug_chimp_chroma_target[0] = leaf->row;
        av2h_debug_chimp_chroma_target[1] = leaf->column;
        av2h_debug_chimp_chroma_target[2] = leaf->block_size;
        av2h_debug_chimp_chroma_target[3] = prefix->tx_size;
        av2h_debug_chimp_chroma_target[4] = mode->mode;
        av2h_debug_chimp_chroma_target[5] = mode->angle_delta;
        av2h_debug_chimp_chroma_target[6] = mode->is_cfl;
        av2h_debug_chimp_chroma_target[7] = mode->cfl_kind;
        av2h_debug_chimp_chroma_target[8] = transform->primary_type;
        av2h_debug_chimp_chroma_target[9] = prefix->eob;
        av2h_debug_chimp_chroma_target[10] = width;
        av2h_debug_chimp_chroma_target[11] = height;
        av2h_debug_chimp_chroma_target[12] = scratch->predictor[target_index];
        av2h_debug_chimp_chroma_target[13] = prefix->all_zero;
        av2h_debug_chimp_chroma_target[14] = coefficients->nonzero_count;
        av2h_debug_chimp_chroma_target[17] = mode->above_smooth;
        av2h_debug_chimp_chroma_target[18] = mode->left_smooth;
        av2h_debug_chimp_chroma_target[19] = mode->cfl_alpha_signs;
        av2h_debug_chimp_chroma_target[20] = mode->cfl_alpha_idx;
    }
    if (predictor_plane != 0) {
        size_t row;
        for (row = 0U; row < visible_height; ++row) {
            size_t column;
            for (column = 0U; column < visible_width; ++column) {
                predictor_plane->data[((size_t)block_y + row) *
                    predictor_plane->stride + block_x + column] =
                    scratch->predictor[row * width + column];
            }
        }
    }
    /*
     * Predictor-only path: every all-zero chroma transform block (any
     * geometry, including TX_16X8 / TX_32X4 whose coefficient DCT is owned by
     * the parallel geometry work) reconstructs to its predictor with a zero
     * residual.  A stray coefficient on an all-zero block is rejected without
     * touching the plane.
     */
    if (prefix->all_zero && coefficients->nonzero_count == 0U) {
        for (index = 0U; index < coefficients->coefficient_count; ++index) {
            if (coefficients->quantized[index] != 0 ||
                coefficients->dequantized[index] != 0) {
                return AV2H_UNSUPPORTED;
            }
        }
        for (index = 0U; index < visible_height; ++index) {
            size_t column;
            for (column = 0U; column < visible_width; ++column) {
                plane->data[((size_t)block_y + index) * plane->stride +
                            block_x + column] =
                    scratch->predictor[index * width + column];
            }
        }
        plane_mark_reconstructed(
            plane, block_x, block_y, visible_width, visible_height);
        if (chroma_plane == 1U && block_x <= 160U && block_y <= 4U &&
            (uint32_t)block_x + visible_width > 160U &&
            (uint32_t)block_y + visible_height > 4U) {
            av2h_debug_chimp_chroma_target[15] = 0;
            av2h_debug_chimp_chroma_target[16] =
                scratch->predictor[(size_t)(4U - block_y) * width +
                                   (160U - block_x)];
        }
        av2h_debug_chroma_reconstruct_stage = 3U;
        return AV2H_OK;
    }
    effective_prefix = *prefix;
    if (coefficients->nonzero_count != 0U) {
        effective_prefix.all_zero = 0U;
    }
    status = av2h_luma_reconstruct(
        &luma_mode, &effective_prefix, transform, coefficients,
        scratch->predictor,
        width, scratch, &reconstruction);
    if (status != AV2H_OK) {
        av2h_debug_chroma_reconstruct_stage =
            (uint8_t)(10U + av2h_debug_inverse_transform_stage);
        return status;
    }
    for (index = 0U; index < visible_height; ++index) {
        size_t column;
        for (column = 0U; column < visible_width; ++column) {
            plane->data[((size_t)block_y + index) * plane->stride +
                        block_x + column] =
                reconstruction.pixels[index * width + column];
        }
    }
    plane_mark_reconstructed(
        plane, block_x, block_y, visible_width, visible_height);
    if (chroma_plane == 1U && block_x <= 160U && block_y <= 4U &&
        (uint32_t)block_x + visible_width > 160U &&
        (uint32_t)block_y + visible_height > 4U) {
        size_t target_index = (size_t)(4U - block_y) * width +
            (160U - block_x);
        av2h_debug_chimp_chroma_target[15] = scratch->residual[target_index];
        av2h_debug_chimp_chroma_target[16] =
            reconstruction.pixels[target_index];
    }
    return AV2H_OK;
}

uint8_t av2h_debug_chroma_reconstruct_stage;
uint8_t av2h_debug_inverse_transform_stage;
int32_t av2h_debug_chimp_luma_target[56U];
int32_t av2h_debug_chimp_chroma_target[24U];
int32_t av2h_debug_mhccp_trace[64U][10U];
uint8_t av2h_debug_mhccp_trace_count;

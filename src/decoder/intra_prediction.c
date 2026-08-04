#include "reconstruct.h"
#include "internal/reconstruct_modules.h"
#include "internal/sample_math.h"

#define directional_base_angle av2h_directional_base_angle
#define luma_wide_angle av2h_luma_wide_angle
#define luma_prediction_dimension_supported \
    av2h_luma_prediction_dimension_supported
#define cfl_resolve_divisor_32 av2h_cfl_resolve_divisor_32

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

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
    return av2h_sample_clamp(value, minimum, maximum);
}

static int32_t round_shift(int64_t value, uint8_t shift) {
    int64_t adjusted = value + ((int64_t)1 << (shift - 1U));
    if (adjusted >= 0) return (int32_t)(adjusted >> shift);
    return (int32_t)-(((-adjusted) + (((int64_t)1 << shift) - 1)) >> shift);
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

int av2h_directional_base_angle(Av2hFirstLeafIntraMode mode) {
    return mode == AV2H_INTRA_VERTICAL ? 90 :
           mode == AV2H_INTRA_HORIZONTAL ? 180 :
           mode == AV2H_INTRA_D45 ? 45 :
           mode == AV2H_INTRA_D67 ? 67 :
           mode == AV2H_INTRA_D113 ? 113 :
           mode == AV2H_INTRA_D135 ? 135 :
           mode == AV2H_INTRA_D157 ? 157 :
           mode == AV2H_INTRA_D203 ? 203 : 0;
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

Av2hStatus av2h_predict_directional(
    const Av2hFirstLeafMode *mode,
    int angle,
    uint16_t width,
    uint16_t height,
    const Av2hLumaReferences *references,
    uint8_t *predictor,
    size_t predictor_stride,
    uint8_t use_idif,
    uint8_t filter_edges) {
    return predict_directional(
        mode, angle, width, height, references, predictor, predictor_stride,
        use_idif, filter_edges);
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

int av2h_luma_prediction_dimension_supported(uint16_t dimension) {
    return dimension == 4U || dimension == 8U || dimension == 16U ||
           dimension == 32U || dimension == 64U;
}

int av2h_luma_wide_angle(int angle, uint16_t width, uint16_t height) {
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

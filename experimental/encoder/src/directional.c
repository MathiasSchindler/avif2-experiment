#include "encoder/directional.h"

#include "normative_directional.inc"

static int directional_abs(int value) {
    return value < 0 ? -value : value;
}

static int directional_round_shift(int value, unsigned int shift) {
    int rounding = 1 << (shift - 1U);
    return value >= 0
        ? (value + rounding) >> shift
        : -((-value + rounding - 1) >> shift);
}

static uint8_t directional_clip(int value) {
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

int enc_directional_token_to_joint(uint8_t token, uint8_t *joint_mode) {
    if (joint_mode == 0) return 0;
    if (token == 0U) *joint_mode = 0U;
    else if (token == 1U || token == 3U) *joint_mode = 22U;
    else if (token == 2U || token == 4U) *joint_mode = 50U;
    else if (token >= 5U && token <= 8U) *joint_mode = token - 4U;
    else if (token >= ENC_DIRECTIONAL_FIRST_TOKEN &&
             token <= ENC_DIRECTIONAL_LAST_TOKEN) {
        *joint_mode = token - 4U;
    } else {
        return 0;
    }
    return 1;
}

int enc_directional_joint_to_mode(
    uint8_t joint_mode, uint8_t *mode, int8_t *angle_delta) {
    static const uint8_t modes[13U] = {
        0U, 9U, 10U, 11U, 12U, 3U, 8U,
        1U, 5U, 4U, 6U, 2U, 7U
    };
    uint8_t base = joint_mode;
    if (mode == 0 || angle_delta == 0 || joint_mode > 60U) return 0;
    *angle_delta = 0;
    if (joint_mode >= 5U) {
        base = (uint8_t)(5U + (joint_mode - 5U) / 7U);
        *angle_delta = (int8_t)((joint_mode - 5U) % 7U) - 3;
    }
    if (base >= 13U) return 0;
    *mode = modes[base];
    return 1;
}

int enc_directional_joint_is_directional(uint8_t joint_mode) {
    return joint_mode >= 5U && joint_mode <= 60U;
}

static int wide_angle(size_t width, size_t height, int angle) {
    int threshold = 0;
    if (height == width * 2U || width == height * 2U) threshold = 61;
    else if (height == width * 4U || width == height * 4U) threshold = 73;
    else if (height == width * 8U || width == height * 8U) threshold = 82;
    else if (height == width * 16U || width == height * 16U) threshold = 86;
    if (threshold != 0 && height > width && angle < threshold) {
        return angle + 180;
    }
    if (threshold != 0 && width > height && angle > 270 - threshold) {
        return angle - 180;
    }
    return angle;
}

static unsigned int edge_strength(
    size_t width, size_t height, int delta) {
    size_t sum = width + height;
    int distance = directional_abs(delta);
    if (sum <= 8U) return distance >= 56 ? 1U : 0U;
    if (sum <= 16U) return distance >= 40 ? 1U : 0U;
    if (sum <= 24U) {
        return distance >= 32 ? 3U :
               distance >= 16 ? 2U : distance >= 8 ? 1U : 0U;
    }
    if (sum <= 32U) {
        return distance >= 32 ? 3U : distance >= 4 ? 2U : 1U;
    }
    return 3U;
}

static void filter_edge(uint8_t *edge, size_t count, unsigned int strength) {
    static const uint8_t kernels[3U][5U] = {
        {0U, 4U, 8U, 4U, 0U},
        {0U, 5U, 6U, 5U, 0U},
        {2U, 4U, 4U, 4U, 2U}
    };
    uint8_t source[66U];
    size_t index;
    if (strength == 0U || count > sizeof(source)) return;
    for (index = 0U; index < count; ++index) source[index] = edge[index];
    for (index = 1U; index < count; ++index) {
        unsigned int sum = 0U;
        size_t tap;
        for (tap = 0U; tap < 5U; ++tap) {
            int position = (int)index - 2 + (int)tap;
            size_t clipped = position < 0 ? 0U :
                (size_t)position >= count ? count - 1U : (size_t)position;
            sum += kernels[strength - 1U][tap] * source[clipped];
        }
        edge[index] = (uint8_t)((sum + 8U) >> 4U);
    }
}

static uint8_t edge_sample(
    const uint8_t *edge, size_t count, int index, uint8_t fallback) {
    if (edge == 0 || count == 0U) return fallback;
    if (index >= -2 && index < 0) return edge[index];
    if (index < -2) return edge[-2];
    return edge[(size_t)index < count ? (size_t)index : count - 1U];
}

int enc_directional_predict(
    uint8_t token, unsigned int plane,
    const uint8_t *above, size_t above_count,
    const uint8_t *left, size_t left_count, uint8_t corner,
    size_t width, size_t height, uint8_t *output) {
    uint8_t joint;
    uint8_t mode;
    int8_t delta;
    int angle;
    uint8_t above_storage[67U];
    uint8_t left_storage[67U];
    uint8_t *above_edge = above_storage + 2U;
    uint8_t *left_edge = left_storage + 2U;
    size_t edge_count = width + height + 3U;
    size_t index;
    size_t row;
    size_t column;
    uint8_t above_fallback = left != 0 ? left[0U] : 127U;
    uint8_t left_fallback = above != 0 ? above[0U] : 129U;
    if (output == 0 || width == 0U || height == 0U ||
        width > 32U || height > 32U || plane > 2U ||
        !enc_directional_token_to_joint(token, &joint) ||
        !enc_directional_joint_to_mode(joint, &mode, &delta) ||
        !enc_directional_joint_is_directional(joint)) {
        return 0;
    }
    angle = wide_angle(
        width, height, directional_mode_angle[mode] + (int)delta * 3);
    for (index = 0U; index < edge_count; ++index) {
        size_t above_limit = angle < 90
            ? (above_count < 2U * width ? above_count : 2U * width)
            : width < above_count ? width : above_count;
        size_t left_limit = angle > 180
            ? (left_count < 2U * height ? left_count : 2U * height)
            : height < left_count ? height : left_count;
        above_edge[index] = edge_sample(
            above, above_limit,
            (int)(index < above_limit ? index :
                  above_limit != 0U ? above_limit - 1U : 0U),
            above_fallback);
        left_edge[index] = edge_sample(
            left, left_limit,
            (int)(index < left_limit ? index :
                  left_limit != 0U ? left_limit - 1U : 0U),
            left_fallback);
    }
    above_edge[-1] = corner;
    left_edge[-1] = corner;
    above_edge[-2] = corner;
    left_edge[-2] = corner;
    if (angle != 90 && angle != 180) {
        int need_right = angle < 90;
        int need_bottom = angle > 180;
        size_t above_size = width + (need_right ? height : 0U) + 1U;
        size_t left_size = height + (need_bottom ? width : 0U) + 1U;
        if (above != 0 && left != 0 && width + height >= 24U) {
            uint8_t filtered = (uint8_t)directional_round_shift(
                (int)left_edge[0U] * 5 + (int)corner * 6 +
                (int)above_edge[0U] * 5, 4U);
            above_edge[-1] = filtered;
            left_edge[-1] = filtered;
        }
        if (above != 0) {
            filter_edge(
                above_edge - 1U, above_size,
                edge_strength(width, height, angle - 90));
        }
        if (left != 0) {
            filter_edge(
                left_edge - 1U, left_size,
                edge_strength(width, height, angle - 180));
        }
    }
    if (angle > 90 && angle < 180) {
        left_edge[height] = left_edge[height - 1U];
        left_edge[height + 1U] = left_edge[height - 1U];
        above_edge[width] = above_edge[width - 1U];
        above_edge[width + 1U] = above_edge[width - 1U];
    } else {
        size_t maximum = width + height - 1U;
        left_edge[maximum + 1U] = left_edge[maximum];
        left_edge[maximum + 2U] = left_edge[maximum];
        above_edge[maximum + 1U] = above_edge[maximum];
        above_edge[maximum + 2U] = above_edge[maximum];
    }
    left_edge[-2] = left_edge[-1];
    above_edge[-2] = above_edge[-1];
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            int base;
            int shift;
            const uint8_t *edge;
            int value;
            if (angle == 90) {
                output[row * width + column] = above_edge[column];
                continue;
            }
            if (angle == 180) {
                output[row * width + column] = left_edge[row];
                continue;
            }
            if (angle < 90) {
                int distance =
                    (int)(row + 1U) * directional_derivative[angle];
                base = (distance >> 6) + (int)column;
                shift = (distance >> 1) & 31;
                edge = above_edge;
            } else if (angle < 180) {
                int distance = ((int)column << 6) -
                    (int)(row + 1U) * directional_derivative[180 - angle];
                base = distance >> 6;
                if (base >= -1) {
                    shift = (distance >> 1) & 31;
                    edge = above_edge;
                } else {
                    distance = ((int)row << 6) -
                        (int)(column + 1U) *
                            directional_derivative[angle - 90];
                    base = distance >> 6;
                    shift = (distance >> 1) & 31;
                    edge = left_edge;
                }
            } else {
                int distance =
                    (int)(column + 1U) * directional_derivative[270 - angle];
                base = (distance >> 6) + (int)row;
                shift = (distance >> 1) & 31;
                edge = left_edge;
            }
            if ((angle < 90 || angle > 180) &&
                base >= (int)(width + height - 1U) +
                    (plane == 0U ? 1 : 0)) {
                output[row * width + column] =
                    edge[width + height - 1U];
                continue;
            }
            if (plane == 0U) {
                int sum = 0;
                size_t tap;
                for (tap = 0U; tap < 4U; ++tap) {
                    sum += directional_interp[shift][tap] *
                        edge_sample(edge, edge_count, base + (int)tap - 1,
                                    edge[0U]);
                }
                value = directional_round_shift(sum, 7U);
            } else {
                value = directional_round_shift(
                    (int)edge_sample(
                        edge, edge_count, base, edge[0U]) * (32 - shift) +
                    (int)edge_sample(
                        edge, edge_count, base + 1, edge[0U]) * shift,
                    5U);
            }
            output[row * width + column] = directional_clip(value);
        }
    }
    return 1;
}

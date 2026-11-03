#include "decoder/internal/intra_prediction.h"
#include "encoder/directional.h"
#include <stdio.h>

static int equal_bytes(const uint8_t *left, const uint8_t *right, size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static int mapped_angle(size_t width, size_t height, int angle) {
    int threshold = 0;
    if (height == width * 2U || width == height * 2U) threshold = 61;
    else if (height == width * 4U || width == height * 4U) threshold = 73;
    if (threshold != 0 && height > width && angle < threshold) {
        return angle + 180;
    }
    if (threshold != 0 && width > height && angle > 270 - threshold) {
        return angle - 180;
    }
    return angle;
}

static int check_full_directional(void) {
    static const size_t sizes[7U][2U] = {
        {4U, 4U}, {4U, 8U}, {8U, 4U}, {8U, 8U},
        {8U, 16U}, {16U, 8U}, {16U, 16U}
    };
    static const uint8_t base_angles[13U] = {
        0U, 90U, 180U, 45U, 135U, 113U, 157U,
        203U, 67U, 0U, 0U, 0U, 0U
    };
    uint8_t above[32U];
    uint8_t left[32U];
    uint8_t expected[256U];
    uint8_t actual[256U];
    Av2hLumaReferences references = {0};
    uint8_t token;
    size_t size_index;
    size_t index;
    for (index = 0U; index < 32U; ++index) {
        above[index] = (uint8_t)(17U + index * 3U);
        left[index] = (uint8_t)(201U - index * 2U);
    }
    references.above = above;
    references.above_count = 32U;
    references.left = left;
    references.left_count = 32U;
    references.above_left = 91U;
    references.has_above_left = 1U;
    for (token = ENC_DIRECTIONAL_FIRST_TOKEN;
         token <= ENC_DIRECTIONAL_LAST_TOKEN; ++token) {
        uint8_t joint;
        uint8_t mode_value;
        int8_t delta;
        Av2hLeafMode mode = {0};
        if (!enc_directional_token_to_joint(token, &joint) ||
            !enc_directional_joint_to_mode(
                joint, &mode_value, &delta)) {
            return 10;
        }
        mode.mode = (Av2hIntraMode)mode_value;
        mode.angle_delta = delta;
        for (size_index = 0U; size_index < 7U; ++size_index) {
            size_t width = sizes[size_index][0U];
            size_t height = sizes[size_index][1U];
            int angle = mapped_angle(
                width, height, (int)base_angles[mode_value] + delta * 3);
            if (av2h_predict_directional(
                    &mode, angle, (uint16_t)width, (uint16_t)height,
                    &references, expected, width, 1U, 1U) != AV2H_OK ||
                !enc_directional_predict(
                    token, 0U, above, 32U, left, 32U, 91U,
                    width, height, actual) ||
                !equal_bytes(expected, actual, width * height)) {
                for (index = 0U; index < width * height; ++index) {
                    if (expected[index] != actual[index]) {
                        fprintf(stderr,
                            "token=%u size=%zux%zu index=%zu expected=%u actual=%u\n",
                            token, width, height, index,
                            expected[index], actual[index]);
                        break;
                    }
                }
                return 11 + (int)(token - ENC_DIRECTIONAL_FIRST_TOKEN);
            }
            if (av2h_predict_directional(
                    &mode, angle, (uint16_t)width, (uint16_t)height,
                    &references, expected, width, 0U, 1U) != AV2H_OK ||
                !enc_directional_predict(
                    token, 1U, above, 32U, left, 32U, 91U,
                    width, height, actual) ||
                !equal_bytes(expected, actual, width * height)) {
                return 70 + (int)(token - ENC_DIRECTIONAL_FIRST_TOKEN);
            }
        }
    }
    return 0;
}

int main(void) {
    static const uint8_t above[] = { 7U, 8U, 9U };
    static const uint8_t left[] = { 20U, 21U };
    static const uint8_t expected_vertical[] = {
        7U, 8U, 9U, 7U, 8U, 9U
    };
    static const uint8_t expected_horizontal[] = {
        20U, 20U, 20U, 21U, 21U, 21U
    };
    Av2hLeafMode mode = { 0 };
    Av2hLumaReferences references = { 0 };
    uint8_t output[16U];
    size_t index;
    references.above = above;
    references.above_count = 3U;
    references.left = left;
    references.left_count = 2U;
    mode.mode = AV2H_INTRA_VERTICAL;
    if (av2h_predict_directional(
            &mode, 90, 3U, 2U, &references, output, 3U, 1U, 1U) != AV2H_OK ||
        !equal_bytes(output, expected_vertical, 6U)) {
        return 1;
    }
    mode.mode = AV2H_INTRA_HORIZONTAL;
    if (av2h_predict_directional(
            &mode, 180, 3U, 2U, &references, output, 3U, 1U, 1U) != AV2H_OK ||
        !equal_bytes(output, expected_horizontal, 6U)) {
        return 2;
    }
    references.above = 0;
    references.above_count = 0U;
    references.left = 0;
    references.left_count = 0U;
    mode.mode = AV2H_INTRA_VERTICAL;
    if (av2h_predict_directional(
            &mode, 90, 4U, 4U, &references, output, 4U, 1U, 1U) != AV2H_OK) {
        return 3;
    }
    for (index = 0U; index < 16U; ++index) {
        if (output[index] != 127U) return 4;
    }
    mode.mode = AV2H_INTRA_HORIZONTAL;
    if (av2h_predict_directional(
            &mode, 180, 4U, 4U, &references, output, 4U, 1U, 1U) != AV2H_OK) {
        return 5;
    }
    for (index = 0U; index < 16U; ++index) {
        if (output[index] != 129U) return 6;
    }
    return check_full_directional();
}

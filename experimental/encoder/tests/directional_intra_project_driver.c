#include "decoder/internal/reconstruct_modules.h"

static int equal_bytes(const uint8_t *left, const uint8_t *right, size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
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
    Av2hFirstLeafMode mode = { 0 };
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
    return 0;
}

#include "encoder/ac8x8.h"

static int equal_bytes(const uint8_t *a, const uint8_t *b, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (a[index] != b[index]) return 0;
    }
    return 1;
}

int main(void) {
    uint8_t input[64U];
    uint8_t workspace[4096U];
    uint8_t stream[256U];
    uint8_t repeat[256U];
    uint8_t reconstruction[64U];
    uint8_t guard[64U];
    EncAc8x8WorkspaceLayout layout;
    EncAc8x8Result first;
    EncAc8x8Result second;
    size_t index;
    for (index = 0U; index < 64U; ++index) {
        input[index] = (uint8_t)(((index / 8U + index % 8U) & 1U)
            ? 32U : 224U);
        reconstruction[index] = 0xA5U;
        guard[index] = 0xA5U;
    }
    if (enc_ac8x8_query_workspace(8U, 8U, &layout) != ENC_OK ||
        layout.size > sizeof(workspace) ||
        enc_ac8x8_i400_q_workspace(
            input, 8U, 8U, 8U, 100U, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, sizeof(workspace), stream, sizeof(stream),
            reconstruction, sizeof(reconstruction), &first) != ENC_OK ||
        first.stream_size == 0U || first.squared_error == 0U ||
        first.searched_blocks != 1U || first.ac_blocks != 1U) {
        return 1;
    }
    if (enc_ac8x8_i400_q_workspace(
            input, 8U, 8U, 8U, 100U, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, sizeof(workspace), repeat, sizeof(repeat),
            guard, sizeof(guard), &second) != ENC_OK ||
        first.stream_size != second.stream_size ||
        first.squared_error != second.squared_error ||
        !equal_bytes(stream, repeat, first.stream_size) ||
        !equal_bytes(reconstruction, guard, sizeof(guard))) {
        return 2;
    }
    for (index = 0U; index < sizeof(guard); ++index) guard[index] = 0x5AU;
    if (enc_ac8x8_i400_q_workspace(
            input, 8U, 8U, 8U, 100U, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, sizeof(workspace), repeat, first.stream_size - 1U,
            guard, sizeof(guard), &second) != ENC_OUT_OF_SPACE ||
        second.stream_size != 0U) {
        return 3;
    }
    for (index = 0U; index < sizeof(guard); ++index) {
        if (guard[index] != 0x5AU) return 4;
    }
    if (enc_ac8x8_i400_q_workspace(
            input, 8U, 8U, 8U, 100U, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, sizeof(workspace), workspace, sizeof(stream),
            guard, sizeof(guard), &second) != ENC_INVALID_ARGUMENT ||
        enc_ac8x8_i400_q_workspace(
            input, 8U, 8U, 8U, 100U, ENC_AC8X8_SEARCH_BOUNDED,
            workspace, layout.size - 1U, repeat, sizeof(repeat),
            guard, sizeof(guard), &second) != ENC_OUT_OF_SPACE) {
        return 5;
    }
    return 0;
}

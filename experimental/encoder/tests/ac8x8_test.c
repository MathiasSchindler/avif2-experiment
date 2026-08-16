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
    uint8_t workspace[16384U];
    uint8_t stream[256U];
    uint8_t repeat[256U];
    uint8_t reconstruction[64U];
    uint8_t guard[64U];
    EncM3Image8 image = {0};
    EncM3MutableImage8 output = {0};
    EncAc8x8WorkspaceLayout layout;
    EncAc8x8YuvWorkspaceLayout yuv_layout;
    EncAc8x8Result first;
    EncAc8x8Result second;
    EncAc8x8YuvResult yuv_result;
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
    image.format = ENC_M3_PIXEL_I400;
    image.width = 8U;
    image.height = 8U;
    image.planes[0U].data = input;
    image.planes[0U].stride = 8U;
    image.planes[0U].capacity = sizeof(input);
    output.format = ENC_M3_PIXEL_I400;
    output.width = 8U;
    output.height = 8U;
    output.planes[0U].data = guard;
    output.planes[0U].stride = 8U;
    output.planes[0U].capacity = sizeof(guard);
    if (enc_ac8x8_query_yuv8_workspace(
            ENC_M3_PIXEL_I400, 8U, 8U, &yuv_layout) != ENC_OK ||
        yuv_layout.size > sizeof(workspace) ||
        yuv_layout.transform_count[0U] != 1U ||
        yuv_layout.baseline_reconstruction_offset == 0U ||
        yuv_layout.coded_reconstruction_offset == 0U ||
        yuv_layout.luma_coefficients_offset == 0U ||
        yuv_layout.stream_staging_offset == 0U ||
        yuv_layout.stream_staging_count != 4U ||
        enc_ac8x8_yuv8_q_workspace(
            &image, 100U, ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
            workspace, sizeof(workspace), repeat, sizeof(repeat),
            &output, &yuv_result) != ENC_OK ||
        yuv_result.transform_count[0U] != 1U ||
        yuv_result.baseline_stream_size == 0U ||
        yuv_result.candidate_stream_size == 0U ||
        yuv_result.baseline_squared_error[0U] == 0U ||
        yuv_result.candidate_squared_error[0U] == 0U ||
        yuv_result.baseline_squared_error[0U] <=
            yuv_result.candidate_squared_error[0U] ||
        yuv_result.stream_size != yuv_result.candidate_stream_size ||
        yuv_result.squared_error[0U] !=
            yuv_result.candidate_squared_error[0U]) {
        return 6;
    }
    {
        EncM3Image8 *workspace_input = (EncM3Image8 *)(void *)workspace;
        EncM3MutableImage8 *workspace_output =
            (EncM3MutableImage8 *)(void *)workspace;
        EncM3Image8 *stream_input = (EncM3Image8 *)(void *)repeat;
        EncM3MutableImage8 *stream_output =
            (EncM3MutableImage8 *)(void *)repeat;
        *workspace_input = image;
        if (enc_ac8x8_yuv8_q_workspace(
                workspace_input, 100U,
                ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
                workspace, sizeof(workspace), repeat, sizeof(repeat),
                &output, &yuv_result) != ENC_INVALID_ARGUMENT) {
            return 7;
        }
        *stream_input = image;
        if (enc_ac8x8_yuv8_q_workspace(
                stream_input, 100U,
                ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
                workspace, sizeof(workspace), repeat, sizeof(repeat),
                &output, &yuv_result) != ENC_INVALID_ARGUMENT) {
            return 8;
        }
        *workspace_output = output;
        if (enc_ac8x8_yuv8_q_workspace(
                &image, 100U, ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
                workspace, sizeof(workspace), repeat, sizeof(repeat),
                workspace_output, &yuv_result) != ENC_INVALID_ARGUMENT) {
            return 9;
        }
        *stream_output = output;
        if (enc_ac8x8_yuv8_q_workspace(
                &image, 100U, ENC_AC8X8_COEFFICIENT_SEARCH_BOUNDED,
                workspace, sizeof(workspace), repeat, sizeof(repeat),
                stream_output, &yuv_result) != ENC_INVALID_ARGUMENT) {
            return 10;
        }
    }
    return 0;
}

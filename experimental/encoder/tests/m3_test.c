#include "encoder/m3.h"

static int bytes_equal(
    const uint8_t *left, const uint8_t *right, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static void fill(uint8_t *data, size_t size, uint8_t value) {
    size_t index;
    for (index = 0U; index < size; ++index) data[index] = value;
}

static int test_flat_levels(void) {
    static const uint8_t inputs[] = {0U, 30U, 55U, 79U, 104U,
                                    128U, 153U, 177U, 202U, 226U, 255U};
    static const uint8_t expected[] = {0U, 30U, 55U, 79U, 105U,
                                      128U, 153U, 176U, 202U, 226U, 255U};
    uint8_t input[64U];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    size_t test;
    size_t index;
    for (test = 0U; test < sizeof(inputs); ++test) {
        fill(input, sizeof(input), inputs[test]);
        if (enc_m3_dc_i400(
                input, 8U, 8U, 8U,
                stream, sizeof(stream),
                reconstruction, sizeof(reconstruction), &result) != ENC_OK ||
            result.stream_size < 19U ||
            result.stream_size > ENC_M3_MAX_STREAM_SIZE) {
            return 1;
        }
        for (index = 0U; index < 64U; ++index) {
            if (reconstruction[index] != expected[test]) return 2;
        }
        if (inputs[test] == expected[test] && result.squared_error != 0U) {
            return 3;
        }
    }
    return 0;
}

static int test_stride_determinism_and_bounds(void) {
    uint8_t input[8U * 12U];
    uint8_t packed[8U * 8U];
    uint8_t first[ENC_M3_MAX_STREAM_SIZE];
    uint8_t second[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result first_result;
    EncM3Result second_result;
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 12U; ++x) {
            input[y * 12U + x] = x < 8U
                ? (uint8_t)(y * 8U + x) : 0xa5U;
            if (x < 8U) packed[y * 8U + x] = input[y * 12U + x];
        }
    }
    if (enc_m3_dc_i400(
            input, 8U, 8U, 12U,
            first, sizeof(first), reconstruction,
            sizeof(reconstruction), &first_result) != ENC_OK ||
        enc_m3_dc_i400(
            packed, 8U, 8U, 8U,
            second, sizeof(second), reconstruction,
            sizeof(reconstruction), &second_result) != ENC_OK ||
        first_result.stream_size != second_result.stream_size ||
        first_result.squared_error != second_result.squared_error ||
        !bytes_equal(first, second, first_result.stream_size)) {
        return 1;
    }
    if (enc_m3_dc_i400(
            input, 8U, 8U, 12U,
            second, first_result.stream_size - 1U,
            reconstruction, sizeof(reconstruction), &second_result) !=
            ENC_OUT_OF_SPACE ||
        second_result.stream_size != 0U) {
        return 2;
    }
    if (enc_m3_dc_i400(
            input, 8U, 8U, 7U,
            second, sizeof(second), reconstruction,
            sizeof(reconstruction), &second_result) != ENC_INVALID_ARGUMENT ||
        enc_m3_dc_i400(
            input, 8U, 8U, 12U,
            second, sizeof(second), reconstruction,
            63U, &second_result) !=
            ENC_OUT_OF_SPACE) {
        return 3;
    }
    if (enc_m3_dc_i400(
            input, 0U, 8U, 12U, second, sizeof(second),
            reconstruction, sizeof(reconstruction), &second_result) !=
            ENC_INVALID_ARGUMENT ||
        enc_m3_dc_i400(
            input, 17U, 8U, 17U, second, sizeof(second),
            reconstruction, sizeof(reconstruction), &second_result) !=
            ENC_UNSUPPORTED) {
        return 4;
    }
    return 0;
}

static int test_workspace_layout_and_capacity(void) {
    EncM3WorkspaceLayout layout;
    uint16_t workspace[ENC_M3_LEGACY_WORKSPACE_SIZE / 2U];
    uint8_t input[64U];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[64U];
    EncM3Result result;
    size_t index;
    if (enc_m3_query_workspace(17U, 15U, &layout) != ENC_OK ||
        layout.padded_width != 24U || layout.padded_height != 16U ||
        layout.padded_pixel_count != 384U || layout.pixel_count != 255U ||
        layout.block_count != 6U || layout.stream_capacity != 640U ||
        layout.size != 2572U ||
        layout.alignment != ENC_M3_WORKSPACE_ALIGNMENT) {
        return 1;
    }
    if (enc_m3_query_workspace(16384U, 16384U, &layout) != ENC_OK ||
        layout.pixel_count != 268435456U ||
        layout.padded_pixel_count != 268435456U ||
        layout.block_count != 4194304U ||
        layout.stream_capacity != 268435712U ||
        layout.size != 1350566656U) {
        return 2;
    }
    if (enc_m3_query_workspace(64U, 64U, &layout) != ENC_OK ||
        layout.pixel_count != ENC_M3_MAX_PIXELS ||
        layout.padded_pixel_count != ENC_M3_MAX_PIXELS ||
        layout.block_count != ENC_M3_MAX_BLOCKS ||
        layout.stream_capacity != ENC_M3_MAX_STREAM_SIZE ||
        layout.size != ENC_M3_MAX_WORKSPACE_SIZE) {
        return 17;
    }
    if (enc_m3_query_workspace(65U, 1U, &layout) != ENC_OK ||
        layout.padded_width != 72U || layout.padded_height != 8U ||
        layout.pixel_count != 65U || layout.padded_pixel_count != 576U ||
        layout.block_count != 9U || layout.stream_capacity != 832U ||
        layout.size != 3156U ||
        enc_m3_query_workspace(896U, 424U, &layout) != ENC_OK ||
        layout.padded_width != 896U || layout.padded_height != 424U ||
        layout.pixel_count != 379904U ||
        layout.padded_pixel_count != 379904U ||
        layout.block_count != 5936U ||
        layout.stream_capacity != 380160U ||
        layout.size != 1912160U) {
        return 18;
    }

    if (enc_m3_query_workspace(SIZE_MAX, 1U, &layout) != ENC_OVERFLOW ||
        enc_m3_query_workspace(SIZE_MAX / 2U, 3U, &layout) != ENC_OVERFLOW ||
        enc_m3_query_workspace(0U, 1U, &layout) != ENC_INVALID_ARGUMENT) {
        return 3;
    }
    if (enc_m3_query_workspace(8U, 8U, &layout) != ENC_OK ||
        layout.size > sizeof(workspace)) {
        return 4;
    }
    fill(input, sizeof(input), 81U);
    fill(stream, sizeof(stream), 0xa5U);
    fill(reconstruction, sizeof(reconstruction), 0x5aU);
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, workspace, layout.size - 1U,
            stream, sizeof(stream), reconstruction, sizeof(reconstruction),
            &result) != ENC_OUT_OF_SPACE) {
        return 5;
    }
    for (index = 0U; index < sizeof(stream); ++index) {
        if (stream[index] != 0xa5U) return 6;
    }
    for (index = 0U; index < sizeof(reconstruction); ++index) {
        if (reconstruction[index] != 0x5aU) return 7;
    }
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, (uint8_t *)workspace + 1U, layout.size,
            stream, sizeof(stream), reconstruction, sizeof(reconstruction),
            &result) != ENC_INVALID_ARGUMENT) {
        return 8;
    }
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, workspace, layout.size,
            workspace, sizeof(stream), reconstruction, sizeof(reconstruction),
            &result) != ENC_INVALID_ARGUMENT) {
        return 9;
    }
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, workspace, layout.size,
            stream, sizeof(stream), reconstruction, sizeof(reconstruction),
            &result) != ENC_OK ||
        result.workspace_size != layout.size) {
        return 10;
    }
    fill(stream, sizeof(stream), 0xa5U);
    fill(reconstruction, sizeof(reconstruction), 0x5aU);
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, workspace, layout.size,
            stream, result.stream_size - 1U,
            reconstruction, sizeof(reconstruction), &result) !=
            ENC_OUT_OF_SPACE) {
        return 11;
    }
    for (index = 0U; index < sizeof(stream); ++index) {
        if (stream[index] != 0xa5U) return 12;
    }
    for (index = 0U; index < sizeof(reconstruction); ++index) {
        if (reconstruction[index] != 0x5aU) return 13;
    }
    if (enc_m3_dc_i400_workspace(
            input, 8U, 8U, 8U, workspace, layout.size,
            stream, sizeof(stream), reconstruction, 63U, &result) !=
            ENC_OUT_OF_SPACE) {
        return 14;
    }
    for (index = 0U; index < sizeof(stream); ++index) {
        if (stream[index] != 0xa5U) return 15;
    }
    for (index = 0U; index < sizeof(reconstruction); ++index) {
        if (reconstruction[index] != 0x5aU) return 16;
    }
    return 0;
}

static int test_maximum_capacity_stride_and_atomicity(void) {
    uint8_t input[64U * 67U];
    uint16_t workspace[ENC_M3_MAX_WORKSPACE_SIZE / 2U];
    uint8_t first[ENC_M3_MAX_STREAM_SIZE];
    uint8_t second[ENC_M3_MAX_STREAM_SIZE];
    uint8_t first_reconstruction[ENC_M3_MAX_PIXELS];
    uint8_t second_reconstruction[ENC_M3_MAX_PIXELS];
    EncM3WorkspaceLayout layout;
    EncM3Result first_result;
    EncM3Result second_result;
    size_t y;
    size_t x;
    size_t index;
    for (y = 0U; y < 64U; ++y) {
        for (x = 0U; x < 67U; ++x) {
            input[y * 67U + x] = x < 64U
                ? (uint8_t)(x * 17U + y * 31U + x * y * 3U)
                : 0xa5U;
        }
    }
    if (enc_m3_query_workspace(64U, 64U, &layout) != ENC_OK ||
        enc_m3_dc_i400_q_workspace(
            input, 64U, 64U, 67U, 191U,
            workspace, sizeof(workspace),
            first, sizeof(first),
            first_reconstruction, sizeof(first_reconstruction),
            &first_result) != ENC_OK ||
        first_result.block_count != ENC_M3_MAX_BLOCKS ||
        first_result.pixel_count != ENC_M3_MAX_PIXELS ||
        first_result.workspace_size != ENC_M3_MAX_WORKSPACE_SIZE ||
        enc_m3_dc_i400_q(
            input, 64U, 64U, 67U, 191U,
            second, sizeof(second),
            second_reconstruction, sizeof(second_reconstruction),
            &second_result) != ENC_OK ||
        first_result.stream_size != second_result.stream_size ||
        first_result.squared_error != second_result.squared_error ||
        !bytes_equal(first, second, first_result.stream_size) ||
        !bytes_equal(
            first_reconstruction, second_reconstruction,
            sizeof(first_reconstruction))) {
        return 1;
    }
    fill(second, sizeof(second), 0xa5U);
    fill(second_reconstruction, sizeof(second_reconstruction), 0x5aU);
    if (enc_m3_dc_i400_q_workspace(
            input, 64U, 64U, 67U, 191U,
            workspace, sizeof(workspace),
            second, first_result.stream_size - 1U,
            second_reconstruction, sizeof(second_reconstruction),
            &second_result) != ENC_OUT_OF_SPACE ||
        second_result.stream_size != 0U) {
        return 2;
    }
    for (index = 0U; index < sizeof(second); ++index) {
        if (second[index] != 0xa5U) return 3;
    }
    for (index = 0U; index < sizeof(second_reconstruction); ++index) {
        if (second_reconstruction[index] != 0x5aU) return 4;
    }
    if (enc_m3_dc_i400_q_workspace(
            input, 64U, 64U, 67U, 191U,
            workspace, sizeof(workspace),
            second, sizeof(second),
            second_reconstruction, ENC_M3_MAX_PIXELS - 1U,
            &second_result) != ENC_OUT_OF_SPACE) {
        return 5;
    }
    for (index = 0U; index < sizeof(second); ++index) {
        if (second[index] != 0xa5U) return 6;
    }
    for (index = 0U; index < sizeof(second_reconstruction); ++index) {
        if (second_reconstruction[index] != 0x5aU) return 7;
    }
    if (enc_m3_dc_i400_q_workspace(
            input, 64U, 64U, SIZE_MAX, 191U,
            workspace, sizeof(workspace),
            second, sizeof(second),
            second_reconstruction, sizeof(second_reconstruction),
            &second_result) != ENC_OVERFLOW) {
        return 8;
    }
    return 0;
}

static int test_stride_overflow(void) {
    uint8_t input[1U] = {128U};
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    if (enc_m3_dc_i400(
            input, 8U, 8U, SIZE_MAX,
            stream, sizeof(stream), reconstruction,
            sizeof(reconstruction), &result) != ENC_OVERFLOW) {
        return 1;
    }
    if (enc_m3_dc_i400(
            input, 8U, 8U, SIZE_MAX / 7U,
            stream, sizeof(stream), reconstruction,
            sizeof(reconstruction), &result) != ENC_OVERFLOW) {
        return 2;
    }
    return 0;
}

static int test_16x16_grid(void) {
    static const uint8_t values[2U][2U] = {
        {128U, 200U}, {50U, 150U}
    };
    uint8_t input[256U];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    size_t block_y;
    size_t block_x;
    size_t y;
    size_t x;
    for (block_y = 0U; block_y < 2U; ++block_y) {
        for (block_x = 0U; block_x < 2U; ++block_x) {
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    input[(block_y * 8U + y) * 16U +
                          block_x * 8U + x] = values[block_y][block_x];
                }
            }
        }
    }
    if (enc_m3_dc_i400(
            input, 16U, 16U, 16U, stream, sizeof(stream),
            reconstruction, sizeof(reconstruction), &result) != ENC_OK ||
        result.block_count != 4U || result.pixel_count != 256U ||
        result.squared_error != 0U) {
        return 1;
    }
    for (block_y = 0U; block_y < 2U; ++block_y) {
        for (block_x = 0U; block_x < 2U; ++block_x) {
            for (y = 0U; y < 8U; ++y) {
                for (x = 0U; x < 8U; ++x) {
                    if (reconstruction[(block_y * 8U + y) * 16U +
                                       block_x * 8U + x] !=
                            values[block_y][block_x]) {
                        return 2;
                    }
                }
            }
        }
    }
    if (enc_m3_dc_i400(
            input, 16U, 16U, 16U, stream, result.stream_size - 1U,
            reconstruction, sizeof(reconstruction), &result) !=
            ENC_OUT_OF_SPACE ||
        result.stream_size != 0U) {
        return 3;
    }
    return 0;
}

static int test_rectangular_grids(void) {
    static const size_t widths[] = {8U, 16U};
    static const size_t heights[] = {16U, 8U};
    uint8_t input[128U];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    size_t test;
    size_t index;
    for (test = 0U; test < 2U; ++test) {
        for (index = 0U; index < 128U; ++index) {
            input[index] = index < 64U ? 40U : 200U;
        }
        if (enc_m3_dc_i400(
                input, widths[test], heights[test], widths[test],
                stream, sizeof(stream), reconstruction,
                sizeof(reconstruction), &result) != ENC_OK ||
            result.block_count != 2U || result.pixel_count != 128U) {
            return 1;
        }
        if (enc_m3_dc_i400(
                input, widths[test], heights[test], widths[test],
                stream, result.stream_size - 1U, reconstruction,
                sizeof(reconstruction), &result) != ENC_OUT_OF_SPACE ||
            result.stream_size != 0U) {
            return 2;
        }
    }
    return 0;
}

static int test_odd_dimensions_and_qindex(void) {
    static const size_t widths[] = {1U, 3U, 7U, 8U, 9U, 15U, 16U};
    static const size_t heights[] = {1U, 2U, 9U, 1U, 7U, 16U, 15U};
    uint8_t input[ENC_M3_MAX_PIXELS];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    size_t test;
    size_t index;
    for (test = 0U; test < sizeof(widths) / sizeof(widths[0]); ++test) {
        size_t pixels = widths[test] * heights[test];
        for (index = 0U; index < pixels; ++index) {
            input[index] = (uint8_t)(index * 37U + test * 11U);
        }
        if (enc_m3_dc_i400_q(
                input, widths[test], heights[test], widths[test], 191U,
                stream, sizeof(stream), reconstruction, pixels, &result) !=
                ENC_OK ||
            result.pixel_count != pixels || result.qindex != 191U ||
            result.block_count !=
                ((widths[test] + 7U) / 8U) *
                ((heights[test] + 7U) / 8U)) {
            return 1;
        }
    }
    if (enc_m3_dc_i400_q(
            input, 1U, 1U, 1U, 0U, stream, sizeof(stream),
            reconstruction, sizeof(reconstruction), &result) !=
            ENC_INVALID_ARGUMENT ||
        enc_m3_dc_i400_q(
            input, 1U, 1U, 1U, 256U, stream, sizeof(stream),
            reconstruction, sizeof(reconstruction), &result) !=
            ENC_INVALID_ARGUMENT) {
        return 2;
    }
    return 0;
}

static int test_failure_atomicity(void) {
    uint8_t input[64U];
    uint8_t stream[ENC_M3_MAX_STREAM_SIZE];
    uint8_t reconstruction[ENC_M3_MAX_PIXELS];
    EncM3Result result;
    size_t index;
    fill(input, sizeof(input), 73U);
    fill(stream, sizeof(stream), 0xa5U);
    fill(reconstruction, sizeof(reconstruction), 0x5aU);
    if (enc_m3_dc_i400(
            input, 8U, 8U, 8U, stream, 1U,
            reconstruction, sizeof(reconstruction), &result) !=
            ENC_OUT_OF_SPACE) {
        return 1;
    }
    for (index = 0U; index < sizeof(stream); ++index) {
        if (stream[index] != 0xa5U) return 2;
    }
    for (index = 0U; index < sizeof(reconstruction); ++index) {
        if (reconstruction[index] != 0x5aU) return 3;
    }
    if (result.stream_size != 0U || result.pixel_count != 0U) return 4;
    if (enc_m3_dc_i400(
            input, 8U, 8U, 8U, stream, sizeof(stream),
            reconstruction, 63U, &result) != ENC_OUT_OF_SPACE) {
        return 5;
    }
    for (index = 0U; index < sizeof(stream); ++index) {
        if (stream[index] != 0xa5U) return 6;
    }
    for (index = 0U; index < sizeof(reconstruction); ++index) {
        if (reconstruction[index] != 0x5aU) return 7;
    }
    return 0;
}

int main(void) {
    static const uint8_t above[] = {8U, 16U, 24U, 32U};
    static const uint8_t left[] = {40U, 48U, 56U, 64U};
    int result = test_flat_levels();
    if (result != 0) return 10 + result;
    result = test_stride_determinism_and_bounds();
    if (result != 0) return 20 + result;
    result = test_stride_overflow();
    if (result != 0) return 30 + result;
    result = test_16x16_grid();
    if (result != 0) return 40 + result;
    result = test_rectangular_grids();
    if (result != 0) return 50 + result;
    result = test_odd_dimensions_and_qindex();
    if (result != 0) return 60 + result;
    result = test_failure_atomicity();
    if (result != 0) return 70 + result;
    result = test_workspace_layout_and_capacity();
    if (result != 0) return 80 + result;
    result = test_maximum_capacity_stride_and_atomicity();
    if (result != 0) return 110 + result;
    if (enc_m3_dc_prediction(0, 0, 8U) != 128U ||
        enc_m3_dc_prediction(above, 0, 4U) != 20U ||
        enc_m3_dc_prediction(0, left, 4U) != 52U ||
        enc_m3_dc_prediction(above, left, 4U) != 36U) {
        return 100;
    }
    return 0;
}

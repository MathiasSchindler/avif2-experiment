#include "encoder/m3.h"

static void fill(uint8_t *data, size_t size, uint8_t value) {
    size_t index;
    for (index = 0U; index < size; ++index) data[index] = value;
}

static int all_equal(const uint8_t *data, size_t size, uint8_t value) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (data[index] != value) return 0;
    }
    return 1;
}

static int run_case(
    EncM3PixelFormat format,
    uint8_t y_value, uint8_t u_value, uint8_t v_value,
    int16_t y_level, int16_t u_level, int16_t v_level) {
    uint8_t y[8U * 10U];
    uint8_t u[8U * 10U];
    uint8_t v[8U * 10U];
    uint8_t ry[8U * 11U];
    uint8_t ru[8U * 11U];
    uint8_t rv[8U * 11U];
    uint8_t workspace[448U];
    uint8_t stream[ENC_M3_LEGACY_MAX_STREAM_SIZE];
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaWorkspaceLayout layout;
    EncM3ChromaResult result;
    size_t chroma_width = format == ENC_M3_PIXEL_I420 ? 4U : 8U;
    size_t chroma_height = chroma_width;
    size_t row;
    fill(y, sizeof(y), 0xa5U);
    fill(u, sizeof(u), 0xa5U);
    fill(v, sizeof(v), 0xa5U);
    fill(ry, sizeof(ry), 0x5aU);
    fill(ru, sizeof(ru), 0x5aU);
    fill(rv, sizeof(rv), 0x5aU);
    for (row = 0U; row < 8U; ++row) fill(y + row * 10U, 8U, y_value);
    for (row = 0U; row < chroma_height; ++row) {
        fill(u + row * 10U, chroma_width, u_value);
        fill(v + row * 10U, chroma_width, v_value);
    }
    input.format = format;
    input.width = 8U;
    input.height = 8U;
    input.planes[0] = (EncM3Plane8){y, 10U, sizeof(y)};
    input.planes[1] = (EncM3Plane8){u, 10U, sizeof(u)};
    input.planes[2] = (EncM3Plane8){v, 10U, sizeof(v)};
    reconstruction.format = format;
    reconstruction.width = 8U;
    reconstruction.height = 8U;
    reconstruction.planes[0] =
        (EncM3MutablePlane8){ry, 11U, sizeof(ry)};
    reconstruction.planes[1] =
        (EncM3MutablePlane8){ru, 11U, sizeof(ru)};
    reconstruction.planes[2] =
        (EncM3MutablePlane8){rv, 11U, sizeof(rv)};
    if (enc_m3_query_chroma_workspace(format, 8U, 8U, &layout) != ENC_OK ||
        layout.stream_capacity != ENC_M3_LEGACY_MAX_STREAM_SIZE ||
        layout.plane_size[0] != 64U ||
        layout.plane_size[1] != chroma_width * chroma_height ||
        layout.reconstruction_size !=
            64U + 2U * chroma_width * chroma_height ||
        layout.size !=
            layout.reconstruction_size + ENC_M3_LEGACY_MAX_STREAM_SIZE ||
        layout.size > sizeof(workspace)) {
        return 1;
    }
    if (enc_m3_dc_yuv8_q_workspace(
            &input, 100U, workspace, layout.size,
            stream, sizeof(stream), &reconstruction, &result) != ENC_OK ||
        result.stream_size < 22U ||
        result.dc_level[0] != y_level ||
        result.dc_level[1] != u_level ||
        result.dc_level[2] != v_level ||
        result.squared_error[0] != 0U ||
        result.squared_error[1] != 0U ||
        result.squared_error[2] != 0U) {
        return 2;
    }
    for (row = 0U; row < 8U; ++row) {
        if (!all_equal(ry + row * 11U, 8U, y_value) ||
            !all_equal(ry + row * 11U + 8U, 3U, 0x5aU)) return 3;
    }
    for (row = 0U; row < chroma_height; ++row) {
        if (!all_equal(ru + row * 11U, chroma_width, u_value) ||
            !all_equal(rv + row * 11U, chroma_width, v_value)) return 4;
    }
    return 0;
}

static int test_atomic_failures(void) {
    uint8_t planes[3U][64U];
    uint8_t outputs[3U][64U];
    uint8_t workspace[448U];
    uint8_t stream[ENC_M3_LEGACY_MAX_STREAM_SIZE];
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaWorkspaceLayout layout;
    EncM3ChromaResult result;
    size_t plane;
    fill(planes[0], sizeof(planes[0]), 128U);
    fill(planes[1], sizeof(planes[1]), 129U);
    fill(planes[2], sizeof(planes[2]), 127U);
    fill(stream, sizeof(stream), 0xa5U);
    for (plane = 0U; plane < 3U; ++plane) fill(outputs[plane], 64U, 0x5aU);
    input.format = ENC_M3_PIXEL_I444;
    input.width = 8U;
    input.height = 8U;
    reconstruction.format = input.format;
    reconstruction.width = 8U;
    reconstruction.height = 8U;
    for (plane = 0U; plane < 3U; ++plane) {
        input.planes[plane] =
            (EncM3Plane8){planes[plane], 8U, sizeof(planes[plane])};
        reconstruction.planes[plane] =
            (EncM3MutablePlane8){outputs[plane], 8U, sizeof(outputs[plane])};
    }
    if (enc_m3_query_chroma_workspace(
            input.format, 8U, 8U, &layout) != ENC_OK) return 1;
    if (enc_m3_dc_yuv8_q_workspace(
            &input, 100U, workspace, layout.size - 1U,
            stream, sizeof(stream), &reconstruction, &result) !=
            ENC_OUT_OF_SPACE) return 2;
    if (!all_equal(stream, sizeof(stream), 0xa5U)) return 3;
    for (plane = 0U; plane < 3U; ++plane) {
        if (!all_equal(outputs[plane], 64U, 0x5aU)) return 4;
    }
    if (enc_m3_dc_yuv8_q_workspace(
            &input, 100U, workspace, layout.size,
            stream, 1U, &reconstruction, &result) != ENC_OUT_OF_SPACE) {
        return 5;
    }
    if (!all_equal(stream, sizeof(stream), 0xa5U)) return 6;
    for (plane = 0U; plane < 3U; ++plane) {
        if (!all_equal(outputs[plane], 64U, 0x5aU)) return 7;
    }
    reconstruction.planes[0].data = planes[0];
    if (enc_m3_dc_yuv8_q_workspace(
            &input, 100U, workspace, layout.size,
            stream, sizeof(stream), &reconstruction, &result) !=
            ENC_INVALID_ARGUMENT) return 8;
    if (enc_m3_query_chroma_workspace(
            ENC_M3_PIXEL_I420, 9U, 8U, &layout) != ENC_UNSUPPORTED ||
        enc_m3_query_chroma_workspace(
            ENC_M3_PIXEL_I400, 8U, 8U, &layout) != ENC_INVALID_ARGUMENT) {
        return 9;
    }
    return 0;
}

int main(void) {
    int status = run_case(
        ENC_M3_PIXEL_I420, 128U, 131U, 125U, 0, 1, -1);
    if (status != 0) return 10 + status;
    status = run_case(
        ENC_M3_PIXEL_I444, 129U, 134U, 127U, 1, 4, -1);
    if (status != 0) return 20 + status;
    status = test_atomic_failures();
    return status == 0 ? 0 : 30 + status;
}

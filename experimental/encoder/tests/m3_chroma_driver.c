#include "encoder/m3.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    uint8_t *planes[3U] = {0, 0, 0};
    uint8_t *reconstruction_planes[3U] = {0, 0, 0};
    void *workspace = 0;
    uint8_t *stream = 0;
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaWorkspaceLayout layout;
    EncM3ChromaResult result;
    EncM3PixelFormat format;
    unsigned int qindex;
    size_t width;
    size_t height;
    size_t chroma_size;
    size_t plane;
    FILE *file;
    if (argc != 10) return 2;
    format = (EncM3PixelFormat)strtoul(argv[1], 0, 10);
    qindex = (unsigned int)strtoul(argv[2], 0, 10);
    width = (size_t)strtoul(argv[3], 0, 10);
    height = (size_t)strtoul(argv[4], 0, 10);
    input.format = format;
    input.width = width;
    input.height = height;
    reconstruction.format = format;
    reconstruction.width = width;
    reconstruction.height = height;
    chroma_size = format == ENC_M3_PIXEL_I420
        ? ((width + 1U) / 2U) * ((height + 1U) / 2U)
        : width * height;
    if (enc_m3_query_chroma_workspace(
            format, width, height, &layout) != ENC_OK) return 3;
    workspace = calloc(1U, layout.size);
    stream = malloc(layout.stream_capacity);
    if (workspace == 0 || stream == 0) return 3;
    for (plane = 0U; plane < 3U; ++plane) {
        size_t plane_width = plane == 0U ? width :
            (format == ENC_M3_PIXEL_I420 ? (width + 1U) / 2U : width);
        size_t size = plane == 0U ? width * height : chroma_size;
        size_t index;
        unsigned long value = strtoul(argv[5U + plane], 0, 10);
        if (value > 256U) return 2;
        planes[plane] = malloc(size);
        reconstruction_planes[plane] = malloc(size);
        if (planes[plane] == 0 || reconstruction_planes[plane] == 0) return 3;
        for (index = 0U; index < size; ++index) {
            planes[plane][index] = value == 256U
                ? (uint8_t)(index * 37U + plane * 71U +
                            (index / plane_width) * 19U)
                : (uint8_t)value;
        }
        input.planes[plane] = (EncM3Plane8){
            planes[plane], plane_width, size};
        reconstruction.planes[plane] = (EncM3MutablePlane8){
            reconstruction_planes[plane], input.planes[plane].stride, size};
    }
    if (enc_m3_dc_yuv8_q_workspace(
            &input, qindex, workspace, layout.size,
            stream, layout.stream_capacity, &reconstruction, &result) !=
            ENC_OK) {
        return 3;
    }
    file = fopen(argv[8], "wb");
    if (file == 0 || fwrite(stream, 1U, result.stream_size, file) !=
            result.stream_size || fclose(file) != 0) return 4;
    file = fopen(argv[9], "wb");
    if (file == 0) return 4;
    for (plane = 0U; plane < 3U; ++plane) {
        size_t size = plane == 0U ? width * height : chroma_size;
        if (fwrite(reconstruction_planes[plane], 1U, size, file) != size) {
            fclose(file);
            return 4;
        }
    }
    if (fclose(file) != 0) return 4;
    return 0;
}

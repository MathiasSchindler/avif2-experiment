#include "encoder/m3.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    uint8_t planes[3U][64U];
    uint8_t reconstruction_planes[3U][64U];
    uint8_t workspace[448U];
    uint8_t stream[ENC_M3_LEGACY_MAX_STREAM_SIZE];
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaWorkspaceLayout layout;
    EncM3ChromaResult result;
    EncM3PixelFormat format;
    unsigned int qindex;
    size_t chroma_size;
    size_t plane;
    FILE *file;
    if (argc != 8) return 2;
    format = (EncM3PixelFormat)strtoul(argv[1], 0, 10);
    qindex = (unsigned int)strtoul(argv[2], 0, 10);
    input.format = format;
    input.width = 8U;
    input.height = 8U;
    reconstruction.format = format;
    reconstruction.width = 8U;
    reconstruction.height = 8U;
    chroma_size = format == ENC_M3_PIXEL_I420 ? 16U : 64U;
    for (plane = 0U; plane < 3U; ++plane) {
        size_t size = plane == 0U ? 64U : chroma_size;
        size_t index;
        unsigned long value = strtoul(argv[3U + plane], 0, 10);
        if (value > 255U) return 2;
        for (index = 0U; index < size; ++index) {
            planes[plane][index] = (uint8_t)value;
        }
        input.planes[plane] = (EncM3Plane8){
            planes[plane], plane == 0U ? 8U :
                (format == ENC_M3_PIXEL_I420 ? 4U : 8U), size};
        reconstruction.planes[plane] = (EncM3MutablePlane8){
            reconstruction_planes[plane], input.planes[plane].stride, size};
    }
    if (enc_m3_query_chroma_workspace(
            format, 8U, 8U, &layout) != ENC_OK ||
        enc_m3_dc_yuv8_q_workspace(
            &input, qindex, workspace, layout.size,
            stream, sizeof(stream), &reconstruction, &result) != ENC_OK) {
        return 3;
    }
    file = fopen(argv[6], "wb");
    if (file == 0 || fwrite(stream, 1U, result.stream_size, file) !=
            result.stream_size || fclose(file) != 0) return 4;
    file = fopen(argv[7], "wb");
    if (file == 0) return 4;
    for (plane = 0U; plane < 3U; ++plane) {
        size_t size = plane == 0U ? 64U : chroma_size;
        if (fwrite(reconstruction_planes[plane], 1U, size, file) != size) {
            fclose(file);
            return 4;
        }
    }
    if (fclose(file) != 0) return 4;
    return 0;
}

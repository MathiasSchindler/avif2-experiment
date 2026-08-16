#include "encoder/m3.h"

#include <stdio.h>
#include <stdlib.h>

static const uint8_t u_plane[16U] = {
    92U, 101U, 110U, 119U, 97U, 106U, 115U, 124U,
    102U, 111U, 120U, 129U, 107U, 116U, 125U, 134U
};

static const uint8_t v_plane[16U] = {
    166U, 156U, 146U, 136U, 162U, 152U, 142U, 132U,
    158U, 148U, 138U, 128U, 154U, 144U, 134U, 124U
};

int main(int argc, char **argv) {
    uint8_t y_plane[64U];
    uint8_t y_reconstruction[64U];
    uint8_t u_reconstruction[16U];
    uint8_t v_reconstruction[16U];
    uint8_t workspace[4096U];
    uint8_t stream[1024U];
    EncM3Image8 input;
    EncM3MutableImage8 reconstruction;
    EncM3ChromaResult result;
    unsigned int qindex;
    int u_qdelta;
    int v_qdelta;
    FILE *output;
    size_t index;
    EncStatus status;
    if (argc != 5) return 2;
    qindex = (unsigned int)strtoul(argv[1], 0, 10);
    u_qdelta = (int)strtol(argv[2], 0, 10);
    v_qdelta = (int)strtol(argv[3], 0, 10);
    for (index = 0U; index < sizeof(y_plane); ++index) y_plane[index] = 128U;
    input.format = ENC_M3_PIXEL_I420;
    input.width = 8U;
    input.height = 8U;
    input.planes[0U] = (EncM3Plane8){
        y_plane, 8U, sizeof(y_plane)
    };
    input.planes[1U] = (EncM3Plane8){
        u_plane, 4U, sizeof(u_plane)
    };
    input.planes[2U] = (EncM3Plane8){
        v_plane, 4U, sizeof(v_plane)
    };
    reconstruction.format = input.format;
    reconstruction.width = input.width;
    reconstruction.height = input.height;
    reconstruction.planes[0U] = (EncM3MutablePlane8){
        y_reconstruction, 8U, sizeof(y_reconstruction)
    };
    reconstruction.planes[1U] = (EncM3MutablePlane8){
        u_reconstruction, 4U, sizeof(u_reconstruction)
    };
    reconstruction.planes[2U] = (EncM3MutablePlane8){
        v_reconstruction, 4U, sizeof(v_reconstruction)
    };
    status = u_qdelta == 999 && v_qdelta == 999
        ? enc_m3_intra_yuv8_q_workspace(
              &input, qindex, ENC_M3_INTRA_SEARCH_PLANE_QDELTA,
              workspace, sizeof(workspace), stream, sizeof(stream),
              &reconstruction, &result)
        : enc_m3_plane_qdelta_yuv8_q_workspace(
              &input, qindex, u_qdelta, v_qdelta,
              workspace, sizeof(workspace), stream, sizeof(stream),
              &reconstruction, &result);
    if (status != ENC_OK) return 3;
    output = fopen(argv[4], "wb");
    if (output == 0 ||
        fwrite(stream, 1U, result.stream_size, output) != result.stream_size ||
        fclose(output) != 0) {
        return 4;
    }
    printf("%zu %llu %llu %d %d\n",
           result.stream_size,
           (unsigned long long)result.squared_error[1U],
           (unsigned long long)result.squared_error[2U],
           (int)result.u_qdelta, (int)result.v_qdelta);
    return 0;
}

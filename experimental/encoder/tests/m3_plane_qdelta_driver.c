#include "encoder/m3.h"
#include "encoder/syntax.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    unsigned int format;
    unsigned int qindex;
    int y_level;
    int u_level;
    int v_level;
    uint8_t tile[256U];
    size_t tile_size;
    FILE *output;
    EncStatus status;
    if (argc != 7) return 2;
    format = (unsigned int)strtoul(argv[1], 0, 10);
    qindex = (unsigned int)strtoul(argv[2], 0, 10);
    y_level = (int)strtol(argv[3], 0, 10);
    u_level = (int)strtol(argv[4], 0, 10);
    v_level = (int)strtol(argv[5], 0, 10);
    if ((format != ENC_M3_PIXEL_I420 && format != ENC_M3_PIXEL_I444) ||
        qindex == 0U || qindex > 255U ||
        y_level < INT16_MIN || y_level > INT16_MAX ||
        u_level < INT16_MIN || u_level > INT16_MAX ||
        v_level < INT16_MIN || v_level > INT16_MAX) {
        return 2;
    }
    status = enc_m3_dc_i420_i444_8x8_tile_q(
        tile, sizeof(tile), &tile_size,
        (int16_t)y_level, (int16_t)u_level, (int16_t)v_level,
        format, qindex);
    if (status != ENC_OK) return 3;
    output = fopen(argv[6], "wb");
    if (output == 0 ||
        fwrite(tile, 1U, tile_size, output) != tile_size ||
        fclose(output) != 0) {
        return 4;
    }
    return 0;
}

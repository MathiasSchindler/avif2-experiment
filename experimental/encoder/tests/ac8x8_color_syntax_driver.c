#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static uint16_t private_sec_tx_cdf[8U];
static uint16_t *private_primary_tx_cdf;

static uint16_t *private_color_primary_tx_cdf(
    EncSyntaxCdfs *cdfs, size_t context) {
    private_primary_tx_cdf =
        enc_syntax_intra_tx_type_set1_cdf(cdfs, context);
    return private_primary_tx_cdf;
}

static EncStatus private_color_range_symbol(
    EncRangeWriter *writer, uint32_t symbol, uint16_t *cdf,
    size_t symbol_count, int update_cdf) {
    EncStatus status = enc_range_writer_symbol(
        writer, symbol, cdf, symbol_count, update_cdf);
    if (status == ENC_OK && cdf == private_primary_tx_cdf) {
        private_primary_tx_cdf = 0;
        status = enc_range_writer_symbol(
            writer, 0U, private_sec_tx_cdf, 4U, 1);
    }
    return status;
}

#define enc_syntax_intra_tx_type_set1_cdf private_color_primary_tx_cdf
#define enc_range_writer_symbol private_color_range_symbol
#include "../src/syntax.c"
#undef enc_range_writer_symbol
#undef enc_syntax_intra_tx_type_set1_cdf

static int read_i16(FILE *file, int16_t *value) {
    uint8_t bytes[2U];
    if (fread(bytes, 1U, sizeof(bytes), file) != sizeof(bytes)) return 0;
    *value = (int16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
    return 1;
}

static EncStatus private_color_ac_tile(
    void *data, size_t capacity, size_t *size,
    size_t width, size_t height, const int16_t *y_levels,
    const int16_t *u_levels, const int16_t *v_levels,
    unsigned int chroma_format_idc, unsigned int qindex) {
    EncSyntaxCdfs cdfs;
    EncRangeWriter writer;
    EncM3ColorTileState state;
    EncM3TileState luma;
    size_t block_count = ((width + 7U) / 8U) * ((height + 7U) / 8U);
    size_t index;
    size_t row;
    size_t column;
    EncStatus status;
    if (size == 0 || (data == 0 && capacity != 0U) ||
        y_levels == 0 || u_levels == 0 || v_levels == 0 ||
        width == 0U || height == 0U ||
        width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        (chroma_format_idc != 0U && chroma_format_idc != 2U) ||
        qindex == 0U || qindex > 255U) {
        return ENC_INVALID_ARGUMENT;
    }
    *size = 0U;
    {
        static const uint16_t thresholds[3U] = {2357U, 9504U, 16641U};
        status = enc_cdf_materialize(
            private_sec_tx_cdf, 8U, thresholds, 4U, 5U, 0U);
        if (status != ENC_OK) return status;
    }
    status = enc_syntax_cdfs_init(&cdfs);
    if (status != ENC_OK) return status;
    enc_range_writer_init(&writer, data, capacity);
    if (width == 8U && height == 8U) {
        size_t q_context = qindex <= 90U ? 0U :
                           qindex <= 140U ? 1U :
                           qindex <= 190U ? 2U : 3U;
        size_t chroma_tx_size = chroma_format_idc == 2U ? 1U : 0U;
        if (enc_range_writer_symbol(
                &writer, 0U, enc_syntax_do_split_cdf(&cdfs, 0U, 0U),
                2U, 1) != ENC_OK ||
            write_m3_intra_mode(
                &writer, &cdfs, 8U, 8U, 2U, 0U, 0U, 0U) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, 0U,
                enc_syntax_tx_do_partition_cdf(&cdfs, 0U, 0U, 1U),
                2U, 1) != ENC_OK ||
            write_ac8x8_coeff(
                &writer, &cdfs, y_levels, 0U, 0U, q_context, 0) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, 0U, enc_syntax_is_cfl_cdf(&cdfs, 0U),
                2U, 1) != ENC_OK ||
            enc_range_writer_symbol(
                &writer, 0U, enc_syntax_uv_mode_cdf(&cdfs, 0U),
                8U, 1) != ENC_OK ||
            write_m3_chroma_dc_coeff(
                &writer, &cdfs, u_levels[0U], chroma_tx_size,
                chroma_tx_size == 0U ? 0U : 2U,
                6U, 0U, q_context, 0) != ENC_OK ||
            write_m3_chroma_dc_coeff(
                &writer, &cdfs, v_levels[0U], chroma_tx_size,
                chroma_tx_size == 0U ? 0U : 2U,
                u_levels[0U] != 0 ? 6U : 0U, 0U, q_context, 1) != ENC_OK ||
            enc_range_writer_finish(&writer) != ENC_OK) {
            return enc_range_writer_status(&writer);
        }
        *size = enc_range_writer_bytes_used(&writer);
        return ENC_OK;
    }
    state.base.writer = &writer;
    state.base.cdfs = &cdfs;
    state.base.width = width;
    state.base.height = height;
    state.base.levels = y_levels;
    state.base.levels_per_block = 64U;
    state.base.q_context = qindex <= 90U ? 0U :
                           qindex <= 140U ? 1U :
                           qindex <= 190U ? 2U : 3U;
    state.base.level_index = 0U;
    state.base.modes = 0;
    state.base.mode_count = 0U;
    state.base.mode_index = 0U;
    state.plane_levels[0U] = y_levels;
    state.plane_levels[1U] = u_levels;
    state.plane_levels[2U] = v_levels;
    state.level_index = 0U;
    state.chroma_level_index = 0U;
    state.chroma_modes = 0;
    state.chroma_mode_count = 0U;
    state.chroma_mode_index = 0U;
    state.luma_mode = 0U;
    state.chroma_format_idc = chroma_format_idc;
    luma.writer = &writer;
    luma.cdfs = &cdfs;
    luma.width = width;
    luma.height = height;
    luma.levels = y_levels;
    luma.levels_per_block = 64U;
    luma.q_context = state.base.q_context;
    luma.level_index = 0U;
    luma.modes = 0;
    luma.mode_count = 0U;
    luma.mode_index = 0U;
    luma.has_chroma = 2;
    luma.u_level = 0;
    luma.v_level = 0;
    for (index = 0U; index < ENC_M3_MAX_MI_COLUMNS; ++index) {
        luma.partition_above[index] = 0U;
        luma.coeff_above[index] = 0U;
        luma.mode_above[index] = 0U;
        state.base.partition_above[index] = 0U;
        state.base.coeff_above[index] = 0U;
        state.base.mode_above[index] = 0U;
        state.chroma_above[0U][index] = 0U;
        state.chroma_above[1U][index] = 0U;
    }
    for (index = 0U; index < 16U; ++index) {
        luma.partition_left[index] = 0U;
        luma.coeff_left[index] = 0U;
        luma.mode_left[index] = 0U;
        state.base.partition_left[index] = 0U;
        state.base.coeff_left[index] = 0U;
        state.base.mode_left[index] = 0U;
        state.chroma_left[0U][index] = 0U;
        state.chroma_left[1U][index] = 0U;
    }
    status = ENC_OK;
    for (row = 0U;
         row < ((height + 7U) / 8U) * 2U && status == ENC_OK;
         row += 16U) {
        if (row != 0U) {
            for (index = 0U; index < 16U; ++index) {
                luma.partition_left[index] = 0U;
                luma.coeff_left[index] = 0U;
                luma.mode_left[index] = 0U;
                state.base.partition_left[index] = 0U;
                state.base.coeff_left[index] = 0U;
                state.base.mode_left[index] = 0U;
                state.chroma_left[0U][index] = 0U;
                state.chroma_left[1U][index] = 0U;
            }
        }
        for (column = 0U;
             column < ((width + 7U) / 8U) * 2U && status == ENC_OK;
             column += 16U) {
            size_t columns = ((width + 7U) / 8U) * 2U;
            size_t rows = ((height + 7U) / 8U) * 2U;
            status = write_m3_large_partition(
                &luma, row, column, 16U, 16U);
            if (status == ENC_OK) {
                if (columns - column > 8U && rows - row > 8U) {
                    status = write_m3_color_partition(
                        &state, row, column, 16U, 8U);
                    if (status == ENC_OK) {
                        status = write_m3_color_partition(
                            &state, row + 8U, column, 16U, 8U);
                    }
                } else {
                    status = write_m3_color_partition(
                        &state, row, column, 16U, 16U);
                }
            }
        }
    }
    if (status != ENC_OK || luma.level_index != block_count) {
        return status != ENC_OK ? status : ENC_INVALID_ARGUMENT;
    }
    if (enc_range_writer_finish(&writer) != ENC_OK) {
        return enc_range_writer_status(&writer);
    }
    *size = enc_range_writer_bytes_used(&writer);
    return ENC_OK;
}

int main(int argc, char **argv) {
    uint8_t sequence[32U];
    uint8_t frame_header[16U];
    uint8_t *tile;
    uint8_t *payload;
    uint8_t *stream;
    int16_t *y_levels;
    int16_t *u_levels;
    int16_t *v_levels;
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    size_t width;
    size_t height;
    size_t y_count;
    size_t uv_count;
    size_t capacity;
    size_t index;
    unsigned int format;
    unsigned int qindex;
    FILE *levels;
    FILE *output;
    if (argc != 7) return 2;
    format = (unsigned int)strtoul(argv[1], 0, 10);
    qindex = (unsigned int)strtoul(argv[2], 0, 10);
    width = (size_t)strtoul(argv[3], 0, 10);
    height = (size_t)strtoul(argv[4], 0, 10);
    y_count = ((width + 7U) / 8U) * ((height + 7U) / 8U);
    uv_count = width == 8U && height == 8U ? 1U :
        ((width + 15U) / 16U) * ((height + 15U) / 16U);
    if ((format != 0U && format != 2U) || qindex == 0U || qindex > 255U ||
        width == 0U || height == 0U ||
        width > ENC_M3_PRODUCTION_MAX_DIMENSION ||
        height > ENC_M3_PRODUCTION_MAX_DIMENSION) {
        return 2;
    }
    y_levels = malloc(y_count * 64U * sizeof(*y_levels));
    u_levels = malloc(uv_count * sizeof(*u_levels));
    v_levels = malloc(uv_count * sizeof(*v_levels));
    capacity = 65536U + y_count * 768U;
    tile = malloc(capacity);
    payload = malloc(capacity + 16U);
    stream = malloc(capacity + 64U);
    if (y_levels == 0 || u_levels == 0 || v_levels == 0 ||
        tile == 0 || payload == 0 || stream == 0) {
        return 3;
    }
    levels = fopen(argv[5], "rb");
    if (levels == 0) return 4;
    for (index = 0U; index < y_count * 64U; ++index) {
        if (!read_i16(levels, &y_levels[index])) return 4;
    }
    for (index = 0U; index < uv_count; ++index) {
        if (!read_i16(levels, &u_levels[index])) return 4;
    }
    for (index = 0U; index < uv_count; ++index) {
        if (!read_i16(levels, &v_levels[index])) return 4;
    }
    if (fgetc(levels) != EOF || fclose(levels) != 0) return 4;
    if (private_color_ac_tile(
            tile, capacity, &tile_size, width, height,
            y_levels, u_levels, v_levels, format, qindex) != ENC_OK ||
        enc_m3_sequence_header_for_format(
            sequence, sizeof(sequence), &sequence_size,
            width, height, format) != ENC_OK ||
        enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            width, height, qindex) != ENC_OK) {
        return 5;
    }
    enc_byte_writer_init(&payload_writer, payload, capacity + 16U);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, tile, tile_size) != ENC_OK) {
        return 5;
    }
    enc_byte_writer_init(&stream_writer, stream, capacity + 64U);
    if (enc_obu_write(&stream_writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        return 5;
    }
    output = fopen(argv[6], "wb");
    if (output == 0 ||
        fwrite(stream, 1U, stream_writer.position, output) !=
            stream_writer.position ||
        fclose(output) != 0) {
        return 6;
    }
    free(y_levels);
    free(u_levels);
    free(v_levels);
    free(tile);
    free(payload);
    free(stream);
    return 0;
}

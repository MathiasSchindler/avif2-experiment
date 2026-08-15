#include "encoder/m3.h"
#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static int round_shift(int value, unsigned int bits) {
    int divisor = 1 << bits;
    int shifted = value + (divisor >> 1);
    return shifted >= 0
        ? shifted / divisor
        : -((-shifted + divisor - 1) / divisor);
}

static int clip_int(int low, int high, int value) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int dc_residual(int level) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    unsigned int dequant_magnitude = (magnitude * 704U + 4U) >> 3U;
    int dequant = level < 0
        ? -(int)dequant_magnitude : (int)dequant_magnitude;
    int row;
    dequant = clip_int(-32768, 32767, dequant);
    row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
    return clip_int(-256, 255, round_shift(64 * row, 11U));
}

static uint8_t reconstructed_value(int level, uint8_t prediction) {
    int value = (int)prediction + dc_residual(level);
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static uint64_t block_error(
    const uint8_t *input, size_t stride, uint8_t reconstructed) {
    uint64_t error = 0U;
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int difference = (int)input[y * stride + x] - (int)reconstructed;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

uint8_t enc_m3_dc_prediction(
    const uint8_t *above, const uint8_t *left, size_t block_size) {
    uint32_t sum = 0U;
    size_t index;
    if (block_size == 0U) return 128U;
    if (above != 0) {
        for (index = 0U; index < block_size; ++index) sum += above[index];
    }
    if (left != 0) {
        for (index = 0U; index < block_size; ++index) sum += left[index];
    }
    if (above != 0 && left != 0) {
        return (uint8_t)((sum + block_size) / (2U * block_size));
    }
    if (above != 0 || left != 0) {
        return (uint8_t)((sum + block_size / 2U) / block_size);
    }
    return 128U;
}

EncStatus enc_m3_dc_i400(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    uint8_t tile[224U];
    uint8_t frame_payload[232U];
    int16_t levels[ENC_M3_MAX_BLOCKS];
    EncByteWriter payload_writer;
    EncByteWriter writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    uint64_t total_error = 0U;
    size_t block_columns;
    size_t block_rows;
    size_t block_count;
    size_t pixel_count;
    size_t last_input_row;
    size_t last_input_offset;
    size_t block;
    EncStatus status;
    if (input == 0 ||
        (width != 8U && width != 16U) ||
        (height != 8U && height != 16U) ||
        stride < width ||
        (stream == 0 && stream_capacity != 0U) ||
        reconstruction == 0 ||
        result == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_multiply(width, height, &pixel_count) ||
        !enc_size_multiply(height - 1U, stride, &last_input_row) ||
        !enc_size_add(last_input_row, width - 1U, &last_input_offset)) {
        return ENC_OVERFLOW;
    }
    if (reconstruction_capacity < pixel_count) {
        return ENC_INVALID_ARGUMENT;
    }
    result->stream_size = 0U;
    block_columns = width / 8U;
    block_rows = height / 8U;
    block_count = block_columns * block_rows;
    for (block = 0U; block < block_count; ++block) {
        size_t block_x = (block % block_columns) * 8U;
        size_t block_y = (block / block_columns) * 8U;
        uint8_t above[8U];
        uint8_t left[8U];
        const uint8_t *above_edge = 0;
        const uint8_t *left_edge = 0;
        uint8_t prediction;
        int best_level = 0;
        uint8_t best_value;
        uint64_t best_error;
        int level;
        size_t edge;
        size_t y;
        size_t x;
        if (block_y != 0U) {
            for (edge = 0U; edge < 8U; ++edge) {
                above[edge] =
                    reconstruction[(block_y - 1U) * width + block_x + edge];
            }
            above_edge = above;
        }
        if (block_x != 0U) {
            for (edge = 0U; edge < 8U; ++edge) {
                left[edge] =
                    reconstruction[(block_y + edge) * width + block_x - 1U];
            }
            left_edge = left;
        }
        prediction = enc_m3_dc_prediction(above_edge, left_edge, 8U);
        best_value = prediction;
        best_error = block_error(
            input + block_y * stride + block_x, stride, best_value);
        for (level = 1; level <= 374; ++level) {
            uint8_t value = reconstructed_value(level, prediction);
            uint64_t error = block_error(
                input + block_y * stride + block_x, stride, value);
            if (error < best_error) {
                best_error = error;
                best_level = level;
                best_value = value;
            }
            value = reconstructed_value(-level, prediction);
            error = block_error(
                input + block_y * stride + block_x, stride, value);
            if (error < best_error) {
                best_error = error;
                best_level = -level;
                best_value = value;
            }
        }
        levels[block] = (int16_t)best_level;
        total_error += best_error;
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                reconstruction[(block_y + y) * width + block_x + x] =
                    best_value;
            }
        }
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size,
        width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header(
        frame_header, sizeof(frame_header), &frame_header_size);
    if (status != ENC_OK) return status;
    status = enc_m3_dc_grid_tile(
        tile, sizeof(tile), &tile_size,
        width, height, levels, block_count);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, frame_payload, sizeof(frame_payload));
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(&writer, stream, stream_capacity);
    if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
        return writer.status;
    }
    result->stream_size = writer.position;
    result->squared_error = total_error;
    result->quant_level = levels[0] < 0
        ? (unsigned int)(-(int)levels[0]) : (unsigned int)levels[0];
    result->quant_sign = levels[0] < 0 ? 1U : 0U;
    result->reconstructed_sample = reconstruction[0];
    result->block_count = block_count;
    result->pixel_count = pixel_count;
    return ENC_OK;
}

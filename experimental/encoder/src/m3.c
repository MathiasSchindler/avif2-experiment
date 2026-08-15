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

static const uint16_t ac_qlookup[25U] = {
    64U, 40U, 41U, 43U, 44U, 45U, 47U, 48U, 49U, 51U, 52U, 54U, 55U,
    57U, 59U, 60U, 62U, 64U, 66U, 68U, 70U, 72U, 74U, 76U, 78U
};

static uint32_t qlookup(unsigned int qindex) {
    if (qindex < 25U) return ac_qlookup[qindex];
    return (uint32_t)ac_qlookup[((qindex - 1U) % 24U) + 1U]
        << ((qindex - 1U) / 24U);
}

static int dc_residual(int level, uint32_t q) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    uint32_t dequant_magnitude =
        ((uint32_t)magnitude * q + 4U) >> 3U;
    int dequant = level < 0
        ? -(int)dequant_magnitude : (int)dequant_magnitude;
    int row;
    dequant = clip_int(-32768, 32767, dequant);
    row = clip_int(-32768, 32767, round_shift(64 * dequant, 7U));
    return clip_int(-256, 255, round_shift(64 * row, 11U));
}

static uint8_t reconstructed_value(
    int level, uint8_t prediction, uint32_t q) {
    int value = (int)prediction + dc_residual(level, q);
    if (value < 0) return 0U;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static uint64_t block_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, uint8_t reconstructed) {
    uint64_t error = 0U;
    size_t y;
    size_t x;
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            size_t source_x = block_x + x < width
                ? block_x + x : width - 1U;
            size_t source_y = block_y + y < height
                ? block_y + y : height - 1U;
            int difference =
                (int)input[source_y * stride + source_x] -
                (int)reconstructed;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

static uint64_t active_block_error(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    size_t block_x, size_t block_y, uint8_t reconstructed) {
    uint64_t error = 0U;
    size_t block_width =
        block_x + 8U <= width ? 8U : width - block_x;
    size_t block_height =
        block_y + 8U <= height ? 8U : height - block_y;
    size_t y;
    size_t x;
    for (y = 0U; y < block_height; ++y) {
        for (x = 0U; x < block_width; ++x) {
            int difference =
                (int)input[(block_y + y) * stride + block_x + x] -
                (int)reconstructed;
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

static void clear_result(EncM3Result *result) {
    if (result == 0) return;
    result->stream_size = 0U;
    result->squared_error = 0U;
    result->quant_level = 0U;
    result->quant_sign = 0U;
    result->reconstructed_sample = 0U;
    result->block_count = 0U;
    result->pixel_count = 0U;
    result->qindex = 0U;
}

EncStatus enc_m3_dc_i400_q(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    unsigned int qindex,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    uint8_t tile[224U];
    uint8_t frame_payload[232U];
    uint8_t padded_reconstruction[ENC_M3_MAX_PIXELS];
    uint8_t staged_reconstruction[ENC_M3_MAX_PIXELS];
    uint8_t staged_stream[ENC_M3_MAX_STREAM_SIZE];
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
    uint32_t q;
    unsigned int max_level;
    EncStatus status;
    clear_result(result);
    if (input == 0 ||
        width < ENC_M3_MIN_DIMENSION || width > ENC_M3_MAX_DIMENSION ||
        height < ENC_M3_MIN_DIMENSION || height > ENC_M3_MAX_DIMENSION ||
        stride < width ||
        (stream == 0 && stream_capacity != 0U) ||
        reconstruction == 0 ||
        result == 0 ||
        qindex < ENC_M3_MIN_QINDEX || qindex > ENC_M3_MAX_QINDEX) {
        return ENC_INVALID_ARGUMENT;
    }
    if (!enc_size_multiply(width, height, &pixel_count) ||
        !enc_size_multiply(height - 1U, stride, &last_input_row) ||
        !enc_size_add(last_input_row, width - 1U, &last_input_offset)) {
        return ENC_OVERFLOW;
    }
    if (reconstruction_capacity < pixel_count) return ENC_OUT_OF_SPACE;
    q = qlookup(qindex);
    max_level = (unsigned int)(
        (UINT32_C(32768) * 8U + q - 1U) / q + 1U);
    block_columns = (width + 7U) / 8U;
    block_rows = (height + 7U) / 8U;
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
                    padded_reconstruction[
                        (block_y - 1U) * 16U + block_x + edge];
            }
            above_edge = above;
        }
        if (block_x != 0U) {
            for (edge = 0U; edge < 8U; ++edge) {
                left[edge] =
                    padded_reconstruction[
                        (block_y + edge) * 16U + block_x - 1U];
            }
            left_edge = left;
        }
        prediction = enc_m3_dc_prediction(above_edge, left_edge, 8U);
        best_value = prediction;
        best_error = block_error(
            input, width, height, stride, block_x, block_y, best_value);
        for (level = 1; level <= (int)max_level; ++level) {
            uint8_t value = reconstructed_value(level, prediction, q);
            uint64_t error = block_error(
                input, width, height, stride, block_x, block_y, value);
            if (error < best_error) {
                best_error = error;
                best_level = level;
                best_value = value;
            }
            value = reconstructed_value(-level, prediction, q);
            error = block_error(
                input, width, height, stride, block_x, block_y, value);
            if (error < best_error) {
                best_error = error;
                best_level = -level;
                best_value = value;
            }
        }
        levels[block] = (int16_t)best_level;
        total_error += active_block_error(
            input, width, height, stride, block_x, block_y, best_value);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                padded_reconstruction[
                    (block_y + y) * 16U + block_x + x] =
                    best_value;
            }
        }
    }
    for (block = 0U; block < height; ++block) {
        size_t x;
        for (x = 0U; x < width; ++x) {
            staged_reconstruction[block * width + x] =
                padded_reconstruction[block * 16U + x];
        }
    }
    status = enc_m3_sequence_header(
        sequence, sizeof(sequence), &sequence_size,
        width, height);
    if (status != ENC_OK) return status;
    status = enc_m3_frame_header_q(
        frame_header, sizeof(frame_header), &frame_header_size, qindex);
    if (status != ENC_OK) return status;
    status = enc_m3_dc_grid_tile_q(
        tile, sizeof(tile), &tile_size,
        width, height, levels, block_count, qindex);
    if (status != ENC_OK) return status;
    enc_byte_writer_init(
        &payload_writer, frame_payload, sizeof(frame_payload));
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(
            &payload_writer, tile, tile_size) != ENC_OK) {
        return payload_writer.status;
    }
    enc_byte_writer_init(&writer, staged_stream, sizeof(staged_stream));
    if (enc_obu_write(&writer, 2U, 0, 0U) != ENC_OK ||
        enc_obu_write(&writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &writer, 4U, frame_payload, payload_writer.position) != ENC_OK) {
        return writer.status;
    }
    if (writer.position > stream_capacity) return ENC_OUT_OF_SPACE;
    status = enc_memory_copy(
        stream, stream_capacity, staged_stream, writer.position);
    if (status != ENC_OK) return status;
    status = enc_memory_copy(
        reconstruction, reconstruction_capacity,
        staged_reconstruction, pixel_count);
    if (status != ENC_OK) return status;
    result->stream_size = writer.position;
    result->squared_error = total_error;
    result->quant_level = levels[0] < 0
        ? (unsigned int)(-(int)levels[0]) : (unsigned int)levels[0];
    result->quant_sign = levels[0] < 0 ? 1U : 0U;
    result->reconstructed_sample = staged_reconstruction[0];
    result->block_count = block_count;
    result->pixel_count = pixel_count;
    result->qindex = qindex;
    return ENC_OK;
}

EncStatus enc_m3_dc_i400(
    const uint8_t *input, size_t width, size_t height, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM3Result *result) {
    return enc_m3_dc_i400_q(
        input, width, height, stride, ENC_M3_DEFAULT_QINDEX,
        stream, stream_capacity, reconstruction, reconstruction_capacity,
        result);
}

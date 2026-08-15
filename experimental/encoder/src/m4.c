#include "encoder/m4.h"
#include "encoder/obu.h"
#include "encoder/syntax.h"
#include "encoder/writer.h"

static const int16_t dct8[8U][8U] = {
    {64, 64, 64, 64, 64, 64, 64, 64},
    {89, 75, 50, 18, -18, -50, -75, -89},
    {83, 35, -35, -83, -83, -35, 35, 83},
    {75, -18, -89, -50, 50, 89, 18, -75},
    {64, -64, -64, 64, 64, -64, -64, 64},
    {50, -89, 18, 75, -75, -18, 89, -50},
    {35, -83, 83, -35, -35, 83, -83, 35},
    {18, -50, 75, -89, 89, -75, 50, -18}
};

static int clip_int(int low, int high, int value) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int round_shift(int value, unsigned int bits) {
    int divisor = 1 << bits;
    int shifted = value + (divisor >> 1);
    return shifted >= 0
        ? shifted / divisor
        : -((-shifted + divisor - 1) / divisor);
}

static int dequantize(int level) {
    unsigned int magnitude =
        level < 0 ? (unsigned int)(-level) : (unsigned int)level;
    unsigned int value = (magnitude * 164U + 4U) >> 3U;
    return clip_int(-32768, 32767, level < 0 ? -(int)value : (int)value);
}

static uint64_t reconstruct(
    const uint8_t *input, size_t stride, int dc_level, int ac_level,
    uint8_t *output) {
    int coeff[8U][8U] = {{0}};
    int row[8U][8U];
    uint64_t error = 0U;
    size_t y;
    size_t x;
    size_t index;
    coeff[0U][0U] = dequantize(dc_level);
    coeff[1U][0U] = dequantize(ac_level);
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int sum = 0;
            for (index = 0U; index < 8U; ++index) {
                sum += dct8[index][x] * coeff[y][index];
            }
            row[y][x] = clip_int(-32768, 32767, round_shift(sum, 7U));
        }
    }
    for (y = 0U; y < 8U; ++y) {
        for (x = 0U; x < 8U; ++x) {
            int sum = 0;
            int value;
            int difference;
            for (index = 0U; index < 8U; ++index) {
                sum += dct8[index][y] * row[index][x];
            }
            value = clip_int(-256, 255, round_shift(sum, 11U)) + 128;
            value = clip_int(0, 255, value);
            output[y * 8U + x] = (uint8_t)value;
            difference = (int)input[y * stride + x] - value;
            error += (uint64_t)(difference * difference);
        }
    }
    return error;
}

static int candidate_is_better(
    uint64_t error, int dc, int ac,
    uint64_t best_error, int best_dc, int best_ac) {
    unsigned int magnitude = (unsigned int)(
        (dc < 0 ? -dc : dc) + (ac < 0 ? -ac : ac));
    unsigned int best_magnitude = (unsigned int)(
        (best_dc < 0 ? -best_dc : best_dc) +
        (best_ac < 0 ? -best_ac : best_ac));
    if (error != best_error) return error < best_error;
    if (magnitude != best_magnitude) return magnitude < best_magnitude;
    if ((ac < 0 ? -ac : ac) != (best_ac < 0 ? -best_ac : best_ac)) {
        return (ac < 0 ? -ac : ac) < (best_ac < 0 ? -best_ac : best_ac);
    }
    if ((dc < 0 ? -dc : dc) != (best_dc < 0 ? -best_dc : best_dc)) {
        return (dc < 0 ? -dc : dc) < (best_dc < 0 ? -best_dc : best_dc);
    }
    if ((dc < 0) != (best_dc < 0)) return dc >= 0;
    return ac >= 0 && best_ac < 0;
}

EncStatus enc_m4_ac_i400(
    const uint8_t *input, size_t stride,
    void *stream, size_t stream_capacity,
    uint8_t *reconstruction, size_t reconstruction_capacity,
    EncM4Result *result) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    uint8_t tile[32U];
    uint8_t frame_payload[40U];
    uint8_t trial[64U];
    EncByteWriter payload_writer;
    EncByteWriter writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    uint64_t best_error = UINT64_MAX;
    int best_dc = 0;
    int best_ac = 0;
    int dc;
    int ac;
    EncStatus status;
    if (input == 0 || stride < 8U ||
        (stream == 0 && stream_capacity != 0U) ||
        reconstruction == 0 || reconstruction_capacity < 64U ||
        result == 0) {
        return ENC_INVALID_ARGUMENT;
    }
    result->stream_size = 0U;
    for (dc = -(int)ENC_M4_MAX_LEVEL_MAGNITUDE;
         dc <= (int)ENC_M4_MAX_LEVEL_MAGNITUDE; ++dc) {
        for (ac = -(int)ENC_M4_MAX_LEVEL_MAGNITUDE;
             ac <= (int)ENC_M4_MAX_LEVEL_MAGNITUDE; ++ac) {
            uint64_t error;
            if ((unsigned int)(dc < 0 ? -dc : dc) <
                    ENC_M4_MIN_LEVEL_MAGNITUDE ||
                (unsigned int)(ac < 0 ? -ac : ac) <
                    ENC_M4_MIN_LEVEL_MAGNITUDE) {
                continue;
            }
            error = reconstruct(input, stride, dc, ac, trial);
            if (best_error == UINT64_MAX ||
                candidate_is_better(
                    error, dc, ac, best_error, best_dc, best_ac)) {
                best_error = error;
                best_dc = dc;
                best_ac = ac;
            }
        }
    }
    if (best_error == UINT64_MAX) return ENC_INVALID_ARGUMENT;
    best_error = reconstruct(
        input, stride, best_dc, best_ac, reconstruction);
    status = enc_m4_sequence_header(
        sequence, sizeof(sequence), &sequence_size);
    if (status != ENC_OK) return status;
    status = enc_m4_frame_header(
        frame_header, sizeof(frame_header), &frame_header_size);
    if (status != ENC_OK) return status;
    status = enc_m4_ac_tile(
        tile, sizeof(tile), &tile_size,
        (int16_t)best_dc, (int16_t)best_ac);
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
    result->squared_error = best_error;
    result->dc_level = (int16_t)best_dc;
    result->ac_level = (int16_t)best_ac;
    result->pixel_count = 64U;
    return ENC_OK;
}

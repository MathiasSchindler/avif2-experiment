#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/ac8x8.c"

#define MAX_FRONTIER_STEPS 32U
#define MAX_CANDIDATE_BLOCKS 16U

typedef struct {
    size_t bytes;
    uint64_t sse;
} Evaluation;

static int read_source(const char *path, uint8_t *data, size_t size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;
    if (fread(data, 1U, size, file) != size || fgetc(file) != EOF) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static void search_block_limit(
    const uint8_t samples[64U], uint8_t pred, unsigned int qindex,
    size_t position_count, int16_t best[64U], uint8_t reconstructed[64U]) {
    int analysis[64U];
    uint8_t trial_reconstruction[64U];
    uint32_t q = qlookup(qindex);
    uint64_t best_sse;
    uint64_t best_cost;
    size_t best_rate;
    size_t positions[ENC_AC8X8_MAX_POSITIONS];
    size_t index;
    if (position_count == 0U) {
        search_block(samples, pred, qindex, 0, 0, best, reconstructed);
        return;
    }
    forward8(samples, pred, analysis);
    for (index = 0U; index < 64U; ++index) best[index] = 0;
    for (index = 0U; index < position_count; ++index) {
        size_t candidate;
        size_t selected = 0U;
        int selected_abs = -1;
        for (candidate = 0U; candidate < 64U; ++candidate) {
            int used = 0;
            int value;
            size_t previous;
            for (previous = 0U; previous < index; ++previous) {
                if (positions[previous] == candidate) used = 1;
            }
            if (used) continue;
            value = analysis[candidate] < 0 ?
                -analysis[candidate] : analysis[candidate];
            if (value > selected_abs ||
                (value == selected_abs &&
                 scan_rank(candidate) < scan_rank(selected))) {
                selected = candidate;
                selected_abs = value;
            }
        }
        positions[index] = selected;
    }
    best_sse = reconstruct8(samples, best, pred, q, reconstructed);
    if (enc_ac8x8_coeff_rate(
            best, qindex, 0U, 0U, &best_rate) != ENC_OK) {
        best_rate = SIZE_MAX / 2U;
    }
    best_cost = rd_cost(best_sse, best_rate, q);
    for (index = 0U; index < position_count; ++index) {
        size_t position = positions[index];
        int ideal = analysis[position] * 8 / (int)q;
        int choices[6U];
        size_t choice;
        int16_t local[64U];
        uint64_t local_sse = best_sse;
        uint64_t local_cost = best_cost;
        size_t local_rate = best_rate;
        for (choice = 0U; choice < 64U; ++choice) local[choice] = best[choice];
        ideal = clip_int(
            -(int)ENC_AC8X8_LEVEL_LIMIT, (int)ENC_AC8X8_LEVEL_LIMIT, ideal);
        choices[0] = 0;
        choices[1] = ideal - 2;
        choices[2] = ideal - 1;
        choices[3] = ideal;
        choices[4] = ideal + 1;
        choices[5] = ideal + 2;
        for (choice = 0U; choice < 6U; ++choice) {
            int16_t trial[64U];
            uint64_t sse;
            uint64_t cost;
            size_t rate;
            size_t copy;
            int value = clip_int(
                -(int)ENC_AC8X8_LEVEL_LIMIT,
                (int)ENC_AC8X8_LEVEL_LIMIT, choices[choice]);
            for (copy = 0U; copy < 64U; ++copy) trial[copy] = best[copy];
            trial[position] = (int16_t)value;
            if (nonzero_count(trial) > ENC_AC8X8_MAX_NONZERO) continue;
            sse = reconstruct8(samples, trial, pred, q, trial_reconstruction);
            if (enc_ac8x8_coeff_rate(
                    trial, qindex, 0U, 0U, &rate) != ENC_OK) continue;
            cost = rd_cost(sse, rate, q);
            if (candidate_better(
                    cost, sse, rate, trial,
                    local_cost, local_sse, local_rate, local)) {
                for (copy = 0U; copy < 64U; ++copy) local[copy] = trial[copy];
                local_sse = sse;
                local_cost = cost;
                local_rate = rate;
            }
        }
        for (choice = 0U; choice < 64U; ++choice) best[choice] = local[choice];
        best_sse = local_sse;
        best_cost = local_cost;
        best_rate = local_rate;
    }
    (void)reconstruct8(samples, best, pred, q, reconstructed);
}

static int evaluate(
    const uint8_t *input, size_t width, size_t height, unsigned int qindex,
    const uint8_t *limits, size_t block_count, size_t padded_width,
    int16_t *levels, uint8_t *padded, uint8_t *tile, uint8_t *payload,
    uint8_t *stream, size_t capacity, Evaluation *result) {
    uint8_t sequence[16U];
    uint8_t frame_header[8U];
    EncByteWriter payload_writer;
    EncByteWriter stream_writer;
    size_t sequence_size;
    size_t frame_header_size;
    size_t tile_size;
    size_t block;
    uint64_t error = 0U;
    for (block = 0U; block < block_count; ++block) {
        size_t block_x;
        size_t block_y;
        size_t y;
        size_t x;
        uint8_t samples[64U];
        uint8_t decoded[64U];
        uint8_t pred;
        block_coordinates(block, width, height, &block_x, &block_y);
        pred = prediction(padded, padded_width, block_x, block_y);
        source_block(input, width, height, width, block_x, block_y, samples);
        search_block_limit(
            samples, pred, qindex, limits[block],
            levels + block * 64U, decoded);
        error += active_error(
            input, width, height, width, block_x, block_y, decoded);
        for (y = 0U; y < 8U; ++y) {
            for (x = 0U; x < 8U; ++x) {
                padded[(block_y + y) * padded_width + block_x + x] =
                    decoded[y * 8U + x];
            }
        }
    }
    if (enc_m3_sequence_header(
            sequence, sizeof(sequence), &sequence_size,
            width, height) != ENC_OK ||
        enc_m3_frame_header_q_for_size(
            frame_header, sizeof(frame_header), &frame_header_size,
            width, height, qindex) != ENC_OK ||
        enc_ac8x8_grid_tile_q(
            tile, capacity, &tile_size, width, height, levels,
            block_count * 64U, qindex) != ENC_OK) {
        return 0;
    }
    enc_byte_writer_init(&payload_writer, payload, capacity);
    if (enc_byte_writer_write(
            &payload_writer, frame_header, frame_header_size) != ENC_OK ||
        enc_byte_writer_write(&payload_writer, tile, tile_size) != ENC_OK) {
        return 0;
    }
    enc_byte_writer_init(&stream_writer, stream, capacity);
    if (enc_obu_write(&stream_writer, 2U, NULL, 0U) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 1U, sequence, sequence_size) != ENC_OK ||
        enc_obu_write(
            &stream_writer, 4U, payload, payload_writer.position) != ENC_OK) {
        return 0;
    }
    result->bytes = stream_writer.position;
    result->sse = error;
    return 1;
}

static int ratio_better(
    uint64_t gain, size_t byte_cost, size_t block,
    uint64_t best_gain, size_t best_byte_cost, size_t best_block) {
    size_t cost = byte_cost == 0U ? 1U : byte_cost;
    size_t best_cost = best_byte_cost == 0U ? 1U : best_byte_cost;
    if ((byte_cost == 0U) != (best_byte_cost == 0U)) return byte_cost == 0U;
    if (gain * best_cost != best_gain * cost) {
        return gain * best_cost > best_gain * cost;
    }
    if (gain != best_gain) return gain > best_gain;
    if (byte_cost != best_byte_cost) return byte_cost < best_byte_cost;
    return block < best_block;
}

int main(int argc, char **argv) {
    size_t width;
    size_t height;
    unsigned int qindex;
    size_t input_size;
    size_t block_count;
    size_t padded_width;
    size_t padded_height;
    size_t capacity;
    uint8_t *input;
    uint8_t *limits;
    uint8_t *trial_limits;
    int16_t *levels;
    uint8_t *padded;
    uint8_t *tile;
    uint8_t *payload;
    uint8_t *stream;
    Evaluation current;
    size_t step;
    if (argc != 5) {
        fprintf(stderr, "usage: %s INPUT WIDTH HEIGHT QINDEX\n", argv[0]);
        return 2;
    }
    errno = 0;
    width = (size_t)strtoull(argv[2], NULL, 10);
    height = (size_t)strtoull(argv[3], NULL, 10);
    qindex = (unsigned int)strtoul(argv[4], NULL, 10);
    if (errno != 0 || width == 0U || height == 0U ||
        qindex == 0U || qindex > 255U) return 2;
    input_size = width * height;
    block_count = ((width + 7U) / 8U) * ((height + 7U) / 8U);
    padded_width = (width + 7U) & ~(size_t)7U;
    padded_height = (height + 7U) & ~(size_t)7U;
    capacity = input_size * 8U + 4096U;
    input = malloc(input_size);
    limits = malloc(block_count);
    trial_limits = malloc(block_count);
    levels = malloc(block_count * 64U * sizeof(*levels));
    padded = malloc(padded_width * padded_height);
    tile = malloc(capacity);
    payload = malloc(capacity);
    stream = malloc(capacity);
    if (input == NULL || limits == NULL || trial_limits == NULL ||
        levels == NULL || padded == NULL || tile == NULL ||
        payload == NULL || stream == NULL ||
        !read_source(argv[1], input, input_size)) {
        return 3;
    }
    for (step = 0U; step < block_count; ++step) limits[step] = 0U;
    if (!evaluate(
            input, width, height, qindex, limits, block_count, padded_width,
            levels, padded, tile, payload, stream, capacity, &current)) {
        return 4;
    }
    printf("{\"step\":0,\"bytes\":%zu,\"sse\":%llu}\n",
           current.bytes, (unsigned long long)current.sse);
    for (step = 1U; step <= MAX_FRONTIER_STEPS; ++step) {
        size_t slot;
        size_t best_block = block_count;
        size_t best_byte_cost = 0U;
        uint64_t best_gain = 0U;
        Evaluation best = current;
        size_t candidate_count = block_count < MAX_CANDIDATE_BLOCKS ?
            block_count : MAX_CANDIDATE_BLOCKS;
        for (slot = 0U; slot < candidate_count; ++slot) {
            size_t block = slot * block_count / candidate_count;
            Evaluation trial;
            size_t index;
            size_t byte_cost;
            uint64_t gain;
            if (limits[block] >= ENC_AC8X8_MAX_POSITIONS) continue;
            for (index = 0U; index < block_count; ++index) {
                trial_limits[index] = limits[index];
            }
            ++trial_limits[block];
            if (!evaluate(
                    input, width, height, qindex, trial_limits, block_count,
                    padded_width, levels, padded, tile, payload, stream,
                    capacity, &trial) ||
                trial.sse >= current.sse) continue;
            gain = current.sse - trial.sse;
            byte_cost = trial.bytes > current.bytes ?
                trial.bytes - current.bytes : 0U;
            if (best_block == block_count || ratio_better(
                    gain, byte_cost, block,
                    best_gain, best_byte_cost, best_block)) {
                best_block = block;
                best_byte_cost = byte_cost;
                best_gain = gain;
                best = trial;
            }
        }
        if (best_block == block_count) break;
        ++limits[best_block];
        current = best;
        printf("{\"step\":%zu,\"block\":%zu,\"positions\":%u,"
               "\"bytes\":%zu,\"sse\":%llu}\n",
               step, best_block, (unsigned int)limits[best_block],
               current.bytes, (unsigned long long)current.sse);
    }
    free(stream);
    free(payload);
    free(tile);
    free(padded);
    free(levels);
    free(trial_limits);
    free(limits);
    free(input);
    return 0;
}

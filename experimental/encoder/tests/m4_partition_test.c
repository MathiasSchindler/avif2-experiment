#include "encoder/ac8x8.h"

static uint8_t workspace[524288U];
static uint8_t stream_a[65536U];
static uint8_t stream_b[65536U];
static uint8_t reconstruction_a[16384U];
static uint8_t reconstruction_b[16384U];
static uint8_t input[16384U];

static int equal_bytes(const uint8_t *a, const uint8_t *b, size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (a[index] != b[index]) return 0;
    }
    return 1;
}

static void smooth(size_t width, size_t height) {
    size_t y;
    size_t x;
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            input[y * width + x] =
                (uint8_t)(64U + x / 16U + y / 32U);
        }
    }
}

static void checker(size_t width, size_t height) {
    size_t y;
    size_t x;
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            input[y * width + x] =
                ((x + y) & 1U) != 0U ? 32U : 224U;
        }
    }
}

static int encode(
    size_t width, size_t height, EncAc8x8Search search,
    uint8_t *stream, uint8_t *reconstruction, EncAc8x8Result *result) {
    EncAc8x8WorkspaceLayout layout;
    if (enc_ac8x8_query_workspace(width, height, &layout) != ENC_OK ||
        layout.size > sizeof(workspace) ||
        layout.stream_capacity > sizeof(stream_a) ||
        width * height > sizeof(reconstruction_a)) {
        return 0;
    }
    return enc_ac8x8_i400_q_workspace(
        input, width, height, width, 100U, search,
        workspace, sizeof(workspace), stream, sizeof(stream_a),
        reconstruction, sizeof(reconstruction_a), result) == ENC_OK;
}

int main(void) {
    EncAc8x8Result first;
    EncAc8x8Result repeat;
    EncAc8x8Result rd;
    EncAc8x8Result adaptive;
    size_t index;

    for (index = 0U; index < 32U * 32U; ++index) input[index] = 128U;
    if (!encode(32U, 32U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_a, reconstruction_a, &first) ||
        first.rd_partition_counts[1U] == 0U ||
        first.rd_transform_counts[1U] == 0U ||
        first.rd_partition_counts[0U] == 0U ||
        !encode(32U, 32U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_b, reconstruction_b, &repeat) ||
        first.stream_size != repeat.stream_size ||
        first.squared_error != repeat.squared_error ||
        !equal_bytes(stream_a, stream_b, first.stream_size) ||
        !equal_bytes(reconstruction_a, reconstruction_b, 32U * 32U) ||
        !encode(32U, 32U, ENC_AC8X8_SEARCH_RD,
                stream_b, reconstruction_b, &rd) ||
        rd.stream_size != first.stream_size ||
        !equal_bytes(stream_a, stream_b, first.stream_size)) {
        return 1;
    }

    checker(16U, 16U);
    if (!encode(16U, 16U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_a, reconstruction_a, &first) ||
        first.rd_partition_counts[0U] != 1U ||
        first.rd_partition_counts[1U] != 0U ||
        first.rd_transform_counts[0U] != 4U) {
        return 2;
    }

    for (index = 0U; index < 16U * 16U; ++index) input[index] = 128U;
    if (!encode(16U, 16U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_a, reconstruction_a, &first) ||
        first.rd_partition_counts[0U] != 1U ||
        first.rd_partition_counts[1U] != 0U) {
        return 3;
    }

    for (index = 0U; index < 31U * 17U; ++index) {
        size_t x = index % 31U;
        size_t y = index / 31U;
        input[index] = (uint8_t)((x * 13U + y * 7U) & 255U);
    }
    if (!encode(31U, 17U, ENC_AC8X8_SEARCH_RD,
                stream_a, reconstruction_a, &rd) ||
        !encode(31U, 17U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_b, reconstruction_b, &adaptive) ||
        rd.stream_size != adaptive.stream_size ||
        rd.squared_error != adaptive.squared_error ||
        !equal_bytes(stream_a, stream_b, rd.stream_size) ||
        !equal_bytes(reconstruction_a, reconstruction_b, 31U * 17U)) {
        return 4;
    }

    smooth(128U, 64U);
    if (!encode(128U, 64U, ENC_AC8X8_SEARCH_PARTITION_RD,
                stream_a, reconstruction_a, &first) ||
        first.rd_partition_counts[0U] +
            first.rd_partition_counts[1U] != 32U ||
        first.candidate_trials == 0U) {
        return 5;
    }
    return 0;
}

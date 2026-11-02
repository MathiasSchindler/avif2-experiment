#include "decoder/internal/decode_diagnostics.h"
#include "decoder/reconstruct.h"
#include "decoder/tx_geometry.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

typedef struct {
    size_t ok;
    size_t unsupported;
    size_t invalid;
    size_t invalid_data;
    uint64_t hash;
    uint64_t diagnostics;
} Snapshot;

static uint64_t hash_value(uint64_t hash, uint32_t value, unsigned int bytes) {
    unsigned int index;
    for (index = 0U; index < bytes; ++index) {
        hash = (hash ^ (uint8_t)(value >> (index * 8U))) *
            UINT64_C(1099511628211);
    }
    return hash;
}

static void plane_case(Snapshot *snapshot, unsigned int path,
                       Av2hFirstTxSize tx_size,
                       Av2hFirstLeafIntraMode intra_mode,
                       unsigned int edges, unsigned int variant) {
    enum { WIDTH = 67, HEIGHT = 65, STRIDE = 71, CELLS = 17 * 17 };
    static uint8_t pixels[STRIDE * HEIGHT];
    static uint16_t pixels_u16[STRIDE * HEIGHT];
    static uint8_t original[sizeof(pixels)];
    static uint16_t original_u16[STRIDE * HEIGHT];
    static uint8_t predictor[STRIDE * HEIGHT];
    static uint8_t ready[CELLS];
    static uint8_t original_ready[CELLS];
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    Av2hReconstructionPlane plane = {
        pixels, STRIDE, WIDTH, HEIGHT, WIDTH, HEIGHT, ready, 17U, CELLS
    };
    Av2hReconstructionPlaneU16 plane_u16 = {
        pixels_u16, STRIDE, WIDTH, HEIGHT, WIDTH, HEIGHT, ready, 17U, CELLS
    };
    Av2hReconstructionPlane predictor_plane = {
        predictor, STRIDE, WIDTH, HEIGHT, WIDTH, HEIGHT, ready, 17U, CELLS
    };
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstChromaMode chroma = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    Av2hFirstCoeffBlock coefficients = { 0 };
    Av2hPartitionNode leaf = { 0 };
    Av2hDecodeDiagnostics diagnostics;
    const Av2hTxDescriptor *descriptor;
    uint8_t format = path >= 4U ? 2U : 0U;
    unsigned int high = path & 1U;
    uint16_t x = edges == 0U || edges == 2U ? 0U :
        edges == 5U ? 64U : 4U;
    uint16_t y = edges <= 1U ? 0U : edges == 5U ? 64U : 4U;
    size_t index;
    Av2hStatus status;
    CHECK(av2h_tx_descriptor_get(tx_size, &descriptor) == AV2H_OK);
    mode.mode = intra_mode;
    chroma.mode = intra_mode;
    chroma.chroma_format_idc = format;
    prefix.tx_size = tx_size;
    prefix.q_index = variant == 2U ? 0U : 152U;
    prefix.all_zero = variant == 0U || variant == 3U || variant == 4U;
    prefix.eob = prefix.all_zero ? 0U : 1U;
    transform.primary_type = variant == 5U ? AV2H_TX_IDTX : AV2H_TX_DCT_DCT;
    mode.fsc_mode = variant == 5U;
    coefficients.width = descriptor->coefficient_width;
    coefficients.height = descriptor->coefficient_height;
    coefficients.coefficient_count = descriptor->eob_capacity;
    if (variant == 3U) --coefficients.coefficient_count;
    if (!prefix.all_zero) {
        coefficients.nonzero_count = 1U;
        coefficients.quantized[0] = -1;
        coefficients.dequantized[0] = variant == 2U ? -17 : -512;
    } else if (variant == 4U) {
        coefficients.dequantized[0] = 8;
    }
    leaf.column = (uint16_t)(x / (path < 2U || format == 2U ? 4U : 2U));
    leaf.row = (uint16_t)(y / (path < 2U || format == 2U ? 4U : 2U));
    leaf.block_size = AV2H_BLOCK_64X64;
    for (index = 0U; index < STRIDE * HEIGHT; ++index) {
        pixels[index] = (uint8_t)(index * 37U + index / STRIDE * 13U);
        pixels_u16[index] = (uint16_t)((index * 37U +
            index / STRIDE * 13U) & 1023U);
    }
    memset(predictor, 0x93, sizeof(predictor));
    memset(ready, 1, sizeof(ready));
    if (edges == 4U) {
        ready[0] = 2U;
        ready[1] = 2U;
        ready[17] = 0U;
    }
    memcpy(original_ready, ready, sizeof(ready));
    memcpy(original, pixels, sizeof(pixels));
    memcpy(original_u16, pixels_u16, sizeof(pixels_u16));
    memset(&scratch, 0x13, sizeof(scratch));
    memset(&scratch_u16, 0x13, sizeof(scratch_u16));
    memset(&diagnostics, 0x5a, sizeof(diagnostics));
    if (path == 0U) {
        status = av2h_luma_reconstruct_to_plane_with_diagnostics(
            &diagnostics, &leaf, &mode, &prefix, &transform, &coefficients,
            &plane, &scratch);
    } else if (path == 1U) {
        status = av2h_luma_reconstruct_to_plane_u16_with_diagnostics(
            &diagnostics, &leaf, &mode, &prefix, &transform, &coefficients,
            &plane_u16, &scratch_u16);
    } else if (!high) {
        status = av2h_chroma_reconstruct_to_plane_format_with_diagnostics(
            &diagnostics, &leaf, &chroma, &prefix, &transform, &coefficients,
            &plane, 0, 1U, &predictor_plane, format, &scratch);
    } else {
        status = av2h_chroma_reconstruct_to_plane_u16_with_diagnostics(
            &diagnostics, &leaf, &chroma, &prefix, &transform, &coefficients,
            &plane_u16, 0, 2U, format, &scratch_u16);
    }
    if (status == AV2H_OK) ++snapshot->ok;
    else if (status == AV2H_UNSUPPORTED) ++snapshot->unsupported;
    else if (status == AV2H_INVALID_ARGUMENT) ++snapshot->invalid;
    else if (status == AV2H_INVALID_DATA) ++snapshot->invalid_data;
    else CHECK(0);
    if (status != AV2H_OK) {
        CHECK(high ? memcmp(original_u16, pixels_u16, sizeof(pixels_u16)) == 0 :
              memcmp(original, pixels, sizeof(pixels)) == 0);
        CHECK(memcmp(original_ready, ready, sizeof(ready)) == 0);
    } else {
        uint16_t visible_width = descriptor->output_width < WIDTH - x ?
            descriptor->output_width : (uint16_t)(WIDTH - x);
        uint16_t visible_height = descriptor->output_height < HEIGHT - y ?
            descriptor->output_height : (uint16_t)(HEIGHT - y);
        for (index = 0U; index < STRIDE * HEIGHT; ++index) {
            size_t row = index / STRIDE;
            size_t column = index % STRIDE;
            if (row >= y && row < y + visible_height &&
                column >= x && column < x + visible_width) continue;
            CHECK(high ? pixels_u16[index] ==
                  original_u16[index] :
                  pixels[index] == original[index]);
        }
        for (index = 0U; index < CELLS; ++index) {
            size_t row = index / 17U;
            size_t column = index % 17U;
            uint8_t expected = row >= y / 4U &&
                row <= ((size_t)y + visible_height - 1U) / 4U &&
                column >= x / 4U &&
                column <= ((size_t)x + visible_width - 1U) / 4U ?
                1U : original_ready[index];
            CHECK(ready[index] == expected);
        }
    }
    snapshot->hash = hash_value(snapshot->hash, status, 4U);
    for (index = 0U; index < STRIDE * HEIGHT; ++index) {
        snapshot->hash = hash_value(snapshot->hash,
            high ? pixels_u16[index] : pixels[index], high ? 2U : 1U);
        if (path >= 2U && !high) {
            snapshot->hash = hash_value(snapshot->hash, predictor[index], 1U);
        }
    }
    for (index = 0U; index < CELLS; ++index) {
        snapshot->hash = hash_value(snapshot->hash, ready[index], 1U);
    }
    snapshot->diagnostics = hash_value(snapshot->diagnostics,
        diagnostics.reconstruction.luma_stage, 1U);
    snapshot->diagnostics = hash_value(snapshot->diagnostics,
        diagnostics.reconstruction.chroma_stage, 1U);
    snapshot->diagnostics = hash_value(snapshot->diagnostics,
        diagnostics.reconstruction.inverse_transform_stage, 1U);
}

int main(void) {
    static const Av2hFirstLeafIntraMode modes[] = {
        AV2H_INTRA_DC, AV2H_INTRA_VERTICAL, AV2H_INTRA_SMOOTH, AV2H_INTRA_D135
    };
    /* Frozen against the pre-M3 implementation, not additional AVM admission. */
    static const Snapshot expected[6] = {
        { 2400U, 1200U, 0U, 0U, UINT64_C(0x8eac3ca102fdf643), 0U },
        { 2400U, 600U, 0U, 600U, UINT64_C(0x30d357c06cc3dc7f), 0U },
        { 3000U, 600U, 0U, 0U, UINT64_C(0xb9118c8b25ac7559), 0U },
        { 2400U, 600U, 0U, 600U, UINT64_C(0x98ac82a3fa25d778), 0U },
        { 3000U, 600U, 0U, 0U, UINT64_C(0xb9118c8b25ac7559), 0U },
        { 2400U, 600U, 0U, 600U, UINT64_C(0x98ac82a3fa25d778), 0U }
    };
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    static const uint64_t expected_diagnostics[6] = {
        UINT64_C(0x630366463e24498d), UINT64_C(0xbf29fe080f0bc125),
        UINT64_C(0xb11c6e05412ede05), UINT64_C(0xbe582f8f7cf21925),
        UINT64_C(0xb11c6e05412ede05), UINT64_C(0xbe582f8f7cf21925)
    };
#else
    static const uint64_t expected_diagnostics[6] = {
        UINT64_C(0x8e58574994a665a5), UINT64_C(0x8e58574994a665a5),
        UINT64_C(0x8e58574994a665a5), UINT64_C(0x8e58574994a665a5),
        UINT64_C(0x8e58574994a665a5), UINT64_C(0x8e58574994a665a5)
    };
#endif
    unsigned int path;
    for (path = 0U; path < 6U; ++path) {
        Snapshot actual = {
            0U, 0U, 0U, 0U, UINT64_C(14695981039346656037),
            UINT64_C(14695981039346656037)
        };
        unsigned int tx;
        size_t mode;
        unsigned int edges;
        unsigned int variant;
        for (tx = 0U; tx < AV2H_TX_SIZE_COUNT; ++tx) {
            for (mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
                for (edges = 0U; edges < 6U; ++edges) {
                    for (variant = 0U; variant < 6U; ++variant) {
                        plane_case(&actual, path, (Av2hFirstTxSize)tx,
                                   modes[mode], edges, variant);
                    }
                }
            }
        }
        printf("%u: %zu %zu %zu %zu %016" PRIx64 " %016" PRIx64 "\n",
               path, actual.ok, actual.unsupported, actual.invalid,
               actual.invalid_data,
               actual.hash, actual.diagnostics);
        CHECK(actual.ok == expected[path].ok);
        CHECK(actual.unsupported == expected[path].unsupported);
        CHECK(actual.invalid == expected[path].invalid);
        CHECK(actual.invalid_data == expected[path].invalid_data);
        CHECK(actual.hash == expected[path].hash);
        CHECK(actual.diagnostics == expected_diagnostics[path]);
    }
    return failures != 0;
}

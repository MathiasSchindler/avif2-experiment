#include "decoder/internal/decode_diagnostics.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/internal/reconstruct_modules.h"
#include "decoder/prediction.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int inventory;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                        \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

typedef struct {
    size_t ok;
    size_t unsupported;
    size_t invalid;
    uint64_t hash;
} PredictionSnapshot;

/* Characterization of this implementation, not additional AVM parity claims. */
static const PredictionSnapshot basic_expected[2U][13U] = {
    {
        { 200U, 0U, 0U, UINT64_C(0xe56b998eb21398aa) },
        { 200U, 0U, 0U, UINT64_C(0xcabd2ba369c4c4f1) },
        { 200U, 0U, 0U, UINT64_C(0x63a1117f724f92a1) },
        { 200U, 0U, 0U, UINT64_C(0xa39319584f1e8083) },
        { 200U, 0U, 0U, UINT64_C(0x3068ece5085168e1) },
        { 200U, 0U, 0U, UINT64_C(0x78de4991cbd1a42d) },
        { 200U, 0U, 0U, UINT64_C(0x9919ffd3ed6541c1) },
        { 200U, 0U, 0U, UINT64_C(0xbf2f482b4414e1e7) },
        { 200U, 0U, 0U, UINT64_C(0x07ba09ea3a0dfba8) },
        { 200U, 0U, 0U, UINT64_C(0x2823542a8ad27235) },
        { 200U, 0U, 0U, UINT64_C(0x886a6ea515ca6a23) },
        { 200U, 0U, 0U, UINT64_C(0x9e1e1fa194e5f7a3) },
        { 200U, 0U, 0U, UINT64_C(0xd00135b07a4ac7bf) }
    },
    {
        { 0U, 200U, 0U, UINT64_C(0x68f5d60d7da4fb99) },
        { 200U, 0U, 0U, UINT64_C(0x899753ecc5730551) },
        { 200U, 0U, 0U, UINT64_C(0x285d8cbf98050da1) },
        { 200U, 0U, 0U, UINT64_C(0x6d3dfa57bf24bac5) },
        { 200U, 0U, 0U, UINT64_C(0x340ca50662da1b4d) },
        { 200U, 0U, 0U, UINT64_C(0xa7d33939f4bdab15) },
        { 200U, 0U, 0U, UINT64_C(0x395936654901e617) },
        { 200U, 0U, 0U, UINT64_C(0xe0c3ee62ae5de476) },
        { 200U, 0U, 0U, UINT64_C(0xc19522f711c34dc1) },
        { 200U, 0U, 0U, UINT64_C(0x7c64047512735b93) },
        { 200U, 0U, 0U, UINT64_C(0xc396a54a2d8c0417) },
        { 200U, 0U, 0U, UINT64_C(0xe8f7cec623d58695) },
        { 200U, 0U, 0U, UINT64_C(0xd295da76cb82aecf) }
    }
};
static const PredictionSnapshot directional_expected[2U][8U] = {
    {
        { 392U, 0U, 56U, UINT64_C(0xe80f777378d0d47e) },
        { 392U, 0U, 56U, UINT64_C(0x255fd8b3cf0adb1c) },
        { 392U, 0U, 56U, UINT64_C(0xb22035525d4f149c) },
        { 392U, 0U, 56U, UINT64_C(0x466e057b92794d27) },
        { 392U, 0U, 56U, UINT64_C(0x4556a89f297add2d) },
        { 392U, 0U, 56U, UINT64_C(0xf6a7f8aa857160a5) },
        { 392U, 0U, 56U, UINT64_C(0x7b51cf2c5f4c9974) },
        { 392U, 0U, 56U, UINT64_C(0x32e7d24f120b92fa) }
    },
    {
        { 392U, 0U, 56U, UINT64_C(0x307e937f6956f9d3) },
        { 392U, 0U, 56U, UINT64_C(0x7f62b36c473299d8) },
        { 392U, 0U, 56U, UINT64_C(0xcfa2de21008d0142) },
        { 392U, 0U, 56U, UINT64_C(0xe087bc38ef9d6af7) },
        { 392U, 0U, 56U, UINT64_C(0x59ff6be62377a397) },
        { 392U, 0U, 56U, UINT64_C(0xf9bcab28821298cd) },
        { 392U, 0U, 56U, UINT64_C(0x58f17a1127c1ed28) },
        { 392U, 0U, 56U, UINT64_C(0xae1e4a01083c164e) }
    }
};
static const PredictionSnapshot dip_expected[6U] = {
    { 200U, 0U, 0U, UINT64_C(0xd26589178c4a9229) },
    { 200U, 0U, 0U, UINT64_C(0x381bb36ce19332b9) },
    { 200U, 0U, 0U, UINT64_C(0xc5f32a9cdf781e89) },
    { 200U, 0U, 0U, UINT64_C(0xc4a05bc21e07be7d) },
    { 200U, 0U, 0U, UINT64_C(0x88bd404505713fb3) },
    { 200U, 0U, 0U, UINT64_C(0x7bd21030e685c4d4) }
};

static uint64_t hash_byte(uint64_t hash, unsigned int value) {
    return (hash ^ (uint8_t)value) * UINT64_C(1099511628211);
}

static void prediction_case(PredictionSnapshot *snapshot,
                            const Av2hFirstLeafMode *mode,
                            uint16_t width, uint16_t height,
                            unsigned int edges, unsigned int high_bit_depth) {
    enum { EDGE_COUNT = 160, STRIDE = 67, SAMPLE_COUNT = STRIDE * 64 };
    uint8_t above[EDGE_COUNT];
    uint8_t left[EDGE_COUNT];
    uint8_t secondary_above[EDGE_COUNT];
    uint8_t secondary_left[EDGE_COUNT];
    uint16_t above_u16[EDGE_COUNT];
    uint16_t left_u16[EDGE_COUNT];
    uint16_t secondary_above_u16[EDGE_COUNT];
    uint16_t secondary_left_u16[EDGE_COUNT];
    uint8_t storage[SAMPLE_COUNT + 2U];
    uint16_t storage_u16[SAMPLE_COUNT + 2U];
    Av2hLumaReferences references = { 0 };
    Av2hLumaReferencesU16 references_u16 = { 0 };
    Av2hStatus status;
    size_t index;
    for (index = 0U; index < EDGE_COUNT; ++index) {
        above_u16[index] = (uint16_t)((31U + 17U * index) & 1023U);
        left_u16[index] = (uint16_t)((7U + 29U * index) & 1023U);
        secondary_above_u16[index] = (uint16_t)((93U + 13U * index) & 1023U);
        secondary_left_u16[index] = (uint16_t)((57U + 23U * index) & 1023U);
        above[index] = (uint8_t)above_u16[index];
        left[index] = (uint8_t)left_u16[index];
        secondary_above[index] = (uint8_t)secondary_above_u16[index];
        secondary_left[index] = (uint8_t)secondary_left_u16[index];
    }
#define SET_REFERENCES(ref, suffix)                                            \
    do {                                                                       \
        (ref).above = (edges & 1U) != 0U ? above##suffix : 0;                    \
        (ref).left = (edges & 2U) != 0U ? left##suffix : 0;                      \
        (ref).above_count = (edges & 1U) != 0U ? EDGE_COUNT : 0U;                \
        (ref).left_count = (edges & 2U) != 0U ? EDGE_COUNT : 0U;                 \
        (ref).secondary_above =                                               \
            (edges & 1U) != 0U ? secondary_above##suffix : 0;                   \
        (ref).secondary_left =                                                \
            (edges & 2U) != 0U ? secondary_left##suffix : 0;                    \
        (ref).secondary_above_count = (ref).above_count;                        \
        (ref).secondary_left_count = (ref).left_count;                          \
        (ref).above_left = 73U;                                                \
        (ref).secondary_above_left = 109U;                                     \
        (ref).has_above_left = edges == 3U;                                    \
        (ref).has_secondary_above_left = edges == 3U;                          \
        (ref).negative_count = 4U;                                            \
        for (index = 0U; index < 5U; ++index) {                                \
            (ref).above_negative[index] = above##suffix[index];               \
            (ref).left_negative[index] = left##suffix[index];                 \
        }                                                                      \
    } while (0)
    SET_REFERENCES(references, );
    SET_REFERENCES(references_u16, _u16);
#undef SET_REFERENCES
    av2h_memory_fill(storage, 0xa5U, sizeof(storage));
    av2h_memory_fill(storage_u16, 0xa5U, sizeof(storage_u16));
    if (high_bit_depth != 0U) {
        status = av2h_luma_predict_u16(
            mode, width, height, &references_u16, storage_u16 + 1U, STRIDE);
    } else {
        status = av2h_luma_predict(
            mode, width, height, &references, storage + 1U, STRIDE);
    }
    if (status == AV2H_OK) {
        ++snapshot->ok;
    } else if (status == AV2H_UNSUPPORTED) {
        ++snapshot->unsupported;
    } else {
        CHECK(status == AV2H_INVALID_ARGUMENT);
        ++snapshot->invalid;
    }
    snapshot->hash = hash_byte(snapshot->hash, width);
    snapshot->hash = hash_byte(snapshot->hash, height);
    snapshot->hash = hash_byte(snapshot->hash, (unsigned int)mode->mode);
    snapshot->hash = hash_byte(snapshot->hash, (unsigned int)(mode->angle_delta + 3));
    snapshot->hash = hash_byte(snapshot->hash, mode->mrl_index);
    snapshot->hash = hash_byte(snapshot->hash, mode->multi_line_mrl);
    snapshot->hash = hash_byte(snapshot->hash, mode->enable_ibp);
    snapshot->hash = hash_byte(snapshot->hash, mode->use_intra_dip);
    snapshot->hash = hash_byte(snapshot->hash, mode->intra_dip_mode);
    snapshot->hash = hash_byte(snapshot->hash, edges);
    snapshot->hash = hash_byte(snapshot->hash, (unsigned int)status);
    for (index = 0U; index < SAMPLE_COUNT; ++index) {
        size_t row = index / STRIDE;
        size_t column = index % STRIDE;
        unsigned int value = high_bit_depth != 0U ?
            storage_u16[index + 1U] : storage[index + 1U];
        if (row < height && column < width) {
            if (status == AV2H_OK) {
                CHECK(value <= (high_bit_depth != 0U ? 1023U : 255U));
            }
            snapshot->hash = hash_byte(snapshot->hash, value);
            if (high_bit_depth != 0U) {
                snapshot->hash = hash_byte(snapshot->hash, value >> 8U);
            }
        } else {
            CHECK(value == (high_bit_depth != 0U ? 0xa5a5U : 0xa5U));
        }
    }
    CHECK(storage[0U] == 0xa5U && storage[SAMPLE_COUNT + 1U] == 0xa5U);
    CHECK(storage_u16[0U] == 0xa5a5U &&
          storage_u16[SAMPLE_COUNT + 1U] == 0xa5a5U);
}

static void check_snapshot(const char *family, unsigned int depth,
                           unsigned int mode,
                           const PredictionSnapshot *actual,
                           const PredictionSnapshot *expected) {
    if (inventory || actual->hash != expected->hash ||
        actual->ok != expected->ok ||
        actual->unsupported != expected->unsupported ||
        actual->invalid != expected->invalid) {
        printf("%s,%u,%u,%zu,%zu,%zu,%016" PRIx64 "\n",
               family, depth, mode, actual->ok, actual->unsupported,
               actual->invalid, actual->hash);
    }
    CHECK(actual->ok == expected->ok);
    CHECK(actual->unsupported == expected->unsupported);
    CHECK(actual->invalid == expected->invalid);
    CHECK(actual->hash == expected->hash);
}

static void test_prediction_matrix(void) {
    static const uint16_t dimensions[] = { 4U, 8U, 16U, 32U, 64U };
    static const uint16_t directional_shapes[][2U] = {
        { 8U, 8U }, { 16U, 16U }, { 8U, 32U }, { 32U, 8U }
    };
    unsigned int high_bit_depth;
    unsigned int mode_index;
    size_t x;
    size_t y;
    unsigned int edges;
    unsigned int ibp;
    for (high_bit_depth = 0U; high_bit_depth < 2U; ++high_bit_depth) {
        for (mode_index = 0U; mode_index < 13U; ++mode_index) {
            PredictionSnapshot snapshot = { 0U, 0U, 0U,
                UINT64_C(14695981039346656037) };
            Av2hFirstLeafMode mode = { 0 };
            mode.mode = (Av2hFirstLeafIntraMode)mode_index;
            for (x = 0U; x < 5U; ++x) {
                for (y = 0U; y < 5U; ++y) {
                    for (edges = 0U; edges < 4U; ++edges) {
                        for (ibp = 0U; ibp < 2U; ++ibp) {
                            mode.enable_ibp = (uint8_t)ibp;
                            prediction_case(&snapshot, &mode,
                                dimensions[x], dimensions[y], edges,
                                high_bit_depth);
                        }
                    }
                }
            }
            check_snapshot("basic", 8U + 2U * high_bit_depth, mode_index,
                           &snapshot, &basic_expected[high_bit_depth][mode_index]);
        }
        for (mode_index = 1U; mode_index <= 8U; ++mode_index) {
            PredictionSnapshot snapshot = { 0U, 0U, 0U,
                UINT64_C(14695981039346656037) };
            Av2hFirstLeafMode mode = { 0 };
            unsigned int mrl;
            unsigned int multi;
            int angle;
            mode.mode = (Av2hFirstLeafIntraMode)mode_index;
            for (x = 0U; x < 4U; ++x) {
                for (mrl = 0U; mrl < 4U; ++mrl) {
                    for (multi = 0U; multi < 2U; ++multi) {
                        for (angle = -3; angle <= 3; ++angle) {
                            for (ibp = 0U; ibp < 2U; ++ibp) {
                                mode.mrl_index = (uint8_t)mrl;
                                mode.multi_line_mrl = (uint8_t)multi;
                                mode.angle_delta = (int8_t)angle;
                                mode.enable_ibp = (uint8_t)ibp;
                                prediction_case(&snapshot, &mode,
                                    directional_shapes[x][0U],
                                    directional_shapes[x][1U], 3U,
                                    high_bit_depth);
                            }
                        }
                    }
                }
            }
            check_snapshot("directional", 8U + 2U * high_bit_depth, mode_index,
                &snapshot, &directional_expected[high_bit_depth][mode_index - 1U]);
        }
    }
    for (mode_index = 0U; mode_index < 6U; ++mode_index) {
        PredictionSnapshot snapshot = { 0U, 0U, 0U,
            UINT64_C(14695981039346656037) };
        Av2hFirstLeafMode mode = { 0 };
        unsigned int transpose;
        mode.use_intra_dip = 1U;
        for (x = 0U; x < 5U; ++x) {
            for (y = 0U; y < 5U; ++y) {
                for (transpose = 0U; transpose < 2U; ++transpose) {
                    for (edges = 0U; edges < 4U; ++edges) {
                        mode.intra_dip_mode = (uint8_t)(
                            mode_index + 16U * transpose);
                        prediction_case(&snapshot, &mode,
                            dimensions[x], dimensions[y], edges, 0U);
                    }
                }
            }
        }
        check_snapshot("dip", 8U, mode_index, &snapshot, &dip_expected[mode_index]);
    }
}

static int bytes_are(const void *pointer, size_t size, uint8_t value) {
    const uint8_t *bytes = pointer;
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (bytes[index] != value) return 0;
    }
    return 1;
}

static void test_prediction_dispatch(void) {
    static const PredictionSnapshot expected[2U][14U] = {
        {
            { 0U, 3005U, 0U, UINT64_C(0x17c2d86b9d208136) },
            { 2244U, 205U, 568U, UINT64_C(0x75fc803c0f09bf18) },
            { 2255U, 203U, 571U, UINT64_C(0x3ea884061abaf181) },
            { 2244U, 205U, 572U, UINT64_C(0xd67ee102cecf217d) },
            { 2253U, 189U, 567U, UINT64_C(0xc2006117ca81293c) },
            { 2332U, 109U, 564U, UINT64_C(0x1039cfd1bdc16d63) },
            { 2257U, 189U, 567U, UINT64_C(0xf15bac5123062e0e) },
            { 2260U, 189U, 568U, UINT64_C(0xd4d8e53c3889bab3) },
            { 2248U, 209U, 572U, UINT64_C(0x554f3aa86113b8b5) },
            { 0U, 3005U, 0U, UINT64_C(0x30db15184db7d739) },
            { 0U, 3005U, 0U, UINT64_C(0x3412e7a6b6200070) },
            { 0U, 3005U, 0U, UINT64_C(0x283819653a893b4b) },
            { 2U, 3003U, 0U, UINT64_C(0x3faa0192965a0ad0) },
            { 0U, 3005U, 0U, UINT64_C(0x7ab4997f9adee41d) }
        },
        {
            { 140U, 136U, 2729U, UINT64_C(0x4acffe132bd07f91) },
            { 2447U, 0U, 570U, UINT64_C(0xb1d9bc6c5e12685e) },
            { 2458U, 0U, 571U, UINT64_C(0xc84013732bef20d4) },
            { 2436U, 16U, 569U, UINT64_C(0x32bd4291727f9127) },
            { 2440U, 0U, 569U, UINT64_C(0xeaf7a8a2fcbca26f) },
            { 2436U, 0U, 569U, UINT64_C(0x2aa5da7109de2180) },
            { 2444U, 0U, 569U, UINT64_C(0xb2ab1fba740f5952) },
            { 2436U, 12U, 569U, UINT64_C(0x859bffbb094761a3) },
            { 2436U, 20U, 573U, UINT64_C(0x440afb20f60be51e) },
            { 276U, 0U, 2729U, UINT64_C(0x388084812ca7f001) },
            { 276U, 0U, 2729U, UINT64_C(0x27a29c993b93a9a7) },
            { 276U, 0U, 2729U, UINT64_C(0x828ef0c74385f815) },
            { 276U, 0U, 2729U, UINT64_C(0xac5c63be45ca7159) },
            { 140U, 136U, 2729U, UINT64_C(0xf22ac22b8c3dce92) }
        }
    };
    static const uint16_t dimensions[] = { 4U, 8U, 16U, 32U, 64U };
    static const struct {
        uint16_t width;
        uint16_t height;
        Av2hFirstLeafIntraMode mode;
        int8_t angle;
        uint8_t mrl;
        uint8_t multiline;
    } compatibility[] = {
        { 8U, 4U, AV2H_INTRA_VERTICAL, 0, 3U, 1U },
        { 4U, 16U, AV2H_INTRA_HORIZONTAL, 0, 3U, 0U },
        { 4U, 4U, AV2H_INTRA_D67, 0, 0U, 1U },
        { 4U, 4U, AV2H_INTRA_D67, 0, 1U, 0U },
        { 64U, 16U, AV2H_INTRA_D203, 3, 0U, 0U },
        { 4U, 4U, AV2H_INTRA_D157, 3, 0U, 0U },
        { 16U, 16U, AV2H_INTRA_HORIZONTAL, -2, 3U, 0U },
        { 64U, 8U, AV2H_INTRA_HORIZONTAL, -1, 0U, 0U },
        { 8U, 16U, AV2H_INTRA_HORIZONTAL, 0, 2U, 1U },
        { 8U, 8U, AV2H_INTRA_HORIZONTAL, 0, 2U, 0U },
        { 8U, 8U, AV2H_INTRA_HORIZONTAL, 2, 3U, 1U },
        { 4U, 4U, AV2H_INTRA_VERTICAL, 0, 3U, 0U },
        { 8U, 16U, AV2H_INTRA_VERTICAL, 2, 0U, 0U },
        { 32U, 32U, AV2H_INTRA_D45, 0, 1U, 0U },
        { 16U, 16U, AV2H_INTRA_D157, 2, 2U, 0U },
        { 16U, 32U, AV2H_INTRA_D135, 0, 2U, 0U },
        { 8U, 8U, AV2H_INTRA_D67, -1, 1U, 0U },
        { 4U, 4U, AV2H_INTRA_D67, -2, 1U, 0U },
        { 8U, 8U, AV2H_INTRA_D67, -2, 1U, 0U },
        { 4U, 4U, AV2H_INTRA_D67, -3, 3U, 0U },
        { 16U, 4U, AV2H_INTRA_D45, -1, 0U, 0U },
        { 8U, 4U, AV2H_INTRA_D45, 3, 0U, 0U },
        { 8U, 16U, AV2H_INTRA_D45, 3, 1U, 0U },
        { 8U, 8U, AV2H_INTRA_D203, 3, 1U, 0U },
        { 8U, 4U, AV2H_INTRA_D203, 3, 0U, 0U }
    };
    for (unsigned int depth = 0U; depth < 2U; ++depth) {
        for (unsigned int index = 0U; index < 14U; ++index) {
            PredictionSnapshot actual = {
                0U, 0U, 0U, UINT64_C(14695981039346656037)
            };
            Av2hFirstLeafMode mode = { 0 };
            mode.mode = (Av2hFirstLeafIntraMode)index;
            for (size_t x = 0U; x < 5U; ++x) {
                for (size_t y = 0U; y < 5U; ++y) {
                    for (int angle = -3; angle <= 3; ++angle) {
                        for (unsigned int mrl = 0U; mrl < 4U; ++mrl) {
                            unsigned int flags = (unsigned int)(angle + 3) + mrl;
                            mode.angle_delta = (int8_t)angle;
                            mode.mrl_index = (uint8_t)mrl;
                            mode.multi_line_mrl = (uint8_t)((flags >> 1U) & 1U);
                            mode.enable_ibp = (uint8_t)(flags & 1U);
                            for (unsigned int edges = 0U; edges < 4U; ++edges) {
                                prediction_case(&actual, &mode,
                                    dimensions[x], dimensions[y], edges, depth);
                            }
                        }
                    }
                    mode.mrl_index = 0U;
                    mode.multi_line_mrl = 0U;
                    mode.enable_ibp = 1U;
                    for (int angle = -4; angle <= 4; angle += 8) {
                        mode.angle_delta = (int8_t)angle;
                        for (unsigned int edges = 0U; edges < 4U; ++edges) {
                            prediction_case(&actual, &mode,
                                dimensions[x], dimensions[y], edges, depth);
                        }
                    }
                }
            }
            for (size_t c = 0U; c < sizeof(compatibility) /
                    sizeof(compatibility[0]); ++c) {
                if (compatibility[c].mode != mode.mode) continue;
                mode.angle_delta = compatibility[c].angle;
                mode.mrl_index = compatibility[c].mrl;
                mode.multi_line_mrl = compatibility[c].multiline;
                mode.enable_ibp = 2U;
                for (unsigned int edges = 0U; edges < 4U; ++edges) {
                    prediction_case(&actual, &mode, compatibility[c].width,
                        compatibility[c].height, edges, depth);
                }
            }
            mode.angle_delta = 0;
            mode.mrl_index = 0U;
            mode.multi_line_mrl = 0U;
            mode.enable_ibp = 0U;
            prediction_case(&actual, &mode, 0U, 0U, 0U, depth);
            prediction_case(&actual, &mode, 3U, 5U, 3U, depth);
            prediction_case(&actual, &mode, 65U, 4U, 3U, depth);
            mode.multi_line_mrl = 2U;
            prediction_case(&actual, &mode, 8U, 8U, 3U, depth);
            mode.multi_line_mrl = 0U;
            mode.mrl_index = 255U;
            prediction_case(&actual, &mode, 8U, 8U, 3U, depth);
            const PredictionSnapshot *baseline = &expected[depth][index];
            if (actual.hash != baseline->hash ||
                actual.ok != baseline->ok ||
                actual.unsupported != baseline->unsupported ||
                actual.invalid != baseline->invalid) {
                fprintf(stderr, "dispatch,%u,%u,%zu,%zu,%zu,%016" PRIx64 "\n",
                    depth, index, actual.ok, actual.unsupported,
                    actual.invalid, actual.hash);
            }
            CHECK(actual.ok == baseline->ok);
            CHECK(actual.unsupported == baseline->unsupported);
            CHECK(actual.invalid == baseline->invalid);
            CHECK(actual.hash == baseline->hash);
        }
    }
}

static void test_unsupported_reconstruction(void) {
    Av2hFirstLeafMode mode = { 0 };
    Av2hFirstCoeffPrefix prefix = { 0 };
    Av2hFirstTransformType transform = { 0 };
    static Av2hFirstCoeffBlock coefficients;
    static Av2hReconstructionScratch scratch;
    static Av2hReconstructionScratchU16 scratch_u16;
    static Av2hFirstReconstruction output;
    static Av2hFirstReconstructionU16 output_u16;
    Av2hDecodeDiagnostics diagnostics = { 0 };
    uint8_t pixels[16U];
    uint16_t pixels_u16[16U];
    uint8_t ready[1U] = { 0U };
    Av2hPartitionNode leaf = { 0 };
    Av2hReconstructionPlane plane = {
        pixels, 4U, 4U, 4U, 4U, 4U, ready, 1U, 1U
    };
    Av2hReconstructionPlaneU16 plane_u16 = {
        pixels_u16, 4U, 4U, 4U, 4U, 4U, ready, 1U, 1U
    };
    prefix.tx_size = AV2H_TX_4X4;
    prefix.q_index = 152U;
    prefix.all_zero = 1U;
    coefficients.width = 4U;
    coefficients.height = 4U;
    coefficients.coefficient_count = 15U;
    av2h_memory_fill(pixels, 0xa5U, sizeof(pixels));
    av2h_memory_fill(pixels_u16, 0xa5U, sizeof(pixels_u16));
    av2h_memory_fill(&output, 0xa5U, sizeof(output));
    av2h_memory_fill(&output_u16, 0xa5U, sizeof(output_u16));
    CHECK(av2h_luma_reconstruct_with_diagnostics(
        &diagnostics, &mode, &prefix, &transform, &coefficients,
        pixels, 4U, &scratch, &output) == AV2H_UNSUPPORTED);
    CHECK(bytes_are(&output, sizeof(output), 0xa5U));
    CHECK(av2h_reconstruct_transform_generic_u16_with_diagnostics(
        &diagnostics, &mode, &prefix, &transform, &coefficients,
        pixels_u16, 4U, &scratch_u16, &output_u16) == AV2H_UNSUPPORTED);
    CHECK(bytes_are(&output_u16, sizeof(output_u16), 0xa5U));
    CHECK(av2h_luma_reconstruct_to_plane_with_diagnostics(
        &diagnostics, &leaf, &mode, &prefix, &transform, &coefficients,
        &plane, &scratch) == AV2H_UNSUPPORTED);
    CHECK(bytes_are(pixels, sizeof(pixels), 0xa5U));
    CHECK(ready[0U] == 0U);
    CHECK(av2h_luma_reconstruct_to_plane_u16_with_diagnostics(
        &diagnostics, &leaf, &mode, &prefix, &transform, &coefficients,
        &plane_u16, &scratch_u16) == AV2H_UNSUPPORTED);
    CHECK(bytes_are(pixels_u16, sizeof(pixels_u16), 0xa5U));
    CHECK(ready[0U] == 0U);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    CHECK(av2h_reconstruction_diagnostics_get(&diagnostics) ==
          &diagnostics.reconstruction);
    CHECK(diagnostics.reconstruction.luma_stage == 2U);
    CHECK(diagnostics.reconstruction.inverse_transform_stage == 10U);
#else
    CHECK(av2h_reconstruction_diagnostics_get(&diagnostics) == 0);
    CHECK(bytes_are(&diagnostics, sizeof(diagnostics), 0U));
#endif
}

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t chroma;
    uint64_t filter[2U];
    uint64_t workspace[2U];
    uint64_t output[2U];
} ResourceSnapshot;

static const ResourceSnapshot resources[] = {
    { 17U, 15U, 0U, { 2163U, 3315U }, { 22327U, 28666U }, { 399U, 798U } },
    { 17U, 15U, 1U, { 1105U, 1873U }, { 20963U, 26724U }, { 255U, 510U } },
    { 17U, 15U, 2U, { 3315U, 5619U }, { 24055U, 32122U }, { 765U, 1530U } },
    { 896U, 598U, 0U, { 3024420U, 4637220U },
        { 5512732U, 7936543U }, { 803712U, 1607424U } },
    { 896U, 598U, 1U, { 1545740U, 2620940U },
        { 3697004U, 5314413U }, { 535808U, 1071616U } },
    { 896U, 598U, 2U, { 4637220U, 7862820U },
        { 7931932U, 12774943U }, { 1607424U, 3214848U } },
    { 16384U, 16384U, 0U, { 1510146048U, 2315452416U },
        { 2735546888U, 3943511051U }, { 402653184U, 805306368U } },
    { 16384U, 16384U, 1U, { 771817472U, 1308688384U },
        { 1829298696U, 2634609673U }, { 268435456U, 536870912U } },
    { 16384U, 16384U, 2U, { 2315452416U, 3926065152U },
        { 3943506440U, UINT64_C(6359430155) }, { 805306368U, 1610612736U } }
};

static void test_resource_baseline(void) {
    size_t index;
    for (index = 0U; index < sizeof(resources) / sizeof(resources[0U]); ++index) {
        const ResourceSnapshot *expected = &resources[index];
        Av2hSequenceState sequence = { 0 };
        Av2hFrameState frame = { 0 };
        unsigned int storage;
        if (sizeof(size_t) < 8U && expected->width == 16384U) {
            puts("maximum resource snapshot unavailable on 32-bit size_t");
            continue;
        }
        sequence.superblock_size = 64U;
        frame.width = expected->width;
        frame.height = expected->height;
        frame.chroma_format_idc = expected->chroma;
        frame.plane_count = expected->chroma == 1U ? 1U : 3U;
        for (storage = 0U; storage < 2U; ++storage) {
            Av2hFilterWorkspaceLayout filter = { 0 };
            Av2hFrameDecodeWorkspaceLayout workspace = { 0 };
            size_t output_size = 0U;
            sequence.info.bit_depth_idc = storage == 0U ? 1U : 0U;
            CHECK(av2h_filter_workspace_query_bytes(
                &frame, (uint8_t)(storage + 1U), &filter) == AV2H_OK);
            CHECK(av2h_frame_decode_workspace_query_bytes(
                &sequence, &frame, (uint8_t)(storage + 1U),
                &workspace) == AV2H_OK);
            CHECK(av2h_planar_output_size_bytes(
                &frame, (uint8_t)(storage + 1U), &output_size) == AV2H_OK);
            if (inventory) {
                printf("resource,%u,%u,%u,%u,%zu,%zu,%zu,%zu,%zu\n",
                    frame.width, frame.height, frame.chroma_format_idc,
                    storage + 1U, filter.size_bytes, workspace.size_bytes,
                    output_size, filter.alignment, workspace.alignment);
            }
            if (sizeof(void *) == 8U && sizeof(size_t) == 8U) {
                CHECK(filter.size_bytes == expected->filter[storage]);
                CHECK(workspace.size_bytes == expected->workspace[storage]);
                CHECK(output_size == expected->output[storage]);
            }
            CHECK(filter.alignment == 2U);
            CHECK(workspace.alignment == 4U);
        }
    }
}

static void test_private_layout_baseline(void) {
#define CHECK_LAYOUT(type, size, alignment)                                    \
    do {                                                                       \
        if (inventory) {                                                       \
            printf("layout,%s,%zu,%zu\n", #type, sizeof(type), _Alignof(type));  \
        }                                                                      \
        if (sizeof(void *) == 8U && sizeof(size_t) == 8U) {                     \
            CHECK(sizeof(type) == (size));                                     \
            CHECK(_Alignof(type) == (alignment));                              \
        }                                                                      \
    } while (0)
    CHECK_LAYOUT(Av2hFrameDecodeWorkspace, 832U, 8U);
    CHECK_LAYOUT(Av2hFrameDecodeResult, 144U, 8U);
    CHECK_LAYOUT(Av2hFilterWorkspaceStorage, 56U, 8U);
    CHECK_LAYOUT(Av2hDecodeDiagnostics, 216U, 8U);
    CHECK_LAYOUT(Av2hFirstLeafMode, 1760U, 8U);
    CHECK_LAYOUT(Av2hFirstChromaMode, 112U, 8U);
    CHECK_LAYOUT(Av2hLumaReferences, 80U, 8U);
    CHECK_LAYOUT(Av2hLumaReferencesU16, 96U, 8U);
    CHECK_LAYOUT(Av2hFirstReconstruction, 12292U, 2U);
    CHECK_LAYOUT(Av2hFirstReconstructionU16, 24580U, 4U);
#undef CHECK_LAYOUT
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--inventory") == 0) {
        inventory = 1;
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--inventory]\n", argv[0]);
        return 2;
    }
    test_prediction_matrix();
    test_prediction_dispatch();
    test_unsupported_reconstruction();
    test_resource_baseline();
    test_private_layout_baseline();
    if (failures != 0) {
        fprintf(stderr, "refactoring baseline: %d failures\n", failures);
        return 1;
    }
    puts("refactoring baseline: passed");
    return 0;
}

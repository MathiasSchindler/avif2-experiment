#include "decoder/tile.h"

#include <stdio.h>

#define STREAM_CAPACITY 65536U
#define MAX_MI_COLUMNS 128U
#define MAX_MI_ROWS 96U
#define MAX_GDF_BLOCKS 128U
#define MAX_CDEF_UNITS 128U

static int failures;
static unsigned char stream[STREAM_CAPACITY];

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define REQUIRE(condition)                                                     \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: required check failed: %s\n",           \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
            return;                                                            \
        }                                                                      \
    } while (0)

typedef struct {
    uint16_t position;
    int32_t quantized;
    int32_t dequantized;
} ArtifactCoefficient;

typedef struct {
    size_t mode_before;
    size_t mode_after_set;
    size_t mode_after_index;
    size_t mode_after_fsc;
    size_t mode_after_mrl;
    Av2hFirstTxSize tx_size;
    uint16_t eob;
    size_t prefix_after_zero;
    size_t prefix_after_point;
    size_t prefix_after_eob;
    Av2hFirstTxType primary_type;
    uint8_t secondary_symbol;
    size_t transform_after;
    uint16_t coefficient_count;
    uint16_t nonzero_count;
    int32_t dc_value;
    size_t levels_before;
    size_t levels_after;
    size_t coefficients_after;
    const ArtifactCoefficient *coefficients;
    size_t coefficients_count;
} ArtifactDecodedExpectation;

typedef struct {
    const char *name;
    uint16_t width;
    uint16_t height;
    uint16_t base_q_index;
    uint8_t allow_intrabc;
    uint8_t enable_gdf;
    uint8_t enable_cdef;
    uint8_t enable_ccso;
    size_t header_bits;
    uint8_t gdf_mode;
    uint8_t gdf_qp_index;
    uint8_t gdf_scale_index;
    Av2hBlockSize first_block;
    size_t first_tell;
    Av2hStatus first_status;
    const ArtifactDecodedExpectation *decoded;
} ArtifactExpectation;

static void check_decoded_block(
    const ArtifactDecodedExpectation *expected,
    const Av2hTileLumaLeaf *leaf,
    const Av2hFirstCoeffBlock *block) {
    size_t index;
    CHECK(leaf->mode.before_mode_bits == expected->mode_before);
    CHECK(leaf->mode.after_mode_set_bits == expected->mode_after_set);
    CHECK(leaf->mode.after_mode_index_bits == expected->mode_after_index);
    CHECK(leaf->mode.after_fsc_bits == expected->mode_after_fsc);
    CHECK(leaf->mode.after_mrl_bits == expected->mode_after_mrl);
    CHECK(leaf->prefix.tx_size == expected->tx_size);
    CHECK(leaf->prefix.eob == expected->eob);
    CHECK(leaf->prefix.after_all_zero_bits == expected->prefix_after_zero);
    CHECK(leaf->prefix.after_eob_point_bits == expected->prefix_after_point);
    CHECK(leaf->prefix.after_eob_bits == expected->prefix_after_eob);
    CHECK(leaf->transform.primary_type == expected->primary_type);
    CHECK(leaf->transform.secondary_symbol == expected->secondary_symbol);
    CHECK(leaf->transform.after_secondary_bits == expected->transform_after);
    CHECK(block->coefficient_count == expected->coefficient_count);
    CHECK(block->nonzero_count == expected->nonzero_count);
    CHECK(block->dc_value == expected->dc_value);
    CHECK(block->before_levels_bits == expected->levels_before);
    CHECK(block->after_levels_bits == expected->levels_after);
    CHECK(block->after_coefficients_bits == expected->coefficients_after);
    for (index = 0U; index < block->coefficient_count; ++index) {
        int32_t quantized = 0;
        int32_t dequantized = 0;
        size_t expected_index;
        for (expected_index = 0U;
             expected_index < expected->coefficients_count;
             ++expected_index) {
            if (expected->coefficients[expected_index].position == index) {
                quantized =
                    expected->coefficients[expected_index].quantized;
                dequantized =
                    expected->coefficients[expected_index].dequantized;
                break;
            }
        }
        CHECK(block->quantized[index] == quantized);
        CHECK(block->dequantized[index] == dequantized);
    }
}

static size_t load_artifact(const char *name) {
    char path[256U];
    FILE *source;
    size_t size;
    int written = snprintf(path, sizeof(path), "images/artifacts/%s", name);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        fprintf(stderr, "artifact path is too long: %s\n", name);
        ++failures;
        return 0U;
    }
    source = fopen(path, "rb");
    if (source == 0) {
        fprintf(stderr, "failed to open artifact: %s\n", path);
        ++failures;
        return 0U;
    }
    size = fread(stream, 1U, sizeof(stream), source);
    if (ferror(source) || !feof(source)) {
        fprintf(stderr, "failed to read artifact: %s\n", path);
        ++failures;
        size = 0U;
    }
    fclose(source);
    return size;
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) {
            return &ledger->entries[index];
        }
    }
    return 0;
}

static void check_first_luma(const ArtifactExpectation *expected,
                             const Av2hObu *frame_obu,
                             const Av2hSequenceState *sequence,
                             const Av2hFrameState *frame) {
    Av2hTileGroup group;
    uint8_t luma_partition_above[MAX_MI_COLUMNS];
    uint8_t luma_partition_left[MAX_MI_ROWS];
    uint8_t chroma_partition_above[MAX_MI_COLUMNS];
    uint8_t chroma_partition_left[MAX_MI_ROWS];
    Av2hLumaModeContext luma_mode_above[MAX_MI_COLUMNS];
    Av2hLumaModeContext luma_mode_left[MAX_MI_ROWS];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * MAX_MI_COLUMNS];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * MAX_MI_ROWS];
    Av2hTileLumaModeCell colocated_luma[MAX_MI_COLUMNS * MAX_MI_ROWS];
    uint8_t gdf_block_flags[MAX_GDF_BLOCKS];
    uint8_t cdef_unit_strengths[MAX_CDEF_UNITS];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        MAX_MI_COLUMNS, MAX_MI_ROWS, MAX_MI_COLUMNS * MAX_MI_ROWS,
        gdf_block_flags, MAX_GDF_BLOCKS,
        cdef_unit_strengths, MAX_CDEF_UNITS
    };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    Av2hStatus status;
    uint8_t has_leaf = 0U;

    REQUIRE(av2h_tile_group_single_parse(
                (Av2hSpan){ stream + frame_obu->payload_offset,
                             frame_obu->payload_size },
                frame->uncompressed_header_bits, &group) == AV2H_OK);
    status = av2h_tile_decoder_init(
        &decoder, group.entropy, sequence, frame, &storage);
    REQUIRE(status == AV2H_OK);
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(&block, 0U, sizeof(block));
    status = av2h_tile_decoder_next_luma(
        &decoder, &leaf, &block, &has_leaf);
    CHECK(status == expected->first_status);
    CHECK(has_leaf == 1U);
    CHECK(leaf.node.row == 0U);
    CHECK(leaf.node.column == 0U);
    CHECK(leaf.node.block_size == expected->first_block);
    CHECK(av2h_range_tell(&decoder.range) == expected->first_tell);
    if (expected->gdf_mode == 2U) {
        uint8_t expected_cdef_count;
        uint8_t expected_cdef_index;
        uint8_t expected_flag =
            expected->base_q_index == 48U ||
            expected->base_q_index == 64U ? 0U : 1U;
        CHECK(leaf.mode.gdf_flags_read == 1U);
        CHECK(leaf.mode.before_gdf_bits + 1U ==
              leaf.mode.after_gdf_bits);
        CHECK(decoder.gdf_entropy.decoded_count == 1U);
        CHECK(gdf_block_flags[0] == expected_flag);
        switch (expected->base_q_index) {
            case 48U:
                expected_cdef_count = 8U;
                expected_cdef_index = 6U;
                break;
            case 64U:
                expected_cdef_count = 8U;
                expected_cdef_index = 3U;
                break;
            case 96U:
                expected_cdef_count = 3U;
                expected_cdef_index = 0U;
                break;
            case 112U:
                expected_cdef_count = 6U;
                expected_cdef_index = 5U;
                break;
            case 128U:
                expected_cdef_count = 3U;
                expected_cdef_index = 1U;
                break;
            default:
                expected_cdef_count = 1U;
                expected_cdef_index = 0U;
                break;
        }
        CHECK(frame->cdef_strength_count == expected_cdef_count);
        CHECK(leaf.mode.before_cdef_bits == leaf.mode.after_gdf_bits);
        CHECK(leaf.mode.cdef_strength_index == expected_cdef_index);
        CHECK(leaf.mode.cdef_strengths_read ==
              (expected_cdef_count > 1U ? 1U : 0U));
        CHECK(leaf.mode.after_cdef_bits == expected->first_tell);
        if (expected_cdef_count > 1U) {
            CHECK(cdef_unit_strengths[0] == expected_cdef_index);
        }
    }
    if (expected->decoded != 0) {
        CHECK(status == AV2H_OK);
        check_decoded_block(expected->decoded, &leaf, &block);
    }
}

static void check_artifact(const ArtifactExpectation *expected) {
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hStatus status;
    size_t stream_size = load_artifact(expected->name);

    REQUIRE(stream_size != 0U);
    REQUIRE(av2h_obu_parse(
                (Av2hSpan){ stream, stream_size }, &ledger, &error) ==
            AV2H_OK);
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    REQUIRE(sequence_obu != 0);
    REQUIRE(frame_obu != 0);
    REQUIRE(av2h_sequence_state_parse(
                (Av2hSpan){ stream, stream_size }, sequence_obu,
                &sequence, &error) == AV2H_OK);

    CHECK(sequence.info.seq_profile_idc == 0U);
    CHECK(sequence.info.bit_depth_idc == 1U);
    CHECK(sequence.info.chroma_format_idc == 0U);
    CHECK(sequence.info.still_picture == 1U);
    CHECK(sequence.superblock_size == 64U);
    CHECK(sequence.enable_ext_partitions == 1U);
    CHECK(sequence.enable_fsc == 1U);
    CHECK(sequence.enable_ist == 1U);
    CHECK(sequence.enable_cfl_intra == 1U);
    CHECK(sequence.enable_mrls == 1U);
    CHECK(sequence.enable_cctx == 1U);
    CHECK(sequence.enable_gdf == expected->enable_gdf);
    CHECK(sequence.enable_cdef == expected->enable_cdef);
    CHECK(sequence.enable_ccso == expected->enable_ccso);

    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){ stream + frame_obu->payload_offset,
                     frame_obu->payload_size },
        &sequence, &frame);
    CHECK(status == AV2H_OK);
    CHECK(frame.width == expected->width);
    CHECK(frame.height == expected->height);
    CHECK(frame.base_q_index == expected->base_q_index);
    CHECK(frame.allow_intrabc == expected->allow_intrabc);
    CHECK(frame.uncompressed_header_bits == expected->header_bits);
    CHECK(frame.gdf_mode == expected->gdf_mode);
    CHECK(frame.gdf_qp_index == expected->gdf_qp_index);
    CHECK(frame.gdf_scale_index == expected->gdf_scale_index);
    if (sequence.enable_gdf) {
        uint16_t block_size =
            sequence.gdf_unit_matches_superblock_size ?
                sequence.superblock_size : 128U;
        uint16_t columns = (uint16_t)(
            ((uint32_t)frame.width + block_size - 1U) / block_size);
        uint16_t rows = (uint16_t)(
            ((uint32_t)frame.height + block_size - 1U) / block_size);
        CHECK(frame.gdf_block_size == block_size);
        CHECK(frame.gdf_block_columns == columns);
        CHECK(frame.gdf_block_rows == rows);
        CHECK(frame.gdf_block_count == (uint32_t)columns * rows);
    }
    if (status == AV2H_OK) {
        check_first_luma(expected, frame_obu, &sequence, &frame);
    }
}

static const ArtifactCoefficient cuckoo_q160_coefficients[] = {
    { 0U, -6, -1488 }, { 16U, -1, -248 },
    { 17U, 1, 248 }, { 32U, 1, 248 }
};
static const ArtifactCoefficient emerald_q208_coefficients[] = {
    { 0U, -2, -1984 }, { 1U, 2, 1984 }, { 33U, 1, 992 }
};
static const ArtifactCoefficient louis_q224_coefficients[] = {
    { 0U, 6, 9408 }
};
static const ArtifactCoefficient pastell_q160_coefficients[] = {
    { 0U, -8, -1984 }, { 1U, -3, -744 }, { 2U, -1, -248 },
    { 3U, -1, -248 }, { 16U, -2, -496 }, { 17U, -4, -992 },
    { 18U, -1, -248 }, { 32U, -2, -496 }, { 33U, -1, -248 },
    { 34U, -1, -248 }
};

static const ArtifactDecodedExpectation cuckoo_q160_decoded = {
    9U, 10U, 11U, 11U, 11U,
    AV2H_TX_16X32, 5U, 11U, 13U, 15U,
    AV2H_TX_DCT_DCT, 0U, 20U,
    512U, 4U, -6, 20U, 31U, 35U,
    cuckoo_q160_coefficients,
    sizeof(cuckoo_q160_coefficients) / sizeof(cuckoo_q160_coefficients[0])
};
static const ArtifactDecodedExpectation emerald_q208_decoded = {
    7U, 7U, 8U, 8U, 8U,
    AV2H_TX_32X32, 5U, 8U, 10U, 12U,
    AV2H_TX_DCT_DCT, 0U, 16U,
    1024U, 3U, -2, 16U, 24U, 27U,
    emerald_q208_coefficients,
    sizeof(emerald_q208_coefficients) / sizeof(emerald_q208_coefficients[0])
};
static const ArtifactDecodedExpectation louis_q224_decoded = {
    10U, 10U, 11U, 11U, 11U,
    AV2H_TX_32X32, 1U, 11U, 13U, 13U,
    AV2H_TX_DCT_DCT, 0U, 13U,
    1024U, 1U, 6, 13U, 26U, 27U,
    louis_q224_coefficients,
    sizeof(louis_q224_coefficients) / sizeof(louis_q224_coefficients[0])
};
static const ArtifactDecodedExpectation pastell_q160_decoded = {
    13U, 13U, 14U, 14U, 14U,
    AV2H_TX_16X32, 13U, 14U, 16U, 19U,
    AV2H_TX_DCT_FLIPADST, 0U, 26U,
    512U, 10U, -8, 26U, 53U, 65U,
    pastell_q160_coefficients,
    sizeof(pastell_q160_coefficients) / sizeof(pastell_q160_coefficients[0])
};

static void test_artifact_profiles(void) {
    static const ArtifactExpectation expected[] = {
        { "chalk-320x240.obu", 320U, 240U, 128U, 1U, 1U, 1U, 1U,
          219U, 1U, 2U, 3U, AV2H_BLOCK_8X16, 14U,
          AV2H_UNSUPPORTED, 0 },
        { "chimp-240x287-qp192.obu", 240U, 287U, 192U, 0U, 1U, 1U, 1U,
          217U, 1U, 1U, 3U, AV2H_BLOCK_8X16, 15U,
          AV2H_UNSUPPORTED, 0 },
        { "chimp-93x111-qp064.obu", 93U, 111U, 64U, 1U, 0U, 1U, 1U,
          179U, 0U, 0U, 0U, AV2H_BLOCK_8X8, 16U,
          AV2H_UNSUPPORTED, 0 },
        { "chinnathurai-128x128-qp112.obu", 128U, 128U, 112U, 1U,
          1U, 1U, 1U, 134U, 1U, 2U, 0U, AV2H_BLOCK_32X64, 5U,
          AV2H_UNSUPPORTED, 0 },
        { "chinnathurai-333x333-qp224.obu", 333U, 333U, 224U, 0U,
          1U, 0U, 1U, 98U, 1U, 3U, 2U, AV2H_BLOCK_64X64, 2U,
          AV2H_UNSUPPORTED, 0 },
        { "cuckoo-157x105-qp160.obu", 157U, 105U, 160U, 1U, 1U, 1U, 0U,
          69U, 1U, 1U, 3U, AV2H_BLOCK_16X32, 35U,
          AV2H_OK, &cuckoo_q160_decoded },
        { "cuckoo-431x287-qp080.obu", 431U, 287U, 80U, 1U, 0U, 1U, 1U,
          174U, 0U, 0U, 0U, AV2H_BLOCK_64X64, 2U,
          AV2H_UNSUPPORTED, 0 },
        { "emerald-192x96-qp208.obu", 192U, 96U, 208U, 0U, 0U, 0U, 0U,
          36U, 0U, 0U, 0U, AV2H_BLOCK_32X32, 27U,
          AV2H_OK, &emerald_q208_decoded },
        { "emerald-480x240-qp128.obu", 480U, 240U, 128U, 1U, 1U, 1U, 1U,
          344U, 2U, 1U, 2U, AV2H_BLOCK_64X64, 4U,
          AV2H_UNSUPPORTED, 0 },
        { "flycatcher-223x149-qp176.obu", 223U, 149U, 176U, 0U,
          1U, 1U, 1U, 125U, 1U, 0U, 3U, AV2H_BLOCK_32X64, 11U,
          AV2H_UNSUPPORTED, 0 },
        { "flycatcher-397x265-qp048.obu", 397U, 265U, 48U, 1U,
          1U, 1U, 1U, 256U, 2U, 3U, 3U, AV2H_BLOCK_32X32, 11U,
          AV2H_UNSUPPORTED, 0 },
        { "laarim-256x171-qp096.obu", 256U, 171U, 96U, 1U, 1U, 1U, 1U,
          120U, 2U, 1U, 3U, AV2H_BLOCK_32X32, 11U,
          AV2H_UNSUPPORTED, 0 },
        { "laarim-448x299-qp192.obu", 448U, 299U, 192U, 0U, 1U, 1U, 1U,
          98U, 1U, 2U, 2U, AV2H_BLOCK_64X64, 5U,
          AV2H_UNSUPPORTED, 0 },
        { "louis-301x222-qp144.obu", 301U, 222U, 144U, 1U, 1U, 1U, 1U,
          192U, 1U, 2U, 3U, AV2H_BLOCK_64X16, 7U,
          AV2H_UNSUPPORTED, 0 },
        { "louis-463x342-qp224.obu", 463U, 342U, 224U, 0U, 1U, 1U, 1U,
          131U, 1U, 3U, 3U, AV2H_BLOCK_32X32, 27U,
          AV2H_OK, &louis_q224_decoded },
        { "pastell-320x213-qp064.obu", 320U, 213U, 64U, 1U, 1U, 1U, 1U,
          252U, 2U, 2U, 1U, AV2H_BLOCK_8X8, 21U,
          AV2H_UNSUPPORTED, 0 },
        { "pastell-512x341-qp160.obu", 512U, 341U, 160U, 1U, 1U, 1U, 1U,
          332U, 1U, 2U, 3U, AV2H_BLOCK_16X32, 65U,
          AV2H_OK, &pastell_q160_decoded },
        { "schwaebisch-353x234-qp112.obu", 353U, 234U, 112U, 1U,
          1U, 1U, 1U, 200U, 2U, 1U, 3U, AV2H_BLOCK_32X8, 16U,
          AV2H_UNSUPPORTED, 0 },
        { "schwaebisch-417x277-qp208.obu", 417U, 277U, 208U, 1U,
          1U, 1U, 1U, 111U, 1U, 3U, 1U, AV2H_BLOCK_64X16, 9U,
          AV2H_UNSUPPORTED, 0 },
        { "silene-384x307-qp080.obu", 384U, 307U, 80U, 1U, 0U, 1U, 1U,
          131U, 0U, 0U, 0U, AV2H_BLOCK_64X64, 2U,
          AV2H_UNSUPPORTED, 0 },
        { "silene-479x383-qp176.obu", 479U, 383U, 176U, 1U, 1U, 1U, 1U,
          175U, 2U, 1U, 3U, AV2H_BLOCK_64X64, 3U,
          AV2H_UNSUPPORTED, 0 }
    };
    size_t index;
    CHECK(sizeof(expected) / sizeof(expected[0]) == 21U);
    for (index = 0U; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        check_artifact(&expected[index]);
    }
}

int main(void) {
    test_artifact_profiles();
    if (failures != 0) {
        fprintf(stderr, "%d artifact profile checks failed\n", failures);
        return 1;
    }
    return 0;
}

#include "decoder/tile.h"

#include <stdio.h>

#define STREAM_CAPACITY 65536U
#define MAX_MI_COLUMNS 128U
#define MAX_MI_ROWS 96U

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
    const char *name;
    uint16_t width;
    uint16_t height;
    uint16_t base_q_index;
    uint8_t allow_intrabc;
    uint8_t enable_gdf;
    uint8_t enable_cdef;
    uint8_t enable_ccso;
    Av2hStatus frame_status;
    Av2hBlockSize first_block;
    size_t first_tell;
} ArtifactExpectation;

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
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        MAX_MI_COLUMNS, MAX_MI_ROWS, MAX_MI_COLUMNS * MAX_MI_ROWS
    };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    uint8_t has_leaf = 0U;

    REQUIRE(av2h_tile_group_single_parse(
                (Av2hSpan){ stream + frame_obu->payload_offset,
                             frame_obu->payload_size },
                frame->uncompressed_header_bits, &group) == AV2H_OK);
    REQUIRE(av2h_tile_decoder_init(
                &decoder, group.entropy, sequence, frame, &storage) ==
            AV2H_OK);
    av2h_memory_fill(&leaf, 0U, sizeof(leaf));
    av2h_memory_fill(&block, 0U, sizeof(block));
    CHECK(av2h_tile_decoder_next_luma(
              &decoder, &leaf, &block, &has_leaf) == AV2H_UNSUPPORTED);
    CHECK(has_leaf == 1U);
    CHECK(leaf.node.row == 0U);
    CHECK(leaf.node.column == 0U);
    CHECK(leaf.node.block_size == expected->first_block);
    CHECK(av2h_range_tell(&decoder.range) == expected->first_tell);
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
    CHECK(status == expected->frame_status);
    CHECK(frame.width == expected->width);
    CHECK(frame.height == expected->height);
    CHECK(frame.base_q_index == expected->base_q_index);
    CHECK(frame.allow_intrabc == expected->allow_intrabc);
    if (status == AV2H_OK) {
        check_first_luma(expected, frame_obu, &sequence, &frame);
    }
}

static void test_artifact_profiles(void) {
    static const ArtifactExpectation expected[] = {
        { "chalk-320x240.obu", 320U, 240U, 128U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "chimp-240x287-qp192.obu", 240U, 287U, 192U, 0U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "chimp-93x111-qp064.obu", 93U, 111U, 64U, 1U, 0U, 1U, 1U,
          AV2H_OK, AV2H_BLOCK_8X8, 16U },
        { "chinnathurai-128x128-qp112.obu", 128U, 128U, 112U, 1U,
          1U, 1U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "chinnathurai-333x333-qp224.obu", 333U, 333U, 224U, 0U,
          1U, 0U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "cuckoo-157x105-qp160.obu", 157U, 105U, 160U, 1U, 1U, 1U, 0U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "cuckoo-431x287-qp080.obu", 431U, 287U, 80U, 1U, 0U, 1U, 1U,
          AV2H_OK, AV2H_BLOCK_64X64, 2U },
        { "emerald-192x96-qp208.obu", 192U, 96U, 208U, 0U, 0U, 0U, 0U,
          AV2H_OK, AV2H_BLOCK_32X32, 7U },
        { "emerald-480x240-qp128.obu", 480U, 240U, 128U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "flycatcher-223x149-qp176.obu", 223U, 149U, 176U, 0U,
          1U, 1U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "flycatcher-397x265-qp048.obu", 397U, 265U, 48U, 1U,
          1U, 1U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "laarim-256x171-qp096.obu", 256U, 171U, 96U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "laarim-448x299-qp192.obu", 448U, 299U, 192U, 0U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "louis-301x222-qp144.obu", 301U, 222U, 144U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "louis-463x342-qp224.obu", 463U, 342U, 224U, 0U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "pastell-320x213-qp064.obu", 320U, 213U, 64U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "pastell-512x341-qp160.obu", 512U, 341U, 160U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "schwaebisch-353x234-qp112.obu", 353U, 234U, 112U, 1U,
          1U, 1U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "schwaebisch-417x277-qp208.obu", 417U, 277U, 208U, 1U,
          1U, 1U, 1U, AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U },
        { "silene-384x307-qp080.obu", 384U, 307U, 80U, 1U, 0U, 1U, 1U,
          AV2H_OK, AV2H_BLOCK_64X64, 2U },
        { "silene-479x383-qp176.obu", 479U, 383U, 176U, 1U, 1U, 1U, 1U,
          AV2H_UNSUPPORTED, AV2H_BLOCK_SIZE_COUNT, 0U }
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

#include "decoder/tile.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void check_vector(const unsigned char *header,
                         size_t header_size,
                         size_t uncompressed_header_bits) {
    unsigned char payload[32];
    Av2hTileGroup group;
    size_t index;
    CHECK(header_size + 1U <= sizeof(payload));
    for (index = 0U; index < header_size; ++index) payload[index] = header[index];
    payload[header_size] = 0x5aU;
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ payload, header_size + 1U },
              uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.header_bits == uncompressed_header_bits + 1U);
    CHECK(group.header_size == header_size);
    CHECK(group.entropy.data == payload + header_size);
    CHECK(group.entropy.size == 1U && group.entropy.data[0] == 0x5aU);
}

static void test_fixture_offsets(void) {
    static const unsigned char tiny[] = {
        0xe3U, 0x00U, 0x33U, 0xd0U, 0xb8U, 0x70U, 0x00U
    };
    static const unsigned char odd[] = {
        0xeeU, 0xc0U, 0x0cU, 0xf7U, 0xc2U, 0x89U, 0xc2U, 0x93U,
        0xcfU, 0xffU, 0xc1U, 0x00U, 0x00U, 0xffU, 0x2aU, 0x52U,
        0x67U, 0xd6U, 0x80U
    };
    static const unsigned char ordinary[] = {
        0xeeU, 0x90U, 0x03U, 0xe9U, 0x55U, 0x0aU, 0x27U,
        0x50U, 0x61U, 0xf7U, 0x27U, 0x9bU, 0x00U
    };
    static const unsigned char detail[] = {
        0xeeU, 0x90U, 0x03U, 0xc2U, 0x2aU,
        0x16U, 0xbcU, 0xc0U, 0x00U
    };
    check_vector(tiny, sizeof(tiny), 49U);
    check_vector(odd, sizeof(odd), 148U);
    check_vector(ordinary, sizeof(ordinary), 101U);
    check_vector(detail, sizeof(detail), 65U);
}

static void test_failures(void) {
    static const unsigned char not_first[] = { 0x00U, 0x80U };
    static const unsigned char header_only[] = { 0x80U };
    static const unsigned char bad_padding[] = { 0x81U, 0x5aU };
    Av2hTileGroup group;
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ not_first, sizeof(not_first) }, 0U,
              &group) == AV2H_UNSUPPORTED);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, 0U,
              &group) == AV2H_TRUNCATED);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ bad_padding, sizeof(bad_padding) }, 0U,
              &group) == AV2H_INVALID_DATA);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, SIZE_MAX,
              &group) == AV2H_OVERFLOW);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ 0, 1U }, 0U, &group) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ header_only, sizeof(header_only) }, 0U,
              0) == AV2H_INVALID_ARGUMENT);
}

static void test_bounded_orchestrator(void) {
    static const unsigned char tiny[] = {
        0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU, 0x2aU,
        0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU, 0x36U,
        0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U, 0x35U,
        0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU, 0x83U,
        0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU, 0xdbU,
        0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU, 0xf5U,
        0x35U, 0x75U, 0xb2U, 0xd6U, 0xd4U, 0xc1U, 0x5bU, 0xe2U,
        0xc6U, 0x99U, 0x46U, 0x47U, 0x4eU, 0x59U, 0x14U, 0xa2U,
        0xe0U, 0x35U, 0x23U, 0x50U, 0x71U, 0x5bU, 0xc0U, 0x5dU,
        0x24U, 0x8bU, 0xa5U, 0x4bU, 0xcbU, 0x90U, 0xa9U, 0x75U,
        0xfcU, 0xb0U, 0xbaU, 0x59U, 0xf2U, 0x43U, 0xc5U, 0x73U,
        0x44U, 0xeeU, 0xd8U
    };
    uint8_t luma_partition_above[16U];
    uint8_t luma_partition_left[16U];
    uint8_t chroma_partition_above[16U];
    uint8_t chroma_partition_left[16U];
    Av2hLumaModeContext luma_mode_above[16U];
    Av2hLumaModeContext luma_mode_left[16U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 16U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 16U];
    Av2hTileLumaModeCell colocated_luma[16U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        16U, 16U, 16U
    };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf luma_leaf;
    Av2hTileChromaLeaf chroma_leaf;
    Av2hFirstCoeffBlock luma_block;
    Av2hFirstCoeffBlock u_block;
    Av2hFirstCoeffBlock v_block;
    uint8_t has_leaf;
    size_t luma_count = 0U;
    size_t chroma_count = 0U;

    sequence.enable_sdp = 1U;
    sequence.enable_ext_partitions = 1U;
    sequence.enable_idtx_intra = 1U;
    sequence.enable_ist = 1U;
    sequence.enable_mrls = 1U;
    sequence.enable_cfl_intra = 1U;
    frame.width = 16U;
    frame.height = 16U;
    frame.base_q_index = 128U;
    frame.tx_mode = 1U;
    frame.cdef_strength_count = 1U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ tiny, sizeof(tiny) }, &sequence, &frame,
              &storage) == AV2H_OK);
    for (;;) {
        CHECK(av2h_tile_decoder_next_luma(
                  &decoder, &luma_leaf, &luma_block, &has_leaf) == AV2H_OK);
        if (!has_leaf) break;
        ++luma_count;
        CHECK(av2h_tile_decoder_commit_leaf(
                  &decoder, luma_leaf.node) == AV2H_OK);
    }
    CHECK(luma_count == 11U);
    CHECK(av2h_range_tell(&decoder.range) == 415U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    for (;;) {
        CHECK(av2h_tile_decoder_next_chroma(
                  &decoder, &chroma_leaf, &u_block, &v_block, &has_leaf) ==
              AV2H_OK);
        if (!has_leaf) break;
        ++chroma_count;
        CHECK(av2h_tile_decoder_commit_leaf(
                  &decoder, chroma_leaf.node) == AV2H_OK);
    }
    CHECK(chroma_count == 4U);
    CHECK(av2h_range_tell(&decoder.range) == 725U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
    CHECK(colocated_luma[0U].mode == AV2H_INTRA_DC);
    CHECK(colocated_luma[2U].mode == AV2H_INTRA_D203);
    CHECK(colocated_luma[8U].mode == AV2H_INTRA_D67);
    CHECK(colocated_luma[8U].angle_delta == -1);
}

static void test_orchestrator_failures(void) {
    static const unsigned char entropy[] = { 0U, 0U, 0U, 0U };
    uint8_t luma_partition_above[4U];
    uint8_t luma_partition_left[4U];
    uint8_t chroma_partition_above[4U];
    uint8_t chroma_partition_left[4U];
    Av2hLumaModeContext luma_mode_above[4U];
    Av2hLumaModeContext luma_mode_left[4U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 4U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 4U];
    Av2hTileLumaModeCell colocated_luma[4U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        4U, 4U, 4U
    };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    uint8_t has_leaf;

    frame.width = 16U;
    frame.height = 16U;
    storage.colocated_luma_count = 15U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence,
              &frame, &storage) == AV2H_OUT_OF_MEMORY);
    storage.colocated_luma_count = 4U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence,
              &frame, &storage) == AV2H_OUT_OF_MEMORY);

    frame.width = 4U;
    frame.height = 4U;
    sequence.enable_ext_partitions = 1U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence,
              &frame, &storage) == AV2H_OK);
    CHECK(av2h_tile_decoder_next_luma(
              &decoder, &leaf, &block, &has_leaf) == AV2H_UNSUPPORTED);
    CHECK(decoder.status == AV2H_UNSUPPORTED);
    CHECK(av2h_tile_decoder_commit_leaf(
              &decoder, (Av2hPartitionNode){ 0U, 0U, AV2H_BLOCK_4X4 }) ==
          AV2H_UNSUPPORTED);
    CHECK(av2h_tile_decoder_init(
              0, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence, &frame,
              &storage) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ 0, 0U }, &sequence, &frame, &storage) ==
          AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_fixture_offsets();
    test_failures();
    test_bounded_orchestrator();
    test_orchestrator_failures();
    if (failures != 0) {
        fprintf(stderr, "%d tile checks failed\n", failures);
        return 1;
    }
    puts("tile unit: pass");
    return 0;
}
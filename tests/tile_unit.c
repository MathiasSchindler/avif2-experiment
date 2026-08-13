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

static size_t load_fixture(const char *path,
                           unsigned char *bytes,
                           size_t capacity) {
    FILE *source = fopen(path, "rb");
    size_t size;
    if (source == 0) {
        fprintf(stderr, "failed to open fixture: %s\n", path);
        ++failures;
        return 0U;
    }
    size = fread(bytes, 1U, capacity, source);
    if (ferror(source) || !feof(source)) {
        fprintf(stderr, "failed to read fixture: %s\n", path);
        ++failures;
        size = 0U;
    }
    fclose(source);
    return size;
}

static void check_plane_crop(const char *name,
                             const uint8_t *actual,
                             size_t stride,
                             const uint8_t *expected,
                             size_t width,
                             size_t height) {
    size_t row;
    size_t column;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            uint8_t value = actual[row * stride + column];
            uint8_t oracle = expected[row * width + column];
            if (value != oracle) {
                fprintf(stderr,
                        "%s mismatch at (%zu,%zu): %u != %u\n",
                        name, row, column, (unsigned int)value,
                        (unsigned int)oracle);
                ++failures;
                return;
            }
        }
    }
}

static uint64_t plane_crop_hash(const uint8_t *actual,
                                size_t stride,
                                size_t width,
                                size_t height) {
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t row;
    size_t column;
    for (row = 0U; row < height; ++row) {
        for (column = 0U; column < width; ++column) {
            hash ^= actual[row * stride + column];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

#if 0
static uint64_t ledger_hash_value(uint64_t hash, uint64_t value) {
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        hash ^= (uint8_t)value;
        hash *= UINT64_C(1099511628211);
        value >>= 8U;
    }
    return hash;
}

static void check_readiness_map(const char *name,
                                const uint8_t *map,
                                size_t size) {
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (map[index] != 1U) {
            fprintf(stderr, "%s readiness missing at %zu\n", name, index);
            ++failures;
            return;
        }
    }
}
#endif

static void check_plane_crop_hash(const char *name,
                                  const uint8_t *actual,
                                  size_t stride,
                                  size_t width,
                                  size_t height,
                                  uint64_t expected) {
    uint64_t hash = plane_crop_hash(actual, stride, width, height);
    if (hash != expected) {
        fprintf(stderr, "%s hash mismatch: %llu != %llu\n", name,
                (unsigned long long)hash,
                (unsigned long long)expected);
        ++failures;
    }
}

#if 0
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
#endif

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

#if 0
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
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        16U, 16U, 16U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf luma_leaf;
    Av2hTileChromaLeaf chroma_leaf;
    uint8_t luma_pixels[16U * 16U];
    uint8_t luma_reconstructed[4U * 4U];
    Av2hReconstructionPlane luma_plane = {
        luma_pixels, 16U, 16U, 16U, 16U, 16U,
        luma_reconstructed, 4U, sizeof(luma_reconstructed)
    };
    Av2hReconstructionScratch reconstruction_scratch;
    Av2hFirstCoeffBlock u_block;
    Av2hFirstCoeffBlock v_block;
    Av2hStatus status;
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
    av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
    av2h_memory_fill(luma_reconstructed, 0U, sizeof(luma_reconstructed));
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ tiny, sizeof(tiny) }, &sequence, &frame,
              &storage) == AV2H_OK);
    for (;;) {
        status = av2h_tile_decoder_reconstruct_next_luma(
            &decoder, &luma_plane, &reconstruction_scratch,
            &luma_leaf, &has_leaf);
        CHECK(status == AV2H_OK);
        if (status != AV2H_OK) {
            fprintf(stderr, "tiny luma failed after %zu leaves at (%u,%u): "
                    "%u\n", luma_count, luma_leaf.node.row,
                    luma_leaf.node.column, (unsigned int)status);
            return;
        }
        if (!has_leaf) break;
        ++luma_count;
    }
    CHECK(luma_count == 11U);
    CHECK(av2h_range_tell(&decoder.range) == 415U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    CHECK(decoder.event == AV2H_TILE_EVENT_ROOT_HANDOFF);
    for (;;) {
        status = av2h_tile_decoder_next_chroma(
            &decoder, &chroma_leaf, &u_block, &v_block, &has_leaf);
        CHECK(status == AV2H_OK);
        if (status != AV2H_OK) return;
        if (!has_leaf) break;
        ++chroma_count;
        CHECK(av2h_tile_decoder_commit_leaf(
                  &decoder, chroma_leaf.node) == AV2H_OK);
    }
    CHECK(chroma_count == 4U);
    CHECK(av2h_range_tell(&decoder.range) == 725U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
    CHECK(decoder.event == AV2H_TILE_EVENT_DONE);
    CHECK(colocated_luma[0U].mode == AV2H_INTRA_DC);
    CHECK(colocated_luma[2U].mode == AV2H_INTRA_D203);
    CHECK(colocated_luma[8U].mode == AV2H_INTRA_D67);
    CHECK(colocated_luma[8U].angle_delta == -1);
}
#endif

static void test_odd_luma_reconstruction(void) {
    static const unsigned char sequence_bytes[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x43U, 0x87U, 0x0eU,
        0x1cU, 0xe6U, 0xf2U, 0x37U, 0x32U, 0x44U
    };
    static const unsigned char frame_bytes[] = {
        0xeeU, 0xc0U, 0x0cU, 0xf7U, 0xc2U, 0x89U, 0xc2U, 0x93U,
        0xcfU, 0xffU, 0xc1U, 0x00U, 0x00U, 0xffU, 0x2aU, 0x52U,
        0x67U, 0xd6U, 0x80U, 0xf3U, 0x99U, 0x9cU, 0x59U, 0x42U,
        0xfbU, 0xe6U, 0x50U, 0x15U, 0x7dU, 0x6eU, 0x92U, 0x8aU,
        0x57U, 0x4aU, 0x49U, 0xd0U, 0xb7U, 0x83U, 0xaeU, 0x2fU,
        0x1dU, 0x7fU, 0xcaU, 0x2dU, 0xd3U, 0x46U, 0xf6U, 0x12U,
        0x54U, 0xa5U, 0x0aU, 0x1fU, 0xb1U, 0x8eU, 0xe1U, 0x60U,
        0x41U, 0xa3U, 0xf1U, 0x29U, 0xb4U, 0x5bU, 0xf3U, 0xa4U,
        0x7cU, 0x8eU, 0x9aU, 0x2eU, 0x0aU, 0x44U, 0x0fU, 0x56U,
        0x21U, 0xbaU, 0x51U, 0xfdU, 0xd5U, 0x16U, 0x78U, 0x3cU,
        0xecU, 0x4bU, 0xeaU, 0x0bU, 0xcfU, 0xe1U, 0xfcU, 0xeeU,
        0xcfU, 0x27U, 0x92U, 0x47U, 0x60U, 0xa1U, 0xd5U, 0x70U,
        0x6bU, 0xddU, 0xc5U, 0x0bU, 0x25U, 0x6cU, 0x72U, 0x4dU,
        0xfbU, 0xc9U
    };
    static const uint8_t expected_y[] = {
        23,37,55,70,83,98,116,130,138,150,161,170,183,203,220,231,243,
        33,47,65,80,93,107,126,140,151,167,181,189,202,220,233,241,8,
        47,61,79,94,107,121,140,154,167,184,196,203,226,254,255,245,12,
        57,71,89,104,117,131,150,163,173,189,201,208,211,244,249,12,12,
        60,80,98,112,124,141,157,169,169,190,209,220,244,249,12,12,13,
        64,85,104,119,134,149,159,175,176,195,213,225,225,233,12,12,37,
        70,90,111,128,144,153,163,184,192,201,220,232,233,12,12,12,37,
        77,97,118,141,152,157,171,192,204,209,225,236,12,255,12,37,37,
        87,110,130,145,153,164,184,193,201,219,232,237,255,12,37,37,38,
        94,120,140,151,157,175,192,197,209,227,229,255,12,13,37,37,62,
        102,128,150,157,164,187,197,203,219,232,238,22,13,37,37,37,62,
        107,137,157,161,173,193,202,212,226,253,0,0,37,37,37,62,62,114,
        145,163,168,185,198,208,222,232,237,255,12,37,37,62,62,87,120,
        150,168,178,191,202,217,231,239,238,16,27,37,62,62,87,87,126,
        154,174,189,194,207,228,236,4,0,38,27,62,62,62,62,111
    };
    static const uint8_t expected_u[] = {
        96,110,130,144,154,169,183,192,209,
        101,115,135,148,160,179,195,204,219,
        108,121,141,155,168,190,207,216,225,
        112,126,146,160,173,195,212,222,229,
        121,138,155,169,187,209,225,230,230,
        131,151,165,178,204,219,225,230,230,
        143,160,174,191,214,219,225,230,230,
        155,169,182,204,214,219,225,230,230
    };
    static const uint8_t expected_v[] = {
        164,177,197,211,212,216,243,6,20,
        168,182,202,216,234,234,6,9,32,
        175,189,209,223,224,255,25,31,40,
        180,193,213,227,247,16,21,37,44,
        187,211,216,250,8,18,40,45,45,
        200,213,236,254,13,34,40,45,45,
        193,227,245,11,29,34,40,45,45,
        220,234,255,4,29,34,40,45,45
    };
    uint8_t luma_partition_above[6U];
    uint8_t luma_partition_left[4U];
    uint8_t chroma_partition_above[6U];
    uint8_t chroma_partition_left[4U];
    Av2hLumaModeContext luma_mode_above[6U];
    Av2hLumaModeContext luma_mode_left[4U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 6U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 4U];
    Av2hTileLumaModeCell colocated_luma[24U];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        6U, 4U, 24U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    uint8_t luma_pixels[24U * 16U];
    uint8_t u_pixels[12U * 8U];
    uint8_t v_pixels[12U * 8U];
    uint8_t luma_reconstructed[6U * 4U];
    uint8_t u_reconstructed[3U * 2U];
    uint8_t v_reconstructed[3U * 2U];
    Av2hReconstructionPlane luma_plane = {
        luma_pixels, 24U, 24U, 16U, 17U, 15U,
        luma_reconstructed, 6U, sizeof(luma_reconstructed)
    };
    Av2hReconstructionPlane u_plane = {
        u_pixels, 12U, 12U, 8U, 9U, 8U,
        u_reconstructed, 3U, sizeof(u_reconstructed)
    };
    Av2hReconstructionPlane v_plane = {
        v_pixels, 12U, 12U, 8U, 9U, 8U,
        v_reconstructed, 3U, sizeof(v_reconstructed)
    };
    Av2hReconstructionScratch reconstruction_scratch;
    Av2hObu obu;
    Av2hObuLedger ledger = { &obu, 1U, 0U };
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hTileChromaLeaf chroma_leaf;
    Av2hFirstCoeffBlock block;
    Av2hError error;
    Av2hStatus status;
    uint8_t has_leaf = 0U;
    size_t leaf_count = 0U;
    size_t chroma_count = 0U;
    CHECK(av2h_obu_parse(
              (Av2hSpan){ sequence_bytes, sizeof(sequence_bytes) },
              &ledger, &error) == AV2H_OK);
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ sequence_bytes, sizeof(sequence_bytes) }, &obu,
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ frame_bytes, sizeof(frame_bytes) }, &sequence,
              &frame) == AV2H_OK);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ frame_bytes, sizeof(frame_bytes) },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
    av2h_memory_fill(u_pixels, 0U, sizeof(u_pixels));
    av2h_memory_fill(v_pixels, 0U, sizeof(v_pixels));
    av2h_memory_fill(luma_reconstructed, 0U, sizeof(luma_reconstructed));
    av2h_memory_fill(u_reconstructed, 0U, sizeof(u_reconstructed));
    av2h_memory_fill(v_reconstructed, 0U, sizeof(v_reconstructed));
        CHECK(av2h_tile_decoder_init(
                  &decoder, group.entropy, &sequence, &frame, &storage) ==
              AV2H_OK);
    decoder.current_q_index = 400U;
    CHECK(av2h_tile_decoder_next_luma(
              &decoder, &leaf, &block, &has_leaf) == AV2H_OK);
    CHECK(has_leaf != 0U);
    CHECK(leaf.prefix.q_index == 400U);
    CHECK(av2h_tile_decoder_init(
              &decoder, group.entropy, &sequence, &frame, &storage) ==
          AV2H_OK);
    has_leaf = 0U;
    do {
        if (leaf_count == 10U) {
            fprintf(stderr,
                    "odd ymode cdf set=%u,%u,%u idx=%u,%u,%u,%u,%u "
                    "range=%u value=%u available=%lld tell=%zu\n",
                    (unsigned int)decoder.leaf_cdfs.y_mode_set[0],
                    (unsigned int)decoder.leaf_cdfs.y_mode_set[1],
                    (unsigned int)decoder.leaf_cdfs.y_mode_set[2],
                    (unsigned int)decoder.leaf_cdfs.y_mode_index[1][0],
                    (unsigned int)decoder.leaf_cdfs.y_mode_index[1][1],
                    (unsigned int)decoder.leaf_cdfs.y_mode_index[1][2],
                    (unsigned int)decoder.leaf_cdfs.y_mode_index[1][3],
                    (unsigned int)decoder.leaf_cdfs.y_mode_index[1][4],
                    (unsigned int)decoder.range.range,
                    (unsigned int)decoder.range.value,
                    (long long)decoder.range.available_bits,
                    av2h_range_tell(&decoder.range));
        }
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        if (status != AV2H_OK) {
            fprintf(stderr,
                    "odd mode raw set/index/joint/fsc/context/block="
                    "%u/%u/%u/%u/%u/%u\n",
                    (unsigned int)leaf.mode.mode_set,
                    (unsigned int)leaf.mode.mode_index,
                    (unsigned int)leaf.mode.joint_mode,
                    (unsigned int)leaf.mode.fsc_mode,
                    (unsigned int)leaf.mode.fsc_neighbor_context,
                    (unsigned int)leaf.mode.fsc_block_context);
            fprintf(stderr,
                    "odd luma syntax failed after %zu leaves at (%u,%u), "
                    "block %u, "
                    "mode %u angle %d mrl %u/%u fsc %u intrabc %u "
                    "mode/drl/prec/max %u/%u/%u/%u dv %d,%d, "
                    "stages %zu/%zu/%zu/%zu/%zu, tx %u/%u/%u "
                    "secondary %u/%u eob %u zero %u coeff %u/%u/%u, "
                    "eob-point/extra %u/%u tell %zu: %u\n",
                    leaf_count, leaf.node.row, leaf.node.column,
                    (unsigned int)leaf.node.block_size,
                    (unsigned int)leaf.mode.mode,
                    (int)leaf.mode.angle_delta,
                    (unsigned int)leaf.mode.mrl_index,
                    (unsigned int)leaf.mode.multi_line_mrl,
                    (unsigned int)leaf.mode.fsc_mode,
                    (unsigned int)leaf.mode.use_intrabc,
                    (unsigned int)leaf.mode.intrabc_mode,
                    (unsigned int)leaf.mode.intrabc_drl_index,
                    (unsigned int)leaf.mode.mv_precision,
                    (unsigned int)frame.max_bvp_drl_bits,
                    (int)leaf.mode.displacement_x,
                    (int)leaf.mode.displacement_y,
                    leaf.mode.before_mode_bits,
                    leaf.mode.after_mode_set_bits,
                    leaf.mode.after_mode_index_bits,
                    leaf.mode.after_fsc_bits,
                    leaf.mode.after_mrl_bits,
                    (unsigned int)leaf.prefix.tx_size,
                    (unsigned int)leaf.transform.primary_type,
                    (unsigned int)leaf.transform.primary_symbol,
                    (unsigned int)leaf.transform.secondary_signaled,
                    (unsigned int)leaf.transform.secondary_symbol,
                    (unsigned int)leaf.prefix.eob,
                    (unsigned int)leaf.prefix.all_zero,
                    (unsigned int)block.diagnostic_position,
                    (unsigned int)block.diagnostic_context,
                    (unsigned int)block.diagnostic_kind,
                    (unsigned int)leaf.prefix.eob_point,
                    (unsigned int)leaf.prefix.eob_extra,
                    av2h_range_tell(&decoder.range), (unsigned int)status);
            break;
        }
        if (has_leaf) {
            size_t coefficient_index;
            fprintf(stderr,
                    "odd leaf %zu (%u,%u) block %u mode %u tx %u/%u/%u "
                    "eob %u zero %u fsc %u/%u/%u stages %zu/%zu/%zu/%zu "
                    "range %u value %u available %lld offset %zu tell %zu\n",
                    leaf_count, leaf.node.row, leaf.node.column,
                    (unsigned int)leaf.node.block_size,
                    (unsigned int)leaf.mode.mode,
                    (unsigned int)leaf.prefix.tx_size,
                    (unsigned int)leaf.transform.primary_type,
                    (unsigned int)leaf.transform.primary_symbol,
                    (unsigned int)leaf.prefix.eob,
                    (unsigned int)leaf.prefix.all_zero,
                    (unsigned int)leaf.mode.fsc_mode,
                    (unsigned int)leaf.mode.fsc_neighbor_context,
                    (unsigned int)leaf.mode.fsc_block_context,
                    leaf.mode.before_mode_bits,
                    leaf.mode.after_mode_set_bits,
                    leaf.mode.after_mode_index_bits,
                    leaf.mode.after_fsc_bits,
                    (unsigned int)decoder.range.range,
                    (unsigned int)decoder.range.value,
                    (long long)decoder.range.available_bits,
                    decoder.range.bit_position,
                    av2h_range_tell(&decoder.range));
            if (leaf.node.row == 3U && leaf.node.column == 2U) {
                fprintf(stderr,
                        "odd levels range=%u value=%u tell=%zu\n",
                        (unsigned int)block.after_levels_range,
                        (unsigned int)block.after_levels_value,
                        block.after_levels_bits);
                for (coefficient_index = 0U;
                     coefficient_index < block.coefficient_count;
                     ++coefficient_index) {
                    if (block.quantized[coefficient_index] != 0) {
                        fprintf(stderr, "odd coeff pos=%zu value=%d\n",
                                coefficient_index,
                                (int)block.quantized[coefficient_index]);
                    }
                }
            }
            status = av2h_luma_reconstruct_to_plane(
                &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
                &block, &luma_plane, &reconstruction_scratch);
            if (status != AV2H_OK) {
                fprintf(stderr,
                        "odd luma reconstruction failed after %zu leaves "
                        "at (%u,%u): %u\n",
                        leaf_count, leaf.node.row, leaf.node.column,
                        (unsigned int)status);
                break;
            }
            CHECK(av2h_tile_decoder_commit_leaf(
                      &decoder, leaf.node) == AV2H_OK);
            ++leaf_count;
        }
    } while (has_leaf);
    CHECK(status == AV2H_OK);
    CHECK(has_leaf == 0U);
    CHECK(leaf_count == 12U);
    CHECK(leaf.mode.use_intrabc == 0U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    {
        Av2hRangeDecoder range_probe = decoder.range;
        Av2hPartitionCdfs cdf_probe = decoder.partition_cdfs;
        uint32_t symbol;
        symbol = av2h_range_read_adaptive(
            &range_probe, cdf_probe.do_split[1][8], 2U);
        fprintf(stderr,
                "odd chroma replay split32 value=%u range=%u value=%u "
                "available=%lld tell=%zu cdf=%u\n",
                (unsigned int)symbol, (unsigned int)range_probe.range,
                (unsigned int)range_probe.value,
                (long long)range_probe.available_bits,
                av2h_range_tell(&range_probe),
                (unsigned int)cdf_probe.do_split[1][8][0]);
        symbol = av2h_range_read_adaptive(
            &range_probe, cdf_probe.rect_type[1][8], 2U);
        fprintf(stderr,
                "odd chroma replay rect32 value=%u range=%u value=%u "
                "available=%lld tell=%zu cdf=%u\n",
                (unsigned int)symbol, (unsigned int)range_probe.range,
                (unsigned int)range_probe.value,
                (long long)range_probe.available_bits,
                av2h_range_tell(&range_probe),
                (unsigned int)cdf_probe.rect_type[1][8][0]);
        symbol = av2h_range_read_adaptive(
            &range_probe, cdf_probe.do_ext_partition[1][8], 2U);
        fprintf(stderr,
                "odd chroma replay ext32 value=%u range=%u value=%u "
                "available=%lld tell=%zu cdf=%u\n",
                (unsigned int)symbol, (unsigned int)range_probe.range,
                (unsigned int)range_probe.value,
                (long long)range_probe.available_bits,
                av2h_range_tell(&range_probe),
                (unsigned int)cdf_probe.do_ext_partition[1][8][0]);
        symbol = av2h_range_read_adaptive(
            &range_probe, cdf_probe.do_split[1][4], 2U);
        fprintf(stderr,
                "odd chroma replay split16 value=%u range=%u value=%u "
                "available=%lld tell=%zu cdf=%u\n",
                (unsigned int)symbol, (unsigned int)range_probe.range,
                (unsigned int)range_probe.value,
                (long long)range_probe.available_bits,
                av2h_range_tell(&range_probe),
                (unsigned int)cdf_probe.do_split[1][4][0]);
    }
    {
        Av2hTileDecoder probe = decoder;
        Av2hFirstLeafPath path;
        size_t decision_index;
        CHECK(av2h_partition_traversal_next(
                  &probe.chroma_traversal, &probe.range,
                  &probe.partition_cdfs, &probe.partition_contexts,
                  &path, &has_leaf) == AV2H_OK);
        for (decision_index = 0U; decision_index < path.decision_count;
             ++decision_index) {
            fprintf(stderr,
                    "odd chroma partition %zu block=%u part=%u forced=%u "
                    "extctx=%zu tell=%zu\n",
                    decision_index,
                    (unsigned int)path.decisions[decision_index].node.block_size,
                    (unsigned int)path.decisions[decision_index].partition,
                    (unsigned int)path.decisions[decision_index].forced,
                    path.decisions[decision_index].extension_context,
                    path.decisions[decision_index].after_bits);
        }
    }
    for (;;) {
        status = av2h_tile_decoder_reconstruct_next_chroma(
            &decoder, &u_plane, &v_plane, &reconstruction_scratch,
            &chroma_leaf, &has_leaf);
        if (status != AV2H_OK) {
            fprintf(stderr,
                    "odd chroma failed after %zu leaves at (%u,%u), "
                    "block %u mode %u angle %d cfl/index %u/%u "
                    "u %u/%u/%u v %u/%u/%u tell %zu: %u\n",
                    chroma_count, chroma_leaf.node.row,
                    chroma_leaf.node.column,
                    (unsigned int)chroma_leaf.node.block_size,
                    (unsigned int)chroma_leaf.mode.mode,
                    (int)chroma_leaf.mode.angle_delta,
                    (unsigned int)chroma_leaf.mode.is_cfl,
                    (unsigned int)chroma_leaf.mode.mode_index,
                    (unsigned int)chroma_leaf.prefix[0].tx_size,
                    (unsigned int)chroma_leaf.transform[0].primary_type,
                    (unsigned int)chroma_leaf.prefix[0].eob,
                    (unsigned int)chroma_leaf.prefix[1].tx_size,
                    (unsigned int)chroma_leaf.transform[1].primary_type,
                    (unsigned int)chroma_leaf.prefix[1].eob,
                    av2h_range_tell(&decoder.range), (unsigned int)status);
            break;
        }
        if (!has_leaf) break;
        fprintf(stderr,
                "odd chroma leaf %zu (%u,%u) block %u mode %u "
                "u %u/%u/%u v %u/%u/%u tell %zu\n",
                chroma_count, chroma_leaf.node.row, chroma_leaf.node.column,
                (unsigned int)chroma_leaf.node.block_size,
                (unsigned int)chroma_leaf.mode.mode,
                (unsigned int)chroma_leaf.prefix[0].tx_size,
                (unsigned int)chroma_leaf.transform[0].primary_type,
                (unsigned int)chroma_leaf.prefix[0].eob,
                (unsigned int)chroma_leaf.prefix[1].tx_size,
                (unsigned int)chroma_leaf.transform[1].primary_type,
                (unsigned int)chroma_leaf.prefix[1].eob,
                av2h_range_tell(&decoder.range));
        ++chroma_count;
    }
    CHECK(status == AV2H_OK);
    CHECK(has_leaf == 0U);
    CHECK(chroma_count == 5U);
    CHECK(av2h_range_tell(&decoder.range) == 696U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
    check_plane_crop("odd Y", luma_pixels, 24U, expected_y, 17U, 15U);
    check_plane_crop("odd U", u_pixels, 12U, expected_u, 9U, 8U);
    check_plane_crop("odd V", v_pixels, 12U, expected_v, 9U, 8U);
}

static void test_ordinary_luma_frontier(void) {
    static const uint8_t seventh_nonzero[10U] = {
        30U, 31U, 45U, 46U, 47U, 59U, 60U, 61U, 62U, 63U
    };
    unsigned char stream[1024U];
    size_t stream_size;
    Av2hObu entries[2U];
    Av2hObuLedger ledger = { entries, 2U, 0U };
    Av2hError error;
    Av2hTileGroup group;
    uint8_t luma_partition_above[40U];
    uint8_t luma_partition_left[30U];
    uint8_t chroma_partition_above[40U];
    uint8_t chroma_partition_left[30U];
    Av2hLumaModeContext luma_mode_above[40U];
    Av2hLumaModeContext luma_mode_left[30U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 40U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 30U];
    Av2hTileLumaModeCell colocated_luma[40U * 30U];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        40U, 30U, 40U * 30U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    uint8_t luma_pixels[160U * 128U];
    uint8_t luma_reconstructed[40U * 32U];
    Av2hReconstructionPlane luma_plane = {
        luma_pixels, 160U, 160U, 128U, 160U, 120U,
        luma_reconstructed, 40U, sizeof(luma_reconstructed)
    };
    uint8_t u_pixels[80U * 64U];
    uint8_t v_pixels[80U * 64U];
    uint8_t u_reconstructed[20U * 16U];
    uint8_t v_reconstructed[20U * 16U];
    Av2hReconstructionPlane u_plane = {
        u_pixels, 80U, 80U, 64U, 80U, 60U,
        u_reconstructed, 20U, sizeof(u_reconstructed)
    };
    Av2hReconstructionPlane v_plane = {
        v_pixels, 80U, 80U, 64U, 80U, 60U,
        v_reconstructed, 20U, sizeof(v_reconstructed)
    };
    Av2hReconstructionScratch scratch;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf leaf;
    Av2hFirstCoeffBlock block;
    Av2hStatus status;
    Av2hStatus reconstruction_status = AV2H_OK;
    uint8_t has_leaf = 0U;
    size_t leaf_count = 0U;

    stream_size = load_fixture("tests/fixtures/ordinary-160x120.obu",
                               stream, sizeof(stream));
    CHECK(stream_size != 0U);
    CHECK(av2h_obu_parse((Av2hSpan){ stream, stream_size }, &ledger,
                         &error) == AV2H_OK);
    CHECK(ledger.count == 2U);
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ stream, stream_size }, &entries[0],
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ stream + entries[1].payload_offset,
                           entries[1].payload_size },
              &sequence, &frame) == AV2H_OK);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ stream + entries[1].payload_offset,
                           entries[1].payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
    av2h_memory_fill(luma_reconstructed, 0U, sizeof(luma_reconstructed));
    CHECK(av2h_tile_decoder_init(
              &decoder, group.entropy, &sequence,
              &frame, &storage) == AV2H_OK);
    for (;;) {
        av2h_memory_fill(&leaf, 0U, sizeof(leaf));
        av2h_memory_fill(&block, 0U, sizeof(block));
        status = av2h_tile_decoder_next_luma(
            &decoder, &leaf, &block, &has_leaf);
        if (status != AV2H_OK || !has_leaf) break;
        if (leaf_count == 5U) {
            CHECK(leaf.node.row == 10U);
            CHECK(leaf.node.column == 0U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X4);
            CHECK(leaf.mode.mode == AV2H_INTRA_D45);
            CHECK(leaf.mode.angle_delta == -1);
            CHECK(leaf.prefix.tx_size == AV2H_TX_16X4);
            CHECK(leaf.prefix.eob == 1U);
            CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(block.nonzero_count == 1U);
            CHECK(block.dequantized[0] == 5096);
            CHECK(leaf.prefix.after_eob_bits == 256U);
            CHECK(block.after_levels_bits == 265U);
            CHECK(block.after_coefficients_bits == 276U);
            CHECK(av2h_range_tell(&decoder.range) == 276U);
        } else if (leaf_count == 6U) {
            size_t coefficient_index;
            size_t nonzero_index = 0U;
            CHECK(leaf.node.row == 11U);
            CHECK(leaf.node.column == 0U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X4);
            CHECK(leaf.mode.mode == AV2H_INTRA_D45);
            CHECK(leaf.mode.angle_delta == -1);
            CHECK(leaf.mode.fsc_mode == 1U);
            CHECK(leaf.prefix.tx_size == AV2H_TX_16X4);
            CHECK(leaf.prefix.eob == 14U);
            CHECK(leaf.transform.primary_type == AV2H_TX_IDTX);
            CHECK(block.nonzero_count == 10U);
            CHECK(block.cul_level == 7U);
            CHECK(leaf.prefix.after_all_zero_bits == 285U);
            CHECK(leaf.prefix.after_eob_point_bits == 288U);
            CHECK(leaf.prefix.after_eob_bits == 291U);
            CHECK(block.before_levels_bits == 291U);
            CHECK(block.after_levels_bits == 322U);
            CHECK(block.after_coefficients_bits == 365U);
            CHECK(av2h_range_tell(&decoder.range) == 365U);
            for (coefficient_index = 0U; coefficient_index < 64U;
                 ++coefficient_index) {
                if (nonzero_index < sizeof(seventh_nonzero) &&
                    coefficient_index ==
                        seventh_nonzero[nonzero_index]) {
                    CHECK(block.quantized[coefficient_index] == -10);
                    CHECK(block.dequantized[coefficient_index] == -1960);
                    ++nonzero_index;
                } else {
                    CHECK(block.quantized[coefficient_index] == 0);
                    CHECK(block.dequantized[coefficient_index] == 0);
                }
            }
            CHECK(nonzero_index == sizeof(seventh_nonzero));
        } else if (leaf_count == 7U) {
            size_t coefficient_index;
            CHECK(leaf.node.row == 10U);
            CHECK(leaf.node.column == 4U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_4X4);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.fsc_mode == 0U);
            CHECK(leaf.mode.displacement_x == -32);
            CHECK(leaf.mode.displacement_y == 24);
            CHECK(leaf.prefix.tx_size == AV2H_TX_4X4);
            CHECK(leaf.prefix.eob == 5U);
            CHECK(leaf.transform.primary_type == AV2H_TX_IDTX);
            CHECK(leaf.transform.primary_long_side_symbol == 0U);
            CHECK(leaf.transform.primary_symbol == 0U);
            CHECK(leaf.transform.before_primary_bits == 396U);
            CHECK(leaf.transform.after_primary_bits == 402U);
            CHECK(block.before_levels_bits == 402U);
            CHECK(block.after_levels_bits == 409U);
            CHECK(block.after_coefficients_bits == 414U);
            CHECK(block.nonzero_count == 1U);
            CHECK(block.cul_level == 7U);
            for (coefficient_index = 0U; coefficient_index < 16U;
                 ++coefficient_index) {
                CHECK(block.quantized[coefficient_index] ==
                      (coefficient_index == 10U ? 10 : 0));
                CHECK(block.dequantized[coefficient_index] ==
                      (coefficient_index == 10U ? 1960 : 0));
            }
        } else if (leaf_count == 8U) {
            size_t coefficient_index;
            CHECK(leaf.node.row == 10U);
            CHECK(leaf.node.column == 5U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_4X4);
            CHECK(leaf.mode.mode == AV2H_INTRA_D203);
            CHECK(leaf.mode.angle_delta == 3);
            CHECK(leaf.mode.fsc_mode == 1U);
            CHECK(leaf.prefix.tx_size == AV2H_TX_4X4);
            CHECK(leaf.prefix.eob == 15U);
            CHECK(leaf.transform.primary_type == AV2H_TX_IDTX);
            CHECK(leaf.prefix.after_eob_bits == 432U);
            CHECK(block.before_levels_bits == 432U);
            CHECK(block.after_levels_bits == 453U);
            CHECK(block.after_coefficients_bits == 457U);
            CHECK(block.nonzero_count == 3U);
            CHECK(block.cul_level == 7U);
            for (coefficient_index = 0U; coefficient_index < 16U;
                 ++coefficient_index) {
                int32_t quantized =
                    coefficient_index == 1U ? 2 :
                    coefficient_index == 2U ? -2 :
                    coefficient_index == 4U ? -4 : 0;
                int32_t dequantized = quantized * 196;
                CHECK(block.quantized[coefficient_index] == quantized);
                CHECK(block.dequantized[coefficient_index] == dequantized);
            }
        } else if (leaf_count == 9U) {
            size_t coefficient_index;
            CHECK(leaf.node.row == 11U);
            CHECK(leaf.node.column == 4U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X4);
            CHECK(leaf.mode.mode == AV2H_INTRA_D203);
            CHECK(leaf.mode.angle_delta == 3);
            CHECK(leaf.mode.fsc_mode == 0U);
            CHECK(leaf.mode.mrl_index == 0U);
            CHECK(leaf.prefix.tx_size == AV2H_TX_8X4);
            CHECK(leaf.prefix.eob == 3U);
            CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf.transform.primary_signaled == 1U);
            CHECK(leaf.transform.primary_symbol == 0U);
            CHECK(leaf.transform.secondary_signaled == 1U);
            CHECK(leaf.transform.secondary_symbol == 0U);
            CHECK(leaf.transform.before_primary_bits == 471U);
            CHECK(leaf.transform.after_primary_bits == 474U);
            CHECK(leaf.transform.after_secondary_bits == 477U);
            CHECK(block.before_levels_bits == 477U);
            CHECK(block.after_levels_bits == 479U);
            CHECK(block.after_coefficients_bits == 482U);
            CHECK(block.nonzero_count == 3U);
            CHECK(block.cul_level == 19U);
            CHECK(block.dc_value == 1);
            for (coefficient_index = 0U; coefficient_index < 32U;
                 ++coefficient_index) {
                int32_t quantized =
                    coefficient_index == 0U ? 1 :
                    coefficient_index == 1U ||
                    coefficient_index == 8U ? -1 : 0;
                CHECK(block.quantized[coefficient_index] == quantized);
                CHECK(block.dequantized[coefficient_index] ==
                      quantized * 196);
            }
        } else if (leaf_count == 10U) {
            size_t coefficient_index;
            CHECK(leaf.node.row == 10U);
            CHECK(leaf.node.column == 6U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X8);
            CHECK(leaf.mode.mode == AV2H_INTRA_D203);
            CHECK(leaf.mode.angle_delta == 3);
            CHECK(leaf.mode.fsc_mode == 0U);
            CHECK(leaf.mode.mrl_index == 1U);
            CHECK(leaf.mode.multi_line_mrl == 0U);
            CHECK(leaf.mode.before_mode_bits == 483U);
            CHECK(leaf.mode.after_mode_set_bits == 483U);
            CHECK(leaf.mode.after_mode_index_bits == 486U);
            CHECK(leaf.mode.after_fsc_bits == 486U);
            CHECK(leaf.mode.after_mrl_bits == 491U);
            CHECK(leaf.prefix.tx_size == AV2H_TX_8X8);
            CHECK(leaf.prefix.eob == 2U);
            CHECK(leaf.prefix.after_all_zero_bits == 491U);
            CHECK(leaf.prefix.after_eob_point_bits == 497U);
            CHECK(leaf.prefix.after_eob_bits == 497U);
            CHECK(leaf.transform.primary_type == AV2H_TX_ADST_ADST);
            CHECK(leaf.transform.primary_signaled == 1U);
            CHECK(leaf.transform.primary_symbol == 1U);
            CHECK(leaf.transform.secondary_signaled == 1U);
            CHECK(leaf.transform.secondary_symbol == 0U);
            CHECK(leaf.transform.before_primary_bits == 497U);
            CHECK(leaf.transform.after_primary_bits == 499U);
            CHECK(leaf.transform.after_secondary_bits == 502U);
            CHECK(block.before_levels_bits == 502U);
            CHECK(block.after_levels_bits == 504U);
            CHECK(block.after_coefficients_bits == 505U);
            CHECK(block.nonzero_count == 2U);
            CHECK(block.dc_value == 2);
            for (coefficient_index = 0U; coefficient_index < 64U;
                 ++coefficient_index) {
                int32_t quantized =
                    coefficient_index == 0U ? 2 :
                    coefficient_index == 8U ? -1 : 0;
                CHECK(block.quantized[coefficient_index] == quantized);
                CHECK(block.dequantized[coefficient_index] ==
                      quantized * 196);
            }
        } else if (leaf_count == 11U) {
            size_t coefficient_index;
            CHECK(leaf.node.row == 12U);
            CHECK(leaf.node.column == 0U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X8);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.intrabc_mode == 0U);
            CHECK(leaf.mode.skip_transform == 1U);
            CHECK(leaf.mode.intrabc_drl_index == 3U);
            CHECK(leaf.mode.mv_precision == 3U);
            CHECK(leaf.mode.displacement_x == 96);
            CHECK(leaf.mode.displacement_y == -64);
            CHECK(leaf.mode.before_mode_bits == 516U);
            CHECK(leaf.mode.after_mode_set_bits == 516U);
            CHECK(leaf.mode.after_mode_index_bits == 519U);
            CHECK(leaf.mode.after_fsc_bits == 520U);
            CHECK(leaf.mode.after_mrl_bits == 520U);
            CHECK(leaf.prefix.tx_size == AV2H_TX_16X8);
            CHECK(leaf.prefix.tx_size_context == 2U);
            CHECK(leaf.prefix.all_zero == 1U);
            CHECK(leaf.prefix.eob == 0U);
            CHECK(leaf.prefix.before_all_zero_bits == 529U);
            CHECK(leaf.prefix.after_eob_bits == 529U);
            CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(leaf.transform.primary_signaled == 0U);
            CHECK(leaf.transform.secondary_signaled == 0U);
            CHECK(block.width == 16U);
            CHECK(block.height == 8U);
            CHECK(block.coefficient_count == 128U);
            CHECK(block.nonzero_count == 0U);
            CHECK(block.cul_level == 0U);
            CHECK(block.before_levels_bits == 529U);
            CHECK(block.after_levels_bits == 529U);
            CHECK(block.after_coefficients_bits == 529U);
            CHECK(av2h_range_tell(&decoder.range) == 529U);
            for (coefficient_index = 0U;
                 coefficient_index < block.coefficient_count;
                 ++coefficient_index) {
                CHECK(block.quantized[coefficient_index] == 0);
                CHECK(block.dequantized[coefficient_index] == 0);
            }
        } else if (leaf_count == 12U) {
            CHECK(leaf.node.row == 12U && leaf.node.column == 4U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X8);
            CHECK(leaf.mode.mode == AV2H_INTRA_SMOOTH);
            CHECK(leaf.prefix.tx_size == AV2H_TX_16X8);
            CHECK(leaf.prefix.eob == 3U);
            CHECK(leaf.transform.primary_type == AV2H_TX_ADST_ADST);
            CHECK(block.quantized[0] == 5);
            CHECK(block.quantized[1] == -1);
            CHECK(block.nonzero_count == 2U);
            CHECK(block.after_coefficients_bits == 551U);
        } else if (leaf_count == 13U) {
            CHECK(leaf.node.row == 14U && leaf.node.column == 0U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_32X8);
            CHECK(leaf.mode.mode == AV2H_INTRA_SMOOTH_HORIZONTAL);
            CHECK(leaf.prefix.tx_size == AV2H_TX_32X8);
            CHECK(leaf.prefix.eob == 3U);
            CHECK(leaf.transform.primary_type == AV2H_TX_ADST_DCT);
            CHECK(block.quantized[0] == 9);
            CHECK(block.quantized[1] == -1);
            CHECK(block.quantized[32] == -1);
            CHECK(block.nonzero_count == 3U);
            CHECK(block.after_coefficients_bits == 576U);
        } else if (leaf_count == 14U) {
            CHECK(leaf.node.row == 8U && leaf.node.column == 8U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X32);
            CHECK(leaf.mode.mode == AV2H_INTRA_PAETH);
            CHECK(leaf.prefix.tx_size == AV2H_TX_8X32);
            CHECK(leaf.prefix.eob == 11U);
            CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(block.quantized[0] == 7);
            CHECK(block.quantized[1] == -3);
            CHECK(block.quantized[24] == -1);
            CHECK(block.quantized[32] == -1);
            CHECK(block.nonzero_count == 4U);
            CHECK(block.after_coefficients_bits == 616U);
        } else if (leaf_count == 15U) {
            CHECK(leaf.node.row == 8U && leaf.node.column == 10U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X32);
            CHECK(leaf.mode.mode == AV2H_INTRA_HORIZONTAL);
            CHECK(leaf.prefix.tx_size == AV2H_TX_8X32);
            CHECK(leaf.prefix.eob == 3U);
            CHECK(leaf.transform.primary_type == AV2H_TX_DCT_DCT);
            CHECK(block.quantized[0] == 19);
            CHECK(block.quantized[1] == -3);
            CHECK(block.nonzero_count == 2U);
            CHECK(block.after_coefficients_bits == 647U);
        } else if (leaf_count == 16U) {
            CHECK(leaf.node.row == 8U && leaf.node.column == 12U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X4);
            CHECK(leaf.mode.mode == AV2H_INTRA_D45);
            CHECK(leaf.prefix.tx_size == AV2H_TX_8X4);
            CHECK(leaf.prefix.all_zero == 1U);
            CHECK(block.nonzero_count == 0U);
            CHECK(block.after_coefficients_bits == 671U);
        } else if (leaf_count == 17U) {
            CHECK(leaf.node.row == 9U && leaf.node.column == 12U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X4);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.skip_transform == 1U);
            CHECK(leaf.mode.intrabc_drl_index == 0U);
            CHECK(leaf.mode.displacement_x == -384);
            CHECK(leaf.mode.displacement_y == 104);
            CHECK(block.after_coefficients_bits == 696U);
        } else if (leaf_count == 18U) {
            CHECK(leaf.node.row == 8U && leaf.node.column == 14U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X8);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.skip_transform == 1U);
            CHECK(leaf.mode.displacement_x == -384);
            CHECK(leaf.mode.displacement_y == 104);
            CHECK(block.after_coefficients_bits == 703U);
        } else if (leaf_count == 19U) {
            CHECK(leaf.node.row == 10U && leaf.node.column == 12U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X16);
            CHECK(leaf.mode.mode == AV2H_INTRA_D203);
            CHECK(leaf.mode.angle_delta == 3);
            CHECK(leaf.mode.mrl_index == 1U);
            CHECK(leaf.prefix.all_zero == 1U);
            CHECK(block.after_coefficients_bits == 723U);
        } else if (leaf_count == 20U) {
            CHECK(leaf.node.row == 10U && leaf.node.column == 14U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_8X16);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.skip_transform == 1U);
            CHECK(leaf.mode.displacement_x == -448);
            CHECK(leaf.mode.displacement_y == -128);
            CHECK(block.after_coefficients_bits == 744U);
        } else if (leaf_count == 21U) {
            CHECK(leaf.node.row == 14U && leaf.node.column == 12U);
            CHECK(leaf.node.block_size == AV2H_BLOCK_16X8);
            CHECK(leaf.mode.use_intrabc == 1U);
            CHECK(leaf.mode.skip_transform == 1U);
            CHECK(leaf.mode.displacement_x == -352);
            CHECK(leaf.mode.displacement_y == -192);
            CHECK(block.after_coefficients_bits == 762U);
        }
        reconstruction_status = av2h_luma_reconstruct_to_plane(
            &leaf.node, &leaf.mode, &leaf.prefix, &leaf.transform,
            &block, &luma_plane, &scratch);
        if (reconstruction_status != AV2H_OK) break;
        CHECK(av2h_tile_decoder_commit_leaf(
                  &decoder, leaf.node) == AV2H_OK);
        ++leaf_count;
    }
    CHECK(status == AV2H_OK);
    CHECK(reconstruction_status == AV2H_OK);
    CHECK(has_leaf == 0U);
    CHECK(leaf_count == 22U);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    CHECK(decoder.luma_traversal.diagnostic_node.row == 14U);
    CHECK(decoder.luma_traversal.diagnostic_node.column == 12U);
    CHECK(decoder.luma_traversal.diagnostic_node.block_size ==
          AV2H_BLOCK_16X8);
    CHECK(decoder.luma_traversal.diagnostic_before_bits == 744U);
    CHECK(av2h_range_tell(&decoder.range) == 762U);
    {
        Av2hTileChromaLeaf chroma_leaf;
        uint8_t has_chroma_leaf = 0U;

        av2h_memory_fill(u_pixels, 0U, sizeof(u_pixels));
        av2h_memory_fill(v_pixels, 0U, sizeof(v_pixels));
        av2h_memory_fill(u_reconstructed, 0U, sizeof(u_reconstructed));
        av2h_memory_fill(v_reconstructed, 0U, sizeof(v_reconstructed));
        while (status == AV2H_OK &&
               decoder.phase != AV2H_TILE_DECODE_DONE) {
            if (decoder.phase == AV2H_TILE_DECODE_LUMA) {
                av2h_memory_fill(&leaf, 0U, sizeof(leaf));
                status = av2h_tile_decoder_reconstruct_next_luma(
                    &decoder, &luma_plane, &scratch, &leaf, &has_leaf);
            } else {
                av2h_memory_fill(&chroma_leaf, 0U, sizeof(chroma_leaf));
                status = av2h_tile_decoder_reconstruct_next_chroma(
                    &decoder, &u_plane, &v_plane, &scratch,
                    &chroma_leaf, &has_chroma_leaf);
            }
        }
        CHECK(status == AV2H_OK);
        CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
        CHECK(av2h_range_tell(&decoder.range) == 6498U);
        check_plane_crop_hash("ordinary Y", luma_pixels, 160U, 160U, 120U,
                              UINT64_C(7945904097577017502));
        check_plane_crop_hash("ordinary U", u_pixels, 80U, 80U, 60U,
                              UINT64_C(4124092689509881817));
        check_plane_crop_hash("ordinary V", v_pixels, 80U, 80U, 60U,
                              UINT64_C(6418349723304023184));
    }
}

#if 0
static void test_high_detail_frame(void) {
    unsigned char stream[8192U];
    size_t stream_size;
    Av2hObu entries[3U];
    Av2hObuLedger ledger = { entries, 3U, 0U };
    Av2hError error;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileGroup group;
    uint8_t luma_partition_above[24U];
    uint8_t luma_partition_left[20U];
    uint8_t chroma_partition_above[24U];
    uint8_t chroma_partition_left[20U];
    Av2hLumaModeContext luma_mode_above[24U];
    Av2hLumaModeContext luma_mode_left[20U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 24U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 20U];
    Av2hTileLumaModeCell colocated_luma[24U * 20U];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        24U, 20U, 24U * 20U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    uint8_t luma_pixels[96U * 80U];
    uint8_t u_pixels[48U * 40U];
    uint8_t v_pixels[48U * 40U];
    uint8_t luma_reconstructed[24U * 20U];
    uint8_t u_reconstructed[12U * 10U];
    uint8_t v_reconstructed[12U * 10U];
    Av2hReconstructionPlane luma_plane = {
        luma_pixels, 96U, 96U, 80U, 96U, 80U,
        luma_reconstructed, 24U, sizeof(luma_reconstructed)
    };
    Av2hReconstructionPlane u_plane = {
        u_pixels, 48U, 48U, 40U, 48U, 40U,
        u_reconstructed, 12U, sizeof(u_reconstructed)
    };
    Av2hReconstructionPlane v_plane = {
        v_pixels, 48U, 48U, 40U, 48U, 40U,
        v_reconstructed, 12U, sizeof(v_reconstructed)
    };
    Av2hReconstructionScratch scratch;
    Av2hTileDecoder decoder;
    Av2hTileLumaLeaf luma_leaf;
    Av2hTileChromaLeaf chroma_leaf;
    Av2hFirstCoeffBlock chroma_blocks[2U];
    Av2hStatus status = AV2H_OK;
    uint8_t has_leaf = 0U;
    size_t luma_count = 0U;
    size_t chroma_count = 0U;
    uint64_t luma_ledger_hash = UINT64_C(1469598103934665603);
    uint64_t chroma_ledger_hash = UINT64_C(1469598103934665603);

    stream_size = load_fixture(
        "tests/fixtures/high-detail-96x80.obu", stream, sizeof(stream));
    CHECK(stream_size != 0U);
    CHECK(av2h_obu_parse((Av2hSpan){ stream, stream_size }, &ledger,
                         &error) == AV2H_OK);
    CHECK(ledger.count == 3U);
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ stream, stream_size }, &entries[1],
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ stream + entries[2].payload_offset,
                           entries[2].payload_size },
              &sequence, &frame) == AV2H_OK);
    CHECK(frame.width == 96U && frame.height == 80U);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ stream + entries[2].payload_offset,
                           entries[2].payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    av2h_memory_fill(luma_pixels, 0U, sizeof(luma_pixels));
    av2h_memory_fill(u_pixels, 0U, sizeof(u_pixels));
    av2h_memory_fill(v_pixels, 0U, sizeof(v_pixels));
    av2h_memory_fill(luma_reconstructed, 0U, sizeof(luma_reconstructed));
    av2h_memory_fill(u_reconstructed, 0U, sizeof(u_reconstructed));
    av2h_memory_fill(v_reconstructed, 0U, sizeof(v_reconstructed));
    CHECK(av2h_tile_decoder_init(
              &decoder, group.entropy, &sequence, &frame, &storage) ==
          AV2H_OK);
    while (status == AV2H_OK && decoder.phase != AV2H_TILE_DECODE_DONE) {
        if (decoder.phase == AV2H_TILE_DECODE_LUMA) {
            av2h_memory_fill(&luma_leaf, 0U, sizeof(luma_leaf));
            status = av2h_tile_decoder_reconstruct_next_luma(
                &decoder, &luma_plane, &scratch, &luma_leaf, &has_leaf);
            if (status == AV2H_OK && has_leaf) {
                ++luma_count;
                luma_ledger_hash = ledger_hash_value(
                    luma_ledger_hash, luma_leaf.node.row);
                luma_ledger_hash = ledger_hash_value(
                    luma_ledger_hash, luma_leaf.node.column);
                luma_ledger_hash = ledger_hash_value(
                    luma_ledger_hash, luma_leaf.node.block_size);
                luma_ledger_hash = ledger_hash_value(
                    luma_ledger_hash, luma_leaf.prefix.tx_size);
                luma_ledger_hash = ledger_hash_value(
                    luma_ledger_hash, av2h_range_tell(&decoder.range));
            }
        } else {
            av2h_memory_fill(&chroma_leaf, 0U, sizeof(chroma_leaf));
            av2h_memory_fill(chroma_blocks, 0U, sizeof(chroma_blocks));
            status = av2h_tile_decoder_next_chroma(
                &decoder, &chroma_leaf, &chroma_blocks[0],
                &chroma_blocks[1], &has_leaf);
            if (status == AV2H_OK && has_leaf) {
                status = av2h_chroma_inverse_cross_transform(
                    &chroma_blocks[0], &chroma_blocks[1],
                    chroma_leaf.transform[0].cctx_type, 8U);
            }
            if (status == AV2H_OK && has_leaf) {
                status = av2h_chroma_reconstruct_to_plane_ex(
                    &chroma_leaf.node, &chroma_leaf.mode,
                    &chroma_leaf.prefix[0], &chroma_leaf.transform[0],
                    &chroma_blocks[0], &u_plane, &luma_plane, 1U, 0,
                    &scratch);
            }
            if (status == AV2H_OK && has_leaf) {
                status = av2h_chroma_reconstruct_to_plane_ex(
                    &chroma_leaf.node, &chroma_leaf.mode,
                    &chroma_leaf.prefix[1], &chroma_leaf.transform[1],
                    &chroma_blocks[1], &v_plane, &luma_plane, 2U, 0,
                    &scratch);
            }
            if (status == AV2H_OK && has_leaf) {
                status = av2h_tile_decoder_commit_leaf(
                    &decoder, chroma_leaf.node);
                if (status == AV2H_OK) {
                    ++chroma_count;
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash, chroma_leaf.node.row);
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash, chroma_leaf.node.column);
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash, chroma_leaf.node.block_size);
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash,
                        chroma_leaf.prefix[0].tx_size);
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash,
                        chroma_leaf.prefix[1].tx_size);
                    chroma_ledger_hash = ledger_hash_value(
                        chroma_ledger_hash,
                        av2h_range_tell(&decoder.range));
                }
            }
        }
    }
    if (status != AV2H_OK) {
        fprintf(stderr,
            "high-detail failed: status=%u phase=%u "
                "Y=%zu UV=%zu tell=%zu leaf=(%u,%u) block=%u\n",
                (unsigned int)status, (unsigned int)decoder.phase,
                luma_count, chroma_count, av2h_range_tell(&decoder.range),
                decoder.phase == AV2H_TILE_DECODE_LUMA ? luma_leaf.node.row :
                    chroma_leaf.node.row,
                decoder.phase == AV2H_TILE_DECODE_LUMA ?
                    luma_leaf.node.column : chroma_leaf.node.column,
                (unsigned int)(decoder.phase == AV2H_TILE_DECODE_LUMA ?
                    luma_leaf.node.block_size : chroma_leaf.node.block_size));
        if (decoder.phase == AV2H_TILE_DECODE_LUMA) {
            fprintf(stderr,
                    "high-detail syntax: mode=%u intrabc=%u "
                    "mode-tells=%zu/%zu prefix=%u/%zu/%zu/%zu/%zu "
                    "primary=%u/%u/%zu secondary=%u/%u/%zu\n",
                    (unsigned int)luma_leaf.mode.mode,
                    (unsigned int)luma_leaf.mode.use_intrabc,
                    luma_leaf.mode.before_mode_bits,
                    luma_leaf.mode.after_fsc_bits,
                    (unsigned int)luma_leaf.prefix.eob,
                    luma_leaf.prefix.before_all_zero_bits,
                    luma_leaf.prefix.after_all_zero_bits,
                    luma_leaf.prefix.after_eob_point_bits,
                    luma_leaf.prefix.after_eob_bits,
                    (unsigned int)luma_leaf.transform.primary_type,
                    (unsigned int)luma_leaf.transform.primary_symbol,
                    luma_leaf.transform.after_primary_bits,
                    (unsigned int)luma_leaf.transform.secondary_set,
                    (unsigned int)luma_leaf.transform.secondary_symbol,
                    luma_leaf.transform.after_secondary_bits);
        }
    }
    CHECK(status == AV2H_OK);
    CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
    CHECK(luma_count == 347U);
    CHECK(chroma_count == 9U);
    CHECK(av2h_range_tell(&decoder.range) == 36736U);
    CHECK(luma_ledger_hash == UINT64_C(14031330387958256267));
    CHECK(chroma_ledger_hash == UINT64_C(6087973010201194980));
    check_readiness_map(
        "high-detail Y", luma_reconstructed, sizeof(luma_reconstructed));
    check_readiness_map(
        "high-detail U", u_reconstructed, sizeof(u_reconstructed));
    check_readiness_map(
        "high-detail V", v_reconstructed, sizeof(v_reconstructed));
    check_plane_crop_hash(
        "high-detail Y", luma_pixels, 96U, 96U, 80U,
        UINT64_C(10966831422412852226));
    check_plane_crop_hash(
        "high-detail U", u_pixels, 48U, 48U, 40U,
        UINT64_C(4413999066018793313));
    check_plane_crop_hash(
        "high-detail V", v_pixels, 48U, 48U, 40U,
        UINT64_C(14579798339307477700));
}
    #endif

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
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        4U, 4U, 4U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
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
    frame.gdf_mode = 2U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence,
              &frame, &storage) == AV2H_INVALID_DATA);
    frame.gdf_mode = 0U;
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

static void initialize_scheduler_decoder(
    Av2hTileDecoder *decoder,
    Av2hSequenceState *sequence,
    Av2hFrameState *frame,
    Av2hTileContextStorage *storage) {
    static const unsigned char entropy[] = { 0U, 0U, 0U, 0U };
    CHECK(av2h_tile_decoder_init(
              decoder, (Av2hSpan){ entropy, sizeof(entropy) }, sequence,
              frame, storage) == AV2H_OK);
}

static void test_sdp_superblock_scheduler(void) {
    static const uint16_t rows[] = { 0U, 0U, 0U, 16U, 16U, 16U };
    static const uint16_t columns[] = { 0U, 16U, 32U, 0U, 16U, 32U };
    uint8_t luma_partition_above[48U];
    uint8_t luma_partition_left[24U];
    uint8_t chroma_partition_above[48U];
    uint8_t chroma_partition_left[24U];
    Av2hLumaModeContext luma_mode_above[48U];
    Av2hLumaModeContext luma_mode_left[24U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 48U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 24U];
    Av2hTileLumaModeCell colocated_luma[48U * 24U];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        48U, 24U, 48U * 24U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;
    uint32_t range;
    uint32_t value;
    size_t index;

    sequence.superblock_size = 64U;
    sequence.enable_sdp = 1U;
    frame.width = 192U;
    frame.height = 96U;
    frame.base_q_index = 128U;
    initialize_scheduler_decoder(&decoder, &sequence, &frame, &storage);
    CHECK(decoder.superblock_mi == 16U);
    CHECK(decoder.root_block_size == AV2H_BLOCK_64X64);
    CHECK(decoder.sb_count == 6U);
    range = decoder.range.range;
    value = decoder.range.value;
    decoder.partition_cdfs.do_split[0][0][0] = 1234U;
    luma_partition_above[0] = 37U;
    chroma_partition_left[0] = 41U;
    luma_mode_above[0].joint_mode = 53U;
    coefficient_above[0] = 43U;
    decoder.colocated_luma[0].context.present = 1U;
    decoder.gdf_entropy.decoded_count = 7U;
    decoder.cdef_entropy.index0_cdf[0][0] = 901U;
    ccso_unit_controls[0][0] = 47U;

    for (index = 0U; index < 6U; ++index) {
        CHECK(decoder.phase == AV2H_TILE_DECODE_LUMA);
        CHECK(decoder.sb_index == index);
        CHECK(decoder.sb_row == rows[index]);
        CHECK(decoder.sb_column == columns[index]);
        decoder.luma_traversal.stack_count = 0U;
        CHECK(av2h_tile_decoder_finish_root(&decoder) == AV2H_OK);
        CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
        CHECK(decoder.event == AV2H_TILE_EVENT_ROOT_HANDOFF);
        CHECK(decoder.sb_row == rows[index]);
        CHECK(decoder.sb_column == columns[index]);

        decoder.chroma_traversal.stack_count = 0U;
        CHECK(av2h_tile_decoder_finish_root(&decoder) == AV2H_OK);
        if (index + 1U < 6U) {
            CHECK(decoder.phase == AV2H_TILE_DECODE_LUMA);
            CHECK(decoder.event == AV2H_TILE_EVENT_ROOT_HANDOFF);
            CHECK(decoder.luma_traversal.stack[0].row == rows[index + 1U]);
            CHECK(decoder.luma_traversal.stack[0].column ==
                  columns[index + 1U]);
            CHECK(decoder.chroma_traversal.stack[0].row ==
                  rows[index + 1U]);
            CHECK(decoder.chroma_traversal.stack[0].column ==
                  columns[index + 1U]);
            CHECK(decoder.luma_traversal.diagnostic_before_bits == 0U);
            CHECK(decoder.chroma_traversal.diagnostic_before_bits == 0U);
        } else {
            CHECK(decoder.phase == AV2H_TILE_DECODE_DONE);
            CHECK(decoder.event == AV2H_TILE_EVENT_DONE);
        }
        CHECK(decoder.range.range == range);
        CHECK(decoder.range.value == value);
        CHECK(decoder.partition_cdfs.do_split[0][0][0] == 1234U);
        CHECK(luma_partition_above[0] == 37U);
        CHECK(chroma_partition_left[0] == 41U);
        CHECK(luma_mode_above[0].joint_mode == 53U);
        CHECK(coefficient_above[0] == 43U);
        CHECK(decoder.colocated_luma[0].context.present == 1U);
        CHECK(decoder.gdf_entropy.decoded_count == 7U);
        CHECK(decoder.cdef_entropy.index0_cdf[0][0] == 901U);
        CHECK(ccso_unit_controls[0][0] == 47U);
    }
}

static void test_scheduler_guards_and_root_size(void) {
    static const unsigned char entropy[] = { 0U, 0U, 0U, 0U };
    uint8_t luma_partition_above[48U];
    uint8_t luma_partition_left[32U];
    uint8_t chroma_partition_above[48U];
    uint8_t chroma_partition_left[32U];
    Av2hLumaModeContext luma_mode_above[48U];
    Av2hLumaModeContext luma_mode_left[32U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 48U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 32U];
    Av2hTileLumaModeCell colocated_luma[48U * 32U];
    uint8_t ccso_unit_controls[AV2H_FRAME_PLANE_COUNT][1U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        48U, 32U, 48U * 32U, 0, 0U, 0, 0U,
        { ccso_unit_controls[0], ccso_unit_controls[1],
          ccso_unit_controls[2] },
                1U, 0, 0U
    };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hTileDecoder decoder;

    sequence.superblock_size = 128U;
    sequence.enable_sdp = 1U;
    frame.width = 192U;
    frame.height = 96U;
    frame.base_q_index = 128U;
    initialize_scheduler_decoder(&decoder, &sequence, &frame, &storage);
    CHECK(decoder.superblock_mi == 32U);
    CHECK(decoder.root_block_size == AV2H_BLOCK_128X128);
    CHECK(decoder.sb_count == 2U);
    CHECK(decoder.luma_traversal.stack[0].block_size ==
          AV2H_BLOCK_128X128);

    decoder.luma_root_partition = AV2H_PARTITION_SPLIT;
    decoder.luma_traversal.stack_count = 0U;
    CHECK(av2h_tile_decoder_finish_root(&decoder) == AV2H_OK);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    CHECK(decoder.chroma_root_forced_partition == AV2H_PARTITION_INVALID);

    decoder.phase = AV2H_TILE_DECODE_LUMA;
    decoder.luma_traversal.has_pending_leaf = 1U;
    CHECK(av2h_tile_decoder_finish_root(&decoder) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(decoder.phase == AV2H_TILE_DECODE_LUMA);

    sequence.superblock_size = 64U;
    frame.width = 64U;
    frame.height = 64U;
    initialize_scheduler_decoder(&decoder, &sequence, &frame, &storage);
    frame.gdf_mode = 2U;
    frame.gdf_block_count = 1U;
    decoder.luma_traversal.stack_count = 0U;
    CHECK(av2h_tile_decoder_finish_root(&decoder) == AV2H_OK);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);
    decoder.chroma_traversal.stack_count = 0U;
    CHECK(av2h_tile_decoder_finish_root(&decoder) == AV2H_UNSUPPORTED);
    CHECK(decoder.phase == AV2H_TILE_DECODE_CHROMA);

    sequence.superblock_size = 256U;
    frame.gdf_mode = 0U;
    CHECK(av2h_tile_decoder_init(
              &decoder, (Av2hSpan){ entropy, sizeof(entropy) }, &sequence,
              &frame, &storage) == AV2H_UNSUPPORTED);
}

int main(void) {
    test_failures();
    test_odd_luma_reconstruction();
    test_ordinary_luma_frontier();
    test_orchestrator_failures();
    test_sdp_superblock_scheduler();
    test_scheduler_guards_and_root_size();
    if (failures != 0) {
        fprintf(stderr, "%d tile checks failed\n", failures);
        return 1;
    }
    puts("tile unit: pass");
    return 0;
}
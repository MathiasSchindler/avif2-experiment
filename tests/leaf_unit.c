#include "decoder/reconstruct.h"
#include "decoder/internal/leaf_modules.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n",                    \
                    __FILE__, __LINE__, #condition);                           \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static void test_larger_mode_ordering(void) {
    Av2hLumaModeNeighbors neighbors = { 0 };
    uint8_t joint_mode = 0U;
    CHECK(av2h_luma_joint_mode_from_index(
              59U, AV2H_BLOCK_8X32, &neighbors, &joint_mode) == AV2H_OK);
    CHECK(joint_mode == 54U);
    neighbors.present[0] = 1U;
    neighbors.joint_mode[0] = 22U;
    CHECK(av2h_luma_joint_mode_from_index(
              5U, AV2H_BLOCK_16X16, &neighbors, &joint_mode) == AV2H_OK);
    CHECK(joint_mode == 22U);
    CHECK(av2h_luma_joint_mode_from_index(
              6U, AV2H_BLOCK_16X16, &neighbors, &joint_mode) == AV2H_OK);
    CHECK(joint_mode == 21U);
    CHECK(av2h_luma_joint_mode_from_index(
              7U, AV2H_BLOCK_16X16, &neighbors, &joint_mode) == AV2H_OK);
    CHECK(joint_mode == 23U);
    CHECK(av2h_luma_joint_mode_from_index(
              6U, AV2H_BLOCK_8X8, &neighbors, &joint_mode) == AV2H_OK);
    CHECK(joint_mode == 50U);
    CHECK(av2h_luma_joint_mode_from_index(
              0U, AV2H_BLOCK_INVALID, &neighbors, &joint_mode) ==
          AV2H_INVALID_ARGUMENT);
}


static void test_arguments(void) {
    Av2hRangeDecoder decoder;
    Av2hLeafCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_8X8 };
    Av2hFirstLeafMode mode;
    Av2hGdfEntropyState gdf;
    Av2hCdefEntropyState cdef;
    Av2hCcsoEntropyState ccso;
    uint8_t bytes[8U] = { 0 };
    uint8_t gdf_flags[4U] = { 1U, 1U, 1U, 1U };
    uint8_t cdef_strengths[4U] = { 0U, 0U, 0U, 0U };
    uint8_t ccso_controls[AV2H_FRAME_PLANE_COUNT][4U] = { { 0U } };
    uint8_t *ccso_planes[AV2H_FRAME_PLANE_COUNT] = {
        ccso_controls[0], 0, 0
    };
    Av2hSpan span = { bytes, sizeof(bytes) };
    size_t tell;
    CHECK(sizeof(Av2hLeafCdfs) == 2522U);
    CHECK(av2h_leaf_cdfs_init(0) == AV2H_INVALID_ARGUMENT);
    CHECK(av2h_first_leaf_mode_read(
              0, &cdfs, &sequence, &frame, &leaf, &mode) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_gdf_entropy_state_init(
              0, &sequence, &frame, gdf_flags, 4U) ==
          AV2H_INVALID_ARGUMENT);
    sequence.superblock_size = 64U;
    frame.width = 128U;
    frame.height = 128U;
    frame.gdf_mode = 2U;
    frame.gdf_block_size = 64U;
    frame.gdf_block_columns = 2U;
    frame.gdf_block_rows = 2U;
    frame.gdf_block_count = 4U;
    CHECK(av2h_gdf_entropy_state_init(
              &gdf, &sequence, &frame, 0, 0U) == AV2H_INVALID_DATA);
    sequence.enable_gdf = 1U;
    sequence.gdf_unit_matches_superblock_size = 1U;
    CHECK(av2h_gdf_entropy_state_init(
              &gdf, &sequence, &frame, 0, 0U) == AV2H_OUT_OF_MEMORY);
    CHECK(av2h_gdf_entropy_state_init(
              &gdf, &sequence, &frame, gdf_flags, 3U) ==
          AV2H_OUT_OF_MEMORY);
    CHECK(av2h_gdf_entropy_state_init(
              &gdf, &sequence, &frame, gdf_flags, 4U) == AV2H_OK);
    CHECK(gdf.cdf[0] == 18175U);
    CHECK(gdf.decoded_count == 0U);
    CHECK(gdf_flags[0] == 0U);
    frame.cdef_strength_count = 3U;
    CHECK(av2h_cdef_entropy_state_init(
              0, &sequence, &frame, cdef_strengths, 4U) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_cdef_entropy_state_init(
              &cdef, &sequence, &frame, cdef_strengths, 4U) ==
          AV2H_INVALID_DATA);
    sequence.enable_cdef = 1U;
    CHECK(av2h_cdef_entropy_state_init(
              &cdef, &sequence, &frame, 0, 0U) == AV2H_OUT_OF_MEMORY);
    CHECK(av2h_cdef_entropy_state_init(
              &cdef, &sequence, &frame, cdef_strengths, 3U) ==
          AV2H_OUT_OF_MEMORY);
    CHECK(av2h_cdef_entropy_state_init(
              &cdef, &sequence, &frame, cdef_strengths, 4U) == AV2H_OK);
    CHECK(cdef.unit_columns == 2U);
    CHECK(cdef.unit_rows == 2U);
    CHECK(cdef.unit_count == 4U);
    CHECK(cdef.index0_cdf[0][0] == 3734U);
    CHECK(cdef.remainder_cdf[0][0] == 15060U);
    CHECK(cdef_strengths[0] == AV2H_CDEF_UNTRANSMITTED);
    frame.ccso[0].enabled = 1U;
    CHECK(av2h_ccso_entropy_state_init(
              0, &sequence, &frame, ccso_planes, 4U) ==
          AV2H_INVALID_ARGUMENT);
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 4U) ==
          AV2H_INVALID_DATA);
    sequence.enable_ccso = 1U;
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, 0, 0U) == AV2H_OUT_OF_MEMORY);
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 0U) ==
          AV2H_OUT_OF_MEMORY);
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 1U) == AV2H_OK);
    CHECK(ccso.unit_size == 256U);
    CHECK(ccso.unit_columns == 1U);
    CHECK(ccso.unit_rows == 1U);
    CHECK(ccso.unit_count == 1U);
    CHECK(ccso_controls[0][0] == AV2H_CCSO_UNTRANSMITTED);
    sequence.ccso_unit_matches_superblock_size = 1U;
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 3U) ==
          AV2H_OUT_OF_MEMORY);
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 4U) == AV2H_OK);
    CHECK(ccso.unit_size == 64U);
    CHECK(ccso.unit_count == 4U);
    sequence.superblock_size = 256U;
    CHECK(av2h_ccso_entropy_state_init(
              &ccso, &sequence, &frame, ccso_planes, 4U) == AV2H_OK);
    CHECK(ccso.unit_size == 256U);
    CHECK(ccso.unit_count == 1U);
    sequence.superblock_size = 64U;
    frame.ccso[0].enabled = 0U;
    frame.cdef_strength_count = 0U;
    frame.gdf_mode = 0U;
    frame.base_q_index = 128U;
    frame.disable_cdf_update = 1U;
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &cdfs, &sequence, &frame, &leaf, &mode) ==
          AV2H_UNSUPPORTED);
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(cdfs.intrabc_shell_class_set1[0] == 12790U);
    CHECK(cdfs.intrabc_shell_class_set1[6] == 0U);
    CHECK(cdfs.intrabc_qtr_shell_class_set1[0] == 14299U);
    CHECK(cdfs.intrabc_shell_offset_other[7][0] == 10205U);
    CHECK(cdfs.intrabc_shell_offset_other[15][0] == 16384U);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    sequence.enable_sdp = 1U;
    frame.disable_cdf_update = 0U;
    frame.allow_intrabc = 1U;
    frame.allow_screen_content_tools = 1U;
    tell = av2h_range_tell(&decoder);
    CHECK(av2h_first_leaf_mode_read(
              &decoder, &cdfs, &sequence, &frame, &leaf, &mode) ==
          AV2H_OK);
    CHECK(av2h_range_tell(&decoder) > tell);
    CHECK(mode.palette_mode == 0U);
    CHECK(mode.before_palette_bits == mode.after_mrl_bits);
    CHECK(mode.after_palette_bits >= mode.before_palette_bits);
}

static void test_gdf_after_intrabc(void) {
    uint8_t bytes[8U] = { 0 };
    uint8_t flags[2U];
    Av2hLumaModeContext above[32U];
    Av2hLumaModeContext left[16U];
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_8X8 };
    Av2hLeafCdfs cdfs;
    Av2hLumaModeContextState contexts;
    Av2hGdfEntropyState gdf;
    Av2hFirstLeafMode mode;
    Av2hRangeDecoder decoder;
    unsigned int first_byte;
    uint8_t found = 0U;

    sequence.superblock_size = 64U;
    sequence.enable_gdf = 1U;
    sequence.gdf_unit_matches_superblock_size = 1U;
    sequence.enable_sdp = 1U;
    frame.width = 128U;
    frame.height = 64U;
    frame.allow_intrabc = 1U;
    frame.gdf_mode = 2U;
    frame.gdf_block_size = 64U;
    frame.gdf_block_columns = 2U;
    frame.gdf_block_rows = 1U;
    frame.gdf_block_count = 2U;
    for (first_byte = 0U; first_byte <= 255U; ++first_byte) {
        Av2hStatus status;
        bytes[0] = (uint8_t)first_byte;
        CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
        CHECK(av2h_luma_mode_context_state_init(
                  &contexts, above, 32U, left, 16U) == AV2H_OK);
        CHECK(av2h_gdf_entropy_state_init(
                  &gdf, &sequence, &frame, flags, 2U) == AV2H_OK);
        CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
        status = av2h_luma_mode_read_with_state_and_gdf(
            &decoder, &cdfs, &sequence, &frame, &leaf, &contexts,
            &gdf, 0, 0, &mode);
        if (mode.use_intrabc != 0U) {
            CHECK(status == AV2H_OK);
            CHECK(mode.gdf_flags_read == 1U);
            CHECK(gdf.decoded_count == 1U);
            found = 1U;
            break;
        }
    }
    CHECK(found == 1U);
}

static void test_chroma_cfl_derived_alpha(void) {
    /* Emerald Q208 leaf (0,32) signals UV_CFL_PRED with cfl_idx == 1
     * (CFL_DERIVED_ALPHA): is_cfl is read, the CfL index is read, and no
     * explicit cfl_alpha_idx / joint sign is coded. These byte vectors are
     * hand-selected so the shared OD_EC range decoder resolves the exact
     * symbol sequence for each CfL branch. */
    Av2hLeafCdfs cdfs;
    Av2hSequenceState sequence = { 0 };
    /* is_cfl == 1, cfl_index == 1 -> derived alpha, DC base predictor. */
    static const uint8_t derived[] = { 0xC4U, 0x44U, 0x00U };
    /* is_cfl == 1, cfl_index == 0 -> explicit alpha. */
    static const uint8_t explicit_alpha[] = { 0x9FU, 0x80U, 0x00U };
    /* is_cfl == 0 -> plain UV_DC_PRED. */
    static const uint8_t dc[] = { 0x00U, 0x00U, 0x00U };
    Av2hSpan span;
    Av2hRangeDecoder decoder;
    Av2hFirstChromaMode mode;

    sequence.enable_cfl_intra = 1U;

    span = (Av2hSpan){ derived, sizeof(derived) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0U, &mode) == AV2H_OK);
    CHECK(mode.is_cfl == 1U);
    CHECK(mode.uses_cfl == 1U);
    CHECK(mode.cfl_index == 1U);
    CHECK(mode.cfl_derived_alpha == 1U);
    CHECK(mode.cfl_kind == AV2H_CFL_DERIVED_ALPHA);
    CHECK(mode.explicit_alpha_coded == 0U);
    CHECK(mode.uv_mode == 13U);
    CHECK(mode.mode == AV2H_INTRA_DC);
    CHECK(mode.angle_delta == 0);
    /* The CfL index read consumes bits after is_cfl; the mode is fully
     * resolved once the index is decoded (no alpha/sign follow). */
    CHECK(mode.after_cfl_index_bits > mode.after_cfl_bits);
    CHECK(mode.after_mode_bits == mode.after_cfl_index_bits);

    span = (Av2hSpan){ explicit_alpha, sizeof(explicit_alpha) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0U, &mode) ==
          AV2H_OK);
    CHECK(mode.is_cfl == 1U);
    CHECK(mode.uses_cfl == 1U);
    CHECK(mode.cfl_index == 0U);
    CHECK(mode.cfl_derived_alpha == 0U);
    CHECK(mode.cfl_kind == AV2H_CFL_EXPLICIT_ALPHA);
    CHECK(mode.explicit_alpha_coded == 1U);

    span = (Av2hSpan){ dc, sizeof(dc) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0U, &mode) == AV2H_OK);
    CHECK(mode.is_cfl == 0U);
    CHECK(mode.uses_cfl == 0U);
    CHECK(mode.cfl_kind == AV2H_CFL_NONE);
    CHECK(mode.uv_mode == 0U);
    CHECK(mode.mode == AV2H_INTRA_DC);
    CHECK(mode.after_cfl_index_bits == mode.after_cfl_bits);

    sequence.enable_mhccp = 1U;
    span = (Av2hSpan){ derived, sizeof(derived) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read_with_delta(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0,
              AV2H_BLOCK_8X8, 0U, &mode) == AV2H_OK);
    CHECK(cdfs.cfl_mhccp_switch[2] == 1U);

    span = (Av2hSpan){ derived, sizeof(derived) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read_with_delta(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0,
              AV2H_BLOCK_4X4, 0U, &mode) == AV2H_OK);
    CHECK(cdfs.cfl_mhccp_switch[2] == 0U);

    sequence.info.chroma_format_idc = 2U;
    span = (Av2hSpan){ derived, sizeof(derived) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read_with_delta(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0,
              AV2H_BLOCK_32X32, 0U, &mode) == AV2H_OK);
    CHECK(cdfs.cfl_mhccp_switch[2] == 1U);

    span = (Av2hSpan){ derived, sizeof(derived) };
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    CHECK(av2h_chroma_mode_read_with_delta(
              &decoder, &cdfs, &sequence, AV2H_INTRA_DC, 0,
              AV2H_BLOCK_16X64, 0U, &mode) == AV2H_OK);
    CHECK(cdfs.cfl_mhccp_switch[2] == 0U);
}

static void test_chroma_cfl_cdf_defaults(void) {
    /* The CfL index and is_cfl CDFs must match the pinned AVM defaults
     * (default_cfl_index_cdf = AVM_CDF2(12507); default_cfl_cdf contexts
     * 20441 / 11610 / 4643). Stored form is the inverse CDF (32768 - p). */
    Av2hLeafCdfs cdfs;
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(cdfs.cfl_index[0] == (uint16_t)(32768U - 12507U));
    CHECK(cdfs.cfl_index[1] == 0U);
    CHECK(cdfs.cfl_index[2] == 0U);
    CHECK(cdfs.cfl_mhccp_switch[0] ==
          (uint16_t)(32768U - 15499U));
    CHECK(cdfs.is_cfl[0][0] == (uint16_t)(32768U - 20441U));
    CHECK(cdfs.is_cfl[1][0] == (uint16_t)(32768U - 11610U));
    CHECK(cdfs.is_cfl[2][0] == (uint16_t)(32768U - 4643U));
}

static void test_intrabc_bank_survives_superblock_boundary(void) {
    Av2hLumaModeContextState state;
    Av2hLumaModeContext above[32U];
    Av2hLumaModeContext left[32U];
    Av2hPartitionNode leaf = { 0U, 16U, AV2H_BLOCK_32X8 };
    Av2hFirstLeafMode mode = { 0 };
    CHECK(av2h_luma_mode_context_state_init(
              &state, above, 32U, left, 32U) == AV2H_OK);
    state.superblock_mi = 16U;
    state.intrabc_bank_count = 1U;
    state.intrabc_bank_start = 2U;
    state.intrabc_bank_x[2U] = 0;
    state.intrabc_bank_y[2U] = -32;
    CHECK(av2h_luma_mode_context_state_update(
              &state, &leaf, &mode) == AV2H_OK);
    CHECK(state.intrabc_bank_count == 1U);
    CHECK(state.intrabc_bank_start == 2U);
    CHECK(state.intrabc_bank_x[2U] == 0);
    CHECK(state.intrabc_bank_y[2U] == -32);
    CHECK(state.intrabc_bank_remaining_hits == 4U);
    CHECK(state.intrabc_bank_unit_hits == 0U);
    CHECK(state.intrabc_bank_superblock_hits == 0U);
}

static void test_intrabc_bank_origin_uses_coded_frame_bounds(void) {
    Av2hFrameState frame = { 0 };
    Av2hPartitionNode leaf = { 70U, 83U, AV2H_BLOCK_8X8 };
    frame.width = 488U;
    frame.height = 324U;
    CHECK(av2h_intrabc_bank_origin_valid(
              &frame, &leaf, 2U, 2U, -520, 368) == 1U);
    CHECK(av2h_intrabc_bank_origin_valid(
              &frame, &leaf, 2U, 2U, -520, 384) == 0U);
}

static void test_luma_mode_bottom_left_clipped_to_frame(void) {
    Av2hLumaModeContextState state;
    Av2hLumaModeContext above[40U];
    Av2hLumaModeContext left[32U];
    Av2hLumaModeContext spatial[27U * 40U];
    Av2hPartitionNode leaf = { 24U, 32U, AV2H_BLOCK_32X16 };
    Av2hLumaModeNeighbors neighbors = { 0 };
    CHECK(av2h_luma_mode_context_state_init(
              &state, above, 40U, left, 32U) == AV2H_OK);
    memset(spatial, 0, sizeof(spatial));
    state.spatial = (uint8_t *)spatial;
    state.spatial_stride = sizeof(Av2hLumaModeContext);
    state.spatial_columns = 40U;
    state.spatial_rows = 27U;
    left[24U].present = 1U;
    left[24U].joint_mode = 1U;
    spatial[26U * 40U + 31U].present = 1U;
    spatial[26U * 40U + 31U].joint_mode = 7U;
    CHECK(av2h_luma_mode_neighbors_from_state(
              &state, &leaf, &neighbors) == AV2H_OK);
    CHECK(neighbors.present[0] == 0U);
    CHECK(neighbors.joint_mode[0] == 0U);
    CHECK(neighbors.edge_smooth[0] == 1U);
    CHECK(neighbors.spatial_present[0] == 1U);
    CHECK(neighbors.spatial_joint_mode[0] == 1U);

    state.spatial_rows = 26U;
    spatial[25U * 40U + 31U].present = 1U;
    spatial[25U * 40U + 31U].joint_mode = 9U;
    CHECK(av2h_luma_mode_neighbors_from_state(
              &state, &leaf, &neighbors) == AV2H_OK);
    CHECK(neighbors.present[0] == 0U);
    CHECK(neighbors.joint_mode[0] == 0U);
}

static void test_intrabc_morph_pred_cdf_defaults(void) {
    Av2hLeafCdfs cdfs;
    CHECK(av2h_leaf_cdfs_init(&cdfs) == AV2H_OK);
    CHECK(cdfs.morph_pred[0][0] == (uint16_t)(32768U - 31715U));
    CHECK(cdfs.morph_pred[1][0] == (uint16_t)(32768U - 19667U));
    CHECK(cdfs.morph_pred[2][0] == (uint16_t)(32768U - 10555U));
}

static void test_large_luma_block_spans_cdef_strength(void) {
    static const uint8_t bytes[8U] = { 0U };
    Av2hSequenceState sequence = { 0 };
    Av2hFrameState frame = { 0 };
    Av2hCdefEntropyState state;
    Av2hFirstLeafMode mode = { 0 };
    Av2hPartitionNode leaf = { 0U, 0U, AV2H_BLOCK_64X128 };
    Av2hSpan span = { bytes, sizeof(bytes) };
    Av2hRangeDecoder decoder;
    uint8_t strengths[4U];
    size_t before;
    sequence.enable_cdef = 1U;
    sequence.superblock_size = 128U;
    frame.width = 128U;
    frame.height = 128U;
    frame.cdef_strength_count = 2U;
    frame.cdef_on_skip_transform = 1U;
    CHECK(av2h_cdef_entropy_state_init(
              &state, &sequence, &frame, strengths, 4U) == AV2H_OK);
    CHECK(av2h_range_init(&decoder, &span, 1U) == AV2H_OK);
    before = av2h_range_tell(&decoder);
    CHECK(av2h_leaf_read_cdef_entropy(
              &decoder, &frame, &leaf, &state, &mode) == AV2H_OK);
    CHECK(mode.cdef_strengths_read == 1U);
    CHECK(mode.after_cdef_bits > before);
    CHECK(strengths[0U] == mode.cdef_strength_index);
    CHECK(strengths[2U] == mode.cdef_strength_index);
    CHECK(strengths[1U] == AV2H_CDEF_UNTRANSMITTED);
    CHECK(strengths[3U] == AV2H_CDEF_UNTRANSMITTED);
}

int main(void) {
    test_larger_mode_ordering();
    test_arguments();
    test_gdf_after_intrabc();
    test_chroma_cfl_derived_alpha();
    test_chroma_cfl_cdf_defaults();
    test_intrabc_bank_survives_superblock_boundary();
    test_intrabc_bank_origin_uses_coded_frame_bounds();
    test_luma_mode_bottom_left_clipped_to_frame();
    test_intrabc_morph_pred_cdf_defaults();
    test_large_luma_block_spans_cdef_strength();
    if (failures != 0) {
        fprintf(stderr, "%d leaf checks failed\n", failures);
        return 1;
    }
    puts("leaf unit: pass");
    return 0;
}
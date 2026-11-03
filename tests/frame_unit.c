#include "decoder/frame.h"
#include "decoder/tile.h"

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

static void test_mi_dimension_alignment(void) {
    CHECK(av2h_frame_mi_dimension(1U) == 2U);
    CHECK(av2h_frame_mi_dimension(8U) == 2U);
    CHECK(av2h_frame_mi_dimension(9U) == 4U);
    CHECK(av2h_frame_mi_dimension(171U) == 44U);
    CHECK(av2h_frame_mi_dimension(256U) == 64U);
}

static void init_tile_layout_state(Av2hSequenceState *sequence,
                                   Av2hFrameState *frame) {
    av2h_memory_fill(sequence, 0U, sizeof(*sequence));
    av2h_memory_fill(frame, 0U, sizeof(*frame));
    sequence->superblock_size = 64U;
    sequence->info.seq_level_idx = 0U;
    frame->width = 320U;
    frame->height = 320U;
}

static void check_two_by_two_layout(const Av2hFrameState *frame) {
    CHECK(frame->tile_column_count == 2U);
    CHECK(frame->tile_row_count == 2U);
    CHECK(frame->tile_column_start_sb[0] == 0U);
    CHECK(frame->tile_column_start_sb[1] == 3U);
    CHECK(frame->tile_column_start_sb[2] == 5U);
    CHECK(frame->tile_row_start_sb[0] == 0U);
    CHECK(frame->tile_row_start_sb[1] == 3U);
    CHECK(frame->tile_row_start_sb[2] == 5U);
}

static void test_tile_layout_reuse(void) {
    static const unsigned char implicit_bits[] = { 0x00U };
    static const unsigned char explicit_bits[] = { 0xa0U };
    static const unsigned char override_bits[] = { 0x40U };
    Av2hBitReader reader;
    Av2hSequenceState sequence;
    Av2hFrameState frame;

    init_tile_layout_state(&sequence, &frame);
    sequence.seq_tile_info_present_flag = 1U;
    sequence.uniform_tile_spacing = 1U;
    sequence.tile_column_log2 = 1U;
    sequence.tile_row_log2 = 1U;
    av2h_bit_reader_init(&reader, implicit_bits, sizeof(implicit_bits), 0U);
    CHECK(av2h_frame_tile_layout_read(&reader, &sequence, &frame));
    CHECK(reader.bit_position == 2U);
    CHECK(frame.uniform_tile_spacing == 1U);
    CHECK(frame.tile_size_bytes == 1U);
    check_two_by_two_layout(&frame);

    init_tile_layout_state(&sequence, &frame);
    sequence.seq_tile_info_present_flag = 1U;
    sequence.allow_tile_info_change = 1U;
    sequence.uniform_tile_spacing = 1U;
    sequence.tile_column_log2 = 1U;
    sequence.tile_row_log2 = 1U;
    av2h_bit_reader_init(&reader, explicit_bits, sizeof(explicit_bits), 0U);
    CHECK(av2h_frame_tile_layout_read(&reader, &sequence, &frame));
    CHECK(reader.bit_position == 3U);
    CHECK(frame.tile_size_bytes == 2U);
    check_two_by_two_layout(&frame);

    init_tile_layout_state(&sequence, &frame);
    sequence.seq_tile_info_present_flag = 1U;
    sequence.allow_tile_info_change = 1U;
    sequence.uniform_tile_spacing = 1U;
    sequence.tile_column_log2 = 1U;
    sequence.tile_row_log2 = 1U;
    av2h_bit_reader_init(&reader, override_bits, sizeof(override_bits), 0U);
    CHECK(av2h_frame_tile_layout_read(&reader, &sequence, &frame));
    CHECK(reader.bit_position == 4U);
    CHECK(frame.uniform_tile_spacing == 1U);
    CHECK(frame.tile_column_count == 1U);
    CHECK(frame.tile_row_count == 1U);
}

static void test_nonuniform_tile_layout(void) {
    static const unsigned char bits[] = { 0x3bU, 0xc0U };
    Av2hBitReader reader;
    Av2hSequenceState sequence;
    Av2hFrameState frame;

    init_tile_layout_state(&sequence, &frame);
    sequence.seq_tile_info_present_flag = 1U;
    sequence.uniform_tile_spacing = 0U;
    sequence.tile_sb_columns = 4U;
    sequence.tile_sb_rows = 5U;
    av2h_bit_reader_init(&reader, bits, sizeof(bits), 0U);
    CHECK(av2h_frame_tile_layout_read(&reader, &sequence, &frame));
    CHECK(reader.bit_position == 11U);
    CHECK(frame.uniform_tile_spacing == 0U);
    CHECK(frame.tile_column_count == 2U);
    CHECK(frame.tile_row_count == 2U);
    CHECK(frame.tile_column_start_sb[0] == 0U);
    CHECK(frame.tile_column_start_sb[1] == 2U);
    CHECK(frame.tile_column_start_sb[2] == 5U);
    CHECK(frame.tile_row_start_sb[0] == 0U);
    CHECK(frame.tile_row_start_sb[1] == 2U);
    CHECK(frame.tile_row_start_sb[2] == 5U);
    CHECK(frame.tile_size_bytes == 3U);
}

typedef struct {
    uint16_t index;
    int8_t value;
} ExpectedCcsoOffset;

static void check_ccso_lut(const Av2hCcsoPlane *plane,
                           const ExpectedCcsoOffset *expected,
                           size_t expected_count) {
    size_t index;
    size_t expected_index;
    for (index = 0U; index < AV2H_CCSO_LUT_SIZE; ++index) {
        int8_t value = 0;
        for (expected_index = 0U; expected_index < expected_count;
             ++expected_index) {
            if (expected[expected_index].index == index) {
                value = expected[expected_index].value;
                break;
            }
        }
        CHECK(plane->filter_offset[index] == value);
    }
}

static void check_fixture_ccso(const Av2hFrameState *frame,
                               unsigned int fixture_index) {
    static const ExpectedCcsoOffset odd_u[] = {
        { 17U, 3 }, { 21U, -30 }, { 32U, -30 }, { 37U, 3 },
        { 48U, -30 }, { 69U, 3 }, { 85U, 3 }, { 101U, 3 }
    };
    static const ExpectedCcsoOffset odd_v[] = {
        { 0U, 21 }, { 1U, 3 }, { 4U, -3 }, { 5U, 3 }
    };
    static const ExpectedCcsoOffset ordinary_y[] = {
        { 0U, 7 }, { 1U, 3 }, { 4U, 1 }, { 6U, -3 },
        { 9U, -1 }, { 10U, -1 }
    };
    static const ExpectedCcsoOffset detail_y[] = {
        { 1U, 1 }, { 2U, -1 }, { 4U, 1 }, { 5U, -3 }, { 8U, -1 }
    };
    if (fixture_index == 0U) {
        CHECK(frame->ccso[0].enabled == 0U);
        CHECK(frame->ccso[1].enabled == 0U);
        CHECK(frame->ccso[2].enabled == 0U);
        check_ccso_lut(&frame->ccso[0], 0, 0U);
        check_ccso_lut(&frame->ccso[1], 0, 0U);
        check_ccso_lut(&frame->ccso[2], 0, 0U);
    } else if (fixture_index == 1U) {
        CHECK(frame->ccso[0].enabled == 0U);
        CHECK(frame->ccso[1].enabled == 1U);
        CHECK(frame->ccso[1].scale_index == 2U);
        CHECK(frame->ccso[1].offset_count == 32U);
        CHECK(frame->ccso[2].enabled == 1U);
        CHECK(frame->ccso[2].scale_index == 2U);
        CHECK(frame->ccso[2].offset_count == 4U);
        check_ccso_lut(&frame->ccso[1], odd_u,
                       sizeof(odd_u) / sizeof(odd_u[0]));
        check_ccso_lut(&frame->ccso[2], odd_v,
                       sizeof(odd_v) / sizeof(odd_v[0]));
    } else if (fixture_index == 2U) {
        CHECK(frame->ccso[0].enabled == 1U);
        CHECK(frame->ccso[0].offset_count == 9U);
        check_ccso_lut(&frame->ccso[0], ordinary_y,
                       sizeof(ordinary_y) / sizeof(ordinary_y[0]));
    } else {
        CHECK(frame->ccso[0].enabled == 1U);
        CHECK(frame->ccso[0].offset_count == 9U);
        check_ccso_lut(&frame->ccso[0], detail_y,
                       sizeof(detail_y) / sizeof(detail_y[0]));
    }
}

static void check_fixture_filters(const Av2hFrameState *frame,
                                  unsigned int fixture_index) {
    CHECK(frame->deblock_enabled[0] == 1U);
    CHECK(frame->deblock_enabled[1] == 1U);
    if (fixture_index < 2U) {
        CHECK(frame->deblock_enabled[2] == 0U);
        CHECK(frame->deblock_enabled[3] == 0U);
        CHECK(frame->deblock_delta[0] == 3);
        CHECK(frame->deblock_delta[1] == 3);
    } else {
        CHECK(frame->deblock_enabled[2] == 1U);
        CHECK(frame->deblock_enabled[3] == 1U);
        CHECK(frame->deblock_delta[0] == (fixture_index == 2U ? -2 : 0));
        CHECK(frame->deblock_delta[1] == frame->deblock_delta[0]);
        CHECK(frame->deblock_delta[2] == frame->deblock_delta[0]);
        CHECK(frame->deblock_delta[3] == frame->deblock_delta[0]);
    }
    if (fixture_index < 3U) {
        CHECK(frame->cdef_damping == 5U);
        CHECK(frame->cdef_strength_count == 1U);
        CHECK(frame->cdef_on_skip_transform == 1U);
        CHECK(frame->cdef_y_strength[0] ==
              (fixture_index == 0U ? 56U : 34U));
        CHECK(frame->cdef_uv_strength[0] ==
              (fixture_index == 2U ? 58U : 56U));
    } else {
        CHECK(frame->cdef_strength_count == 0U);
    }
    CHECK(frame->gdf_mode == (fixture_index == 1U ? 1U : 0U));
    CHECK(frame->gdf_qp_index == (fixture_index == 1U ? 3U : 0U));
    CHECK(frame->gdf_scale_index == (fixture_index == 1U ? 3U : 0U));
}

static void check_validation_parity(Av2hSpan payload, const Av2hSequenceState *sequence) {
    Av2hFrameState materialized;
    Av2hFrameState metadata;
    Av2hFrameSyntaxStorage syntax;
    Av2hFrameSyntaxStorage deferred_syntax;
    Av2hFrameSyntaxReferences references;
    Av2hError materialized_error = {0};
    Av2hError metadata_error = {0};
    Av2hStatus status = av2h_frame_header_closed_key_parse(
        payload, sequence, &materialized, &syntax, NULL, &materialized_error);
    CHECK(av2h_frame_header_closed_key_parse(
        payload, sequence, &metadata, NULL, &references, &metadata_error) == status);
    CHECK(memcmp(&materialized_error, &metadata_error, sizeof(metadata_error)) == 0);
    if (status == AV2H_OK) {
        av2h_memory_fill(&deferred_syntax, 0xa5U, sizeof(deferred_syntax));
        CHECK(av2h_frame_syntax_materialize(payload, sequence, &metadata, &references,
                                             &deferred_syntax, &metadata_error) == AV2H_OK);
        CHECK(memcmp(&syntax, &deferred_syntax, sizeof(syntax)) == 0);
        av2h_frame_syntax_storage_bind(&metadata, NULL);
    }
    av2h_frame_syntax_storage_bind(&materialized, NULL);
    CHECK(memcmp(&materialized, &metadata, sizeof(metadata)) == 0);
}

static void check_fixture(const unsigned char *sequence_bytes,
                          size_t sequence_size,
                          const unsigned char *payload,
                          size_t payload_size,
                          size_t expected_header_bits,
                          unsigned int expected_intrabc,
                          unsigned int fixture_index) {
    Av2hObu obu;
    Av2hObuLedger ledger = { &obu, 1U, 0U };
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hError error;
    size_t prefix_size;
    CHECK(av2h_obu_parse((Av2hSpan){ sequence_bytes, sequence_size },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ sequence_bytes, sequence_size }, &obu,
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ payload, payload_size }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    CHECK(frame.uncompressed_header_bits == expected_header_bits);
    CHECK(frame.width == sequence.info.max_frame_width_minus_1 + 1U);
    CHECK(frame.height == sequence.info.max_frame_height_minus_1 + 1U);
    CHECK(frame.base_q_index == 128U);
    CHECK(frame.allow_intrabc == expected_intrabc);
    CHECK(frame.disable_cdf_update == 0U);
    CHECK(frame.segmentation_enabled == 0U);
    CHECK(frame.using_qmatrix == 0U);
    CHECK(frame.delta_q_present == 0U);
    CHECK(frame.tx_mode == 1U);
    CHECK(frame.reduced_tx_set == 0U);
    check_validation_parity((Av2hSpan){payload, payload_size}, &sequence);
    check_fixture_ccso(&frame, fixture_index);
    check_fixture_filters(&frame, fixture_index);
    sequence.seq_seg_info_present_flag = 1U;
    sequence.seg_allow_info_change = 0U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ payload, payload_size }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    CHECK(frame.segmentation_enabled == 0U);
    sequence.seq_seg_info_present_flag = 0U;
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ payload, payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.header_size == (expected_header_bits + 8U) / 8U);
    for (prefix_size = 0U; prefix_size < group.header_size; ++prefix_size) {
        check_validation_parity((Av2hSpan){payload, prefix_size}, &sequence);
        CHECK(av2h_frame_header_closed_key_parse(
            (Av2hSpan){ payload, prefix_size }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) != AV2H_OK);
    }
}

static void test_fixtures(void) {
    static const unsigned char tiny_sequence[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
        0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
    };
    static const unsigned char odd_sequence[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x43U, 0x87U, 0x0eU,
        0x1cU, 0xe6U, 0xf2U, 0x37U, 0x32U, 0x44U
    };
    static const unsigned char ordinary_sequence[] = {
        0x0dU, 0x04U, 0x82U, 0x0aU, 0x76U, 0x9fU, 0xeeU,
        0x38U, 0x7bU, 0x9bU, 0xc8U, 0xdcU, 0x92U, 0x20U
    };
    static const unsigned char detail_sequence[] = {
        0x0dU, 0x04U, 0x82U, 0x0aU, 0x66U, 0xbfU, 0x3cU,
        0x70U, 0xe7U, 0x37U, 0x91U, 0xb8U, 0x24U, 0x40U
    };
    static const unsigned char tiny[] = {
        0xe3U, 0x00U, 0x33U, 0xd0U, 0xb8U, 0x70U, 0x00U, 0x5aU
    };
    static const unsigned char odd[] = {
        0xeeU, 0xc0U, 0x0cU, 0xf7U, 0xc2U, 0x89U, 0xc2U, 0x93U,
        0xcfU, 0xffU, 0xc1U, 0x00U, 0x00U, 0xffU, 0x2aU, 0x52U,
        0x67U, 0xd6U, 0x80U, 0x5aU
    };
    static const unsigned char ordinary[] = {
        0xeeU, 0x90U, 0x03U, 0xe9U, 0x55U, 0x0aU, 0x27U,
        0x50U, 0x61U, 0xf7U, 0x27U, 0x9bU, 0x00U, 0x5aU
    };
    static const unsigned char detail[] = {
        0xeeU, 0x90U, 0x03U, 0xc2U, 0x2aU,
        0x16U, 0xbcU, 0xc0U, 0x00U, 0x5aU
    };
    check_fixture(tiny_sequence, sizeof(tiny_sequence), tiny, sizeof(tiny),
                  49U, 0U, 0U);
    check_fixture(odd_sequence, sizeof(odd_sequence), odd, sizeof(odd),
                  148U, 1U, 1U);
    check_fixture(ordinary_sequence, sizeof(ordinary_sequence),
                  ordinary, sizeof(ordinary), 101U, 1U, 2U);
    check_fixture(detail_sequence, sizeof(detail_sequence),
                  detail, sizeof(detail), 65U, 1U, 3U);
}

static void test_failures(void) {
    static const unsigned char payload[] = { 0x00U, 0x00U };
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    sequence.info.still_picture = 1U;
    sequence.info.bit_depth_idc = 1U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ payload, sizeof(payload) }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_UNSUPPORTED);
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ payload, sizeof(payload) }, 0, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_INVALID_ARGUMENT);
}

static void test_10bit_wp0_header(void) {
    unsigned char input[64U];
    unsigned char mutated[64U];
    uint8_t luma_partition_above[4U];
    uint8_t luma_partition_left[16U];
    uint8_t chroma_partition_above[4U];
    uint8_t chroma_partition_left[16U];
    Av2hLumaModeContext luma_mode_above[4U];
    Av2hLumaModeContext luma_mode_left[16U];
    uint8_t coefficient_above[AV2H_FRAME_PLANE_COUNT * 4U];
    uint8_t coefficient_left[AV2H_FRAME_PLANE_COUNT * 16U];
    Av2hTileLumaModeCell colocated_luma[16U];
    Av2hTileContextStorage storage = {
        luma_partition_above, luma_partition_left,
        chroma_partition_above, chroma_partition_left,
        luma_mode_above, luma_mode_left,
        coefficient_above, coefficient_left, colocated_luma,
        4U, 16U, 16U, 0, 0U, 0, 0U, { 0, 0, 0 }, 0U, 0, 0U, 0, 0U
    };
    Av2hObu entries[3U];
    Av2hObuLedger ledger = { entries, 3U, 0U };
    Av2hError error;
    Av2hSequenceState sequence;
    Av2hSequenceState changed_sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hTileDecoder decoder;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    FILE *source = fopen(
        "tests/fixtures/10bit/avm-reference/"
        "tiny-constrained-16x16-yuv420p10le-qp208.obu", "rb");
    size_t size;
    size_t index;
    CHECK(source != 0);
    if (source == 0) return;
    size = fread(input, 1U, sizeof(input), source);
    CHECK(!ferror(source));
    CHECK(feof(source));
    fclose(source);
    CHECK(size == 45U);
    CHECK(av2h_obu_parse(
              (Av2hSpan){ input, size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (ledger.entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence_obu = &ledger.entries[index];
        } else if (ledger.entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame_obu = &ledger.entries[index];
        }
    }
    CHECK(sequence_obu != 0);
    CHECK(frame_obu != 0);
    if (sequence_obu == 0 || frame_obu == 0) return;
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ input, size }, sequence_obu,
              &sequence, &error) == AV2H_OK);
    CHECK(sequence.info.seq_profile_idc == 0U);
    CHECK(sequence.info.bit_depth_idc == 0U);
    CHECK(sequence.info.chroma_format_idc == 0U);
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    CHECK(frame.width == 16U);
    CHECK(frame.height == 16U);
    CHECK(frame.base_q_index == 256U);
    CHECK(frame.uncompressed_header_bits == 25U);
    CHECK(frame.deblock_enabled[0] == 0U);
    CHECK(frame.deblock_enabled[1] == 0U);
    CHECK(frame.deblock_enabled[2] == 0U);
    CHECK(frame.deblock_enabled[3] == 0U);
    CHECK(frame.cdef_strength_count == 0U);
    CHECK(frame.gdf_mode == 0U);
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.header_size == 4U);
    CHECK(av2h_tile_decoder_init(
        &decoder, group.entropy, &sequence, &frame, &storage, UINT16_MAX) == AV2H_OK);
    CHECK(decoder.superblock.current_q_index == 256U);
    CHECK(decoder.entropy_state.coeff_cdfs.q_context == 3U);
    CHECK(decoder.entropy_state.level_cdfs.q_context == 3U);
    for (index = 0U; index < group.header_size; ++index) {
        CHECK(av2h_frame_header_closed_key_parse(
            (Av2hSpan){ input + frame_obu->payload_offset, index }, &sequence,
            &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) != AV2H_OK);
    }

    changed_sequence = sequence;
    changed_sequence.info.bit_depth_idc = 2U;
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
              &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, &error) == AV2H_UNSUPPORTED);
    CHECK(error.context == AV2H_CONTEXT_FRAME);
    CHECK(error.diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_FRAME_HEADER);
    CHECK(error.diagnostic_reason ==
          AV2H_DIAGNOSTIC_REASON_UNSUPPORTED_TUPLE);
    CHECK(error.syntax_stage == AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE);
    CHECK(error.parsed_value == 0x200U);
    changed_sequence = sequence;
    changed_sequence.info.chroma_format_idc = 3U;
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
              &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, &error) == AV2H_UNSUPPORTED);
    CHECK(error.syntax_stage == AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE);
    CHECK(error.parsed_value == 0x003U);
    changed_sequence = sequence;
    changed_sequence.info.still_picture = 0U;
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
              &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, &error) == AV2H_UNSUPPORTED);
    CHECK(error.syntax_stage == AV2H_DIAGNOSTIC_SYNTAX_FRAME_PROFILE);
    changed_sequence = sequence;
    changed_sequence.info.chroma_format_idc = 1U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_UNSUPPORTED);
    changed_sequence = sequence;
    changed_sequence.info.seq_profile_idc = 4U;
    changed_sequence.info.chroma_format_idc = 2U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    changed_sequence.info.seq_profile_idc = 0U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_UNSUPPORTED);
    changed_sequence.info.seq_profile_idc = 4U;
    changed_sequence.info.chroma_format_idc = 0U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &changed_sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_UNSUPPORTED);

    av2h_memory_copy(
        mutated, input + frame_obu->payload_offset, frame_obu->payload_size);
    mutated[2] |= 0x40U;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ mutated, frame_obu->payload_size }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    CHECK(frame.using_qmatrix == 1U);
    CHECK(frame.qm_u[0] == frame.qm_v[0]);
    av2h_memory_copy(
        mutated, input + frame_obu->payload_offset, frame_obu->payload_size);
    mutated[0] &= 0xfeU;
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ mutated, frame_obu->payload_size }, &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
}

static void test_emerald_q208_deblock_header(void) {
    unsigned char input[256U];
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    FILE *source = fopen(
        "tests/fixtures/emerald-192x96-qp208.obu", "rb");
    size_t size;
    size_t index;
    CHECK(source != 0);
    if (source == 0) return;
    size = fread(input, 1U, sizeof(input), source);
    CHECK(!ferror(source));
    CHECK(feof(source));
    fclose(source);
    CHECK(size == 115U);
    CHECK(av2h_obu_parse(
              (Av2hSpan){ input, size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (ledger.entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence_obu = &ledger.entries[index];
        } else if (ledger.entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame_obu = &ledger.entries[index];
        }
    }
    CHECK(sequence_obu != 0);
    CHECK(frame_obu != 0);
    if (sequence_obu == 0 || frame_obu == 0) return;
    CHECK(av2h_sequence_state_parse(
              (Av2hSpan){ input, size }, sequence_obu,
              &sequence, &error) == AV2H_OK);
    CHECK(av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input + frame_obu->payload_offset,
                           frame_obu->payload_size },
        &sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, NULL) == AV2H_OK);
    CHECK(frame.width == 192U);
    CHECK(frame.height == 96U);
    CHECK(frame.base_q_index == 208U);
    CHECK(frame.deblock_enabled[0] == 1U);
    CHECK(frame.deblock_enabled[1] == 1U);
    CHECK(frame.deblock_enabled[2] == 1U);
    CHECK(frame.deblock_enabled[3] == 1U);
    CHECK(frame.deblock_delta[0] == 2);
    CHECK(frame.deblock_delta[1] == 0);
    CHECK(frame.deblock_delta[2] == 0);
    CHECK(frame.deblock_delta[3] == 0);
    CHECK(sequence.enable_restoration == 0U);
    CHECK(sequence.enable_gdf == 0U);
    CHECK(sequence.enable_cdef == 0U);
    CHECK(sequence.enable_ccso == 0U);
    CHECK(frame.cdef_strength_count == 0U);
    CHECK(frame.ccso[0].enabled == 0U);
    CHECK(frame.ccso[1].enabled == 0U);
    CHECK(frame.ccso[2].enabled == 0U);
}

int main(void) {
    test_mi_dimension_alignment();
    test_tile_layout_reuse();
    test_nonuniform_tile_layout();
    test_fixtures();
    test_failures();
    test_10bit_wp0_header();
    test_emerald_q208_deblock_header();
    if (failures != 0) {
        fprintf(stderr, "%d frame checks failed\n", failures);
        return 1;
    }
    puts("frame unit: pass");
    return 0;
}

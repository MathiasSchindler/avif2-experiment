#include "decoder/frame.h"
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

static void check_fixture(const unsigned char *sequence_bytes,
                          size_t sequence_size,
                          const unsigned char *payload,
                          size_t payload_size,
                          size_t expected_header_bits,
                          unsigned int expected_intrabc) {
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
              (Av2hSpan){ payload, payload_size }, &sequence,
              &frame) == AV2H_OK);
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
    CHECK(av2h_tile_group_single_parse(
              (Av2hSpan){ payload, payload_size },
              frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK(group.header_size == (expected_header_bits + 8U) / 8U);
    for (prefix_size = 0U; prefix_size < group.header_size; ++prefix_size) {
        CHECK(av2h_frame_header_closed_key_parse(
                  (Av2hSpan){ payload, prefix_size }, &sequence,
                  &frame) != AV2H_OK);
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
                  49U, 0U);
    check_fixture(odd_sequence, sizeof(odd_sequence), odd, sizeof(odd),
                  148U, 1U);
    check_fixture(ordinary_sequence, sizeof(ordinary_sequence),
                  ordinary, sizeof(ordinary), 101U, 1U);
    check_fixture(detail_sequence, sizeof(detail_sequence),
                  detail, sizeof(detail), 65U, 1U);
}

static void test_failures(void) {
    static const unsigned char payload[] = { 0x00U, 0x00U };
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    av2h_memory_fill(&sequence, 0U, sizeof(sequence));
    sequence.info.still_picture = 1U;
    sequence.info.bit_depth_idc = 1U;
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ payload, sizeof(payload) }, &sequence,
              &frame) == AV2H_UNSUPPORTED);
    CHECK(av2h_frame_header_closed_key_parse(
              (Av2hSpan){ payload, sizeof(payload) }, 0,
              &frame) == AV2H_INVALID_ARGUMENT);
}

int main(void) {
    test_fixtures();
    test_failures();
    if (failures != 0) {
        fprintf(stderr, "%d frame checks failed\n", failures);
        return 1;
    }
    puts("frame unit: pass");
    return 0;
}
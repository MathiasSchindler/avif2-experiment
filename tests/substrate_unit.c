#include "base.h"
#include "decoder/sequence.h"

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

static void test_arithmetic(void) {
    size_t result = 0U;
    CHECK(av2h_size_add(2U, 3U, &result) && result == 5U);
    CHECK(!av2h_size_add(SIZE_MAX, 1U, &result));
    CHECK(av2h_size_multiply(7U, 9U, &result) && result == 63U);
    CHECK(!av2h_size_multiply(SIZE_MAX, 2U, &result));
    CHECK(av2h_size_align(17U, 8U, &result) && result == 24U);
    CHECK(!av2h_size_align(17U, 3U, &result));
}

static void test_readers(void) {
    static const unsigned char bytes[] = { 0x12U, 0x34U, 0xabU, 0xcdU };
    Av2hByteReader byte_reader;
    Av2hBitReader bit_reader;
    av2h_byte_reader_init(&byte_reader, bytes, sizeof(bytes), 10U);
    CHECK(av2h_byte_reader_u16be(&byte_reader) == 0x1234U);
    CHECK(av2h_byte_reader_offset(&byte_reader) == 12U);
    CHECK(av2h_byte_reader_u32be(&byte_reader) == 0U);
    CHECK(byte_reader.status == AV2H_TRUNCATED);

    av2h_bit_reader_init(&bit_reader, bytes, sizeof(bytes), 0U);
    CHECK(av2h_bit_reader_read(&bit_reader, 4U) == 1U);
    CHECK(av2h_bit_reader_read(&bit_reader, 8U) == 0x23U);
    CHECK(av2h_bit_reader_remaining(&bit_reader) == 20U);
}

static void test_raw_syntax_readers(void) {
    static const unsigned char uvlc[] = { 0xa6U, 0x42U, 0x80U };
    static const unsigned char svlc[] = { 0xa6U, 0x40U };
    static const unsigned char signed_values[] = { 0xf8U, 0x70U };
    static const unsigned char quniform[] = { 0x1bU, 0x70U };
    static const unsigned char zero_padding[] = { 0xa0U };
    static const unsigned char nonzero_padding[] = { 0xa1U };
    static const unsigned char too_long[4] = { 0U, 0U, 0U, 0U };
    Av2hBitReader reader;
    av2h_bit_reader_init(&reader, uvlc, sizeof(uvlc), 0U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 0U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 1U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 2U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 3U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 4U);

    av2h_bit_reader_init(&reader, svlc, sizeof(svlc), 0U);
    CHECK(av2h_bit_reader_svlc(&reader) == 0);
    CHECK(av2h_bit_reader_svlc(&reader) == 1);
    CHECK(av2h_bit_reader_svlc(&reader) == -1);
    CHECK(av2h_bit_reader_svlc(&reader) == 2);

    av2h_bit_reader_init(&reader, signed_values, sizeof(signed_values), 0U);
    CHECK(av2h_bit_reader_inverse_signed(&reader, 3U) == -1);
    CHECK(av2h_bit_reader_inverse_signed(&reader, 3U) == -8);
    CHECK(av2h_bit_reader_inverse_signed(&reader, 3U) == 7);

    av2h_bit_reader_init(&reader, quniform, sizeof(quniform), 0U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 5U) == 0U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 5U) == 1U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 5U) == 2U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 5U) == 3U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 5U) == 4U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 1U) == 0U);

    av2h_bit_reader_init(&reader, zero_padding, sizeof(zero_padding), 0U);
    CHECK(av2h_bit_reader_read(&reader, 3U) == 5U);
    CHECK(av2h_bit_reader_align_zero(&reader) == AV2H_OK);
    av2h_bit_reader_init(&reader, nonzero_padding,
                         sizeof(nonzero_padding), 0U);
    CHECK(av2h_bit_reader_read(&reader, 3U) == 5U);
    CHECK(av2h_bit_reader_align_zero(&reader) == AV2H_INVALID_DATA);

    av2h_bit_reader_init(&reader, too_long, sizeof(too_long), 0U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 0U);
    CHECK(reader.status == AV2H_INVALID_DATA);
    av2h_bit_reader_init(&reader, zero_padding, 0U, 0U);
    CHECK(av2h_bit_reader_uvlc(&reader) == 0U);
    CHECK(reader.status == AV2H_TRUNCATED);
    av2h_bit_reader_init(&reader, zero_padding, sizeof(zero_padding), 0U);
    CHECK(av2h_bit_reader_quasi_uniform(&reader, 0U) == 0U);
    CHECK(reader.status == AV2H_INVALID_ARGUMENT);
}

static void test_writers(void) {
    unsigned char bytes[8];
    Av2hByteWriter byte_writer;
    Av2hBitWriter bit_writer;
    Av2hByteWriter sizing;
    av2h_byte_writer_init(&byte_writer, bytes, sizeof(bytes));
    CHECK(av2h_byte_writer_u16be(&byte_writer, 0x1234U) == AV2H_OK);
    CHECK(av2h_byte_writer_u32be(&byte_writer, 0xabcdef01U) == AV2H_OK);
    CHECK(av2h_byte_writer_required(&byte_writer) == 6U);
    CHECK(bytes[0] == 0x12U && bytes[5] == 0x01U);

    av2h_byte_writer_init_sizing(&sizing);
    CHECK(av2h_byte_writer_write(&sizing, bytes, 100U) == AV2H_OK);
    CHECK(av2h_byte_writer_required(&sizing) == 100U);

    av2h_bit_writer_init(&bit_writer, bytes, sizeof(bytes));
    CHECK(av2h_bit_writer_write(&bit_writer, 5U, 3U) == AV2H_OK);
    CHECK(av2h_bit_writer_write(&bit_writer, 3U, 2U) == AV2H_OK);
    CHECK(av2h_bit_writer_align_zero(&bit_writer) == AV2H_OK);
    CHECK(bytes[0] == 0xb8U);
}

static void test_arena(void) {
    _Alignas(16) unsigned char memory[64];
    Av2hArena arena;
    Av2hArena sizing;
    void *first;
    void *second;
    size_t offset;
    av2h_arena_init(&arena, memory + 1U, sizeof(memory) - 1U);
    first = av2h_arena_allocate(&arena, 3U, 4U);
    second = av2h_arena_allocate(&arena, 8U, 8U);
    CHECK(first != 0 && ((uintptr_t)first & 3U) == 0U);
    CHECK(second != 0 && ((uintptr_t)second & 7U) == 0U);
    CHECK(av2h_arena_required(&arena) <= sizeof(memory) - 1U);

    av2h_arena_init_sizing(&sizing);
    (void)av2h_arena_allocate(&sizing, 3U, 4U);
    (void)av2h_arena_allocate(&sizing, 8U, 8U);
    for (offset = 0U; offset < 16U; ++offset) {
        av2h_arena_init(&arena, memory + offset, sizeof(memory) - offset);
        CHECK(av2h_arena_allocate(&arena, 3U, 4U) != 0);
        CHECK(av2h_arena_allocate(&arena, 8U, 8U) != 0);
        CHECK(av2h_arena_required(&arena) <= av2h_arena_required(&sizing));
    }
    av2h_arena_init_sizing(&sizing);
    CHECK(av2h_arena_allocate(&sizing, 1U, 3U) == 0);
    CHECK(sizing.status == AV2H_INVALID_ARGUMENT);
}

static void test_leb128_and_obus(void) {
    static const unsigned char stream[] = { 0x01U, 0x08U, 0x01U, 0x10U };
    static const unsigned char noncanonical[] = { 0x81U, 0x00U, 0x04U };
    static const unsigned char incomplete[] = { 0x80U };
    static const unsigned char overwide[] = {
        0x80U, 0x80U, 0x80U, 0x80U, 0x80U,
        0x80U, 0x80U, 0x80U, 0x00U
    };
    static const size_t values[] = {
        0U, 127U, 128U, 16384U, (size_t)1U << 21,
        (size_t)1U << 28, (size_t)1U << 35,
        (size_t)1U << 42, (size_t)1U << 49,
        ((size_t)1U << 56) - 1U
    };
    Av2hObu entries[2];
    Av2hObuLedger ledger = { entries, 2U, 0U };
    Av2hError error;
    size_t value;
    size_t encoded_size;
    size_t index;
    unsigned char encoded[8];
    CHECK(av2h_leb128_read((Av2hSpan){ stream, sizeof(stream) },
                           &value, &encoded_size, &error) == AV2H_OK);
    CHECK(value == 1U && encoded_size == 1U);
    CHECK(av2h_leb128_write(300U, encoded, sizeof(encoded),
                            &encoded_size) == AV2H_OK);
    CHECK(encoded_size == 2U && encoded[0] == 0xacU && encoded[1] == 0x02U);
    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        size_t decoded = 0U;
        size_t decoded_size = 0U;
        CHECK(av2h_leb128_write(values[index], encoded, sizeof(encoded),
                                &encoded_size) == AV2H_OK);
        CHECK(av2h_leb128_read((Av2hSpan){ encoded, encoded_size },
                               &decoded, &decoded_size, &error) == AV2H_OK);
        CHECK(decoded == values[index] && decoded_size == encoded_size);
    }
    CHECK(av2h_leb128_write((size_t)1U << 56, encoded, sizeof(encoded),
                            &encoded_size) == AV2H_OVERFLOW);
    CHECK(av2h_leb128_read((Av2hSpan){ incomplete, sizeof(incomplete) },
                           &value, &encoded_size, &error) == AV2H_TRUNCATED);
    CHECK(error.context == AV2H_CONTEXT_LEB128 && error.offset == 1U);
    CHECK(av2h_leb128_read((Av2hSpan){ overwide, sizeof(overwide) },
                           &value, &encoded_size, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ stream, sizeof(stream) },
                         &ledger, &error) == AV2H_OK);
    CHECK(ledger.count == 2U);
    CHECK(entries[0].type == AV2H_OBU_TEMPORAL_DELIMITER);
    CHECK(entries[1].type == AV2H_OBU_CLOSED_LOOP_KEY);
    CHECK(av2h_obu_parse((Av2hSpan){ noncanonical, sizeof(noncanonical) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(entries[0].xlayer_id == 31U);
}

static void test_obu_header_rules(void) {
    static const unsigned char msdo[] = { 0x01U, 0x50U };
    static const unsigned char extended_td[] = { 0x02U, 0x88U, 0x00U };
    static const unsigned char layered_sequence[] = { 0x02U, 0x84U, 0x20U };
    static const unsigned char local_msdo[] = { 0x02U, 0xd0U, 0x00U };
    static const unsigned char global_key[] = { 0x03U, 0x90U, 0x1fU, 0xaaU };
    static const unsigned char temporal_key[] = { 0x02U, 0x11U, 0xaaU };
    static const unsigned char short_extension[] = { 0x01U, 0x90U };
    static const unsigned char truncated[] = { 0x03U, 0x10U, 0xaaU };
    static const unsigned char zero_size[] = { 0x00U };
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hError error;
    CHECK(av2h_obu_parse((Av2hSpan){ msdo, sizeof(msdo) },
                         &ledger, &error) == AV2H_OK);
    CHECK(entry.type == AV2H_OBU_MULTI_STREAM_DECODER_OPERATION);
    CHECK(entry.xlayer_id == 31U && entry.mlayer_id == 0U);
    CHECK(av2h_obu_parse((Av2hSpan){ extended_td, sizeof(extended_td) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(error.obu_index == 0U && error.obu_type == AV2H_OBU_TEMPORAL_DELIMITER);
    CHECK(av2h_obu_parse((Av2hSpan){ layered_sequence, sizeof(layered_sequence) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ local_msdo, sizeof(local_msdo) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ global_key, sizeof(global_key) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ temporal_key, sizeof(temporal_key) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ short_extension, sizeof(short_extension) },
                         &ledger, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_obu_parse((Av2hSpan){ truncated, sizeof(truncated) },
                         &ledger, &error) == AV2H_TRUNCATED);
    CHECK(av2h_obu_parse((Av2hSpan){ zero_size, sizeof(zero_size) },
                         &ledger, &error) == AV2H_INVALID_DATA);
}

static void check_sequence(const unsigned char *bytes,
                           size_t size,
                           unsigned int width,
                           unsigned int height,
                           unsigned int width_bits,
                           unsigned int height_bits,
                           unsigned int enable_cdef,
                           unsigned int enable_gdf,
                           unsigned int enable_ccso) {
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hSequenceInfo sequence;
    Av2hSequenceState state;
    Av2hError error;
    Av2hStatus status;
    CHECK(av2h_obu_parse((Av2hSpan){ bytes, size }, &ledger, &error) == AV2H_OK);
    CHECK(ledger.count == 1U);
    status = av2h_sequence_header_parse((Av2hSpan){ bytes, size }, &entry,
                                        &sequence, &error);
    if (status != AV2H_OK) {
        fprintf(stderr, "%ux%u: sequence status=%d offset=%zu context=%u\n",
                width, height, (int)status, error.offset, error.context);
    }
    CHECK(status == AV2H_OK);
    CHECK(sequence.seq_profile_idc == 0U);
    CHECK(sequence.seq_level_idx == 0U);
    CHECK(sequence.chroma_format_idc == 0U);
    CHECK(sequence.bit_depth_idc == 1U);
    CHECK(sequence.still_picture == 1U);
    CHECK(sequence.monotonic_output_order_flag == 1U);
    CHECK(sequence.film_grain_params_present == 0U);
    CHECK(sequence.max_frame_width_minus_1 == width - 1U);
    CHECK(sequence.max_frame_height_minus_1 == height - 1U);
    CHECK(av2h_sequence_state_parse((Av2hSpan){ bytes, size }, &entry,
                                    &state, &error) == AV2H_OK);
    CHECK(state.num_bits_width == width_bits);
    CHECK(state.num_bits_height == height_bits);
    CHECK(state.force_screen_content_tools == 2U);
    CHECK(state.force_integer_mv == 2U);
    CHECK(state.enable_tcq == 0U);
    CHECK(state.enable_sdp == 1U);
    CHECK(state.enable_mrls == 1U);
    CHECK(state.enable_fsc == 1U);
    CHECK(state.enable_idtx_intra == 1U);
    CHECK(state.enable_ist == 1U);
    CHECK(state.enable_cfl_intra == 1U);
    CHECK(state.enable_mhccp == 1U);
    CHECK(state.enable_cctx == 1U);
    CHECK(state.enable_cdef == enable_cdef);
    CHECK(state.enable_gdf == enable_gdf);
    CHECK(state.enable_restoration == 0U);
    CHECK(state.enable_ccso == enable_ccso);
    CHECK(state.enable_parity_hiding == 0U);
    CHECK(state.enable_ext_seg == 0U);
    CHECK(state.seq_seg_info_present_flag == 0U);
    CHECK(state.seq_tile_info_present_flag == 0U);
}

static void test_sequence_headers(void) {
    static const unsigned char tiny[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
        0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
    };
    static const unsigned char odd[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x43U, 0x87U, 0x0eU,
        0x1cU, 0xe6U, 0xf2U, 0x37U, 0x32U, 0x44U
    };
    static const unsigned char ordinary[] = {
        0x0dU, 0x04U, 0x82U, 0x0aU, 0x76U, 0x9fU, 0xeeU,
        0x38U, 0x7bU, 0x9bU, 0xc8U, 0xdcU, 0x92U, 0x20U
    };
    static const unsigned char detail[] = {
        0x0dU, 0x04U, 0x82U, 0x0aU, 0x66U, 0xbfU, 0x3cU,
        0x70U, 0xe7U, 0x37U, 0x91U, 0xb8U, 0x24U, 0x40U
    };
    unsigned char film_grain[sizeof(tiny)];
    static const unsigned char short_sequence[] = {
        0x04U, 0x04U, 0x82U, 0x0aU, 0x33U
    };
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hSequenceInfo sequence;
    Av2hError error;
    check_sequence(tiny, sizeof(tiny), 16U, 16U, 4U, 4U, 1U, 0U, 0U);
    check_sequence(odd, sizeof(odd), 17U, 15U, 5U, 4U, 1U, 1U, 1U);
    check_sequence(ordinary, sizeof(ordinary), 160U, 120U,
                   8U, 7U, 1U, 0U, 1U);
    check_sequence(detail, sizeof(detail), 96U, 80U,
                   7U, 7U, 0U, 0U, 1U);
    av2h_memory_copy(film_grain, tiny, sizeof(tiny));
    film_grain[sizeof(film_grain) - 1U] |= 0x80U;
    CHECK(av2h_obu_parse((Av2hSpan){ film_grain, sizeof(film_grain) },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_sequence_header_parse(
              (Av2hSpan){ film_grain, sizeof(film_grain) }, &entry,
              &sequence, &error) == AV2H_UNSUPPORTED);
    CHECK(error.context == AV2H_CONTEXT_SEQUENCE_HEADER &&
          error.obu_type == AV2H_OBU_SEQUENCE_HEADER);
    CHECK(av2h_obu_parse((Av2hSpan){ short_sequence, sizeof(short_sequence) },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_sequence_header_parse(
              (Av2hSpan){ short_sequence, sizeof(short_sequence) }, &entry,
              &sequence, &error) == AV2H_TRUNCATED);
}

static void test_av2c_and_carriage(void) {
    static const unsigned char config_obu[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
        0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
    };
    static const unsigned char payload[] = {
        0x63U, 0x10U, 0xe3U, 0x00U, 0x33U, 0xd0U, 0xb8U, 0x70U,
        0x00U, 0xd7U, 0xc3U, 0x43U, 0x86U, 0xffU, 0x01U, 0xfcU,
        0x2aU, 0x52U, 0xaaU, 0x8fU, 0x5dU, 0x63U, 0xc5U, 0x9cU,
        0x36U, 0x73U, 0x4cU, 0xe8U, 0x9eU, 0xe7U, 0x78U, 0x71U,
        0x35U, 0x9cU, 0x23U, 0x32U, 0xb7U, 0xaaU, 0x18U, 0x5cU,
        0x83U, 0x17U, 0x25U, 0x0fU, 0xfdU, 0xb5U, 0x2aU, 0x0fU,
        0xdbU, 0x55U, 0xb9U, 0xd5U, 0xe7U, 0xc8U, 0x3eU, 0xebU,
        0xf5U, 0x35U, 0x75U, 0xb2U, 0xd6U, 0xd4U, 0xc1U, 0x5bU,
        0xe2U, 0xc6U, 0x99U, 0x46U, 0x47U, 0x4eU, 0x59U, 0x14U,
        0xa2U, 0xe0U, 0x35U, 0x23U, 0x50U, 0x71U, 0x5bU, 0xc0U,
        0x5dU, 0x24U, 0x8bU, 0xa5U, 0x4bU, 0xcbU, 0x90U, 0xa9U,
        0x75U, 0xfcU, 0xb0U, 0xbaU, 0x59U, 0xf2U, 0x43U, 0xc5U,
        0x73U, 0x44U, 0xeeU, 0xd8U
    };
    Av2hSequenceInfo sequence;
    Av2hAv2C configuration;
    Av2hAv2C parsed;
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hObuLedger no_storage = { 0, 0U, 0U };
    Av2hError error;
    unsigned char bytes[64];
    size_t written;
    CHECK(av2h_carriage_validate(
              (Av2hSpan){ config_obu, sizeof(config_obu) },
              (Av2hSpan){ payload, sizeof(payload) }, &sequence, &error) == AV2H_OK);
    configuration.configuration_version = 1U;
    configuration.sequence = sequence;
    configuration.config_obus.data = config_obu;
    configuration.config_obus.size = sizeof(config_obu);
    CHECK(av2h_av2c_write(&configuration, bytes, sizeof(bytes), &written) == AV2H_OK);
    CHECK(written == 9U + sizeof(config_obu));
    CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written }, &parsed,
                          &ledger, &error) == AV2H_OK);
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written }, &parsed,
                      &no_storage, &error) == AV2H_LIMIT_EXCEEDED);
        CHECK(av2h_av2c_write(&configuration, 0, 0U, &written) == AV2H_OK);
        CHECK(written == 9U + sizeof(config_obu));
        CHECK(av2h_av2c_write(&configuration, bytes, 8U, &written) ==
            AV2H_OUT_OF_MEMORY);
        CHECK(av2h_av2c_write(&configuration, bytes, sizeof(bytes), &written) == AV2H_OK);
    bytes[1] ^= 0x08U;
    CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written }, &parsed,
                          &ledger, &error) == AV2H_INVALID_DATA);
        bytes[1] ^= 0x08U;
        bytes[4] |= 0x01U;
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written }, &parsed,
                      &ledger, &error) == AV2H_INVALID_DATA);
        bytes[4] &= 0xfeU;
        bytes[3] |= 0x10U;
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written }, &parsed,
                      &ledger, &error) == AV2H_UNSUPPORTED);
        bytes[3] &= 0xefU;
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, 8U }, &parsed,
                      &ledger, &error) == AV2H_TRUNCATED);
        bytes[written] = 0x01U;
        bytes[written + 1U] = (unsigned char)(AV2H_OBU_PADDING << 2);
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written + 2U }, &parsed,
                      &no_storage, &error) == AV2H_INVALID_DATA);
        CHECK(error.context == AV2H_CONTEXT_AV2C && error.obu_index == 1U &&
            error.obu_type == AV2H_OBU_PADDING && error.offset == written);
        CHECK(av2h_av2c_parse((Av2hSpan){ bytes, written + 2U }, &parsed,
                      &ledger, &error) == AV2H_INVALID_DATA);
        configuration.sequence.film_grain_params_present = 1U;
        CHECK(av2h_av2c_write(&configuration, bytes, sizeof(bytes), &written) ==
            AV2H_UNSUPPORTED);
}

    static void test_carriage_rejections(void) {
        static const unsigned char config_obu[] = {
          0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
          0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
        };
        static const unsigned char valid_payload[] = { 0x02U, 0x10U, 0xaaU };
        static const unsigned char empty_payload[] = { 0x01U, 0x10U };
        static const unsigned char extra_payload[] = {
          0x02U, 0x10U, 0xaaU, 0x01U, 0x64U
        };
        static const unsigned char extended_payload[] = {
          0x03U, 0x90U, 0x00U, 0xaaU
        };
        unsigned char duplicate_config[sizeof(config_obu) * 2U];
        Av2hSequenceInfo sequence;
        Av2hError error;
        av2h_memory_copy(duplicate_config, config_obu, sizeof(config_obu));
        av2h_memory_copy(duplicate_config + sizeof(config_obu),
                   config_obu, sizeof(config_obu));
        CHECK(av2h_carriage_validate(
              (Av2hSpan){ config_obu, sizeof(config_obu) },
              (Av2hSpan){ valid_payload, sizeof(valid_payload) },
              &sequence, &error) == AV2H_OK);
        CHECK(av2h_carriage_validate(
              (Av2hSpan){ config_obu, sizeof(config_obu) },
              (Av2hSpan){ empty_payload, sizeof(empty_payload) },
              &sequence, &error) == AV2H_INVALID_DATA);
        CHECK(error.context == AV2H_CONTEXT_OBU_ORDER && error.obu_index == 0U &&
            error.obu_type == AV2H_OBU_CLOSED_LOOP_KEY);
        CHECK(av2h_carriage_validate(
              (Av2hSpan){ config_obu, sizeof(config_obu) },
              (Av2hSpan){ extra_payload, sizeof(extra_payload) },
              &sequence, &error) == AV2H_INVALID_DATA);
        CHECK(error.context == AV2H_CONTEXT_OBU_ORDER && error.obu_index == 1U &&
            error.obu_type == AV2H_OBU_PADDING && error.offset == 3U);
        CHECK(av2h_carriage_validate(
              (Av2hSpan){ duplicate_config, sizeof(duplicate_config) },
              (Av2hSpan){ valid_payload, sizeof(valid_payload) },
              &sequence, &error) == AV2H_INVALID_DATA);
        CHECK(error.context == AV2H_CONTEXT_OBU_ORDER && error.obu_index == 1U &&
            error.obu_type == AV2H_OBU_SEQUENCE_HEADER &&
            error.offset == sizeof(config_obu));
        CHECK(av2h_carriage_validate(
              (Av2hSpan){ config_obu, sizeof(config_obu) },
              (Av2hSpan){ extended_payload, sizeof(extended_payload) },
              &sequence, &error) == AV2H_INVALID_DATA);
    }

static void test_content_interpretation(void) {
    static const unsigned char reserved_id[] = {
        0x03U, 0x60U, 0x20U, 0xaaU
    };
    static const unsigned char truncated[] = { 0x02U, 0x60U, 0x20U };
    unsigned char bytes[16];
    Av2hBitWriter writer;
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    Av2hColorInfo color;
    Av2hError error;
    size_t payload_size;
    bytes[1] = (unsigned char)(AV2H_OBU_CONTENT_INTERPRETATION << 2);
    av2h_bit_writer_init(&writer, bytes + 2U, sizeof(bytes) - 2U);
    (void)av2h_bit_writer_write(&writer, 0U, 2U);
    (void)av2h_bit_writer_write(&writer, 1U, 1U);
    (void)av2h_bit_writer_write(&writer, 0U, 5U);
    (void)av2h_bit_writer_write(&writer, 0U, 3U);
    (void)av2h_bit_writer_write(&writer, 1U, 8U);
    (void)av2h_bit_writer_write(&writer, 13U, 8U);
    (void)av2h_bit_writer_write(&writer, 0U, 8U);
    (void)av2h_bit_writer_write(&writer, 1U, 1U);
    (void)av2h_bit_writer_write(&writer, 0U, 1U);
    (void)av2h_bit_writer_write(&writer, 1U, 1U);
    (void)av2h_bit_writer_align_zero(&writer);
    payload_size = av2h_bit_writer_required(&writer);
    bytes[0] = (unsigned char)(payload_size + 1U);
    CHECK(av2h_obu_parse((Av2hSpan){ bytes, payload_size + 2U },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_content_interpretation_parse(
              (Av2hSpan){ bytes, payload_size + 2U }, &entry,
              &color, &error) == AV2H_OK);
    CHECK(color.color_primaries == 1U);
    CHECK(color.transfer_characteristics == 13U);
    CHECK(color.matrix_coefficients == 0U);
    CHECK(color.full_range_flag == 1U);
    CHECK(av2h_obu_parse((Av2hSpan){ reserved_id, sizeof(reserved_id) },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_content_interpretation_parse(
              (Av2hSpan){ reserved_id, sizeof(reserved_id) }, &entry,
              &color, &error) == AV2H_OK);
    CHECK(color.color_description_idc == 6U && color.color_primaries == 2U &&
          color.transfer_characteristics == 2U && color.matrix_coefficients == 2U);
    CHECK(av2h_obu_parse((Av2hSpan){ truncated, sizeof(truncated) },
                         &ledger, &error) == AV2H_OK);
    CHECK(av2h_content_interpretation_parse(
              (Av2hSpan){ truncated, sizeof(truncated) }, &entry,
              &color, &error) == AV2H_TRUNCATED);
}

static void test_box_framing(void) {
    unsigned char bytes[16];
    static const unsigned char too_small[] = {
        0U, 0U, 0U, 4U, 0x61U, 0x76U, 0x32U, 0x43U
    };
    static const unsigned char large_truncated[] = {
        0U, 0U, 0U, 1U, 0x61U, 0x76U, 0x32U, 0x43U
    };
    Av2hBoxHeader box;
    Av2hError error;
    size_t written;
    CHECK(av2h_box_header_write(0x61763243U, 12U, bytes, sizeof(bytes),
                                &written) == AV2H_OK);
    CHECK(written == 8U);
    CHECK(av2h_box_header_parse((Av2hSpan){ bytes, 8U }, 0U,
                                &box, &error) == AV2H_TRUNCATED);
    bytes[0] = 0U; bytes[1] = 0U; bytes[2] = 0U; bytes[3] = 8U;
    CHECK(av2h_box_header_parse((Av2hSpan){ bytes, 8U }, 0U,
                                &box, &error) == AV2H_OK);
    CHECK(box.type == 0x61763243U && box.payload_size == 0U);
    CHECK(av2h_box_header_parse((Av2hSpan){ bytes, 7U }, 0U,
                                &box, &error) == AV2H_TRUNCATED);
    CHECK(av2h_box_header_parse((Av2hSpan){ too_small, sizeof(too_small) }, 0U,
                                &box, &error) == AV2H_INVALID_DATA);
    CHECK(av2h_box_header_parse(
              (Av2hSpan){ large_truncated, sizeof(large_truncated) }, 0U,
              &box, &error) == AV2H_TRUNCATED);
    CHECK(av2h_box_header_write(0x61763243U, UINT64_MAX, bytes, sizeof(bytes),
                                &written) == AV2H_OVERFLOW);
    CHECK(av2h_box_header_write(0x61763243U, UINT32_MAX, 0, 0U,
                                &written) == AV2H_OK);
    CHECK(written == 16U);
}

int main(void) {
    test_arithmetic();
    test_readers();
    test_raw_syntax_readers();
    test_writers();
    test_arena();
    test_leb128_and_obus();
    test_obu_header_rules();
    test_sequence_headers();
    test_av2c_and_carriage();
    test_carriage_rejections();
    test_content_interpretation();
    test_box_framing();
    if (failures != 0) return 1;
    puts("substrate_unit: ok");
    return 0;
}
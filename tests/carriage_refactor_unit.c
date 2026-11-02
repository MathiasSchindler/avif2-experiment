#include "base.h"
#include "container.h"
#include "decoder/sequence.h"

#include <inttypes.h>
#include <stdio.h>

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n",                         \
                    __FILE__, __LINE__, #condition);                             \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

typedef struct {
    uint64_t hash;
    size_t count;
} Characterization;

static void hash_bytes(Characterization *record, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;
    for (index = 0U; index < size; ++index) {
        record->hash ^= bytes[index];
        record->hash *= UINT64_C(1099511628211);
    }
}

static void hash_value(Characterization *record, uint64_t value) {
    unsigned int index;
    for (index = 0U; index < 8U; ++index) {
        unsigned char byte = (unsigned char)(value >> (index * 8U));
        hash_bytes(record, &byte, 1U);
    }
}

static void hash_values(Characterization *record,
                        const uint64_t *values, size_t count) {
    size_t index;
    for (index = 0U; index < count; ++index) hash_value(record, values[index]);
}

static Av2hError seeded_error(void) {
    const Av2hError error = {
        AV2H_OUT_OF_MEMORY, 11U, 12U, 13U, 14U, 15U, 16U,
        17U, 18U, 19U, 20U, 21U, 22U
    };
    return error;
}

static void hash_error(Characterization *record, const Av2hError *error,
                        int full) {
    const uint64_t core[] = {
        error->status, error->offset, error->context,
        error->obu_index == SIZE_MAX ? UINT64_MAX : error->obu_index,
        error->obu_type
    };
    hash_values(record, core, sizeof(core) / sizeof(core[0]));
    if (full) {
        const uint64_t diagnostics[] = {
            error->diagnostic_stage, error->diagnostic_reason,
            error->syntax_stage, error->plane, error->row, error->column,
            error->entropy_tell, error->parsed_value
        };
        hash_values(record, diagnostics,
                    sizeof(diagnostics) / sizeof(diagnostics[0]));
    }
}

static void hash_span(Characterization *record, Av2hSpan span, Av2hSpan input) {
    uintptr_t address = (uintptr_t)span.data;
    uintptr_t base = (uintptr_t)input.data;
    if (span.data == 0) {
        hash_value(record, 0U);
    } else {
        CHECK(address >= base && address - base <= input.size);
        hash_value(record, address - base + 1U);
    }
    hash_value(record, span.size);
}

static void hash_sequence(Characterization *record,
                           const Av2hSequenceInfo *sequence) {
    const uint64_t fields[] = {
        sequence->seq_profile_idc, sequence->seq_level_idx, sequence->seq_tier,
        sequence->chroma_format_idc, sequence->bit_depth_idc,
        sequence->monotonic_output_order_flag, sequence->still_picture,
        sequence->film_grain_params_present,
        sequence->initial_display_delay_present,
        sequence->initial_display_delay_minus_1,
        sequence->max_frame_width_minus_1, sequence->max_frame_height_minus_1
    };
    hash_values(record, fields, sizeof(fields) / sizeof(fields[0]));
}

static void hash_color(Characterization *record, const Av2hColorInfo *color) {
    const uint64_t fields[] = {
        color->color_description_present, color->color_description_idc,
        color->color_primaries, color->transfer_characteristics,
        color->matrix_coefficients, color->full_range_flag,
        color->chroma_sample_position_x, color->chroma_sample_position_y
    };
    hash_values(record, fields, sizeof(fields) / sizeof(fields[0]));
}

static void hash_obu(Characterization *record, const Av2hObu *entry) {
    const uint64_t fields[] = {
        entry->index, entry->offset, entry->length_field_size,
        entry->obu_size, entry->header_size, entry->payload_offset,
        entry->payload_size, entry->type, entry->temporal_id,
        entry->mlayer_id, entry->xlayer_id, entry->extension_flag
    };
    hash_values(record, fields, sizeof(fields) / sizeof(fields[0]));
}

static void hash_state(Characterization *record, const Av2hSequenceState *state) {
    const uint64_t fields[] = {
        state->sequence_header_id, state->superblock_size,
        state->num_bits_width, state->num_bits_height,
        state->force_screen_content_tools, state->force_integer_mv,
        state->enable_sdp, state->enable_ext_partitions,
        state->enable_uneven_4way_partitions,
        state->max_pb_aspect_ratio_log2_minus_1, state->enable_ext_seg,
        state->seq_seg_info_present_flag, state->seg_allow_info_change,
        state->seg_last_active_id, state->enable_intra_dip,
        state->enable_intra_edge_filter, state->enable_mrls,
        state->enable_cfl_intra, state->cfl_downsample_filter_index,
        state->enable_mhccp, state->enable_ibp, state->enable_refmvbank,
        state->enable_drl_reorder, state->default_max_bvp_drl_bits,
        state->allow_frame_max_bvp_drl_bits, state->enable_bawp,
        state->enable_fsc, state->enable_idtx_intra, state->enable_ist,
        state->enable_inter_ist, state->enable_chroma_dctonly,
        state->reduced_tx_partition_set, state->enable_cctx, state->enable_tcq,
        state->enable_parity_hiding, state->separate_uv_delta_q,
        state->equal_ac_dc_q, state->y_dc_delta_q_enabled,
        state->uv_dc_delta_q_enabled, state->uv_ac_delta_q_enabled,
        (uint64_t)state->base_y_dc_delta_q,
        (uint64_t)state->base_uv_dc_delta_q,
        (uint64_t)state->base_uv_ac_delta_q,
        state->disable_loopfilters_across_tiles, state->enable_cdef,
        state->enable_gdf, state->gdf_unit_matches_superblock_size,
        state->enable_restoration, state->lr_tools_disable_mask[0],
        state->lr_tools_disable_mask[1], state->enable_ccso,
        state->ccso_unit_matches_superblock_size,
        state->deblock_parameter_bits_minus_2,
        state->seq_tile_info_present_flag, state->allow_tile_info_change,
        state->uniform_tile_spacing, state->tile_column_log2,
        state->tile_row_log2, state->tile_column_count, state->tile_row_count,
        state->tile_sb_columns, state->tile_sb_rows
    };
    size_t index;
    hash_sequence(record, &state->info);
    hash_values(record, fields, sizeof(fields) / sizeof(fields[0]));
    for (index = 0U; index < AV2H_SEGMENT_COUNT; ++index) {
        hash_value(record, state->seg_feature_mask[index]);
        hash_value(record, (uint64_t)state->seg_alt_q[index]);
    }
    for (index = 0U; index <= AV2H_MAX_TILE_COLUMNS; ++index) {
        hash_value(record, state->tile_column_start_sb[index]);
    }
    for (index = 0U; index <= AV2H_MAX_TILE_ROWS; ++index) {
        hash_value(record, state->tile_row_start_sb[index]);
    }
}

static void test_framing(Characterization *leb, Characterization *obus) {
    static const unsigned char tails[] = { 0U, 1U, 30U, 31U, 32U, 63U, 224U, 255U };
    unsigned int first;
    size_t tail;
    for (first = 0U; first < 256U; ++first) {
        for (tail = 0U; tail < sizeof(tails); ++tail) {
            unsigned char encoded[] = { (unsigned char)first, tails[tail] };
            unsigned char bytes[] = { 2U, (unsigned char)first, tails[tail] };
            size_t length;
            for (length = 0U; length <= sizeof(encoded); ++length) {
                Av2hError error = seeded_error();
                size_t value = 23U;
                size_t consumed = 24U;
                hash_value(leb, av2h_leb128_read(
                    (Av2hSpan){ encoded, length }, &value, &consumed, &error));
                hash_value(leb, value);
                hash_value(leb, consumed);
                hash_error(leb, &error, 1);
                ++leb->count;
            }
            for (length = 0U; length <= sizeof(bytes); ++length) {
                size_t capacity;
                for (capacity = 0U; capacity <= 2U; ++capacity) {
                    Av2hObu entries[2] = { { 0 } };
                    Av2hObuLedger ledger = { entries, capacity, 0U };
                    Av2hError error = seeded_error();
                    size_t index;
                    hash_value(obus, av2h_obu_parse(
                        (Av2hSpan){ bytes, length }, &ledger, &error));
                    hash_error(obus, &error, 1);
                    hash_value(obus, ledger.count);
                    for (index = 0U; index < 2U; ++index) {
                        hash_obu(obus, &entries[index]);
                    }
                    ++obus->count;
                }
            }
        }
    }
}

static void sequence_case(Characterization *record, Av2hSpan input,
                           const Av2hObu *entry) {
    Av2hSequenceState state = { 0 };
    Av2hSequenceInfo info = { 0 };
    Av2hError error = seeded_error();
    Av2hError public_error = seeded_error();
    Av2hStatus status = av2h_sequence_state_parse(input, entry, &state, &error);
    Av2hStatus public_status = av2h_sequence_header_parse(
        input, entry, &info, &public_error);
    CHECK(status == public_status);
    hash_value(record, status);
    hash_error(record, &error, 1);
    hash_state(record, &state);
    hash_sequence(record, &info);
    hash_error(record, &public_error, 1);
    ++record->count;
}

static void test_sequence(Characterization *record, Av2hSpan config) {
    Av2hObu entry;
    Av2hObuLedger ledger = { &entry, 1U, 0U };
    unsigned char bytes[256];
    size_t length;
    size_t index;
    Av2hStatus status;
    CHECK(config.size <= sizeof(bytes));
    if (config.size > sizeof(bytes)) return;
    status = av2h_obu_parse(config, &ledger, 0);
    CHECK(status == AV2H_OK);
    if (status != AV2H_OK) return;
    av2h_memory_copy(bytes, config.data, config.size);
    for (length = 0U; length <= entry.payload_size; ++length) {
        Av2hObu truncated = entry;
        truncated.payload_size = length;
        sequence_case(record,
            (Av2hSpan){ bytes, entry.payload_offset + length }, &truncated);
    }
    for (index = entry.payload_offset; index < config.size; ++index) {
        unsigned int bit;
        for (bit = 0U; bit < 8U; ++bit) {
            bytes[index] ^= (unsigned char)(1U << bit);
            sequence_case(record, (Av2hSpan){ bytes, config.size }, &entry);
            bytes[index] ^= (unsigned char)(1U << bit);
        }
    }
}

static void test_content(Characterization *record) {
    static const unsigned char tails[] = { 0U, 0x80U, 0xa4U, 0xffU };
    unsigned int first;
    size_t tail;
    for (first = 0U; first < 256U; ++first) {
        for (tail = 0U; tail < sizeof(tails); ++tail) {
            unsigned char bytes[] = { 3U, 0x60U, (unsigned char)first, tails[tail] };
            Av2hObu entry;
            Av2hObuLedger ledger = { &entry, 1U, 0U };
            Av2hColorInfo color = { 0 };
            Av2hError error = seeded_error();
            CHECK(av2h_obu_parse((Av2hSpan){ bytes, sizeof(bytes) },
                                  &ledger, 0) == AV2H_OK);
            hash_value(record, av2h_content_interpretation_parse(
                (Av2hSpan){ bytes, sizeof(bytes) }, &entry, &color, &error));
            hash_error(record, &error, 1);
            hash_color(record, &color);
            ++record->count;
        }
    }
}

static void configuration_case(Characterization *record, Av2hSpan input) {
    Av2hAv2C configuration = { 0 };
    Av2hObu entries[2] = { { 0 } };
    Av2hObuLedger ledger = { entries, 2U, 0U };
    Av2hError error = seeded_error();
    hash_value(record, av2h_av2c_parse(input, &configuration, &ledger, &error));
    hash_error(record, &error, 1);
    hash_value(record, ledger.count);
    hash_obu(record, &entries[0]);
    hash_obu(record, &entries[1]);
    hash_value(record, configuration.configuration_version);
    hash_sequence(record, &configuration.sequence);
    hash_span(record, configuration.config_obus, input);
    ++record->count;
}

static void container_case(Characterization *record, Av2hSpan input) {
    Av2hHeifImage image = { 0 };
    Av2hContainerProbe probe = { 0 };
    Av2hDialect dialect = AV2H_DIALECT_UNKNOWN;
    Av2hError error = seeded_error();
    hash_value(record, av2h_heif_parse(input, &image, &error));
    hash_error(record, &error, 1);
    hash_span(record, image.source.config_obus, input);
    hash_span(record, image.source.item_payload, input);
    hash_sequence(record, &image.sequence);
    hash_color(record, &image.source.color);
    {
        const uint64_t fields[] = {
            image.width, image.height, image.channel_count, image.bits_per_channel,
            image.property_count, image.pixel_format.chroma_format_idc,
            image.pixel_format.bit_depth, image.pixel_format.bytes_per_sample,
            image.pixel_format.sample_storage, image.pixel_format.byte_order,
            image.pixel_format.plane_order
        };
        hash_values(record, fields, sizeof(fields) / sizeof(fields[0]));
    }
    error = seeded_error();
    hash_value(record, av2h_container_probe(input, &probe, &error));
    hash_error(record, &error, 1);
    hash_span(record, probe.av2c_payload, input);
    hash_span(record, probe.item_payload, input);
    hash_value(record, probe.has_mif1_brand);
    hash_value(record, probe.has_avif_brand);
    hash_value(record, probe.has_primary_av02);
    hash_value(record, probe.av2c_associated);
    error = seeded_error();
    hash_value(record, av2h_dialect_detect(input, &dialect, &error));
    hash_value(record, dialect);
    /* Strict-error forwarding does not initialize the extended fields yet. */
    hash_error(record, &error, 0);
    ++record->count;
}

static void test_source(Characterization *sequence, Characterization *configuration,
                         Characterization *container, const Av2hHeifSource *source) {
    Av2hAv2C config = { 1U, { 0 }, { 0, 0U } };
    unsigned char encoded[256];
    unsigned char bytes[4096];
    size_t written = 0U;
    size_t length;
    size_t index;
    Av2hError error = seeded_error();
    Av2hObu entries[2];
    Av2hObuLedger ledger = { entries, 2U, 0U };
    Av2hStatus status = av2h_obu_parse(source->config_obus, &ledger, &error);
    CHECK(status == AV2H_OK);
    if (status != AV2H_OK) return;
    test_sequence(sequence, (Av2hSpan){
        source->config_obus.data, entries[0].length_field_size + entries[0].obu_size
    });
    status = av2h_carriage_validate(source->config_obus, source->item_payload,
                                     &config.sequence, &error);
    CHECK(status == AV2H_OK);
    if (status != AV2H_OK) return;
    config.config_obus = source->config_obus;
    status = av2h_av2c_write(&config, encoded, sizeof(encoded), &written);
    CHECK(status == AV2H_OK);
    if (status != AV2H_OK) return;
    hash_bytes(configuration, encoded, written);
    for (length = 0U; length <= written; ++length) {
        configuration_case(configuration, (Av2hSpan){ encoded, length });
    }
    for (index = 0U; index < written; ++index) {
        unsigned int bit;
        for (bit = 0U; bit < 8U; ++bit) {
            encoded[index] ^= (unsigned char)(1U << bit);
            configuration_case(configuration, (Av2hSpan){ encoded, written });
            encoded[index] ^= (unsigned char)(1U << bit);
        }
    }
    status = av2h_heif_write(source, bytes, sizeof(bytes), &written, &error);
    CHECK(status == AV2H_OK && written != 0U && written < sizeof(bytes));
    if (status != AV2H_OK || written == 0U || written >= sizeof(bytes)) return;
    hash_bytes(container, bytes, written);
    for (length = 0U; length <= written; ++length) {
        container_case(container, (Av2hSpan){ bytes, length });
    }
    for (index = 0U; index < written; ++index) {
        unsigned int bit;
        for (bit = 0U; bit < 8U; ++bit) {
            bytes[index] ^= (unsigned char)(1U << bit);
            container_case(container, (Av2hSpan){ bytes, written });
            bytes[index] ^= (unsigned char)(1U << bit);
        }
    }
    for (index = 0U; index < 4U; ++index) {
        const size_t capacities[] = { 0U, written - 1U, written, written + 1U };
        unsigned char output[4096];
        size_t required = 123U;
        error = seeded_error();
        av2h_memory_fill(output, 0x5aU, sizeof(output));
        hash_value(container, av2h_heif_write(
            source, output, capacities[index], &required, &error));
        hash_error(container, &error, 1);
        hash_value(container, required);
        hash_bytes(container, output, sizeof(output));
        ++container->count;
    }
}

static void test_fixture(Characterization *sequence, Characterization *configuration,
                          Characterization *container, const char *path) {
    unsigned char bytes[4096];
    Av2hObu entries[8];
    Av2hObuLedger ledger = { entries, 8U, 0U };
    Av2hHeifSource source = { { 0, 0U }, { 0, 0U }, { 0 } };
    FILE *file = fopen(path, "rb");
    size_t size;
    size_t index;
    int readable;
    Av2hStatus status;
    CHECK(file != 0);
    if (file == 0) return;
    size = fread(bytes, 1U, sizeof(bytes), file);
    readable = ferror(file) == 0 && feof(file);
    CHECK(readable);
    CHECK(fclose(file) == 0);
    if (!readable) return;
    status = av2h_obu_parse((Av2hSpan){ bytes, size }, &ledger, 0);
    CHECK(status == AV2H_OK);
    if (status != AV2H_OK) return;
    for (index = 0U; index < ledger.count; ++index) {
        Av2hObu *entry = &entries[index];
        Av2hSpan span = { bytes + entry->offset,
                         entry->length_field_size + entry->obu_size };
        if (entry->type == AV2H_OBU_SEQUENCE_HEADER) source.config_obus = span;
        if (entry->type == AV2H_OBU_CLOSED_LOOP_KEY) source.item_payload = span;
    }
    CHECK(source.config_obus.data != 0 && source.item_payload.data != 0);
    if (source.config_obus.data == 0 || source.item_payload.data == 0) return;
    av2h_color_info_default(&source.color);
    test_source(sequence, configuration, container, &source);
}

int main(void) {
    static const unsigned char config[] = {
        0x0cU, 0x04U, 0x82U, 0x0aU, 0x33U, 0xffU, 0x1cU,
        0x39U, 0xcdU, 0xe4U, 0x6eU, 0x42U, 0x20U
    };
    static const unsigned char payload[] = { 0x02U, 0x10U, 0xaaU };
    static const unsigned char content[] = { 0x03U, 0x60U, 0x10U, 0xa4U };
    static const char *const paths[] = {
        "tests/fixtures/monochrome-tiny.obu",
        "tests/fixtures/i444-tiny.obu",
        "tests/fixtures/10bit/avm-reference/tiny-constrained-16x16-yuv420p10le-qp208.obu",
        "tests/fixtures/10bit/avm-reference/tiny-precision-16x16-yuv444p10le-qp160.obu"
    };
    static const char *const names[] = {
        "leb128", "obu", "sequence", "content", "configuration", "container"
    };
    static const uint64_t expected[] = {
        UINT64_C(0x1ab561e3a7fc4825), UINT64_C(0x33c82387f94cd6c5),
        UINT64_C(0x11dd8e51712eb827), UINT64_C(0x1986dc98fde279c5),
        UINT64_C(0x930c0e080d97ed0c), UINT64_C(0xee7fea65ce3bd619)
    };
    Characterization records[6];
    unsigned char config_with_content[sizeof(config) + sizeof(content)];
    Av2hHeifSource source = {
        { config, sizeof(config) }, { payload, sizeof(payload) }, { 0 }
    };
    size_t index;
    for (index = 0U; index < 6U; ++index) {
        records[index].hash = UINT64_C(14695981039346656037);
        records[index].count = 0U;
    }
    test_framing(&records[0], &records[1]);
    test_content(&records[3]);
    av2h_color_info_default(&source.color);
    test_source(&records[2], &records[4], &records[5], &source);
    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        test_fixture(&records[2], &records[4], &records[5], paths[index]);
    }
    av2h_memory_copy(config_with_content, config, sizeof(config));
    av2h_memory_copy(config_with_content + sizeof(config), content, sizeof(content));
    source.config_obus = (Av2hSpan){ config_with_content, sizeof(config_with_content) };
    source.color.chroma_sample_position_x = AV2H_CHROMA_SAMPLE_POSITION_LEFT;
    source.color.chroma_sample_position_y = AV2H_CHROMA_SAMPLE_POSITION_CENTER;
    test_source(&records[2], &records[4], &records[5], &source);
    for (index = 0U; index < 6U; ++index) {
        printf("%s: %zu cases, %016" PRIx64 "\n",
               names[index], records[index].count, records[index].hash);
        CHECK(records[index].hash == expected[index]);
    }
    return failures != 0;
}

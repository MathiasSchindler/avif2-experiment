#include "container.h"
#include "decode.h"
#include "error.h"
#include "output_layout.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int failures;
static unsigned char raw[16384U];
static _Alignas(max_align_t) unsigned char container[2U * 1024U * 1024U];
static _Alignas(max_align_t) unsigned char workspace[2U * 1024U * 1024U];
static _Alignas(max_align_t) unsigned char output[32768U];
static size_t raw_size;
static size_t container_size;
static Av2hObu sequence_obu;
static Av2hObu frame_obu;
static Av2hInfo info;
static Av2hDecodePreparation preparation;
static Av2hFrameSyntaxStorage preparation_syntax;
static Av2hTileGroup preparation_group;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
        return; \
    } \
} while (0)

typedef struct {
    uint64_t hash;
    size_t count;
} Record;

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const unsigned char *bytes = data;
    size_t index;
    for (index = 0U; index < size; ++index) {
        hash = (hash ^ bytes[index]) * UINT64_C(1099511628211);
    }
    return hash;
}

static void hash_value(Record *record, uint64_t value) {
    unsigned int byte;
    for (byte = 0U; byte < 8U; ++byte) {
        unsigned char part = (unsigned char)(value >> (byte * 8U));
        record->hash = hash_bytes(record->hash, &part, 1U);
    }
}

static void seed_error(Av2hError *error, unsigned int seed) {
    memset(error, 0xa5, sizeof(*error));
    error->status = AV2H_OUT_OF_MEMORY;
    error->offset = 11U + seed;
    error->context = 12U + seed;
    error->obu_index = 13U + seed;
    error->obu_type = (uint8_t)(14U ^ seed);
    error->diagnostic_stage = (uint8_t)(15U ^ seed);
    error->diagnostic_reason = (uint8_t)(16U ^ seed);
    error->syntax_stage = (uint8_t)(17U ^ seed);
    error->plane = (uint8_t)(18U ^ seed);
    error->row = (uint16_t)(19U + seed);
    error->column = (uint16_t)(20U + seed);
    error->entropy_tell = 21U + seed;
    error->parsed_value = 22U + seed;
}

static void record_error(Record *record, Av2hStatus status,
                         const Av2hError *error) {
    const uint64_t fields[] = {
        status, error->status, error->offset, error->context,
        error->obu_index == SIZE_MAX ? UINT64_MAX : error->obu_index,
        error->obu_type, error->diagnostic_stage, error->diagnostic_reason,
        error->syntax_stage, error->plane, error->row, error->column,
        error->entropy_tell, error->parsed_value
    };
    size_t index;
    for (index = 0U; index < sizeof(fields) / sizeof(fields[0]); ++index) {
        hash_value(record, fields[index]);
    }
    ++record->count;
}

static void check_record(const char *name, const Record *record,
                          size_t count, uint64_t expected) {
    printf("%s: %zu cases %016" PRIx64 "\n", name, record->count, record->hash);
    CHECK(record->count == count);
    CHECK(record->hash == expected);
}

static void test_error_helpers(void) {
    static const unsigned int seeds[] = { 0U, 165U, 255U };
    static const size_t offsets[] = { 0U, 1U, SIZE_MAX - 1U, SIZE_MAX };
    size_t core_cases = 0U;
    size_t diagnostic_cases = 0U;
    size_t seed;
    for (seed = 0U; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        unsigned int status;
        unsigned int context;
        size_t offset;
        unsigned int stage;
        unsigned int reason;
        for (status = AV2H_OK; status <= AV2H_UNSUPPORTED; ++status) {
            for (context = 0U; context <= 19U; ++context) {
                uint32_t value = context == 19U ? UINT32_MAX : context;
                for (offset = 0U; offset < sizeof(offsets) / sizeof(offsets[0]);
                     ++offset) {
                    Av2hError error;
                    Av2hError expected;
                    seed_error(&error, seeds[seed]);
                    memcpy(&expected, &error, sizeof(error));
                    expected.status = (Av2hStatus)status;
                    expected.offset = offsets[offset];
                    expected.context = value;
                    expected.obu_index = SIZE_MAX;
                    expected.obu_type = 0U;
                    CHECK(av2h_error_set_core(&error, (Av2hStatus)status,
                              offsets[offset], value) == (Av2hStatus)status);
                    CHECK(memcmp(&error, &expected, sizeof(error)) == 0);
                    CHECK(av2h_error_set_core(0, (Av2hStatus)status,
                              offsets[offset], value) == (Av2hStatus)status);
                    ++core_cases;
                }
            }
        }
        for (stage = 0U; stage <= AV2H_DIAGNOSTIC_STAGE_COMPLETE + 1U; ++stage) {
            for (reason = 0U; reason <= AV2H_DIAGNOSTIC_REASON_RESOURCE + 1U;
                 ++reason) {
                Av2hError error;
                Av2hError expected;
                uint8_t stage_value = stage > AV2H_DIAGNOSTIC_STAGE_COMPLETE ?
                    UINT8_MAX : (uint8_t)stage;
                uint8_t reason_value = reason > AV2H_DIAGNOSTIC_REASON_RESOURCE ?
                    UINT8_MAX : (uint8_t)reason;
                seed_error(&error, seeds[seed]);
                memcpy(&expected, &error, sizeof(error));
                expected.diagnostic_stage = stage_value;
                expected.diagnostic_reason = reason_value;
                expected.syntax_stage = 0U;
                expected.plane = 0U;
                expected.row = 0U;
                expected.column = 0U;
                expected.entropy_tell = 0U;
                expected.parsed_value = 0U;
                av2h_error_reset_diagnostics(&error, stage_value, reason_value);
                CHECK(memcmp(&error, &expected, sizeof(error)) == 0);
                av2h_error_reset_diagnostics(0, stage_value, reason_value);
                ++diagnostic_cases;
            }
        }
    }
    CHECK(core_cases == 1920U);
    CHECK(diagnostic_cases == 396U);
    printf("error helpers: %zu cases exact\n", core_cases + diagnostic_cases);
}

static void prepare_fixture(void) {
    Av2hObu entries[8];
    Av2hObuLedger ledger = { entries, 8U, 0U };
    Av2hHeifSource source = { 0 };
    Av2hError error;
    size_t index;
    FILE *file = fopen("tests/fixtures/i444-high-detail.obu", "rb");
    CHECK(file != 0);
    raw_size = fread(raw, 1U, sizeof(raw), file);
    CHECK(!ferror(file) && feof(file));
    CHECK(fclose(file) == 0);
    CHECK(raw_size != 0U);
    CHECK(av2h_obu_parse((Av2hSpan){ raw, raw_size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            sequence_obu = entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            frame_obu = entries[index];
        }
    }
    CHECK(sequence_obu.type != 0U && frame_obu.type != 0U);
    source.config_obus = (Av2hSpan){
        raw + sequence_obu.offset,
        sequence_obu.length_field_size + sequence_obu.obu_size
    };
    source.item_payload = (Av2hSpan){
        raw + frame_obu.offset,
        frame_obu.length_field_size + frame_obu.obu_size
    };
    av2h_color_info_default(&source.color);
    CHECK(av2h_heif_write(&source, container, sizeof(container),
                          &container_size, &error) == AV2H_OK);
    CHECK(av2h_query((Av2hSpan){ container, container_size },
                     0, &info, &error) == AV2H_OK);
    CHECK(av2h_decode_prepare(&info.image, &preparation, &preparation_syntax,
                              &preparation_group, NULL, &error) == AV2H_OK);
    CHECK(info.workspace_size < sizeof(workspace));
    CHECK(info.output_size <= sizeof(output));
    CHECK(preparation.group->entropy.size >= 8U);
}

static void test_core_errors(void) {
    Record record = { UINT64_C(14695981039346656037), 0U };
    unsigned int seed;
    for (seed = 0U; seed <= 1U; ++seed) {
        Av2hError error;
        Av2hStatus status;
        Av2hHeifImage image;
        Av2hBoxHeader box;
        size_t index;
        unsigned int variant;
        for (index = 0U; index <= container_size; ++index) {
            seed_error(&error, seed * 165U);
            status = av2h_heif_parse((Av2hSpan){ container, index }, &image, &error);
            record_error(&record, status, &error);
        }
        for (index = 0U; index < 32U; ++index) {
            for (variant = 0U; variant < 8U; ++variant) {
                container[index] ^= (unsigned char)(1U << variant);
                seed_error(&error, seed * 165U);
                status = av2h_heif_parse(
                    (Av2hSpan){ container, container_size }, &image, &error);
                record_error(&record, status, &error);
                container[index] ^= (unsigned char)(1U << variant);
            }
        }
        for (index = 0U; index <= 24U; ++index) {
            seed_error(&error, seed * 165U);
            status = av2h_box_header_parse(
                (Av2hSpan){ container, index }, index / 2U, &box, &error);
            record_error(&record, status, &error);
        }
        for (variant = 0U; variant < 12U; ++variant) {
            Av2hLimits limits;
            Av2hInfo queried;
            av2h_limits_default(&limits);
            switch (variant) {
                case 0U: limits.max_input_size = 0U; break;
                case 1U: limits.max_config_obus_size = 0U; break;
                case 2U: limits.max_item_payload_size = 0U; break;
                case 3U: limits.max_width = 0U; break;
                case 4U: limits.max_height = 0U; break;
                case 5U: limits.max_obu_count = 0U; break;
                case 6U: limits.max_property_count = 0U; break;
                case 7U: limits.max_recursion_depth = 0U; break;
                case 8U: limits.max_pixels = 0U; break;
                case 9U: limits.max_output_size = 0U; break;
                case 10U: limits.max_workspace_size = 0U; break;
                default: break;
            }
            seed_error(&error, seed * 165U);
            status = av2h_query((Av2hSpan){ container, container_size },
                                &limits, &queried, &error);
            CHECK(status == (variant == 11U ? AV2H_OK : AV2H_LIMIT_EXCEEDED));
            record_error(&record, status, &error);
        }
        for (variant = 0U; variant < 8U; ++variant) {
            Av2hPlaneLayout planes[3];
            unsigned char before[sizeof(planes)];
            size_t output_size = 99U;
            memset(planes, 0xa5, sizeof(planes));
            memcpy(before, planes, sizeof(planes));
            seed_error(&error, seed * 165U);
            status = av2h_output_layout_calculate(
                variant == 0U ? 0U : 17U, variant == 1U ? 0U : 15U,
                variant == 2U ? 3U : 0U,
                variant == 3U ? 0U : variant == 4U ? SIZE_MAX : 1U,
                variant == 5U ? 0 : planes,
                variant == 6U ? 0 : &output_size, &error);
            if (status != AV2H_OK) {
                CHECK(memcmp(before, planes, sizeof(planes)) == 0);
                CHECK(output_size == 99U);
            }
            record_error(&record, status, &error);
        }
    }
    check_record("core errors", &record, 22188U, UINT64_C(0xf0c503598e08b4d9));
}

static void test_syntax_errors(void) {
    Record record = { UINT64_C(14695981039346656037), 0U };
    unsigned int seed;
    for (seed = 0U; seed <= 1U; ++seed) {
        Av2hError error;
        Av2hStatus status;
        Av2hSequenceState sequence;
        Av2hFrameState frame;
        size_t index;
        unsigned int bit;
        for (index = 0U; index <= raw_size; ++index) {
            Av2hObu entries[8];
            Av2hObuLedger ledger = { entries, 8U, 0U };
            seed_error(&error, seed * 165U);
            status = av2h_obu_parse((Av2hSpan){ raw, index }, &ledger, &error);
            record_error(&record, status, &error);
        }
        for (index = 0U; index <= sequence_obu.payload_size; ++index) {
            Av2hObu shortened = sequence_obu;
            shortened.payload_size = index;
            seed_error(&error, seed * 165U);
            status = av2h_sequence_state_parse(
                (Av2hSpan){ raw, raw_size }, &shortened, &sequence, &error);
            record_error(&record, status, &error);
        }
        for (index = 0U; index < sequence_obu.payload_size; ++index) {
            for (bit = 0U; bit < 8U; ++bit) {
                raw[sequence_obu.payload_offset + index] ^= (unsigned char)(1U << bit);
                seed_error(&error, seed * 165U);
                status = av2h_sequence_state_parse(
                    (Av2hSpan){ raw, raw_size }, &sequence_obu, &sequence, &error);
                record_error(&record, status, &error);
                raw[sequence_obu.payload_offset + index] ^= (unsigned char)(1U << bit);
            }
        }
        for (index = 0U; index <= 64U; ++index) {
            seed_error(&error, seed * 165U);
            status = av2h_frame_header_closed_key_parse(
                (Av2hSpan){ raw + frame_obu.payload_offset, index },
                &preparation.sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, &error);
            record_error(&record, status, &error);
        }
        for (index = 0U; index < 8U; ++index) {
            for (bit = 0U; bit < 8U; ++bit) {
                raw[frame_obu.payload_offset + index] ^= (unsigned char)(1U << bit);
                seed_error(&error, seed * 165U);
                status = av2h_frame_header_closed_key_parse(
                    (Av2hSpan){ raw + frame_obu.payload_offset, frame_obu.payload_size },
                    &preparation.sequence, &frame, &(Av2hFrameSyntaxStorage){0},  NULL, &error);
                record_error(&record, status, &error);
                raw[frame_obu.payload_offset + index] ^= (unsigned char)(1U << bit);
            }
        }
    }
    check_record("syntax errors", &record, 21534U, UINT64_C(0x32793b145c5456fd));
}

static void test_public_errors(void) {
    Record record = { UINT64_C(14695981039346656037), 0U };
    unsigned int seed;
    size_t stages[AV2H_DIAGNOSTIC_STAGE_COMPLETE + 1U] = { 0 };
    for (seed = 0U; seed <= 1U; ++seed) {
        unsigned int variant;
        for (variant = 0U; variant < 13U; ++variant) {
            Av2hError error;
            Av2hError *reported = &error;
            Av2hInfo bound = info;
            Av2hSpan input = { container, container_size };
            void *memory = workspace;
            void *destination = output;
            size_t memory_size = info.workspace_size;
            size_t destination_size = info.output_size;
            uint64_t workspace_hash;
            uint64_t output_hash;
            Av2hStatus status;
            memset(workspace, 0x5a, info.workspace_size);
            memset(output, 0xa5, info.output_size);
            seed_error(&error, seed * 165U);
            switch (variant) {
                case 1U: input = (Av2hSpan){ 0, 1U }; break;
                case 2U: ++bound.output_size; memory = 0; break;
                case 3U: memory = 0; destination = 0; break;
                case 4U: --memory_size; destination = 0; break;
                case 5U: memory = workspace + 1U; destination = 0; break;
                case 6U: destination = 0; break;
                case 7U: --destination_size; break;
                case 8U: destination = workspace; break;
                case 9U: destination = container; break;
                case 10U: memory = container; break;
                case 11U:
                    memcpy(workspace, &error, sizeof(error));
                    reported = (Av2hError *)workspace;
                    break;
                case 12U:
                    memcpy(output, &error, sizeof(error));
                    reported = (Av2hError *)output;
                    break;
                default: break;
            }
            workspace_hash = hash_bytes(0U, workspace, info.workspace_size);
            output_hash = hash_bytes(0U, output, info.output_size);
            status = av2h_decode(input, variant == 0U ? 0 : &bound,
                                 memory, memory_size, destination,
                                 destination_size, reported);
            CHECK(status != AV2H_OK);
            CHECK(hash_bytes(0U, workspace, info.workspace_size) == workspace_hash);
            CHECK(hash_bytes(0U, output, info.output_size) == output_hash);
            if (reported != &error) memcpy(&error, reported, sizeof(error));
            record_error(&record, status, &error);
        }
        for (variant = 0U; variant <= 64U; ++variant) {
            Av2hInfo queried;
            Av2hError error;
            Av2hStatus status;
            size_t index;
            size_t offset = (size_t)(preparation.group->entropy.data - container);
            unsigned char mask = variant ?
                (unsigned char)(1U << ((variant - 1U) % 8U)) : 0U;
            offset += variant ? (variant - 1U) / 8U : 0U;
            container[offset] ^= mask;
            CHECK(av2h_query((Av2hSpan){ container, container_size },
                             0, &queried, &error) == AV2H_OK);
            memset(output, 0xa5, info.output_size);
            seed_error(&error, seed * 165U);
            status = av2h_decode((Av2hSpan){ container, container_size },
                                 &queried, workspace, sizeof(workspace),
                                 output, sizeof(output), &error);
            container[offset] ^= mask;
            CHECK(error.diagnostic_stage <= AV2H_DIAGNOSTIC_STAGE_COMPLETE);
            ++stages[error.diagnostic_stage];
            if (status != AV2H_OK) {
                for (index = 0U; index < info.output_size; ++index) {
                    CHECK(output[index] == 0xa5U);
                }
            }
            record_error(&record, status, &error);
            hash_value(&record, hash_bytes(0U, output, info.output_size));
        }
    }
    CHECK(stages[AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION] != 0U);
    CHECK(stages[AV2H_DIAGNOSTIC_STAGE_COMPLETE] != 0U);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    check_record("public errors", &record, 156U, UINT64_C(0x33dcc7194e86770d));
#else
    check_record("public errors", &record, 156U, UINT64_C(0xe21571ba9b343da5));
#endif
}

int main(void) {
    test_error_helpers();
    prepare_fixture();
    if (failures != 0) return 1;
    test_core_errors();
    test_syntax_errors();
    test_public_errors();
    return failures != 0;
}

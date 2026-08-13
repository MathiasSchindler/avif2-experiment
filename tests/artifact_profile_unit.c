#include "decoder/tile.h"

#include <stdio.h>

#define STREAM_CAPACITY 65536U

typedef struct {
    const char *name;
    uint16_t width;
    uint16_t height;
    uint16_t base_q_index;
} ArtifactCase;

static const ArtifactCase artifacts[] = {
    { "chimp-93x111-qp064", 93U, 111U, 64U },
    { "chimp-240x287-qp192", 240U, 287U, 192U },
    { "chinnathurai-128x128-qp112", 128U, 128U, 112U },
    { "chinnathurai-333x333-qp224", 333U, 333U, 224U },
    { "cuckoo-157x105-qp160", 157U, 105U, 160U },
    { "cuckoo-431x287-qp080", 431U, 287U, 80U },
    { "emerald-192x96-qp208", 192U, 96U, 208U },
    { "emerald-480x240-qp128", 480U, 240U, 128U },
    { "flycatcher-223x149-qp176", 223U, 149U, 176U },
    { "flycatcher-397x265-qp048", 397U, 265U, 48U },
    { "laarim-256x171-qp096", 256U, 171U, 96U },
    { "laarim-448x299-qp192", 448U, 299U, 192U },
    { "louis-301x222-qp144", 301U, 222U, 144U },
    { "louis-463x342-qp224", 463U, 342U, 224U },
    { "pastell-320x213-qp064", 320U, 213U, 64U },
    { "pastell-512x341-qp160", 512U, 341U, 160U },
    { "schwaebisch-353x234-qp112", 353U, 234U, 112U },
    { "schwaebisch-417x277-qp208", 417U, 277U, 208U },
    { "silene-384x307-qp080", 384U, 307U, 80U },
    { "silene-479x383-qp176", 479U, 383U, 176U },
    { "chalk-271x181-qp040", 271U, 181U, 40U },
    { "chimp-166x199-qp104", 166U, 199U, 104U },
    { "chinnathurai-257x257-qp152", 257U, 257U, 152U },
    { "cuckoo-289x193-qp216", 289U, 193U, 216U },
    { "emerald-352x176-qp072", 352U, 176U, 72U },
    { "flycatcher-311x207-qp136", 311U, 207U, 136U },
    { "laarim-389x259-qp024", 389U, 259U, 24U },
    { "louis-377x279-qp184", 377U, 279U, 184U },
    { "pastell-431x287-qp232", 431U, 287U, 232U },
    { "silene-425x340-qp120", 425U, 340U, 120U },
    { "chalk-320x240-qp128", 320U, 240U, 128U }
};

static unsigned char stream[STREAM_CAPACITY];
static int failures;

static void fail(const char *name, const char *check) {
    fprintf(stderr, "%s: check failed: %s\n", name, check);
    ++failures;
}

#define CHECK_ARTIFACT(name, expression) \
    do {                                  \
        if (!(expression)) {              \
            fail((name), #expression);    \
            return;                       \
        }                                 \
    } while (0)

static size_t load_artifact(const char *name) {
    char path[256U];
    FILE *source;
    long file_size;
    size_t stream_size;

    if (snprintf(path, sizeof(path),
                 "tests/fixtures/artifact-profile/%s.obu", name) < 0) {
        return 0U;
    }
    source = fopen(path, "rb");
    if (source == 0) return 0U;
    if (fseek(source, 0L, SEEK_END) != 0) {
        fclose(source);
        return 0U;
    }
    file_size = ftell(source);
    if (file_size <= 0L || (unsigned long)file_size > STREAM_CAPACITY ||
        fseek(source, 0L, SEEK_SET) != 0) {
        fclose(source);
        return 0U;
    }
    stream_size = fread(stream, 1U, (size_t)file_size, source);
    if (stream_size != (size_t)file_size || fclose(source) != 0) return 0U;
    return stream_size;
}

static void check_artifact(const ArtifactCase *artifact) {
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    const Av2hObu *sequence_obu = 0;
    const Av2hObu *frame_obu = 0;
    size_t stream_size = load_artifact(artifact->name);
    size_t index;

    CHECK_ARTIFACT(artifact->name, stream_size != 0U);
    CHECK_ARTIFACT(
        artifact->name,
        av2h_obu_parse(
            (Av2hSpan){ stream, stream_size }, &ledger, &error) == AV2H_OK);
    for (index = 0U; index < ledger.count; ++index) {
        if (entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            CHECK_ARTIFACT(artifact->name, sequence_obu == 0);
            sequence_obu = &entries[index];
        } else if (entries[index].type == AV2H_OBU_CLOSED_LOOP_KEY) {
            CHECK_ARTIFACT(artifact->name, frame_obu == 0);
            frame_obu = &entries[index];
        }
    }
    CHECK_ARTIFACT(artifact->name, sequence_obu != 0);
    CHECK_ARTIFACT(artifact->name, frame_obu != 0);
    CHECK_ARTIFACT(
        artifact->name,
        av2h_sequence_state_parse(
            (Av2hSpan){ stream, stream_size }, sequence_obu,
            &sequence, &error) == AV2H_OK);
    CHECK_ARTIFACT(
        artifact->name,
        av2h_frame_header_closed_key_parse(
            (Av2hSpan){ stream + frame_obu->payload_offset,
                        frame_obu->payload_size },
            &sequence, &frame) == AV2H_OK);
    CHECK_ARTIFACT(artifact->name, frame.width == artifact->width);
    CHECK_ARTIFACT(artifact->name, frame.height == artifact->height);
    CHECK_ARTIFACT(
        artifact->name, frame.base_q_index == artifact->base_q_index);
    CHECK_ARTIFACT(
        artifact->name,
        av2h_tile_group_single_parse(
            (Av2hSpan){ stream + frame_obu->payload_offset,
                        frame_obu->payload_size },
            frame.uncompressed_header_bits, &group) == AV2H_OK);
    CHECK_ARTIFACT(artifact->name, group.entropy.data != 0);
    CHECK_ARTIFACT(artifact->name, group.entropy.size != 0U);
}

int main(void) {
    size_t index;
    for (index = 0U; index < sizeof(artifacts) / sizeof(artifacts[0]);
         ++index) {
        check_artifact(&artifacts[index]);
    }
    if (failures != 0) {
        fprintf(stderr, "%d artifact profile checks failed\n", failures);
        return 1;
    }
    printf("artifact profile unit: pass (%zu artifacts)\n",
           sizeof(artifacts) / sizeof(artifacts[0]));
    return 0;
}

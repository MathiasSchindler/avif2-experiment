#define _POSIX_C_SOURCE 200112L

#include "base.h"

#include "decoder/frame_output.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/partition.h"
#include "decoder/qmatrix.h"
#include "decoder/tile.h"
#include "preview.h"
#include "shared/png_writer.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AV2H_CLI_MAX_INPUT (256U * 1024U * 1024U)
#define AV2H_BMP_HEADER_SIZE 54U

typedef struct {
    void *memory;
    size_t size;
} Av2hOwnedMemory;

static void release_memory(Av2hOwnedMemory *owned) {
    free(owned->memory);
    owned->memory = NULL;
    owned->size = 0U;
}

static int allocate_memory(size_t size,
                           size_t alignment,
                           Av2hOwnedMemory *owned) {
    void *memory = NULL;
    size_t allocation_size = size == 0U ? 1U : size;
    if (alignment < sizeof(void *)) alignment = sizeof(void *);
    if (posix_memalign(&memory, alignment, allocation_size) != 0) return 0;
    owned->memory = memory;
    owned->size = size;
    return 1;
}

static int read_entire_file(const char *path, Av2hOwnedMemory *owned) {
    FILE *file = fopen(path, "rb");
    long length;
    size_t read_size;
    if (file == NULL) return 0;
    if (fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) <= 0L ||
        (unsigned long)length > AV2H_CLI_MAX_INPUT ||
        fseek(file, 0L, SEEK_SET) != 0 ||
        !allocate_memory((size_t)length, sizeof(void *), owned)) {
        fclose(file);
        return 0;
    }
    read_size = fread(owned->memory, 1U, owned->size, file);
    if (fclose(file) != 0 || read_size != owned->size) {
        release_memory(owned);
        return 0;
    }
    return 1;
}

static int write_entire_file(const char *path,
                             const void *data,
                             size_t size) {
    FILE *file = fopen(path, "wb");
    int result;
    if (file == NULL) return 0;
    result = fwrite(data, 1U, size, file) == size;
    if (fclose(file) != 0) result = 0;
    return result;
}

static int ends_with(const char *text, const char *suffix) {
    size_t text_size = strlen(text);
    size_t suffix_size = strlen(suffix);
    size_t index;
    if (suffix_size > text_size) return 0;
    for (index = 0U; index < suffix_size; ++index) {
        unsigned char left =
            (unsigned char)text[text_size - suffix_size + index];
        unsigned char right = (unsigned char)suffix[index];
        if (left >= 'A' && left <= 'Z') left = (unsigned char)(left + 32U);
        if (right >= 'A' && right <= 'Z') right = (unsigned char)(right + 32U);
        if (left != right) return 0;
    }
    return 1;
}

static int is_raw_output_path(const char *path) {
    size_t index;
    for (index = 0U; index < av2h_raw_output_extension_count(); ++index) {
        if (ends_with(path, av2h_raw_output_extension_at(index))) return 1;
    }
    return 0;
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return &ledger->entries[index];
    }
    return NULL;
}

static Av2hStatus decode_obu(Av2hSpan input,
                            Av2hOwnedMemory *output,
                            uint16_t *width,
                            uint16_t *height,
                            uint8_t *chroma_format_idc,
                            Av2hPixelFormat *output_format,
                            Av2hFrameDecodeResult *decode_result,
                            Av2hError *diagnostic) {
    Av2hObu entries[16U];
    Av2hObuLedger ledger = { entries, 16U, 0U };
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hOwnedMemory workspace_memory = { NULL, 0U };
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeWorkspaceLayout workspace_layout;
    size_t output_size = 0U;
    size_t bytes_per_sample;
    Av2hStatus status;
    av2h_memory_fill(diagnostic, 0U, sizeof(*diagnostic));
    status = av2h_obu_parse(input, &ledger, diagnostic);
    if (status != AV2H_OK) return status;
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    if (sequence_obu == NULL || frame_obu == NULL) return AV2H_INVALID_DATA;
    status = av2h_sequence_state_parse(
        input, sequence_obu, &sequence, diagnostic);
    if (status != AV2H_OK) return status;
    status = av2h_frame_header_closed_key_parse_diagnostic(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        &sequence, &frame, diagnostic);
    if (status != AV2H_OK) {
        diagnostic->offset += frame_obu->payload_offset;
        return status;
    }
    av2h_qmatrix_state_reset(&frame);
    status = av2h_qmatrix_ledger_parse(
        input, &ledger, &frame, diagnostic);
    if (status == AV2H_OK) status = av2h_qmatrix_references_validate(&frame);
    if (status != AV2H_OK) return status;
    status = av2h_tile_groups_ledger_parse(input, &ledger, &frame, &group);
    if (status != AV2H_OK) return status;
    bytes_per_sample = sequence.info.bit_depth_idc == 0U ? 2U : 1U;
    if ((bytes_per_sample == 2U &&
         !(sequence.info.chroma_format_idc == 0U ?
             av2h_pixel_format_init_10bit_420(output_format) :
             sequence.info.chroma_format_idc == 2U ?
             av2h_pixel_format_init_10bit_444(output_format) : 0)) ||
        (bytes_per_sample == 1U &&
         !av2h_pixel_format_init_8bit(
             sequence.info.chroma_format_idc, output_format))) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_planar_output_size_bytes(
        &frame, bytes_per_sample, &output_size);
    if (status != AV2H_OK) return status;
    status = av2h_frame_decode_workspace_query_bytes(
        &sequence, &frame, bytes_per_sample, &workspace_layout);
    if (status != AV2H_OK) return status;
    if (!allocate_memory(
            workspace_layout.size_bytes, workspace_layout.alignment,
            &workspace_memory) ||
        !allocate_memory(output_size, sizeof(void *), output)) {
        release_memory(&workspace_memory);
        release_memory(output);
        return AV2H_OUT_OF_MEMORY;
    }
    status = av2h_frame_decode_workspace_init_bytes(
        &sequence, &frame, bytes_per_sample, &workspace_layout,
        workspace_memory.memory, workspace_memory.size,
        &workspace, &filter_workspace, &filter_storage);
    if (status == AV2H_OK) {
        status = av2h_decode_frame_group_planar_private(
            &group, &sequence, &frame, &workspace,
            output->memory, output->size, decode_result);
    }
    release_memory(&workspace_memory);
    if (status != AV2H_OK) {
        release_memory(output);
        return status;
    }
    diagnostic->status = AV2H_OK;
    diagnostic->diagnostic_stage = AV2H_DIAGNOSTIC_STAGE_COMPLETE;
    diagnostic->diagnostic_reason = AV2H_DIAGNOSTIC_REASON_NONE;
    diagnostic->syntax_stage = AV2H_DIAGNOSTIC_SYNTAX_NONE;
    diagnostic->entropy_tell = decode_result->entropy_tell;
    *width = frame.width;
    *height = frame.height;
    *chroma_format_idc = frame.chroma_format_idc;
    return AV2H_OK;
}

static int write_bmp(const char *path,
                     const uint8_t *planar,
                     uint16_t width,
                     uint16_t height,
                     uint8_t chroma_format_idc) {
    size_t row_bytes;
    size_t pixel_bytes;
    size_t file_size;
    uint8_t *row_data;
    uint8_t header[AV2H_BMP_HEADER_SIZE];
    Av2hPlanarPreview preview;
    FILE *file;
    size_t row;
    int result = 0;
    if (!av2h_size_multiply(width, 3U, &row_bytes) ||
        !av2h_size_align(row_bytes, 4U, &row_bytes) ||
        !av2h_size_multiply(row_bytes, height, &pixel_bytes) ||
        !av2h_size_add(AV2H_BMP_HEADER_SIZE, pixel_bytes, &file_size) ||
        file_size > UINT32_MAX ||
        av2h_planar_preview_init_tight_8bit(
            planar, width, height, chroma_format_idc,
            AV2H_COLOR_RANGE_LIMITED, 6U, &preview) != AV2H_OK) {
        return 0;
    }
    row_data = (uint8_t *)malloc(row_bytes);
    if (row_data == NULL) return 0;
    av2h_memory_fill(header, 0U, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    av2h_store_u32le(header + 2U, (uint32_t)file_size);
    av2h_store_u32le(header + 10U, AV2H_BMP_HEADER_SIZE);
    av2h_store_u32le(header + 14U, 40U);
    av2h_store_u32le(header + 18U, width);
    av2h_store_u32le(header + 22U, height);
    av2h_store_u16le(header + 26U, 1U);
    av2h_store_u16le(header + 28U, 24U);
    av2h_store_u32le(header + 34U, (uint32_t)pixel_bytes);
    file = fopen(path, "wb");
    if (file == NULL) goto done;
    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header)) goto close;
    for (row = height; row != 0U; --row) {
        size_t y_row = row - 1U;
        av2h_memory_fill(row_data, 0U, row_bytes);
        if (av2h_planar_to_bgr8_row(
                &preview, (uint32_t)y_row,
                row_data, row_bytes) != AV2H_OK) goto close;
        if (fwrite(row_data, 1U, row_bytes, file) != row_bytes) goto close;
    }
    result = 1;
close:
    if (fclose(file) != 0) result = 0;
done:
    free(row_data);
    return result;
}

static Av2hStatus png_write_file(
    void *context,
    const uint8_t *data,
    size_t size) {
    return fwrite(data, 1U, size, (FILE *)context) == size ?
        AV2H_OK : AV2H_INVALID_DATA;
}

static Av2hStatus png_preview_row(
    void *context,
    uint32_t row,
    uint8_t *destination,
    size_t destination_size_bytes) {
    return av2h_planar_to_rgb8_row(
        (const Av2hPlanarPreview *)context, row,
        destination, destination_size_bytes);
}

static int write_png(const char *path,
                     const uint8_t *planar,
                     uint16_t width,
                     uint16_t height,
                     uint8_t chroma_format_idc) {
    Av2hPlanarPreview preview;
    Av2hPngRgbImage image;
    size_t row_size;
    uint8_t *row_data;
    FILE *file;
    int result;
    if (!av2h_size_multiply(width, 3U, &row_size) ||
        av2h_planar_preview_init_tight_8bit(
            planar, width, height, chroma_format_idc,
            AV2H_COLOR_RANGE_LIMITED, 6U, &preview) != AV2H_OK) {
        return 0;
    }
    row_data = (uint8_t *)malloc(row_size);
    if (row_data == NULL) return 0;
    image.width = width;
    image.height = height;
    image.bit_depth = 8U;
    image.row_callback = png_preview_row;
    image.row_context = &preview;
    file = fopen(path, "wb");
    if (file == NULL) {
        free(row_data);
        return 0;
    }
    result = av2h_png_write_rgb(
        &image, row_data, row_size, png_write_file, file) == AV2H_OK;
    if (fclose(file) != 0) result = 0;
    free(row_data);
    return result;
}

static void usage(const char *program) {
    fprintf(stderr,
            "usage: %s INPUT.obu -o "
            "OUTPUT.i400|OUTPUT.i420|OUTPUT.i444|"
            "OUTPUT.yuv420p10le|OUTPUT.yuv444p10le|OUTPUT.bmp|OUTPUT.png "
            "[--diagnostics]\n",
            program);
}

static void write_diagnostics_json(const Av2hError *diagnostic,
                                   const Av2hFrameDecodeResult *result) {
    uint8_t stage = diagnostic->diagnostic_stage;
    uint8_t reason = diagnostic->diagnostic_reason;
    uint8_t syntax = diagnostic->syntax_stage;
    size_t tell = diagnostic->entropy_tell;
    uint16_t row = diagnostic->row;
    uint16_t column = diagnostic->column;
    uint8_t plane = diagnostic->plane;
    uint32_t parsed_value = diagnostic->parsed_value;
    if (stage == AV2H_DIAGNOSTIC_STAGE_NONE && result->stage != 0U) {
        stage = result->stage == AV2H_FRAME_DECODE_STAGE_FILTER ?
            AV2H_DIAGNOSTIC_STAGE_FILTER :
            result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT ||
            result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM ||
            result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT ?
                AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION :
            result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_PARSE ||
            result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE ?
                (result->syntax_stage != 0U ?
                    AV2H_DIAGNOSTIC_STAGE_COEFFICIENT :
                    AV2H_DIAGNOSTIC_STAGE_LEAF) :
                AV2H_DIAGNOSTIC_STAGE_TILE;
        reason = result->cause == AV2H_INVALID_ARGUMENT ?
            AV2H_DIAGNOSTIC_REASON_ARGUMENT :
            result->cause == AV2H_TRUNCATED ?
            AV2H_DIAGNOSTIC_REASON_TRUNCATED :
            result->cause == AV2H_INVALID_DATA ?
                AV2H_DIAGNOSTIC_REASON_MALFORMED :
            result->cause == AV2H_OUT_OF_MEMORY ||
            result->cause == AV2H_LIMIT_EXCEEDED ||
            result->cause == AV2H_OVERFLOW ?
                AV2H_DIAGNOSTIC_REASON_RESOURCE :
                AV2H_DIAGNOSTIC_REASON_UNSUPPORTED_TUPLE;
        syntax = result->syntax_stage;
        tell = result->entropy_tell;
        row = result->row;
        column = result->column;
        plane = result->plane;
        parsed_value = stage == AV2H_DIAGNOSTIC_STAGE_LEAF ? result->mode :
            stage == AV2H_DIAGNOSTIC_STAGE_COEFFICIENT ?
                ((uint32_t)result->coefficient_kind << 24U) |
                ((uint32_t)result->coefficient_context << 16U) |
                result->coefficient_position :
            stage == AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION ?
                result->inverse_transform_stage :
            stage == AV2H_DIAGNOSTIC_STAGE_FILTER ? result->filter_reason : 0U;
    }
    printf("{\"status\":\"%s\",\"stage\":\"%s\","
           "\"reason\":\"%s\",\"syntax\":\"%s\","
           "\"offset\":%zu,\"entropy_tell\":%zu,"
           "\"parsed_value\":%u,\"row\":%u,\"column\":%u,"
           "\"plane\":%u,\"luma_leaves\":%zu,"
           "\"chroma_leaves\":%zu}\n",
           av2h_status_string(diagnostic->status),
           av2h_diagnostic_stage_string(stage),
           av2h_diagnostic_reason_string(reason),
           av2h_diagnostic_syntax_string(syntax), diagnostic->offset, tell,
           parsed_value, row, column, plane, result->luma_leaf_count,
           result->chroma_leaf_count);
}

int main(int argc, char **argv) {
    Av2hOwnedMemory input = { NULL, 0U };
    Av2hOwnedMemory output = { NULL, 0U };
    uint16_t width = 0U;
    uint16_t height = 0U;
    uint8_t chroma_format_idc = 0U;
    Av2hPixelFormat output_format;
    Av2hFrameDecodeResult decode_result = { 0 };
    Av2hError diagnostic = { 0 };
    const char *raw_extension;
    Av2hStatus status;
    int written;
    int diagnostics_enabled = argc == 5 &&
        strcmp(argv[4], "--diagnostics") == 0;
    if ((argc != 4 && !diagnostics_enabled) || strcmp(argv[2], "-o") != 0 ||
        !(is_raw_output_path(argv[3]) || ends_with(argv[3], ".bmp") ||
          ends_with(argv[3], ".png"))) {
        usage(argv[0]);
        return 2;
    }
    if (!read_entire_file(argv[1], &input)) {
        fprintf(stderr, "error: cannot read input OBU\n");
        return 1;
    }
    status = decode_obu(
        (Av2hSpan){ input.memory, input.size }, &output,
        &width, &height, &chroma_format_idc, &output_format, &decode_result,
        &diagnostic);
    if (status != AV2H_OK) {
        diagnostic.status = status;
        if (diagnostics_enabled) {
            write_diagnostics_json(&diagnostic, &decode_result);
        }
        fprintf(stderr,
            "error: decode failed: %s (cause %u, stage %u, tell %zu)\n",
                av2h_status_string(status), decode_result.cause,
            decode_result.stage, decode_result.entropy_tell);
        fprintf(stderr,
                "  leaf=(%u,%u) block=%u plane=%u syntax=%u mode=%u "
                "mrl=%u/%u tx=%u primary=%u secondary=%u/%u fsc=%u "
                "skip=%u zero=%u eob=%u coeff=%u/%u/%u inverse=%u\n",
            decode_result.row, decode_result.column,
            decode_result.block_size, decode_result.plane,
                decode_result.syntax_stage, decode_result.mode,
                decode_result.mrl_index, decode_result.multi_line_mrl,
                decode_result.tx_size, decode_result.primary_type,
                decode_result.secondary_symbol, decode_result.secondary_set,
                decode_result.fsc_mode, decode_result.skip_transform,
                decode_result.all_zero, decode_result.eob,
                decode_result.coefficient_position,
                decode_result.coefficient_context,
                decode_result.coefficient_kind,
                decode_result.inverse_transform_stage);
        release_memory(&input);
        return status == AV2H_UNSUPPORTED ? 3 : 1;
    }
    raw_extension = av2h_raw_output_extension(&output_format);
    if (!ends_with(argv[3], ".bmp") && !ends_with(argv[3], ".png") &&
        (raw_extension == NULL || !ends_with(argv[3], raw_extension))) {
        fprintf(stderr,
                "error: raw output extension does not match decoded format\n");
        release_memory(&output);
        release_memory(&input);
        return 2;
    }
    if ((ends_with(argv[3], ".bmp") || ends_with(argv[3], ".png")) &&
        output_format.bytes_per_sample != 1U) {
        fprintf(stderr, "error: preview output requires 8-bit input\n");
        written = 0;
    } else if (ends_with(argv[3], ".bmp")) {
        written = write_bmp(
            argv[3], output.memory, width, height, chroma_format_idc);
    } else if (ends_with(argv[3], ".png")) {
        written = write_png(
            argv[3], output.memory, width, height, chroma_format_idc);
    } else {
        written = write_entire_file(argv[3], output.memory, output.size);
    }
    if (!written) fprintf(stderr, "error: cannot write output\n");
    if (written && diagnostics_enabled) {
        write_diagnostics_json(&diagnostic, &decode_result);
    }
    release_memory(&output);
    release_memory(&input);
    return written ? 0 : 1;
}
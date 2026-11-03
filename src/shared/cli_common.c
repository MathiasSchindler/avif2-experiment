#include "cli_common.h"

#include "decoder/internal/frame_workspace.h"
#include "decoder/partition.h"
#include "decoder/qmatrix.h"
#include "output_layout.h"
#include "preview.h"

#include <stddef.h>

#define AV2H_BMP_HEADER_SIZE 54U

static int allocate_owned(const Av2hCliAllocator *allocator, size_t size, size_t alignment,
                          Av2hOwnedMemory *owned) {
    return alignment != 0U && allocator->allocate(allocator->context, size, alignment, owned) &&
           owned->memory != NULL && owned->size >= size &&
           (uintptr_t)owned->memory % alignment == 0U;
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type)
            return &ledger->entries[index];
    }
    return NULL;
}

static Av2hStatus initialize_format(const Av2hSequenceState *sequence, Av2hPixelFormat *format) {
    int initialized;
    if (sequence->info.bit_depth_idc == 0U) {
        initialized =
            sequence->info.chroma_format_idc == 0U   ? av2h_pixel_format_init_10bit_420(format)
            : sequence->info.chroma_format_idc == 2U ? av2h_pixel_format_init_10bit_444(format)
                                                     : 0;
    } else {
        initialized = av2h_pixel_format_init_8bit(sequence->info.chroma_format_idc, format);
    }
    return initialized ? AV2H_OK : AV2H_UNSUPPORTED;
}

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static Av2hStatus allocate_trace(const Av2hCliAllocator *allocator, Av2hOwnedMemory *memory) {
    size_t size = sizeof(Av2hDecodeDiagnostics) +
                  AV2H_PARTITION_DIAGNOSTIC_CAPACITY * sizeof(Av2hPartitionDecision) +
                  AV2H_PARTITION_DIAGNOSTIC_CAPACITY * sizeof(uint8_t) +
                  AV2H_PARTITION_LEAF_DIAGNOSTIC_CAPACITY * sizeof(Av2hPartitionLeafDiagnostic) +
                  _Alignof(Av2hPartitionDecision) + _Alignof(Av2hPartitionLeafDiagnostic);
    return allocate_owned(allocator, size, _Alignof(Av2hDecodeDiagnostics), memory)
               ? AV2H_OK
               : AV2H_OUT_OF_MEMORY;
}

static void bind_trace(const Av2hOwnedMemory *memory, Av2hFrameDecodeWorkspace *workspace) {
    uintptr_t cursor = (uintptr_t)memory->memory;
    Av2hDecodeDiagnostics *diagnostics = (Av2hDecodeDiagnostics *)cursor;
    cursor += sizeof(*diagnostics);
    cursor = (cursor + _Alignof(Av2hPartitionDecision) - 1U) &
             ~(uintptr_t)(_Alignof(Av2hPartitionDecision) - 1U);
    av2h_memory_fill(diagnostics, 0U, sizeof(*diagnostics));
    diagnostics->partition_trace = (Av2hPartitionDecision *)cursor;
    cursor += AV2H_PARTITION_DIAGNOSTIC_CAPACITY * sizeof(Av2hPartitionDecision);
    diagnostics->partition_trace_structure = (uint8_t *)cursor;
    cursor += AV2H_PARTITION_DIAGNOSTIC_CAPACITY;
    cursor = (cursor + _Alignof(Av2hPartitionLeafDiagnostic) - 1U) &
             ~(uintptr_t)(_Alignof(Av2hPartitionLeafDiagnostic) - 1U);
    diagnostics->partition_leaf_trace = (Av2hPartitionLeafDiagnostic *)cursor;
    diagnostics->partition_trace_capacity = AV2H_PARTITION_DIAGNOSTIC_CAPACITY;
    diagnostics->partition_leaf_trace_capacity = AV2H_PARTITION_LEAF_DIAGNOSTIC_CAPACITY;
    workspace->diagnostics = diagnostics;
    workspace->filter_workspace->diagnostics = diagnostics;
}
#endif

static Av2hStatus decode_image(Av2hSpan input, const Av2hCliAllocator *allocator,
                               const Av2hCliDecodeOptions *options, Av2hCliImage *image) {
    Av2hObu entries[16U];
    Av2hObuLedger ledger = {entries, 16U, 0U};
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hOwnedMemory workspace_memory = {NULL, 0U};
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeWorkspaceLayout layout;
    size_t output_size;
    size_t bytes_per_sample;
    Av2hStatus status = av2h_obu_parse(input, &ledger, &image->error);
    if (status != AV2H_OK)
        return status;
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    if (sequence_obu == NULL || frame_obu == NULL)
        return AV2H_INVALID_DATA;
    status = av2h_sequence_state_parse(input, sequence_obu, &sequence, &image->error);
    if (status != AV2H_OK)
        return status;
    if (options->validate_format_first) {
        status = initialize_format(&sequence, &image->format);
        if (status != AV2H_OK)
            return status;
    }
    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){input.data + frame_obu->payload_offset, frame_obu->payload_size}, &sequence,
        &frame, NULL, NULL, options->validate_format_first ? NULL : &image->error);
    if (status != AV2H_OK) {
        if (!options->validate_format_first) {
            image->error.offset += frame_obu->payload_offset;
        }
        return status;
    }
    av2h_qmatrix_state_reset(&frame);
    status = av2h_qmatrix_ledger_parse(input, &ledger, &frame, NULL, &image->error);
    if (status == AV2H_OK)
        status = av2h_qmatrix_references_validate(&frame);
    if (status != AV2H_OK)
        return status;
    status = av2h_tile_groups_ledger_validate(input, &ledger, &frame);
    if (status != AV2H_OK)
        return status;
    if (!options->validate_format_first) {
        status = initialize_format(&sequence, &image->format);
        if (status != AV2H_OK)
            return status;
    }
    bytes_per_sample = image->format.bytes_per_sample;
    status = av2h_planar_output_size(&frame, bytes_per_sample, &output_size);
    if (status != AV2H_OK)
        return status;
    status = av2h_frame_decode_workspace_query(&sequence, &frame, bytes_per_sample,
                                               options->execution, &layout);
    if (status != AV2H_OK)
        return status;
    if (!allocate_owned(allocator, layout.size_bytes, layout.alignment, &workspace_memory) ||
        !allocate_owned(allocator, output_size, sizeof(void *), &image->output)) {
        allocator->release(allocator->context, &workspace_memory);
        allocator->release(allocator->context, &image->output);
        return AV2H_OUT_OF_MEMORY;
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    if (options->retain_trace) {
        status = allocate_trace(allocator, &image->diagnostic_memory);
        if (status != AV2H_OK) {
            allocator->release(allocator->context, &workspace_memory);
            allocator->release(allocator->context, &image->output);
            return status;
        }
    }
#endif
    status = av2h_frame_decode_workspace_init(
        &sequence, &frame, bytes_per_sample, &layout, workspace_memory.memory,
        workspace_memory.size, &workspace, &filter_workspace, &filter_storage, options->execution);
    if (status == AV2H_OK) {
        Av2hDecodePreparation *preparation = &workspace.decode_scratch->preparation;
        av2h_memory_copy(&preparation->sequence, &sequence, sizeof(sequence));
        preparation->group = &workspace.decode_scratch->group;
        status = av2h_frame_header_closed_key_parse(
            (Av2hSpan){input.data + frame_obu->payload_offset, frame_obu->payload_size},
            &preparation->sequence, &preparation->frame, &workspace.decode_scratch->syntax, NULL,
            options->validate_format_first ? NULL : &image->error);
        if (status != AV2H_OK && !options->validate_format_first)
            image->error.offset += frame_obu->payload_offset;
        if (status == AV2H_OK) {
            av2h_qmatrix_state_reset(&preparation->frame);
            status =
                av2h_qmatrix_ledger_parse(input, &ledger, &preparation->frame, NULL, &image->error);
        }
        if (status == AV2H_OK)
            status = av2h_qmatrix_references_validate(&preparation->frame);
        if (status == AV2H_OK)
            status = av2h_tile_groups_ledger_parse(input, &ledger, &preparation->frame,
                                                   preparation->group);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        if (options->retain_trace)
            bind_trace(&image->diagnostic_memory, &workspace);
#endif
        if (status == AV2H_OK)
            status = av2h_decode_frame_group_planar_private(
                preparation->group, &preparation->sequence, &preparation->frame, &workspace,
                image->output.memory, image->output.size, &image->result, options->execution);
    }
    if (image->diagnostic_memory.memory == NULL) {
        image->result.diagnostics = NULL;
    }
    allocator->release(allocator->context, &workspace_memory);
    if (status != AV2H_OK) {
        allocator->release(allocator->context, &image->output);
        return status;
    }
    image->error.diagnostic_stage = AV2H_DIAGNOSTIC_STAGE_COMPLETE;
    image->error.diagnostic_reason = AV2H_DIAGNOSTIC_REASON_NONE;
    image->error.syntax_stage = AV2H_DIAGNOSTIC_SYNTAX_NONE;
    image->error.entropy_tell = image->result.entropy_tell;
    image->width = frame.width;
    image->height = frame.height;
    image->chroma_format_idc = frame.chroma_format_idc;
    return AV2H_OK;
}

AV2H_NOINLINE Av2hStatus av2h_cli_decode(Av2hSpan input, const Av2hCliAllocator *allocator,
                                         const Av2hCliDecodeOptions *options, Av2hCliImage *image) {
    Av2hStatus status;
    if ((input.data == NULL && input.size != 0U) || allocator == NULL ||
        allocator->allocate == NULL || allocator->release == NULL || options == NULL ||
        image == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(image, 0U, sizeof(*image));
    status = decode_image(input, allocator, options, image);
    image->error.status = status;
    return status;
}

void av2h_cli_image_release(const Av2hCliAllocator *allocator, Av2hCliImage *image) {
    allocator->release(allocator->context, &image->output);
    allocator->release(allocator->context, &image->diagnostic_memory);
    image->result.diagnostics = NULL;
}

static uint16_t path_unit(Av2hCliPath path, size_t index) {
    return path.text != NULL ? (uint8_t)path.text[index] : path.wide[index];
}

static int path_ends_with(Av2hCliPath path, const char *suffix) {
    size_t length = 0U;
    size_t suffix_length = 0U;
    size_t index;
    while (path_unit(path, length) != 0U)
        ++length;
    while (suffix[suffix_length] != '\0')
        ++suffix_length;
    if (suffix_length > length)
        return 0;
    for (index = 0U; index < suffix_length; ++index) {
        uint16_t left = path_unit(path, length - suffix_length + index);
        uint8_t right = (uint8_t)suffix[index];
        if (left >= 'A' && left <= 'Z')
            left = (uint16_t)(left + 32U);
        if (right >= 'A' && right <= 'Z')
            right = (uint8_t)(right + 32U);
        if (left != right)
            return 0;
    }
    return 1;
}

Av2hStatus av2h_cli_output_parse(Av2hCliPath path, Av2hCliOutput *output) {
    size_t index;
    if (output == NULL || (path.text == NULL) == (path.wide == NULL)) {
        return AV2H_INVALID_ARGUMENT;
    }
    output->kind = AV2H_CLI_OUTPUT_INVALID;
    output->raw_extension = NULL;
    for (index = 0U; index < av2h_raw_output_extension_count(); ++index) {
        const char *extension = av2h_raw_output_extension_at(index);
        if (path_ends_with(path, extension)) {
            output->kind = AV2H_CLI_OUTPUT_RAW;
            output->raw_extension = extension;
            return AV2H_OK;
        }
    }
    if (path_ends_with(path, ".bmp"))
        output->kind = AV2H_CLI_OUTPUT_BMP;
    else if (path_ends_with(path, ".png"))
        output->kind = AV2H_CLI_OUTPUT_PNG;
    return output->kind != AV2H_CLI_OUTPUT_INVALID ? AV2H_OK : AV2H_UNSUPPORTED;
}

static Av2hStatus png_preview_row(void *context, uint32_t row, uint8_t *destination, size_t size) {
    return av2h_planar_to_rgb8_row((const Av2hPlanarPreview *)context, row, destination, size);
}

static int extensions_equal(const char *left, const char *right) {
    size_t index = 0U;
    if (left == NULL || right == NULL)
        return 0;
    while (left[index] != '\0' && left[index] == right[index])
        ++index;
    return left[index] == right[index];
}

Av2hCliWriteResult av2h_cli_write_output(const Av2hCliImage *image, const Av2hCliOutput *output,
                                         const Av2hCliAllocator *allocator,
                                         const Av2hCliFile *file) {
    Av2hOwnedMemory row_memory = {NULL, 0U};
    Av2hPlanarPreview preview;
    Av2hPlaneLayout planes[3U];
    uint8_t header[AV2H_BMP_HEADER_SIZE];
    size_t row_size;
    size_t pixel_size = 0U;
    size_t file_size = 0U;
    size_t row;
    size_t output_size;
    Av2hStatus status;
    if (image == NULL || output == NULL || allocator == NULL || allocator->allocate == NULL ||
        allocator->release == NULL || file == NULL || file->open == NULL || file->write == NULL ||
        file->close == NULL || output->kind < AV2H_CLI_OUTPUT_RAW ||
        output->kind > AV2H_CLI_OUTPUT_PNG || image->output.memory == NULL ||
        image->output.size == 0U) {
        return AV2H_CLI_WRITE_FAILED;
    }
    if (output->kind == AV2H_CLI_OUTPUT_RAW) {
        const char *extension = av2h_raw_output_extension(&image->format);
        if (!extensions_equal(extension, output->raw_extension)) {
            return AV2H_CLI_WRITE_RAW_MISMATCH;
        }
    } else if (image->format.bytes_per_sample != 1U) {
        return AV2H_CLI_WRITE_PREVIEW_DEPTH;
    }
    if (av2h_output_layout_calculate(image->width, image->height, image->chroma_format_idc,
                                     image->format.bytes_per_sample, planes, &output_size,
                                     NULL) != AV2H_OK ||
        image->output.size < output_size) {
        return AV2H_CLI_WRITE_FAILED;
    }
    if (output->kind != AV2H_CLI_OUTPUT_RAW) {
        if (!av2h_size_multiply(image->width, 3U, &row_size) ||
            (output->kind == AV2H_CLI_OUTPUT_BMP &&
             (!av2h_size_align(row_size, 4U, &row_size) ||
              !av2h_size_multiply(row_size, image->height, &pixel_size) ||
              !av2h_size_add(sizeof(header), pixel_size, &file_size) || file_size > UINT32_MAX)) ||
            av2h_planar_preview_init_tight_8bit(image->output.memory, image->width, image->height,
                                                image->chroma_format_idc, AV2H_COLOR_RANGE_LIMITED,
                                                6U, &preview) != AV2H_OK ||
            !allocate_owned(allocator, row_size, sizeof(void *), &row_memory)) {
            allocator->release(allocator->context, &row_memory);
            return AV2H_CLI_WRITE_FAILED;
        }
    }
    status = file->open(file->context);
    if (status != AV2H_OK) {
        allocator->release(allocator->context, &row_memory);
        return AV2H_CLI_WRITE_FAILED;
    }
    if (output->kind == AV2H_CLI_OUTPUT_RAW) {
        status = file->write(file->context, image->output.memory, output_size);
    } else if (output->kind == AV2H_CLI_OUTPUT_PNG) {
        Av2hPngRgbImage png;
        png.width = image->width;
        png.height = image->height;
        png.bit_depth = 8U;
        png.row_callback = png_preview_row;
        png.row_context = &preview;
        status = av2h_png_write_rgb(&png, row_memory.memory, row_memory.size, file->write,
                                    file->context);
    } else {
        av2h_memory_fill(header, 0U, sizeof(header));
        header[0] = 'B';
        header[1] = 'M';
        av2h_store_u32le(header + 2U, (uint32_t)file_size);
        av2h_store_u32le(header + 10U, sizeof(header));
        av2h_store_u32le(header + 14U, 40U);
        av2h_store_u32le(header + 18U, image->width);
        av2h_store_u32le(header + 22U, image->height);
        av2h_store_u16le(header + 26U, 1U);
        av2h_store_u16le(header + 28U, 24U);
        av2h_store_u32le(header + 34U, (uint32_t)pixel_size);
        status = file->write(file->context, header, sizeof(header));
        for (row = image->height; status == AV2H_OK && row != 0U; --row) {
            av2h_memory_fill(row_memory.memory, 0U, row_size);
            status = av2h_planar_to_bgr8_row(&preview, (uint32_t)(row - 1U), row_memory.memory,
                                             row_size);
            if (status == AV2H_OK) {
                status = file->write(file->context, row_memory.memory, row_size);
            }
        }
    }
    if (file->close(file->context) != AV2H_OK)
        status = AV2H_INVALID_DATA;
    allocator->release(allocator->context, &row_memory);
    return status == AV2H_OK ? AV2H_CLI_WRITE_OK : AV2H_CLI_WRITE_FAILED;
}

int av2h_cli_write_exit_code(Av2hCliWriteResult result) {
    return result == AV2H_CLI_WRITE_OK ? 0 : result == AV2H_CLI_WRITE_RAW_MISMATCH ? 2 : 1;
}

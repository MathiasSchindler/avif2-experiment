#include "decode.h"
#include "pixel_format.h"

static uint8_t diagnostic_reason_from_status(Av2hStatus status) {
    switch (status) {
        case AV2H_INVALID_ARGUMENT: return AV2H_DIAGNOSTIC_REASON_ARGUMENT;
        case AV2H_TRUNCATED: return AV2H_DIAGNOSTIC_REASON_TRUNCATED;
        case AV2H_INVALID_DATA: return AV2H_DIAGNOSTIC_REASON_MALFORMED;
        case AV2H_UNSUPPORTED:
            return AV2H_DIAGNOSTIC_REASON_UNSUPPORTED_TUPLE;
        case AV2H_OVERFLOW:
        case AV2H_LIMIT_EXCEEDED:
        case AV2H_OUT_OF_MEMORY:
            return AV2H_DIAGNOSTIC_REASON_RESOURCE;
        default: return AV2H_DIAGNOSTIC_REASON_NONE;
    }
}

static void decode_error_diagnostics_clear(Av2hError *error) {
    if (error == 0) return;
    error->diagnostic_stage = AV2H_DIAGNOSTIC_STAGE_NONE;
    error->diagnostic_reason = AV2H_DIAGNOSTIC_REASON_NONE;
    error->syntax_stage = 0U;
    error->plane = 0U;
    error->row = 0U;
    error->column = 0U;
    error->entropy_tell = 0U;
    error->parsed_value = 0U;
}

static Av2hStatus decode_fail(Av2hError *error,
                              Av2hStatus status,
                              uint32_t context) {
    if (error != 0) {
        error->status = status;
        error->offset = 0U;
        error->context = context;
        error->obu_index = SIZE_MAX;
        error->obu_type = 0U;
        decode_error_diagnostics_clear(error);
        error->diagnostic_reason = diagnostic_reason_from_status(status);
    }
    return status;
}

static uint8_t decode_result_stage(const Av2hFrameDecodeResult *result) {
    switch (result->stage) {
        case AV2H_FRAME_DECODE_STAGE_LUMA_PARSE:
        case AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE:
            return result->syntax_stage != 0U ?
                AV2H_DIAGNOSTIC_STAGE_COEFFICIENT :
                AV2H_DIAGNOSTIC_STAGE_LEAF;
        case AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT:
        case AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM:
        case AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT:
            return AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION;
        case AV2H_FRAME_DECODE_STAGE_FILTER:
            return AV2H_DIAGNOSTIC_STAGE_FILTER;
        case AV2H_FRAME_DECODE_STAGE_OUTPUT:
            return AV2H_DIAGNOSTIC_STAGE_OUTPUT;
        default: return AV2H_DIAGNOSTIC_STAGE_TILE;
    }
}

static void decode_result_diagnostics(Av2hError *error,
                                      const Av2hFrameDecodeResult *result) {
    if (error == 0 || result == 0) return;
    error->diagnostic_stage = decode_result_stage(result);
    error->diagnostic_reason = diagnostic_reason_from_status(result->cause);
    if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION) {
        error->diagnostic_reason = AV2H_DIAGNOSTIC_REASON_RECONSTRUCTION;
    } else if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_FILTER) {
        error->diagnostic_reason = AV2H_DIAGNOSTIC_REASON_FILTER;
    } else if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_COEFFICIENT &&
               error->diagnostic_reason == AV2H_DIAGNOSTIC_REASON_NONE) {
        error->diagnostic_reason = AV2H_DIAGNOSTIC_REASON_ENTROPY;
    }
    error->syntax_stage = result->syntax_stage;
    error->plane = result->plane;
    error->row = result->row;
    error->column = result->column;
    error->entropy_tell = result->entropy_tell;
    if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_LEAF) {
        error->parsed_value = result->mode;
    } else if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_COEFFICIENT) {
        error->parsed_value = ((uint32_t)result->coefficient_kind << 24U) |
            ((uint32_t)result->coefficient_context << 16U) |
            result->coefficient_position;
    } else if (error->diagnostic_stage ==
               AV2H_DIAGNOSTIC_STAGE_RECONSTRUCTION) {
        error->parsed_value = result->inverse_transform_stage;
    } else if (error->diagnostic_stage == AV2H_DIAGNOSTIC_STAGE_FILTER) {
        error->parsed_value = result->filter_reason;
    }
}

static const Av2hObu *find_unique_obu(const Av2hObuLedger *ledger,
                                      uint8_t type) {
    const Av2hObu *found = 0;
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type != type) continue;
        if (found != 0) return 0;
        found = &ledger->entries[index];
    }
    return found;
}

static int ledger_has_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return 1;
    }
    return 0;
}

static int ranges_overlap(const void *left,
                          size_t left_size,
                          const void *right,
                          size_t right_size) {
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    uintptr_t maximum = (uintptr_t)-1;
    if (left_size == 0U || right_size == 0U) return 0;
    if (left == 0 || right == 0 || left_size > maximum - left_start ||
        right_size > maximum - right_start) {
        return 1;
    }
    return left_start < right_start + right_size &&
           right_start < left_start + left_size;
}

static int info_matches(const Av2hInfo *left, const Av2hInfo *right) {
    unsigned int plane;
    if (left->dialect != right->dialect ||
        left->image.source.config_obus.data !=
            right->image.source.config_obus.data ||
        left->image.source.config_obus.size !=
            right->image.source.config_obus.size ||
        left->image.source.item_payload.data !=
            right->image.source.item_payload.data ||
        left->image.source.item_payload.size !=
            right->image.source.item_payload.size ||
        left->image.width != right->image.width ||
        left->image.height != right->image.height ||
        !av2h_pixel_format_equal(&left->image.pixel_format,
                     &right->image.pixel_format) ||
        left->output_size != right->output_size ||
        left->workspace_size != right->workspace_size ||
        left->workspace_alignment != right->workspace_alignment ||
        left->input_size != right->input_size ||
        left->input_fingerprint != right->input_fingerprint) {
        return 0;
    }
    for (plane = 0U; plane < 3U; ++plane) {
        if (left->planes[plane].width != right->planes[plane].width ||
            left->planes[plane].height != right->planes[plane].height ||
            left->planes[plane].row_bytes != right->planes[plane].row_bytes ||
            left->planes[plane].offset != right->planes[plane].offset ||
            left->planes[plane].size != right->planes[plane].size) {
            return 0;
        }
    }
    return 1;
}

static uint32_t decode_result_context(
    const Av2hFrameDecodeResult *result) {
    if (result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT ||
        result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM ||
        result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT) {
        return AV2H_CONTEXT_RECONSTRUCTION;
    }
    if (result->stage == AV2H_FRAME_DECODE_STAGE_FILTER) {
        return AV2H_CONTEXT_FILTER;
    }
    return AV2H_CONTEXT_DECODE;
}

Av2hStatus av2h_decode_prepare(const Av2hHeifImage *image,
                               Av2hDecodePreparation *preparation,
                               Av2hError *error) {
    Av2hObu config_entries[16U];
    Av2hObu payload_entries[16U];
    Av2hObuLedger config = { config_entries, 16U, 0U };
    Av2hObuLedger payload = { payload_entries, 16U, 0U };
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hStatus status;
    if (image == 0 || preparation == 0) {
        return decode_fail(error, AV2H_INVALID_ARGUMENT,
                           AV2H_CONTEXT_DECODE);
    }
    status = av2h_obu_parse(image->source.config_obus, &config, error);
    if (status != AV2H_OK) return status;
    status = av2h_obu_parse(image->source.item_payload, &payload, error);
    if (status != AV2H_OK) return status;
    if (ledger_has_obu(&config, AV2H_OBU_QUANTIZATION_MATRIX) ||
        ledger_has_obu(&payload, AV2H_OBU_QUANTIZATION_MATRIX)) {
        return decode_fail(error, AV2H_UNSUPPORTED,
                           AV2H_CONTEXT_DECODE);
    }
    sequence_obu = find_unique_obu(&config, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_unique_obu(&payload, AV2H_OBU_CLOSED_LOOP_KEY);
    if (sequence_obu == 0 || frame_obu == 0) {
        return decode_fail(error, AV2H_INVALID_DATA,
                           AV2H_CONTEXT_DECODE);
    }
    status = av2h_sequence_state_parse(
        image->source.config_obus, sequence_obu,
        &preparation->sequence, error);
    if (status != AV2H_OK) return status;
    status = av2h_frame_header_closed_key_parse_diagnostic(
        (Av2hSpan){
            image->source.item_payload.data + frame_obu->payload_offset,
            frame_obu->payload_size
        },
        &preparation->sequence, &preparation->frame, error);
    if (status != AV2H_OK) {
        if (error != 0) error->offset += frame_obu->payload_offset;
        return status;
    }
    if (preparation->frame.width != image->width ||
        preparation->frame.height != image->height) {
        return decode_fail(error, AV2H_INVALID_DATA,
                           AV2H_CONTEXT_FRAME);
    }
    status = av2h_tile_group_single_parse(
        (Av2hSpan){
            image->source.item_payload.data + frame_obu->payload_offset,
            frame_obu->payload_size
        },
        preparation->frame.uncompressed_header_bits, &preparation->group);
    if (status != AV2H_OK) {
        return decode_fail(error, status, AV2H_CONTEXT_FRAME);
    }
    status = av2h_planar_output_size_bytes(
        &preparation->frame, image->pixel_format.bytes_per_sample,
        &preparation->output_size);
    if (status != AV2H_OK) {
        return decode_fail(error, status, AV2H_CONTEXT_OUTPUT);
    }
    status = av2h_frame_decode_workspace_query_bytes(
        &preparation->sequence, &preparation->frame,
        image->pixel_format.bytes_per_sample, &preparation->workspace);
    if (status != AV2H_OK) {
        return decode_fail(error, status, AV2H_CONTEXT_WORKSPACE);
    }
    return AV2H_OK;
}

Av2hStatus av2h_decode(Av2hSpan input,
                       const Av2hInfo *info,
                       void *workspace_memory,
                       size_t workspace_size,
                       void *output_memory,
                       size_t output_size,
                       Av2hError *error) {
    Av2hInfo fresh;
    Av2hDecodePreparation preparation;
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeResult result;
    Av2hStatus status;
    if (info == 0 || (input.data == 0 && input.size != 0U)) {
        return decode_fail(error, AV2H_INVALID_ARGUMENT,
                           AV2H_CONTEXT_DECODE);
    }
    if (error != 0 &&
        (ranges_overlap(workspace_memory, workspace_size,
                        error, sizeof(*error)) ||
         ranges_overlap(output_memory, output_size,
                        error, sizeof(*error)))) {
        return AV2H_INVALID_ARGUMENT;
    }
    decode_error_diagnostics_clear(error);
    status = av2h_query(input, 0, &fresh, error);
    if (status != AV2H_OK) return status;
    if (!info_matches(info, &fresh)) {
        return decode_fail(error, AV2H_INVALID_DATA,
                           AV2H_CONTEXT_BINDING);
    }
    if (workspace_memory == 0 || workspace_size < fresh.workspace_size) {
        return decode_fail(error, AV2H_OUT_OF_MEMORY,
                           AV2H_CONTEXT_WORKSPACE);
    }
    if (((uintptr_t)workspace_memory &
         (fresh.workspace_alignment - 1U)) != 0U) {
        return decode_fail(error, AV2H_INVALID_ARGUMENT,
                           AV2H_CONTEXT_WORKSPACE);
    }
    if (output_memory == 0 || output_size < fresh.output_size) {
        return decode_fail(error, AV2H_OUT_OF_MEMORY,
                           AV2H_CONTEXT_OUTPUT);
    }
    if (ranges_overlap(workspace_memory, fresh.workspace_size,
                       input.data, input.size) ||
        ranges_overlap(output_memory, fresh.output_size,
                       input.data, input.size) ||
        ranges_overlap(workspace_memory, fresh.workspace_size,
                       info, sizeof(*info)) ||
        ranges_overlap(output_memory, fresh.output_size,
                       info, sizeof(*info)) ||
        ranges_overlap(output_memory, fresh.output_size,
                       workspace_memory, fresh.workspace_size)) {
        return decode_fail(error, AV2H_INVALID_ARGUMENT,
                           AV2H_CONTEXT_OUTPUT);
    }
    status = av2h_decode_prepare(&fresh.image, &preparation, error);
    if (status != AV2H_OK) return status;
    status = av2h_frame_decode_workspace_init_bytes(
        &preparation.sequence, &preparation.frame,
        fresh.image.pixel_format.bytes_per_sample,
        &preparation.workspace, workspace_memory, workspace_size,
        &workspace, &filter_workspace, &filter_storage);
    if (status != AV2H_OK) {
        return decode_fail(error, status, AV2H_CONTEXT_WORKSPACE);
    }
    if (fresh.image.pixel_format.bytes_per_sample == 2U) {
        status = av2h_planar_output_bind_u16(
            &preparation.frame, output_memory, output_size,
            &workspace.output_u16);
        if (status != AV2H_OK) {
            return decode_fail(error, status, AV2H_CONTEXT_OUTPUT);
        }
    }
    av2h_memory_fill(&result, 0U, sizeof(result));
    status = av2h_decode_frame_planar_private(
        preparation.group.entropy, &preparation.sequence,
        &preparation.frame, &workspace, (uint8_t *)output_memory,
        output_size, &result);
    if (status != AV2H_OK) {
        status = decode_fail(error, status, decode_result_context(&result));
        decode_result_diagnostics(error, &result);
        return status;
    }
    if (error != 0) {
        decode_error_diagnostics_clear(error);
        error->status = AV2H_OK;
        error->diagnostic_stage = AV2H_DIAGNOSTIC_STAGE_COMPLETE;
    }
    return AV2H_OK;
}
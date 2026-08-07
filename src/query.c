#include "base.h"
#include "decode.h"

static Av2hStatus query_fail(Av2hError *error,
                             Av2hStatus status,
                             size_t offset,
                             uint32_t context) {
    if (error != 0) {
        error->status = status;
        error->offset = offset;
        error->context = context;
        error->obu_index = SIZE_MAX;
        error->obu_type = 0U;
    }
    return status;
}

static uint64_t input_fingerprint(Av2hSpan input) {
    uint64_t value = (uint64_t)14695981039346656037ULL;
    size_t index;
    for (index = 0U; index < input.size; ++index) {
        value ^= input.data[index];
        value *= (uint64_t)1099511628211ULL;
    }
    return value;
}

static Av2hStatus span_has_sequence_header(Av2hSpan span,
                                           size_t skip,
                                           int *has_sequence,
                                           Av2hError *error) {
    Av2hObu entries[16];
    Av2hObuLedger ledger = { entries, 16U, 0U };
    Av2hSpan obus;
    size_t index;
    Av2hStatus status;
    *has_sequence = 0;
    if (skip > span.size) {
        return query_fail(error, AV2H_INVALID_ARGUMENT, skip,
                          AV2H_CONTEXT_DIALECT);
    }
    obus.data = span.data + skip;
    obus.size = span.size - skip;
    status = av2h_obu_parse(obus, &ledger, error);
    if (status != AV2H_OK) return status;
    for (index = 0U; index < ledger.count; ++index) {
        if (ledger.entries[index].type == AV2H_OBU_SEQUENCE_HEADER) {
            *has_sequence = 1;
            break;
        }
    }
    return AV2H_OK;
}

static Av2hStatus plane_layout(uint32_t width,
                               uint32_t height,
                               size_t offset,
                               Av2hPlaneLayout *plane,
                               Av2hError *error) {
    size_t size;
    if (!av2h_size_multiply((size_t)width, (size_t)height, &size)) {
        return query_fail(error, AV2H_OVERFLOW, 0U, AV2H_CONTEXT_QUERY);
    }
    plane->width = width;
    plane->height = height;
    plane->row_bytes = width;
    plane->offset = offset;
    plane->size = size;
    return AV2H_OK;
}

void av2h_limits_default(Av2hLimits *limits) {
    if (limits == 0) return;
    limits->max_input_size = (size_t)UINT32_MAX;
    limits->max_config_obus_size = 1024U * 1024U;
    limits->max_item_payload_size = 256U * 1024U * 1024U;
    limits->max_pixels = 268435456U;
    limits->max_output_size = 402653184U;
    limits->max_workspace_size = 1073741824U;
    limits->max_width = 16384U;
    limits->max_height = 16384U;
    limits->max_obu_count = 2U;
    limits->max_property_count = 16U;
    limits->max_recursion_depth = 3U;
}

Av2hStatus av2h_dialect_detect(Av2hSpan input,
                               Av2hDialect *dialect,
                               Av2hError *error) {
    Av2hHeifImage image;
    Av2hContainerProbe probe;
    Av2hError strict_error;
    Av2hStatus status;
    Av2hStatus strict_status;
    int draft_shape;
    int legacy_shape;
    int config_has_sequence;
    int payload_has_sequence;
    if (dialect == 0 || (input.data == 0 && input.size != 0U)) {
        return query_fail(error, AV2H_INVALID_ARGUMENT, 0U,
                          AV2H_CONTEXT_DIALECT);
    }
    *dialect = AV2H_DIALECT_UNKNOWN;
    strict_status = av2h_heif_parse(input, &image, &strict_error);
    if (strict_status == AV2H_OK) {
        *dialect = AV2H_DIALECT_CANONICAL_V0;
        return AV2H_OK;
    }
    status = av2h_container_probe(input, &probe, error);
    if (status != AV2H_OK) return status;
    draft_shape = probe.av2c_payload.size > 9U &&
                  probe.av2c_payload.data[0] == 1U;
    legacy_shape = probe.av2c_payload.size == 4U &&
                   (probe.av2c_payload.data[0] & 0x80U) != 0U &&
                   (probe.av2c_payload.data[0] & 0x7fU) == 1U;
    if ((probe.has_avif_brand && draft_shape) ||
        (!probe.has_avif_brand && legacy_shape) ||
        (probe.has_avif_brand && !legacy_shape)) {
        *dialect = AV2H_DIALECT_CONFLICTING;
        return query_fail(error, AV2H_INVALID_DATA, 0U,
                          AV2H_CONTEXT_DIALECT);
    }
    if (draft_shape) {
        status = span_has_sequence_header(probe.av2c_payload, 9U,
                                          &config_has_sequence, error);
        if (status == AV2H_OK) {
            status = span_has_sequence_header(probe.item_payload, 0U,
                                              &payload_has_sequence, error);
        }
        if (status == AV2H_OK && config_has_sequence && payload_has_sequence) {
            *dialect = AV2H_DIALECT_CONFLICTING;
            return query_fail(error, AV2H_INVALID_DATA, 0U,
                              AV2H_CONTEXT_DIALECT);
        }
        if (probe.has_mif1_brand) {
            if (error != 0) *error = strict_error;
            return strict_status;
        }
    }
    if (probe.has_avif_brand && legacy_shape) {
        status = span_has_sequence_header(probe.item_payload, 0U,
                                          &payload_has_sequence, error);
        if (status != AV2H_OK) return status;
        if (payload_has_sequence) {
            *dialect = AV2H_DIALECT_LIBAVIF_AVM_2026_07;
            return AV2H_OK;
        }
    }
    if (probe.has_mif1_brand) {
        if (error != 0) *error = strict_error;
        return strict_status;
    }
    return query_fail(error, AV2H_UNSUPPORTED, 0U, AV2H_CONTEXT_DIALECT);
}

Av2hStatus av2h_query(Av2hSpan input,
                      const Av2hLimits *limits,
                      Av2hInfo *info,
                      Av2hError *error) {
    Av2hLimits defaults;
    const Av2hLimits *active = limits;
    Av2hStatus status;
    size_t pixels;
    size_t output_size;
    Av2hDecodePreparation preparation;
    uint32_t chroma_width;
    uint32_t chroma_height;
    if (info == 0 || (input.data == 0 && input.size != 0U)) {
        return query_fail(error, AV2H_INVALID_ARGUMENT, 0U,
                          AV2H_CONTEXT_QUERY);
    }
    if (active == 0) {
        av2h_limits_default(&defaults);
        active = &defaults;
    }
    if (input.size > active->max_input_size) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, input.size,
                          AV2H_CONTEXT_LIMITS);
    }
    status = av2h_dialect_detect(input, &info->dialect, error);
    if (status != AV2H_OK) return status;
    if (info->dialect != AV2H_DIALECT_CANONICAL_V0) {
        return query_fail(error, AV2H_UNSUPPORTED, 0U,
                          AV2H_CONTEXT_DIALECT);
    }
    status = av2h_heif_parse(input, &info->image, error);
    if (status != AV2H_OK) return status;
    if (info->image.source.config_obus.size > active->max_config_obus_size ||
        info->image.source.item_payload.size > active->max_item_payload_size ||
        info->image.width > active->max_width ||
        info->image.height > active->max_height) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, 0U,
                          AV2H_CONTEXT_LIMITS);
    }
    info->obu_count = 2U;
    info->property_count = info->image.property_count;
    info->container_depth = 3U;
    if (info->obu_count > active->max_obu_count ||
        info->property_count > active->max_property_count ||
        info->container_depth > active->max_recursion_depth) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, 0U,
                          AV2H_CONTEXT_LIMITS);
    }
    if (!av2h_size_multiply((size_t)info->image.width,
                            (size_t)info->image.height, &pixels)) {
        return query_fail(error, AV2H_OVERFLOW, 0U, AV2H_CONTEXT_QUERY);
    }
    if (pixels > active->max_pixels) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, 0U,
                          AV2H_CONTEXT_LIMITS);
    }
    status = plane_layout(info->image.width, info->image.height, 0U,
                          &info->planes[0], error);
    if (status != AV2H_OK) return status;
    av2h_memory_fill(&info->planes[1], 0U, sizeof(info->planes[1]));
    av2h_memory_fill(&info->planes[2], 0U, sizeof(info->planes[2]));
    output_size = info->planes[0].size;
    if (info->image.channel_count == 1U) goto output_layout_complete;
    chroma_width = info->image.width / 2U + info->image.width % 2U;
    chroma_height = info->image.height / 2U + info->image.height % 2U;
    status = plane_layout(chroma_width, chroma_height,
                          info->planes[0].size, &info->planes[1], error);
    if (status != AV2H_OK) return status;
    if (!av2h_size_add(info->planes[1].offset, info->planes[1].size,
                       &output_size)) {
        return query_fail(error, AV2H_OVERFLOW, 0U, AV2H_CONTEXT_QUERY);
    }
    status = plane_layout(chroma_width, chroma_height, output_size,
                          &info->planes[2], error);
    if (status != AV2H_OK) return status;
    if (!av2h_size_add(info->planes[2].offset, info->planes[2].size,
                       &output_size)) {
        return query_fail(error, AV2H_OVERFLOW, 0U, AV2H_CONTEXT_QUERY);
    }
output_layout_complete:
    if (output_size > active->max_output_size) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, 0U,
                          AV2H_CONTEXT_LIMITS);
    }
    info->output_size = output_size;
    status = av2h_decode_prepare(&info->image, &preparation, error);
    if (status != AV2H_OK) return status;
    if (preparation.output_size != output_size) {
        return query_fail(error, AV2H_INVALID_DATA, 0U,
                          AV2H_CONTEXT_OUTPUT);
    }
    info->workspace_size = preparation.workspace.size;
    info->workspace_alignment = preparation.workspace.alignment;
    if (info->workspace_alignment == 0U ||
        (info->workspace_alignment &
         (info->workspace_alignment - 1U)) != 0U) {
        return query_fail(error, AV2H_INVALID_DATA, 0U,
                          AV2H_CONTEXT_WORKSPACE);
    }
    if (info->workspace_size > active->max_workspace_size) {
        return query_fail(error, AV2H_LIMIT_EXCEEDED, 0U,
                          AV2H_CONTEXT_LIMITS);
    }
    info->input_size = input.size;
    info->input_fingerprint = input_fingerprint(input);
    return AV2H_OK;
}

Av2hStatus av2h_validate(Av2hSpan input,
                         const Av2hLimits *limits,
                         Av2hError *error) {
    Av2hInfo info;
    return av2h_query(input, limits, &info, error);
}
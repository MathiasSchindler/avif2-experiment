#include "cli_common.h"

#include "decoder/internal/coeff_modules.h"

#include <stddef.h>

typedef struct {
    Av2hPngWriteCallback write;
    void *context;
    Av2hStatus status;
} Av2hCliText;

static void write_console(Av2hCliText *writer, const char *text) {
    size_t length = 0U;
    while (text[length] != '\0') ++length;
    if (writer->status == AV2H_OK) {
        writer->status = writer->write(
            writer->context, (const uint8_t *)text, length);
    }
}

static void write_unsigned(Av2hCliText *writer, uint64_t value) {
    char buffer[21U];
    size_t index = sizeof(buffer);
    buffer[--index] = '\0';
    do {
        buffer[--index] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    write_console(writer, buffer + index);
}

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static void write_signed(Av2hCliText *writer, int value) {
    if (value < 0) {
        write_console(writer, "-");
        write_unsigned(writer, (uint64_t)(-(int64_t)value));
    } else {
        write_unsigned(writer, (uint64_t)value);
    }
}
#endif

static void write_value(
    Av2hCliText *writer, const char *label, uint64_t value) {
    write_console(writer, label);
    write_unsigned(writer, value);
}

static const char *line_ending(Av2hCliDiagnosticStyle style) {
    return style == AV2H_CLI_DIAGNOSTIC_TRACE ? "\r\n" :
        style == AV2H_CLI_DIAGNOSTIC_COMPACT ||
        style == AV2H_CLI_DIAGNOSTIC_STATUS ? "\n" : NULL;
}

static void write_json(
    Av2hCliText *writer,
    const Av2hError *diagnostic,
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
            result->cause == AV2H_TRUNCATED ? AV2H_DIAGNOSTIC_REASON_TRUNCATED :
            result->cause == AV2H_INVALID_DATA ?
                AV2H_DIAGNOSTIC_REASON_MALFORMED :
            result->cause == AV2H_OUT_OF_MEMORY ||
            result->cause == AV2H_LIMIT_EXCEEDED ||
            result->cause == AV2H_OVERFLOW ? AV2H_DIAGNOSTIC_REASON_RESOURCE :
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
    write_console(writer, "{\"status\":\"");
    write_console(writer, av2h_status_string(diagnostic->status));
    write_console(writer, "\",\"stage\":\"");
    write_console(writer, av2h_diagnostic_stage_string(stage));
    write_console(writer, "\",\"reason\":\"");
    write_console(writer, av2h_diagnostic_reason_string(reason));
    write_console(writer, "\",\"syntax\":\"");
    write_console(writer, av2h_diagnostic_syntax_string(syntax));
    write_value(writer, "\",\"offset\":", diagnostic->offset);
    write_value(writer, ",\"entropy_tell\":", tell);
    write_value(writer, ",\"parsed_value\":", parsed_value);
    write_value(writer, ",\"row\":", row);
    write_value(writer, ",\"column\":", column);
    write_value(writer, ",\"plane\":", plane);
    write_value(writer, ",\"luma_leaves\":", result->luma_leaf_count);
    write_value(writer, ",\"chroma_leaves\":", result->chroma_leaf_count);
    write_console(writer, "}\n");
}

static void write_compact_error(
    Av2hCliText *writer, const Av2hFrameDecodeResult *result) {
    write_value(writer, " (cause ", result->cause);
    write_value(writer, ", stage ", result->stage);
    write_value(writer, ", tell ", result->entropy_tell);
    write_value(writer, ")\n  leaf=(", result->row);
    write_value(writer, ",", result->column);
    write_value(writer, ") block=", result->block_size);
    write_value(writer, " plane=", result->plane);
    write_value(writer, " syntax=", result->syntax_stage);
    write_value(writer, " mode=", result->mode);
    write_value(writer, " mrl=", result->mrl_index);
    write_value(writer, "/", result->multi_line_mrl);
    write_value(writer, " tx=", result->tx_size);
    write_value(writer, " primary=", result->primary_type);
    write_value(writer, " secondary=", result->secondary_symbol);
    write_value(writer, "/", result->secondary_set);
    write_value(writer, " fsc=", result->fsc_mode);
    write_value(writer, " skip=", result->skip_transform);
    write_value(writer, " zero=", result->all_zero);
    write_value(writer, " eob=", result->eob);
    write_value(writer, " coeff=", result->coefficient_position);
    write_value(writer, "/", result->coefficient_context);
    write_value(writer, "/", result->coefficient_kind);
    write_value(writer, " inverse=", result->inverse_transform_stage);
    write_console(writer, "\n");
}

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static const char *decode_stage_name(Av2hFrameDecodeStage stage) {
    switch (stage) {
        case AV2H_FRAME_DECODE_STAGE_INITIALIZE: return "initialize";
        case AV2H_FRAME_DECODE_STAGE_LUMA_PARSE: return "luma parse";
        case AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT:
            return "luma reconstruction";
        case AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK: return "luma deblock map";
        case AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT: return "luma commit";
        case AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE: return "chroma parse";
        case AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM:
            return "chroma cross transform";
        case AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT:
            return "chroma reconstruction";
        case AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK:
            return "chroma deblock map";
        case AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT: return "chroma commit";
        case AV2H_FRAME_DECODE_STAGE_COMPLETENESS: return "completeness check";
        case AV2H_FRAME_DECODE_STAGE_FILTER: return "frame filter";
        case AV2H_FRAME_DECODE_STAGE_OUTPUT: return "planar output";
        default: return "unknown";
    }
}

static const char *luma_syntax_stage_name(uint8_t stage) {
    switch (stage) {
        case 0U: return "partition or geometry";
        case 1U: return "mode or palette";
        case 2U: return "coefficient prefix";
        case 3U: return "transform type";
        case 4U: return "coefficient levels";
        default: return "unknown";
    }
}

static const char *chroma_syntax_stage_name(uint8_t stage) {
    switch (stage) {
        case 0U: return "partition or mode";
        case 1U: return "U coefficient prefix";
        case 2U: return "U transform type";
        case 3U: return "U coefficient levels";
        case 4U: return "V coefficient prefix";
        case 5U: return "V transform type";
        case 6U: return "V coefficient levels";
        default: return "unknown";
    }
}

static const char *reconstruction_stage_name(uint8_t stage) {
    switch (stage) {
        case 0U: return "setup or special predictor";
        case 1U: return "prediction";
        case 2U: return "inverse transform";
        case 3U: return "output geometry";
        case 10U: return "IntraBC precision";
        case 11U: return "IntraBC source bounds";
        case 12U: return "IntraBC reference availability";
        case 13U: return "IntraBC inverse transform";
        default: return "unknown";
    }
}

static const char *chroma_reconstruction_stage_name(uint8_t stage) {
    switch (stage) {
        case 0U: return "setup";
        case 1U: return "prediction";
        case 2U: return "output";
        case 3U: return "all-zero output";
        case 11U: return "inverse transform geometry";
        case 12U: return "inverse transform type";
        case 13U: return "secondary inverse transform";
        case 14U: return "primary inverse transform";
        default: return "unknown";
    }
}

static const char *block_size_name(uint8_t block_size) {
    static const char *const names[AV2H_BLOCK_SIZE_COUNT] = {
        "4x4", "4x8", "8x4", "8x8", "8x16", "16x8", "16x16",
        "16x32", "32x16", "32x32", "32x64", "64x32", "64x64",
        "64x128", "128x64", "128x128", "128x256", "256x128",
        "256x256", "4x16", "16x4", "8x32", "32x8", "16x64",
        "64x16", "4x32", "32x4", "8x64", "64x8"
    };
    return block_size < AV2H_BLOCK_SIZE_COUNT ? names[block_size] : "unknown";
}

static void write_decode_diagnostic(
    Av2hCliText *writer, const Av2hFrameDecodeResult *result) {
    static const uint16_t zero_cdf[4U] = { 0U, 0U, 0U, 0U };
    static const uint64_t zero_range[4U] = { 0U, 0U, 0U, 0U };
    const uint16_t *coefficient_cdf;
    const uint64_t *coefficient_range;
    const Av2hPartitionDecision *partition_trace;
    const uint8_t *partition_trace_structure;
    const Av2hPartitionLeafDiagnostic *leaf_trace;
    size_t partition_trace_count;
    size_t partition_trace_index;
    size_t leaf_trace_count;
    size_t leaf_trace_index;
    write_console(writer, "  cause: ");
    write_console(writer, av2h_status_string(result->cause));
    write_console(writer, "\r\n  stage: ");
    write_console(writer, decode_stage_name(result->stage));
    if (result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_PARSE) {
        write_console(writer, " / ");
        write_console(writer, luma_syntax_stage_name(result->syntax_stage));
    } else if (result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE) {
        write_console(writer, " / ");
        write_console(writer, chroma_syntax_stage_name(result->syntax_stage));
    } else if (result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT ||
               result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT) {
        write_console(writer, " / ");
        write_console(writer, result->stage ==
            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT ?
            chroma_reconstruction_stage_name(result->syntax_stage) :
            reconstruction_stage_name(result->syntax_stage));
    }
    write_console(writer, "\r\n  leaf: (");
    write_unsigned(writer, result->row);
    write_console(writer, ",");
    write_unsigned(writer, result->column);
    write_console(writer, ") block=");
    write_console(writer, block_size_name(result->block_size));
    write_console(writer, " plane=");
    write_console(writer, result->plane == 0U ? "Y" : result->plane == 1U ? "U" : "V");
    write_console(writer, "\r\n  entropy tell: ");
    write_unsigned(writer, result->entropy_tell);
    write_console(writer, "; completed leaves: Y=");
    write_unsigned(writer, result->luma_leaf_count);
    write_console(writer, " UV=");
    write_unsigned(writer, result->chroma_leaf_count);
    write_console(writer, "\r\n  syntax: mode=");
    write_unsigned(writer, result->mode);
    write_console(writer, " angle=");
    if (result->angle_delta < 0) {
        write_console(writer, "-");
        write_unsigned(writer, (size_t)(-result->angle_delta));
    } else {
        write_unsigned(writer, (size_t)result->angle_delta);
    }
    write_console(writer, " mrl=");
    write_unsigned(writer, result->mrl_index);
    write_console(writer, " multi-line=");
    write_unsigned(writer, result->multi_line_mrl);
    write_console(writer, " ibp=");
    write_unsigned(writer, result->enable_ibp);
    write_console(writer, " intrabc=");
    write_unsigned(writer, result->use_intrabc);
    write_console(writer, " displacement=");
    write_signed(writer, result->displacement_x);
    write_console(writer, "/");
    write_signed(writer, result->displacement_y);
    write_console(writer, " morph=");
    write_unsigned(writer, result->morph_pred);
    write_console(writer, " fsc=");
    write_unsigned(writer, result->fsc_mode);
    write_console(writer, " skip-transform=");
    write_unsigned(writer, result->skip_transform);
    write_console(writer, " palette-size=");
    write_unsigned(writer, result->palette_size);
    write_console(writer, " tx-size=");
    write_unsigned(writer, result->tx_size);
    write_console(writer, " inverse-stage=");
    write_unsigned(writer, result->inverse_transform_stage);
    write_console(writer, " intrabc-source=");
    write_unsigned(writer, result->intrabc_source_x);
    write_console(writer, "/");
    write_unsigned(writer, result->intrabc_source_y);
    write_console(writer, "/");
    write_unsigned(writer, result->intrabc_map_index);
    write_console(writer, " prefix-stage=");
    write_unsigned(writer, result->prefix_stage);
    write_console(writer, " syntax-stage=");
    write_unsigned(writer, result->syntax_stage);
    write_console(writer, " skip-ctx=");
    write_unsigned(writer, result->txb_skip_context);
    write_console(writer, " all-zero=");
    write_unsigned(writer, result->all_zero);
    write_console(writer, " eob-point=");
    write_unsigned(writer, result->eob_point);
    write_console(writer, " tx-type=");
    write_unsigned(writer, result->primary_type);
    write_console(writer, " primary-symbol=");
    write_unsigned(writer, result->primary_symbol);
    write_console(writer, " primary-set-symbol=");
    write_unsigned(writer, result->primary_set_symbol);
    write_console(writer, " secondary-symbol=");
    write_unsigned(writer, result->secondary_symbol);
    write_console(writer, " secondary-set=");
    write_unsigned(writer, result->secondary_set);
    write_console(writer, " cctx-type=");
    write_unsigned(writer, result->cctx_type);
    write_console(writer, " is-cfl=");
    write_unsigned(writer, result->is_cfl);
    write_console(writer, " cfl-kind=");
    write_unsigned(writer, result->cfl_kind);
    write_console(writer, " mhccp-direction=");
    write_unsigned(writer, result->mhccp_direction);
    write_console(writer, " cfl-filter=");
    write_unsigned(writer, result->cfl_filter_index);
    write_console(writer, " mhccp-reason=");
    write_unsigned(writer, result->mhccp_reason);
    write_console(writer, " chroma-parse-reason=");
    write_unsigned(writer, result->chroma_parse_reason);
    write_console(writer, " chroma-mode-index=");
    write_unsigned(writer, result->chroma_mode_index);
    write_console(writer, " filter-reason=");
    write_unsigned(writer, result->filter_reason);
    write_console(writer, " eob=");
    write_unsigned(writer, result->eob);
    write_console(writer, " uv-coeff-counts=");
    write_unsigned(writer, result->u_coefficient_count);
    write_console(writer, "/");
    write_unsigned(writer, result->v_coefficient_count);
    write_console(writer, " coeff-kind=");
    write_unsigned(writer, result->coefficient_kind);
    write_console(writer, " coeff-pos=");
    write_unsigned(writer, result->coefficient_position);
    write_console(writer, " coeff-ctx=");
    write_unsigned(writer, result->coefficient_context);
    write_console(writer, " prefix-tells=");
    write_unsigned(writer, result->after_all_zero_tell);
    write_console(writer, "/");
    write_unsigned(writer, result->after_eob_point_tell);
    write_console(writer, " eob-range=");
    write_unsigned(writer, result->eob_range);
    write_console(writer, " skip-range=");
    write_unsigned(writer, result->skip_range);
    write_console(writer, " prefix-range=");
    write_unsigned(writer, result->before_prefix_range);
    write_console(writer, " eob-tx/class=");
    write_unsigned(writer, result->eob_tx_size);
    write_console(writer, "/");
    write_unsigned(writer, result->eob_class);
    coefficient_cdf = av2h_coeff_level_diagnostic_cdf(result->diagnostics);
    if (coefficient_cdf == NULL) coefficient_cdf = zero_cdf;
    write_console(writer, "\r\n  coeff-cdf=");
    write_unsigned(writer, coefficient_cdf[0]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_cdf[1]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_cdf[2]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_cdf[3]);
    coefficient_range = av2h_coeff_level_diagnostic_range(
        result->diagnostics);
    if (coefficient_range == NULL) coefficient_range = zero_range;
    {
        size_t candidate_count;
        size_t candidate;
        const int16_t *candidates =
            av2h_intrabc_diagnostic_candidates(
                result->diagnostics, &candidate_count);
        write_console(writer, "\r\n  intrabc-candidates=");
        for (candidate = 0U; candidate < candidate_count; ++candidate) {
            if (candidate != 0U) write_console(writer, " ");
            write_signed(writer, candidates[candidate * 2U]);
            write_console(writer, "/");
            write_signed(writer, candidates[candidate * 2U + 1U]);
        }
        candidates = av2h_intrabc_diagnostic_bank(
            result->diagnostics, &candidate_count);
        write_console(writer, "\r\n  intrabc-bank=");
        for (candidate = 0U; candidate < candidate_count; ++candidate) {
            if (candidate != 0U) write_console(writer, " ");
            write_signed(writer, candidates[candidate * 2U]);
            write_console(writer, "/");
            write_signed(writer, candidates[candidate * 2U + 1U]);
        }
    }
    write_console(writer, "\r\n  coeff-range=");
    write_unsigned(writer, coefficient_range[0]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_range[1]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_range[2]);
    write_console(writer, "/");
    write_unsigned(writer, coefficient_range[3]);
    partition_trace = av2h_partition_diagnostic_trace(
        result->diagnostics, &partition_trace_count);
    partition_trace_structure = av2h_partition_diagnostic_trace_structure(
        result->diagnostics);
    for (partition_trace_index = 0U;
         partition_trace_index < partition_trace_count;
         ++partition_trace_index) {
        const Av2hPartitionDecision *decision =
            &partition_trace[partition_trace_index];
        write_console(writer, partition_trace_structure[partition_trace_index] == 0U ?
            "\r\n  local-part: (" : "\r\n  local-cpart: (");
        write_unsigned(writer, decision->node.row);
        write_console(writer, ",");
        write_unsigned(writer, decision->node.column);
        write_console(writer, ") bsize=");
        write_unsigned(writer, decision->node.block_size);
        write_console(writer, " partition=");
        write_unsigned(writer, decision->partition);
        write_console(writer, " contexts=");
        write_unsigned(writer, decision->split_context);
        write_console(writer, "/");
        write_unsigned(writer, decision->rect_type_context);
        write_console(writer, "/");
        write_unsigned(writer, decision->extension_context);
        write_console(writer, " tell=");
        write_unsigned(writer, decision->after_bits);
    }
    leaf_trace = av2h_partition_diagnostic_leaf_trace(
        result->diagnostics, &leaf_trace_count);
    for (leaf_trace_index = 0U; leaf_trace_index < leaf_trace_count;
         ++leaf_trace_index) {
        write_console(writer, "\r\n  local-leaf: stage=");
        write_unsigned(writer, leaf_trace[leaf_trace_index].stage);
        write_console(writer, " mode=");
        write_unsigned(writer, leaf_trace[leaf_trace_index].mode);
        write_console(writer, " eob=");
        write_unsigned(writer, leaf_trace[leaf_trace_index].eob);
        write_console(writer, " tell=");
        write_unsigned(writer, leaf_trace[leaf_trace_index].tell);
    }
    write_console(writer, "\r\n");
}
#endif

Av2hStatus av2h_cli_write_usage(
    const char *program, int diagnostics_supported,
    Av2hCliDiagnosticStyle style, Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    const char *ending = line_ending(style);
    if (program == NULL || write == NULL || ending == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    write_console(&writer, "usage: ");
    write_console(&writer, program);
    write_console(&writer,
        " INPUT.obu -o OUTPUT.i400|OUTPUT.i420|OUTPUT.i444|"
        "OUTPUT.yuv420p10le|OUTPUT.yuv444p10le|OUTPUT.bmp|OUTPUT.png");
    if (diagnostics_supported) write_console(&writer, " [--diagnostics]");
    write_console(&writer, ending);
    return writer.status;
}

Av2hStatus av2h_cli_write_message(
    Av2hCliMessage message, Av2hCliDiagnosticStyle style,
    Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    const char *ending = line_ending(style);
    const char *text;
    if (write == NULL || ending == NULL) return AV2H_INVALID_ARGUMENT;
    switch (message) {
        case AV2H_CLI_MESSAGE_READ_INPUT:
            text = "error: cannot read input OBU"; break;
        case AV2H_CLI_MESSAGE_WORKERS:
            text = "error: cannot initialize decode workers"; break;
        case AV2H_CLI_MESSAGE_COMMAND_LINE:
            text = "error: cannot allocate command line"; break;
        case AV2H_CLI_MESSAGE_DIAGNOSTICS:
            text = "error: cannot write diagnostics"; break;
        default: return AV2H_INVALID_ARGUMENT;
    }
    write_console(&writer, text);
    write_console(&writer, ending);
    return writer.status;
}

Av2hStatus av2h_cli_write_decode_error(
    const Av2hCliImage *image, Av2hStatus status,
    Av2hCliDiagnosticStyle style,
    Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    const char *ending = line_ending(style);
    if (image == NULL || write == NULL || ending == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    write_console(&writer, "error: decode failed: ");
    write_console(&writer, av2h_status_string(status));
    if (style == AV2H_CLI_DIAGNOSTIC_COMPACT) {
        write_compact_error(&writer, &image->result);
    } else {
        write_console(&writer, ending);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        if (style == AV2H_CLI_DIAGNOSTIC_TRACE) {
            write_decode_diagnostic(&writer, &image->result);
        }
#endif
    }
    return writer.status;
}

Av2hStatus av2h_cli_write_output_error(
    Av2hCliWriteResult result, Av2hCliDiagnosticStyle style,
    Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    const char *ending = line_ending(style);
    if (write == NULL || ending == NULL) return AV2H_INVALID_ARGUMENT;
    if (result == AV2H_CLI_WRITE_RAW_MISMATCH) {
        write_console(&writer,
            "error: raw output extension does not match decoded format");
        write_console(&writer, ending);
    } else if (result == AV2H_CLI_WRITE_PREVIEW_DEPTH ||
               result == AV2H_CLI_WRITE_FAILED) {
        if (result == AV2H_CLI_WRITE_PREVIEW_DEPTH) {
            write_console(&writer, "error: preview output requires 8-bit input");
            write_console(&writer, ending);
        }
        write_console(&writer, "error: cannot write output");
        write_console(&writer, ending);
    } else if (result != AV2H_CLI_WRITE_OK) {
        return AV2H_INVALID_ARGUMENT;
    }
    return writer.status;
}

Av2hStatus av2h_cli_write_json(
    const Av2hCliImage *image, Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    if (image == NULL || write == NULL) return AV2H_INVALID_ARGUMENT;
    write_json(&writer, &image->error, &image->result);
    return writer.status;
}

Av2hStatus av2h_cli_write_trace(
    const Av2hFrameDecodeResult *result,
    Av2hPngWriteCallback write, void *context) {
    Av2hCliText writer = { write, context, AV2H_OK };
    if (result == NULL || write == NULL) return AV2H_INVALID_ARGUMENT;
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    write_decode_diagnostic(&writer, result);
#endif
    return writer.status;
}

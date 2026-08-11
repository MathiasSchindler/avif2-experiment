#include "base.h"
#include "decoder/frame_output.h"
#include "decoder/internal/coeff_modules.h"
#include "decoder/internal/frame_workspace.h"
#include "decoder/partition.h"
#include "decoder/tile.h"
#include "platform/windows/win32_min.h"
#include "preview.h"

#define AV2H_CLI_MAX_INPUT (256U * 1024U * 1024U)
#define AV2H_CLI_ARGUMENT_CAPACITY 4
#define AV2H_BMP_HEADER_SIZE 54U

typedef struct {
    void *memory;
    size_t size;
} Av2hOwnedMemory;

static size_t wide_length(const Av2hWinChar *text) {
    size_t length = 0U;
    while (text[length] != 0U) ++length;
    return length;
}

static int ascii_equal(const Av2hWinChar *wide, const char *ascii) {
    size_t index = 0U;
    while (wide[index] != 0U && ascii[index] != '\0') {
        if (wide[index] != (uint8_t)ascii[index]) return 0;
        ++index;
    }
    return wide[index] == 0U && ascii[index] == '\0';
}

static int wide_ends_with_ascii(const Av2hWinChar *wide, const char *ascii) {
    size_t wide_size = wide_length(wide);
    size_t ascii_size = 0U;
    size_t index;
    while (ascii[ascii_size] != '\0') ++ascii_size;
    if (ascii_size > wide_size) return 0;
    for (index = 0U; index < ascii_size; ++index) {
        uint16_t left = wide[wide_size - ascii_size + index];
        uint8_t right = (uint8_t)ascii[index];
        if (left >= 'A' && left <= 'Z') left = (uint16_t)(left + 32U);
        if (right >= 'A' && right <= 'Z') right = (uint8_t)(right + 32U);
        if (left != right) return 0;
    }
    return 1;
}

static int is_raw_output_path(const Av2hWinChar *path) {
    size_t index;
    for (index = 0U; index < av2h_raw_output_extension_count(); ++index) {
        if (wide_ends_with_ascii(
                path, av2h_raw_output_extension_at(index))) return 1;
    }
    return 0;
}

static void write_console(const char *text) {
    Av2hWinHandle error = GetStdHandle(AV2H_WIN_STD_ERROR_HANDLE);
    size_t length = 0U;
    Av2hWinDword written;
    while (text[length] != '\0') ++length;
    if (error != 0 && error != AV2H_WIN_INVALID_HANDLE &&
        length <= UINT32_MAX) {
        (void)WriteFile(error, text, (Av2hWinDword)length, &written, 0);
    }
}

static void write_status(const char *prefix, Av2hStatus status) {
    write_console(prefix);
    write_console(av2h_status_string(status));
    write_console("\r\n");
}

#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
static void write_unsigned(size_t value) {
    char buffer[3U * sizeof(size_t) + 1U];
    size_t index = sizeof(buffer);
    buffer[--index] = '\0';
    do {
        buffer[--index] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    write_console(buffer + index);
}

static void write_signed(int value) {
    if (value < 0) {
        write_console("-");
        write_unsigned((size_t)(-(int64_t)value));
    } else {
        write_unsigned((size_t)value);
    }
}

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

static void write_decode_diagnostic(const Av2hFrameDecodeResult *result) {
    const uint16_t *coefficient_cdf;
    const uint64_t *coefficient_range;
    const Av2hPartitionDecision *partition_trace;
    const uint8_t *partition_trace_structure;
    const Av2hPartitionLeafDiagnostic *leaf_trace;
    size_t partition_trace_count;
    size_t partition_trace_index;
    size_t leaf_trace_count;
    size_t leaf_trace_index;
    write_console("  cause: ");
    write_console(av2h_status_string(result->cause));
    write_console("\r\n  stage: ");
    write_console(decode_stage_name(result->stage));
    if (result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_PARSE) {
        write_console(" / ");
        write_console(luma_syntax_stage_name(result->syntax_stage));
    } else if (result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE) {
        write_console(" / ");
        write_console(chroma_syntax_stage_name(result->syntax_stage));
    } else if (result->stage == AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT ||
               result->stage == AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT) {
        write_console(" / ");
        write_console(result->stage ==
            AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT ?
            chroma_reconstruction_stage_name(result->syntax_stage) :
            reconstruction_stage_name(result->syntax_stage));
    }
    write_console("\r\n  leaf: (");
    write_unsigned(result->row);
    write_console(",");
    write_unsigned(result->column);
    write_console(") block=");
    write_console(block_size_name(result->block_size));
    write_console(" plane=");
    write_console(result->plane == 0U ? "Y" : result->plane == 1U ? "U" : "V");
    write_console("\r\n  entropy tell: ");
    write_unsigned(result->entropy_tell);
    write_console("; completed leaves: Y=");
    write_unsigned(result->luma_leaf_count);
    write_console(" UV=");
    write_unsigned(result->chroma_leaf_count);
    write_console("\r\n  syntax: mode=");
    write_unsigned(result->mode);
    write_console(" angle=");
    if (result->angle_delta < 0) {
        write_console("-");
        write_unsigned((size_t)(-result->angle_delta));
    } else {
        write_unsigned((size_t)result->angle_delta);
    }
    write_console(" mrl=");
    write_unsigned(result->mrl_index);
    write_console(" multi-line=");
    write_unsigned(result->multi_line_mrl);
    write_console(" ibp=");
    write_unsigned(result->enable_ibp);
    write_console(" intrabc=");
    write_unsigned(result->use_intrabc);
    write_console(" displacement=");
    write_signed(result->displacement_x);
    write_console("/");
    write_signed(result->displacement_y);
    write_console(" morph=");
    write_unsigned(result->morph_pred);
    write_console(" fsc=");
    write_unsigned(result->fsc_mode);
    write_console(" skip-transform=");
    write_unsigned(result->skip_transform);
    write_console(" palette-size=");
    write_unsigned(result->palette_size);
    write_console(" tx-size=");
    write_unsigned(result->tx_size);
    write_console(" inverse-stage=");
    write_unsigned(result->inverse_transform_stage);
    write_console(" intrabc-source=");
    write_unsigned(result->intrabc_source_x);
    write_console("/");
    write_unsigned(result->intrabc_source_y);
    write_console("/");
    write_unsigned(result->intrabc_map_index);
    write_console(" prefix-stage=");
    write_unsigned(result->prefix_stage);
    write_console(" syntax-stage=");
    write_unsigned(result->syntax_stage);
    write_console(" skip-ctx=");
    write_unsigned(result->txb_skip_context);
    write_console(" all-zero=");
    write_unsigned(result->all_zero);
    write_console(" eob-point=");
    write_unsigned(result->eob_point);
    write_console(" tx-type=");
    write_unsigned(result->primary_type);
    write_console(" primary-symbol=");
    write_unsigned(result->primary_symbol);
    write_console(" primary-set-symbol=");
    write_unsigned(result->primary_set_symbol);
    write_console(" secondary-symbol=");
    write_unsigned(result->secondary_symbol);
    write_console(" secondary-set=");
    write_unsigned(result->secondary_set);
    write_console(" cctx-type=");
    write_unsigned(result->cctx_type);
    write_console(" is-cfl=");
    write_unsigned(result->is_cfl);
    write_console(" cfl-kind=");
    write_unsigned(result->cfl_kind);
    write_console(" mhccp-direction=");
    write_unsigned(result->mhccp_direction);
    write_console(" cfl-filter=");
    write_unsigned(result->cfl_filter_index);
    write_console(" mhccp-reason=");
    write_unsigned(result->mhccp_reason);
    write_console(" chroma-parse-reason=");
    write_unsigned(result->chroma_parse_reason);
    write_console(" chroma-mode-index=");
    write_unsigned(result->chroma_mode_index);
    write_console(" filter-reason=");
    write_unsigned(result->filter_reason);
    write_console(" eob=");
    write_unsigned(result->eob);
    write_console(" uv-coeff-counts=");
    write_unsigned(result->u_coefficient_count);
    write_console("/");
    write_unsigned(result->v_coefficient_count);
    write_console(" coeff-kind=");
    write_unsigned(result->coefficient_kind);
    write_console(" coeff-pos=");
    write_unsigned(result->coefficient_position);
    write_console(" coeff-ctx=");
    write_unsigned(result->coefficient_context);
    write_console(" prefix-tells=");
    write_unsigned(result->after_all_zero_tell);
    write_console("/");
    write_unsigned(result->after_eob_point_tell);
    write_console(" eob-range=");
    write_unsigned(result->eob_range);
    write_console(" skip-range=");
    write_unsigned(result->skip_range);
    write_console(" prefix-range=");
    write_unsigned(result->before_prefix_range);
    write_console(" eob-tx/class=");
    write_unsigned(result->eob_tx_size);
    write_console("/");
    write_unsigned(result->eob_class);
    coefficient_cdf = av2h_coeff_level_diagnostic_cdf();
    write_console("\r\n  coeff-cdf=");
    write_unsigned(coefficient_cdf[0]);
    write_console("/");
    write_unsigned(coefficient_cdf[1]);
    write_console("/");
    write_unsigned(coefficient_cdf[2]);
    write_console("/");
    write_unsigned(coefficient_cdf[3]);
    coefficient_range = av2h_coeff_level_diagnostic_range();
    {
        size_t candidate_count;
        size_t candidate;
        const int16_t *candidates =
            av2h_intrabc_diagnostic_candidates(&candidate_count);
        write_console("\r\n  intrabc-candidates=");
        for (candidate = 0U; candidate < candidate_count; ++candidate) {
            if (candidate != 0U) write_console(" ");
            write_signed(candidates[candidate * 2U]);
            write_console("/");
            write_signed(candidates[candidate * 2U + 1U]);
        }
        candidates = av2h_intrabc_diagnostic_bank(&candidate_count);
        write_console("\r\n  intrabc-bank=");
        for (candidate = 0U; candidate < candidate_count; ++candidate) {
            if (candidate != 0U) write_console(" ");
            write_signed(candidates[candidate * 2U]);
            write_console("/");
            write_signed(candidates[candidate * 2U + 1U]);
        }
    }
    write_console("\r\n  coeff-range=");
    write_unsigned(coefficient_range[0]);
    write_console("/");
    write_unsigned(coefficient_range[1]);
    write_console("/");
    write_unsigned(coefficient_range[2]);
    write_console("/");
    write_unsigned(coefficient_range[3]);
    partition_trace = av2h_partition_diagnostic_trace(&partition_trace_count);
    partition_trace_structure = av2h_partition_diagnostic_trace_structure();
    for (partition_trace_index = 0U;
         partition_trace_index < partition_trace_count;
         ++partition_trace_index) {
        const Av2hPartitionDecision *decision =
            &partition_trace[partition_trace_index];
        write_console(partition_trace_structure[partition_trace_index] == 0U ?
            "\r\n  local-part: (" : "\r\n  local-cpart: (");
        write_unsigned(decision->node.row);
        write_console(",");
        write_unsigned(decision->node.column);
        write_console(") bsize=");
        write_unsigned(decision->node.block_size);
        write_console(" partition=");
        write_unsigned(decision->partition);
        write_console(" contexts=");
        write_unsigned(decision->split_context);
        write_console("/");
        write_unsigned(decision->rect_type_context);
        write_console("/");
        write_unsigned(decision->extension_context);
        write_console(" tell=");
        write_unsigned(decision->after_bits);
    }
    leaf_trace = av2h_partition_diagnostic_leaf_trace(&leaf_trace_count);
    for (leaf_trace_index = 0U; leaf_trace_index < leaf_trace_count;
         ++leaf_trace_index) {
        write_console("\r\n  local-leaf: stage=");
        write_unsigned(leaf_trace[leaf_trace_index].stage);
        write_console(" mode=");
        write_unsigned(leaf_trace[leaf_trace_index].mode);
        write_console(" eob=");
        write_unsigned(leaf_trace[leaf_trace_index].eob);
        write_console(" tell=");
        write_unsigned(leaf_trace[leaf_trace_index].tell);
    }
    write_console("\r\n");
}
#endif

static int parse_arguments(Av2hWinChar *text,
                           Av2hWinChar **arguments,
                           int capacity) {
    Av2hWinChar *read = text;
    Av2hWinChar *write = text;
    int count = 0;
    while (*read != 0U) {
        int quoted = 0;
        while (*read == ' ' || *read == '\t') ++read;
        if (*read == 0U) break;
        if (count == capacity) return -1;
        arguments[count++] = write;
        while (*read != 0U) {
            if (*read == '"') {
                quoted = !quoted;
                ++read;
                continue;
            }
            if (!quoted && (*read == ' ' || *read == '\t')) break;
            *write++ = *read++;
        }
        if (quoted) return -1;
        if (*read != 0U) ++read;
        *write++ = 0U;
        while (*read == ' ' || *read == '\t') ++read;
    }
    return count;
}

static void release_memory(Av2hOwnedMemory *owned) {
    if (owned->memory != 0) {
        (void)VirtualFree(owned->memory, 0U, AV2H_WIN_MEM_RELEASE);
    }
    owned->memory = 0;
    owned->size = 0U;
}

static int allocate_memory(size_t size, Av2hOwnedMemory *owned) {
    owned->memory = VirtualAlloc(
        0, size == 0U ? 1U : size,
        AV2H_WIN_MEM_RESERVE | AV2H_WIN_MEM_COMMIT,
        AV2H_WIN_PAGE_READWRITE);
    owned->size = owned->memory != 0 ? size : 0U;
    return owned->memory != 0;
}

static int read_entire_file(const Av2hWinChar *path,
                            Av2hOwnedMemory *owned) {
    Av2hWinHandle file = CreateFileW(
        path, AV2H_WIN_GENERIC_READ, AV2H_WIN_FILE_SHARE_READ, 0,
        AV2H_WIN_OPEN_EXISTING, AV2H_WIN_FILE_ATTRIBUTE_NORMAL, 0);
    Av2hWinLargeInteger size;
    Av2hWinDword read = 0U;
    int result = 0;
    if (file == AV2H_WIN_INVALID_HANDLE) return 0;
    if (!GetFileSizeEx(file, &size) || size.quad <= 0 ||
        (uint64_t)size.quad > AV2H_CLI_MAX_INPUT ||
        !allocate_memory((size_t)size.quad, owned)) {
        (void)CloseHandle(file);
        return 0;
    }
    if (ReadFile(file, owned->memory, (Av2hWinDword)owned->size, &read, 0) &&
        read == owned->size) {
        result = 1;
    }
    (void)CloseHandle(file);
    if (!result) release_memory(owned);
    return result;
}

static int write_all(Av2hWinHandle file, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    while (size != 0U) {
        Av2hWinDword chunk = size > UINT32_MAX ? UINT32_MAX :
            (Av2hWinDword)size;
        Av2hWinDword written = 0U;
        if (!WriteFile(file, bytes, chunk, &written, 0) || written != chunk) {
            return 0;
        }
        bytes += chunk;
        size -= chunk;
    }
    return 1;
}

static int write_entire_file(const Av2hWinChar *path,
                             const void *data,
                             size_t size) {
    Av2hWinHandle file = CreateFileW(
        path, AV2H_WIN_GENERIC_WRITE, 0U, 0, AV2H_WIN_CREATE_ALWAYS,
        AV2H_WIN_FILE_ATTRIBUTE_NORMAL, 0);
    int result;
    if (file == AV2H_WIN_INVALID_HANDLE) return 0;
    result = write_all(file, data, size);
    if (!CloseHandle(file)) result = 0;
    return result;
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return &ledger->entries[index];
    }
    return 0;
}

static Av2hStatus decode_obu(Av2hSpan input,
                            Av2hOwnedMemory *output,
                            uint16_t *width,
                            uint16_t *height,
                            uint8_t *chroma_format_idc,
                            Av2hFrameDecodeResult *decode_result) {
    Av2hObu entries[4U];
    Av2hObuLedger ledger = { entries, 4U, 0U };
    Av2hError error;
    const Av2hObu *sequence_obu;
    const Av2hObu *frame_obu;
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hOwnedMemory workspace_memory = { 0, 0U };
    Av2hFrameDecodeWorkspace workspace;
    Av2hFilterWorkspace filter_workspace;
    Av2hFilterWorkspaceStorage filter_storage;
    Av2hFrameDecodeWorkspaceLayout workspace_layout;
    size_t workspace_size = 0U;
    size_t output_size = 0U;
    Av2hStatus status = av2h_obu_parse(input, &ledger, &error);
    if (status != AV2H_OK) return status;
    sequence_obu = find_obu(&ledger, AV2H_OBU_SEQUENCE_HEADER);
    frame_obu = find_obu(&ledger, AV2H_OBU_CLOSED_LOOP_KEY);
    if (sequence_obu == 0 || frame_obu == 0) return AV2H_INVALID_DATA;
    status = av2h_sequence_state_parse(input, sequence_obu, &sequence, &error);
    if (status != AV2H_OK) return status;
    status = av2h_frame_header_closed_key_parse(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        &sequence, &frame);
    if (status != AV2H_OK) return status;
    status = av2h_tile_group_single_parse(
        (Av2hSpan){ input.data + frame_obu->payload_offset,
                    frame_obu->payload_size },
        frame.uncompressed_header_bits, &group);
    if (status != AV2H_OK) return status;
    status = av2h_planar_output_size(&frame, &output_size);
    if (status != AV2H_OK) return status;
    status = av2h_frame_decode_workspace_query(
        &sequence, &frame, &workspace_layout);
    if (status != AV2H_OK) return status;
    workspace_size = workspace_layout.size_bytes;
    if (!allocate_memory(workspace_size, &workspace_memory) ||
        !allocate_memory(output_size, output)) {
        release_memory(&workspace_memory);
        release_memory(output);
        return AV2H_OUT_OF_MEMORY;
    }
    status = av2h_frame_decode_workspace_init(
        &sequence, &frame, &workspace_layout,
        workspace_memory.memory, workspace_memory.size,
        &workspace, &filter_workspace, &filter_storage);
    if (status == AV2H_OK) {
        status = av2h_decode_frame_planar_private(
            group.entropy, &sequence, &frame, &workspace,
            output->memory, output->size, decode_result);
    }
    release_memory(&workspace_memory);
    if (status != AV2H_OK) {
        release_memory(output);
        return status;
    }
    *width = frame.width;
    *height = frame.height;
    *chroma_format_idc = frame.chroma_format_idc;
    return AV2H_OK;
}

static void store_u16le(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static void store_u32le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static int write_bmp(const Av2hWinChar *path,
                     const uint8_t *planar,
                     uint16_t width,
                     uint16_t height,
                     uint8_t chroma_format_idc) {
    size_t row_bytes;
    size_t pixel_bytes;
    size_t file_size;
    Av2hOwnedMemory row_memory = { 0, 0U };
    uint8_t header[AV2H_BMP_HEADER_SIZE];
    Av2hPlanarPreview preview;
    Av2hWinHandle file;
    size_t row;
    int result = 0;
    if (!av2h_size_multiply(width, 3U, &row_bytes) ||
        !av2h_size_align(row_bytes, 4U, &row_bytes) ||
        !av2h_size_multiply(row_bytes, height, &pixel_bytes) ||
        !av2h_size_add(AV2H_BMP_HEADER_SIZE, pixel_bytes, &file_size) ||
        file_size > UINT32_MAX ||
        av2h_planar_preview_init_tight_8bit(
            planar, width, height, chroma_format_idc,
            AV2H_COLOR_RANGE_LIMITED, 6U, &preview) != AV2H_OK ||
        !allocate_memory(row_bytes, &row_memory)) {
        return 0;
    }
    av2h_memory_fill(header, 0U, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    store_u32le(header + 2U, (uint32_t)file_size);
    store_u32le(header + 10U, AV2H_BMP_HEADER_SIZE);
    store_u32le(header + 14U, 40U);
    store_u32le(header + 18U, width);
    store_u32le(header + 22U, height);
    store_u16le(header + 26U, 1U);
    store_u16le(header + 28U, 24U);
    store_u32le(header + 34U, (uint32_t)pixel_bytes);
    file = CreateFileW(
        path, AV2H_WIN_GENERIC_WRITE, 0U, 0, AV2H_WIN_CREATE_ALWAYS,
        AV2H_WIN_FILE_ATTRIBUTE_NORMAL, 0);
    if (file == AV2H_WIN_INVALID_HANDLE) {
        release_memory(&row_memory);
        return 0;
    }
    if (!write_all(file, header, sizeof(header))) goto done;
    for (row = height; row != 0U; --row) {
        size_t y_row = row - 1U;
        uint8_t *destination = row_memory.memory;
        av2h_memory_fill(destination, 0U, row_bytes);
        if (av2h_planar_to_bgr8_row(
                &preview, (uint32_t)y_row,
                destination, row_bytes) != AV2H_OK) goto done;
        if (!write_all(file, destination, row_bytes)) goto done;
    }
    result = 1;
done:
    if (!CloseHandle(file)) result = 0;
    release_memory(&row_memory);
    return result;
}

static unsigned int run_decoder(void) {
    const Av2hWinChar *system_command_line = GetCommandLineW();
    size_t command_line_units = wide_length(system_command_line) + 1U;
    Av2hOwnedMemory command_line = { 0, 0U };
    Av2hOwnedMemory input = { 0, 0U };
    Av2hOwnedMemory output = { 0, 0U };
    Av2hWinChar *arguments[AV2H_CLI_ARGUMENT_CAPACITY];
    int argument_count;
    uint16_t width = 0U;
    uint16_t height = 0U;
    uint8_t chroma_format_idc = 0U;
    Av2hPixelFormat output_format;
    const char *raw_extension;
    Av2hFrameDecodeResult decode_result;
    Av2hStatus status;
    int written;
    av2h_memory_fill(&decode_result, 0U, sizeof(decode_result));
    if (!allocate_memory(
            command_line_units * sizeof(Av2hWinChar), &command_line)) {
        write_console("error: cannot allocate command line\r\n");
        return 1U;
    }
    av2h_memory_copy(
        command_line.memory, system_command_line,
        command_line_units * sizeof(Av2hWinChar));
    argument_count = parse_arguments(
        command_line.memory, arguments, AV2H_CLI_ARGUMENT_CAPACITY);
    if (argument_count != 4 || !ascii_equal(arguments[2], "-o") ||
        !(is_raw_output_path(arguments[3]) ||
          wide_ends_with_ascii(arguments[3], ".bmp"))) {
        write_console(
                        "usage: av2heif-decoder.exe INPUT.obu -o OUTPUT.i400|OUTPUT.i420|OUTPUT.i444|OUTPUT.bmp\r\n");
        release_memory(&command_line);
        return 2U;
    }
    if (!read_entire_file(arguments[1], &input)) {
        write_console("error: cannot read input OBU\r\n");
        release_memory(&command_line);
        return 1U;
    }
    status = decode_obu(
        (Av2hSpan){ input.memory, input.size }, &output, &width, &height,
        &chroma_format_idc, &decode_result);
    if (status != AV2H_OK) {
        write_status("error: decode failed: ", status);
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
        write_decode_diagnostic(&decode_result);
#endif
        release_memory(&output);
        release_memory(&input);
        release_memory(&command_line);
        return status == AV2H_UNSUPPORTED ? 3U : 1U;
    }
#if defined(AV2HEIF_ENABLE_DIAGNOSTICS)
    write_decode_diagnostic(&decode_result);
#endif
    if (!av2h_pixel_format_init_8bit(
            chroma_format_idc, &output_format)) {
        release_memory(&output);
        release_memory(&input);
        release_memory(&command_line);
        return 1U;
    }
    raw_extension = av2h_raw_output_extension(&output_format);
    if (!wide_ends_with_ascii(arguments[3], ".bmp") &&
        (raw_extension == 0 ||
         !wide_ends_with_ascii(arguments[3], raw_extension))) {
        write_console("error: raw output extension does not match decoded format\r\n");
        release_memory(&output);
        release_memory(&input);
        release_memory(&command_line);
        return 2U;
    }
    if (wide_ends_with_ascii(arguments[3], ".bmp")) {
        written = write_bmp(
            arguments[3], output.memory, width, height, chroma_format_idc);
    } else {
        written = write_entire_file(arguments[3], output.memory, output.size);
    }
    if (!written) write_console("error: cannot write output\r\n");
    release_memory(&output);
    release_memory(&input);
    release_memory(&command_line);
    return written ? 0U : 1U;
}

__declspec(noreturn) void mainCRTStartup(void) {
    ExitProcess(run_decoder());
}
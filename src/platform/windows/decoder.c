#include "base.h"
#include "decoder/frame_output.h"
#include "decoder/partition.h"
#include "decoder/tile.h"
#include "platform/windows/win32_min.h"

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

static void write_signed(int32_t value) {
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
        case AV2H_FRAME_DECODE_STAGE_OUTPUT: return "I420 output";
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

static void write_partition_path(void) {
    size_t count = av2h_debug_partition_trace_count;
    size_t first = count > AV2H_DEBUG_PARTITION_TRACE_CAPACITY ?
        count - AV2H_DEBUG_PARTITION_TRACE_CAPACITY : 0U;
    size_t index;
    if (count == 0U) return;
    write_console("\r\n  recent partitions:");
    for (index = first; index < count; ++index) {
        const Av2hPartitionDecision *decision =
            &av2h_debug_partition_trace[
                index % AV2H_DEBUG_PARTITION_TRACE_CAPACITY];
        write_console(" (");
        write_unsigned(decision->node.row);
        write_console(",");
        write_unsigned(decision->node.column);
        write_console(",");
        write_console(block_size_name(decision->node.block_size));
        write_console(")=");
        write_unsigned(decision->partition);
        write_console("@");
        write_unsigned(decision->after_bits);
        write_console("[");
        write_unsigned(decision->split_context);
        write_console("/");
        write_unsigned(decision->rect_type_context);
        write_console("/");
        write_unsigned(decision->extension_context);
        write_console("]");
    }
}

static void write_leaf_trace(const char *name,
                             const Av2hDebugLeafTraceEntry *trace,
                             size_t count) {
    size_t first = count > AV2H_DEBUG_LEAF_TRACE_CAPACITY ?
        count - AV2H_DEBUG_LEAF_TRACE_CAPACITY : 0U;
    size_t index;
    if (count == 0U) return;
    write_console("\r\n  recent ");
    write_console(name);
    write_console(" leaves:");
    for (index = first; index < count; ++index) {
        const Av2hDebugLeafTraceEntry *entry =
            &trace[index % AV2H_DEBUG_LEAF_TRACE_CAPACITY];
        write_console(" (");
        write_unsigned(entry->node.row);
        write_console(",");
        write_unsigned(entry->node.column);
        write_console(",");
        write_console(block_size_name(entry->node.block_size));
        write_console(")@");
        write_unsigned(entry->after_bits);
    }
}

static void write_decode_diagnostic(const Av2hFrameDecodeResult *result) {
    size_t index;
    if (result->stage == AV2H_FRAME_DECODE_STAGE_NONE) return;
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
    write_partition_path();
    write_leaf_trace(
        "Y", av2h_debug_luma_leaf_trace,
        av2h_debug_luma_leaf_trace_count);
    write_leaf_trace(
        "UV", av2h_debug_chroma_leaf_trace,
        av2h_debug_chroma_leaf_trace_count);
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
    write_console(" filter-stage-hashes=");
    for (size_t stage = 0U; stage < AV2H_FILTER_STAGE_COUNT; ++stage) {
        if (stage != 0U) write_console(":");
        for (index = 0U; index < AV2H_FRAME_PLANE_COUNT; ++index) {
            if (index != 0U) write_console("/");
            write_unsigned(
                (size_t)av2h_debug_filter_stage_hash[stage][index]);
        }
    }
    write_console(" intrabc-geometry=");
    for (index = 0U; index < 10U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_intrabc_geometry[index]);
    }
    write_console(" intrabc-shell=");
    for (index = 0U; index < 11U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_intrabc_shell[index]);
    }
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
    write_console("\r\n");
}

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

static void write_debug_filter_stage(const Av2hWinChar *path,
                                     const Av2hFilterPlanes *planes,
                                     uint16_t width,
                                     uint16_t height) {
    Av2hWinHandle file = CreateFileW(
        path, AV2H_WIN_GENERIC_WRITE, 0U, 0, AV2H_WIN_CREATE_ALWAYS,
        AV2H_WIN_FILE_ATTRIBUTE_NORMAL, 0);
    unsigned int plane;
    if (file == AV2H_WIN_INVALID_HANDLE) return;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t plane_width = plane == 0U ? width : ((size_t)width + 1U) / 2U;
        size_t plane_height = plane == 0U ? height :
            ((size_t)height + 1U) / 2U;
        size_t row;
        for (row = 0U; row < plane_height; ++row) {
            if (!write_all(file, planes->plane[plane].data +
                           row * planes->plane[plane].stride,
                           plane_width)) {
                (void)CloseHandle(file);
                return;
            }
        }
    }
    (void)CloseHandle(file);
}

static const Av2hObu *find_obu(const Av2hObuLedger *ledger, uint8_t type) {
    size_t index;
    for (index = 0U; index < ledger->count; ++index) {
        if (ledger->entries[index].type == type) return &ledger->entries[index];
    }
    return 0;
}

static void *arena_array(Av2hArena *arena,
                         size_t count,
                         size_t element_size,
                         size_t alignment) {
    size_t bytes;
    if (!av2h_size_multiply(count, element_size, &bytes)) {
        arena->status = AV2H_OVERFLOW;
        return 0;
    }
    return av2h_arena_allocate(arena, bytes, alignment);
}

static Av2hStatus prepare_decode_workspace(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    void *memory,
    size_t memory_size,
    Av2hFrameDecodeWorkspace *workspace,
    Av2hFilterWorkspace *filter_workspace,
    Av2hFilterWorkspaceStorage *filter_storage,
    size_t *required) {
    Av2hFilterWorkspaceLayout layout;
    Av2hArena arena;
    void *filter_memory;
    size_t mi_columns;
    size_t mi_rows;
    size_t context_columns;
    size_t context_rows;
    size_t superblock_mi;
    size_t mi_cells;
    size_t cdef_columns;
    size_t cdef_rows;
    size_t cdef_count;
    unsigned int plane;
    Av2hStatus status;
    if (sequence == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    mi_columns = ((size_t)frame->width + 3U) / 4U;
    mi_rows = ((size_t)frame->height + 3U) / 4U;
    superblock_mi = sequence->superblock_size == 128U ? 32U : 16U;
    context_columns = (mi_columns + superblock_mi - 1U) /
        superblock_mi * superblock_mi;
    context_rows = (mi_rows + superblock_mi - 1U) /
        superblock_mi * superblock_mi;
    cdef_columns = ((size_t)frame->width + 63U) / 64U;
    cdef_rows = ((size_t)frame->height + 63U) / 64U;
    if (!av2h_size_multiply(mi_columns, mi_rows, &mi_cells) ||
        !av2h_size_multiply(cdef_columns, cdef_rows, &cdef_count)) {
        return AV2H_OVERFLOW;
    }
    if (memory == 0) {
        av2h_arena_init_sizing(&arena);
    } else {
        av2h_arena_init(&arena, memory, memory_size);
        av2h_memory_fill(workspace, 0U, sizeof(*workspace));
    }
    filter_memory = av2h_arena_allocate(&arena, layout.size, layout.alignment);
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        uint8_t *data = arena_array(
            &arena, layout.stage_plane_bytes[plane], sizeof(uint8_t),
            _Alignof(uint8_t));
        uint8_t *reconstructed = arena_array(
            &arena, layout.deblock_cell_count[plane], sizeof(uint8_t),
            _Alignof(uint8_t));
        if (memory != 0) {
            workspace->coded[plane] = (Av2hReconstructionPlane){
                data, layout.plane_width[plane],
                layout.plane_width[plane], layout.plane_height[plane],
                plane == 0U ? frame->width :
                    (uint16_t)(((uint32_t)frame->width + 1U) / 2U),
                plane == 0U ? frame->height :
                    (uint16_t)(((uint32_t)frame->height + 1U) / 2U),
                reconstructed, layout.deblock_columns[plane],
                layout.deblock_cell_count[plane]
            };
            workspace->coded_data_size[plane] =
                layout.stage_plane_bytes[plane];
        }
    }
#define AV2H_BIND_FIELD(field, count, type)                                    \
    do {                                                                       \
        void *allocated = arena_array(                                         \
            &arena, (count), sizeof(type), _Alignof(type));                    \
        if (memory != 0) workspace->tile_context.field = (type *)allocated;    \
    } while (0)
    AV2H_BIND_FIELD(luma_partition_above, context_columns, uint8_t);
    AV2H_BIND_FIELD(luma_partition_left, context_rows, uint8_t);
    AV2H_BIND_FIELD(chroma_partition_above, context_columns, uint8_t);
    AV2H_BIND_FIELD(chroma_partition_left, context_rows, uint8_t);
    AV2H_BIND_FIELD(luma_mode_above, context_columns, Av2hLumaModeContext);
    AV2H_BIND_FIELD(luma_mode_left, context_rows, Av2hLumaModeContext);
    AV2H_BIND_FIELD(coefficient_above,
                    context_columns * AV2H_FRAME_PLANE_COUNT, uint8_t);
    AV2H_BIND_FIELD(coefficient_left,
                    context_rows * AV2H_FRAME_PLANE_COUNT, uint8_t);
    AV2H_BIND_FIELD(colocated_luma, mi_cells, Av2hTileLumaModeCell);
    AV2H_BIND_FIELD(gdf_block_flags, frame->gdf_block_count, uint8_t);
    AV2H_BIND_FIELD(cdef_unit_strengths, cdef_count, uint8_t);
#undef AV2H_BIND_FIELD
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        uint8_t *controls = arena_array(
            &arena, layout.ccso_control_count, sizeof(uint8_t),
            _Alignof(uint8_t));
        if (memory != 0) {
            workspace->tile_context.ccso_unit_controls[plane] = controls;
        }
    }
    {
        Av2hReconstructionScratch *scratch = arena_array(
            &arena, 1U, sizeof(Av2hReconstructionScratch),
            _Alignof(Av2hReconstructionScratch));
        if (memory != 0) workspace->reconstruction_scratch = scratch;
    }
    if (arena.status != AV2H_OK) return arena.status;
    *required = av2h_arena_required(&arena);
    if (memory == 0) return AV2H_OK;
    workspace->tile_context.above_count = context_columns;
    workspace->tile_context.left_count = context_rows;
    workspace->tile_context.colocated_luma_count = mi_cells;
    workspace->tile_context.gdf_block_flag_capacity = frame->gdf_block_count;
    workspace->tile_context.cdef_unit_strength_capacity = cdef_count;
    workspace->tile_context.ccso_unit_control_capacity =
        layout.ccso_control_count;
    status = av2h_filter_workspace_init(
        frame, filter_memory, layout.size, filter_workspace, filter_storage);
    if (status != AV2H_OK) return status;
    workspace->filter_workspace = filter_workspace;
    return AV2H_OK;
}

static Av2hStatus decode_obu(Av2hSpan input,
                            Av2hOwnedMemory *output,
                            uint16_t *width,
                            uint16_t *height,
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
    status = av2h_i420_output_size(&frame, &output_size);
    if (status != AV2H_OK) return status;
    status = prepare_decode_workspace(
        &sequence, &frame, 0, 0U, &workspace, &filter_workspace, &filter_storage,
        &workspace_size);
    if (status != AV2H_OK) return status;
    if (!allocate_memory(workspace_size, &workspace_memory) ||
        !allocate_memory(output_size, output)) {
        release_memory(&workspace_memory);
        release_memory(output);
        return AV2H_OUT_OF_MEMORY;
    }
    status = prepare_decode_workspace(
        &sequence, &frame, workspace_memory.memory, workspace_memory.size,
        &workspace, &filter_workspace, &filter_storage, &workspace_size);
    if (status == AV2H_OK) {
        status = av2h_decode_frame_i420_private(
            group.entropy, &sequence, &frame, &workspace,
            output->memory, output->size, decode_result);
        if (status == AV2H_OK && frame.width == 93U && frame.height == 111U) {
            write_debug_filter_stage(
                L"build/chimp-local-reconstruction.i420",
                &av2h_debug_filter_stage_planes[0], frame.width, frame.height);
            write_debug_filter_stage(
                L"build/chimp-local-deblocked.i420",
                &av2h_debug_filter_stage_planes[1], frame.width, frame.height);
            write_debug_filter_stage(
                L"build/chimp-local-cdef.i420",
                &av2h_debug_filter_stage_planes[2], frame.width, frame.height);
            write_debug_filter_stage(
                L"build/chimp-local-ccso.i420",
                &av2h_debug_filter_stage_planes[3], frame.width, frame.height);
        }
    }
    release_memory(&workspace_memory);
    if (status != AV2H_OK) {
        release_memory(output);
        return status;
    }
    *width = frame.width;
    *height = frame.height;
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

static uint8_t scale_sample(int value) {
    if (value <= 0) return 0U;
    value /= 256;
    if (value > 255) return 255U;
    return (uint8_t)value;
}

static int write_bmp(const Av2hWinChar *path,
                     const uint8_t *i420,
                     uint16_t width,
                     uint16_t height) {
    size_t luma_size;
    size_t chroma_width = ((size_t)width + 1U) / 2U;
    size_t chroma_height = ((size_t)height + 1U) / 2U;
    size_t chroma_size;
    size_t row_bytes;
    size_t pixel_bytes;
    size_t file_size;
    Av2hOwnedMemory row_memory = { 0, 0U };
    uint8_t header[AV2H_BMP_HEADER_SIZE];
    const uint8_t *u;
    const uint8_t *v;
    Av2hWinHandle file;
    size_t row;
    int result = 0;
    if (!av2h_size_multiply(width, height, &luma_size) ||
        !av2h_size_multiply(chroma_width, chroma_height, &chroma_size) ||
        !av2h_size_multiply(width, 3U, &row_bytes) ||
        !av2h_size_align(row_bytes, 4U, &row_bytes) ||
        !av2h_size_multiply(row_bytes, height, &pixel_bytes) ||
        !av2h_size_add(AV2H_BMP_HEADER_SIZE, pixel_bytes, &file_size) ||
        file_size > UINT32_MAX || !allocate_memory(row_bytes, &row_memory)) {
        return 0;
    }
    u = i420 + luma_size;
    v = u + chroma_size;
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
        size_t column;
        uint8_t *destination = row_memory.memory;
        av2h_memory_fill(destination, 0U, row_bytes);
        for (column = 0U; column < width; ++column) {
            int y = i420[y_row * width + column];
            int cb = u[(y_row / 2U) * chroma_width + column / 2U];
            int cr = v[(y_row / 2U) * chroma_width + column / 2U];
            int c = y - 16;
            int d = cb - 128;
            int e = cr - 128;
            if (c < 0) c = 0;
            destination[column * 3U] =
                scale_sample(298 * c + 516 * d + 128);
            destination[column * 3U + 1U] =
                scale_sample(298 * c - 100 * d - 208 * e + 128);
            destination[column * 3U + 2U] =
                scale_sample(298 * c + 409 * e + 128);
        }
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
    Av2hFrameDecodeResult decode_result = { 0 };
    Av2hStatus status;
    int written;
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
        !(wide_ends_with_ascii(arguments[3], ".i420") ||
          wide_ends_with_ascii(arguments[3], ".bmp"))) {
        write_console(
            "usage: av2heif-decoder.exe INPUT.obu -o OUTPUT.i420|OUTPUT.bmp\r\n");
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
        &decode_result);
    if (status != AV2H_OK) {
        write_status("error: decode failed: ", status);
        write_decode_diagnostic(&decode_result);
        release_memory(&output);
        release_memory(&input);
        release_memory(&command_line);
        return status == AV2H_UNSUPPORTED ? 3U : 1U;
    }
    write_console("filter-stage-hashes=");
    for (size_t stage = 0U; stage < AV2H_FILTER_STAGE_COUNT; ++stage) {
        if (stage != 0U) write_console(":");
        for (size_t plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            if (plane != 0U) write_console("/");
            write_unsigned(
                (size_t)av2h_debug_filter_stage_hash[stage][plane]);
        }
    }
    write_console("\r\n");
    write_console("chimp-luma-target=");
    for (size_t index = 0U; index < 56U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_chimp_luma_target[index]);
    }
    write_console("\r\n");
    write_console("chimp-chroma-target=");
    for (size_t index = 0U; index < 24U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_chimp_chroma_target[index]);
    }
    write_console("\r\n");
    write_console("chimp-mhccp-trace=");
    for (size_t entry = 0U; entry < av2h_debug_mhccp_trace_count; ++entry) {
        if (entry != 0U) write_console(":");
        for (size_t field = 0U; field < 10U; ++field) {
            if (field != 0U) write_console("/");
            write_signed(av2h_debug_mhccp_trace[entry][field]);
        }
    }
    write_console("\r\n");
    write_console("chimp-directional-edge=");
    for (size_t index = 0U; index < 13U; ++index) {
        if (index != 0U) write_console("/");
        write_unsigned(av2h_debug_directional_edges[index]);
    }
    write_console("\r\n");
    write_console("chimp-second-sb=");
    for (size_t index = 0U; index < 40U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_chimp_second_sb[index]);
    }
    write_console("\r\n");
    write_console("chimp-use-intrabc=");
    for (size_t index = 0U; index < 10U; ++index) {
        if (index != 0U) write_console("/");
        write_unsigned(av2h_debug_use_intrabc[index]);
    }
    write_console("\r\n");
    write_console("chimp-intrabc-shell=");
    for (size_t index = 0U; index < 11U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_intrabc_shell[index]);
    }
    write_console("\r\n");
    write_console("chimp-intrabc-candidates=");
    for (size_t index = 0U; index < 9U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_intrabc_candidates[index]);
    }
    write_console("\r\n");
    write_console("chimp-intrabc-owners=");
    for (size_t index = 0U; index < 32U; ++index) {
        if (index != 0U) write_console("/");
        write_signed(av2h_debug_intrabc_owners[index]);
    }
    write_console("\r\n");
    write_console("chimp-fsc-bob=");
    for (size_t index = 0U; index < 12U; ++index) {
        if (index != 0U) write_console("/");
        write_unsigned(av2h_debug_fsc_bob[index]);
    }
    write_console("\r\n");
    if (wide_ends_with_ascii(arguments[3], ".bmp")) {
        written = write_bmp(arguments[3], output.memory, width, height);
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
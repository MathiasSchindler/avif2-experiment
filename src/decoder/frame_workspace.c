#include "decoder/internal/frame_workspace.h"

#include "decoder/filter.h"
#include "decoder/frame_output.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/parallel.h"
#include "decoder/internal/sample_ops.h"

#include <stddef.h>

typedef struct {
    size_t luma_mode_above;
    size_t luma_mode_left;
    size_t coefficient_above;
    size_t coefficient_left;
    size_t colocated_luma;
} Av2hFrameContextSizes;

typedef struct {
    Av2hFilterWorkspaceLayout filter;
    size_t context_columns;
    size_t context_rows;
    size_t mi_cells;
    size_t coefficient_above;
    size_t coefficient_left;
    size_t cdef_count;
    size_t restoration_count;
    size_t restoration_tap_count;
} Av2hFrameDecodeWorkspaceGeometry;

static int sized_u8_plane_bytes(size_t stride_samples, size_t height, size_t *size_bytes);
static int sized_u16_plane_bytes(size_t stride_samples, size_t height, size_t *size_bytes);
static void frame_tile_workspace_bind(Av2hArena *arena, const Av2hFrameState *frame,
                                      size_t bytes_per_sample,
                                      const Av2hFrameDecodeWorkspaceGeometry *geometry,
                                      Av2hFrameDecodeWorkspace *workspace);
static void frame_parallel_workspace_bind(Av2hArena *arena, const Av2hFrameState *frame,
                                          size_t bytes_per_sample,
                                          const Av2hFrameDecodeWorkspaceGeometry *geometry,
                                          unsigned int worker_count,
                                          Av2hFrameDecodeWorkspace *workspace);

static void *workspace_arena_array(Av2hArena *arena, size_t count, size_t element_size,
                                   size_t alignment) {
    size_t bytes;
    if (!av2h_size_multiply(count, element_size, &bytes)) {
        arena->status = AV2H_OVERFLOW;
        return NULL;
    }
    return av2h_arena_allocate(arena, bytes, alignment);
}

static size_t maximum_alignment(size_t left, size_t right) {
    return left > right ? left : right;
}

static void *workspace_reconstruction_scratch(Av2hArena *arena, size_t bytes_per_sample) {
    size_t scratch_bytes = bytes_per_sample == 1U ? sizeof(Av2hReconstructionScratch)
                                                  : sizeof(Av2hReconstructionScratchU16);
    size_t alignment = bytes_per_sample == 1U ? _Alignof(Av2hReconstructionScratch)
                                              : _Alignof(Av2hReconstructionScratchU16);
    return av2h_arena_allocate(arena, scratch_bytes, alignment);
}

static Av2hStatus frame_decode_workspace_geometry(const Av2hSequenceState *sequence,
                                                  const Av2hFrameState *frame,
                                                  size_t bytes_per_sample,
                                                  Av2hFrameDecodeWorkspaceGeometry *geometry) {
    size_t mi_columns;
    size_t mi_rows;
    size_t superblock_mi;
    size_t rounded;
    size_t cdef_columns;
    size_t cdef_rows;
    unsigned int plane;
    Av2hStatus status;
    if (sequence == NULL || frame == NULL || geometry == NULL ||
        (sequence->superblock_size != 64U && sequence->superblock_size != 128U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_filter_workspace_query(frame, bytes_per_sample, &geometry->filter);
    if (status != AV2H_OK)
        return status;
    mi_columns = av2h_frame_mi_dimension(frame->width);
    mi_rows = av2h_frame_mi_dimension(frame->height);
    superblock_mi = sequence->superblock_size == 128U ? 32U : 16U;
    if (!av2h_size_add(mi_columns, superblock_mi - 1U, &rounded) ||
        !av2h_size_multiply(rounded / superblock_mi, superblock_mi, &geometry->context_columns) ||
        !av2h_size_add(mi_rows, superblock_mi - 1U, &rounded) ||
        !av2h_size_multiply(rounded / superblock_mi, superblock_mi, &geometry->context_rows) ||
        !av2h_size_multiply(mi_columns, mi_rows, &geometry->mi_cells) ||
        !av2h_size_multiply(geometry->context_columns, av2h_frame_plane_count(frame),
                            &geometry->coefficient_above) ||
        !av2h_size_multiply(geometry->context_rows, av2h_frame_plane_count(frame),
                            &geometry->coefficient_left)) {
        return AV2H_OVERFLOW;
    }
    cdef_columns = ((size_t)frame->width + 63U) / 64U;
    cdef_rows = ((size_t)frame->height + 63U) / 64U;
    if (!av2h_size_multiply(cdef_columns, cdef_rows, &geometry->cdef_count)) {
        return AV2H_OVERFLOW;
    }
    geometry->restoration_count = 0U;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (frame->restoration_type[plane] != AV2H_RESTORE_NONE &&
            !av2h_size_add(geometry->restoration_count, frame->restoration_unit_count[plane],
                           &geometry->restoration_count)) {
            return AV2H_OVERFLOW;
        }
    }
    if (!av2h_size_multiply(geometry->restoration_count, AV2H_WIENER_NONSEP_MAX_TAPS,
                            &geometry->restoration_tap_count)) {
        return AV2H_OVERFLOW;
    }
    return AV2H_OK;
}

static void frame_decode_workspace_bind(Av2hArena *arena, const Av2hFrameState *frame,
                                        size_t bytes_per_sample,
                                        const Av2hFrameDecodeWorkspaceGeometry *geometry,
                                        Av2hFrameDecodeWorkspace *workspace, void **filter_memory,
                                        unsigned int worker_count) {
    unsigned int plane;
    Av2hFrameDecodeScratch *decode_scratch = av2h_arena_allocate(
        arena, sizeof(Av2hFrameDecodeScratch), _Alignof(Av2hFrameDecodeScratch));
    if (workspace != NULL)
        workspace->decode_scratch = decode_scratch;
    *filter_memory =
        av2h_arena_allocate(arena, geometry->filter.size_bytes, geometry->filter.alignment);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        void *data = workspace_arena_array(arena, geometry->filter.stage_plane_bytes[plane],
                                           sizeof(uint8_t), bytes_per_sample);
        uint8_t *reconstructed = workspace_arena_array(
            arena, geometry->filter.deblock_cell_count[plane], sizeof(uint8_t), _Alignof(uint8_t));
        if (workspace != NULL) {
            if (bytes_per_sample == 1U) {
                workspace->coded[plane] = (Av2hReconstructionPlane){
                    data,
                    geometry->filter.plane_width[plane],
                    geometry->filter.plane_width[plane],
                    geometry->filter.plane_height[plane],
                    (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->width),
                    (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->height),
                    reconstructed,
                    geometry->filter.deblock_columns[plane],
                    geometry->filter.deblock_cell_count[plane],
                    {0}};
            } else {
                workspace->coded_u16[plane] = (Av2hReconstructionPlaneU16){
                    data,
                    geometry->filter.plane_width[plane],
                    geometry->filter.plane_width[plane],
                    geometry->filter.plane_height[plane],
                    (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->width),
                    (uint16_t)av2h_frame_plane_dimension(frame, plane, frame->height),
                    reconstructed,
                    geometry->filter.deblock_columns[plane],
                    geometry->filter.deblock_cell_count[plane],
                    {0}};
            }
            workspace->coded_data_size_bytes[plane] = geometry->filter.stage_plane_bytes[plane];
        }
    }
    frame_tile_workspace_bind(arena, frame, bytes_per_sample, geometry, workspace);
    frame_parallel_workspace_bind(arena, frame, bytes_per_sample, geometry, worker_count,
                                  workspace);
}

static void frame_tile_workspace_bind(Av2hArena *arena, const Av2hFrameState *frame,
                                      size_t bytes_per_sample,
                                      const Av2hFrameDecodeWorkspaceGeometry *geometry,
                                      Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
#define AV2H_BIND_WORKSPACE_FIELD(field, count, type)                                              \
    do {                                                                                           \
        void *allocated = workspace_arena_array(arena, (count), sizeof(type), _Alignof(type));     \
        if (workspace != NULL) {                                                                   \
            workspace->tile_context.field = (type *)allocated;                                     \
        }                                                                                          \
    } while (0)
    AV2H_BIND_WORKSPACE_FIELD(luma_partition_above, geometry->context_columns, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(luma_partition_left, geometry->context_rows, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(chroma_partition_above, geometry->context_columns, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(chroma_partition_left, geometry->context_rows, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(luma_mode_above, geometry->context_columns, Av2hLumaModeContext);
    AV2H_BIND_WORKSPACE_FIELD(luma_mode_left, geometry->context_rows, Av2hLumaModeContext);
    AV2H_BIND_WORKSPACE_FIELD(coefficient_above, geometry->coefficient_above, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(coefficient_left, geometry->coefficient_left, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(colocated_luma, geometry->mi_cells, Av2hTileLumaModeCell);
    AV2H_BIND_WORKSPACE_FIELD(gdf_block_flags, frame->gdf_block_count, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(cdef_unit_strengths, geometry->cdef_count, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(restoration_unit_flags, geometry->restoration_count, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(restoration_unit_taps, geometry->restoration_tap_count, int16_t);
#undef AV2H_BIND_WORKSPACE_FIELD
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint8_t *controls = workspace_arena_array(arena, geometry->filter.ccso_control_count,
                                                  sizeof(uint8_t), _Alignof(uint8_t));
        if (workspace != NULL) {
            workspace->tile_context.ccso_unit_controls[plane] = controls;
        }
    }
    {
        void *scratch = workspace_reconstruction_scratch(arena, bytes_per_sample);
        if (workspace != NULL) {
            if (bytes_per_sample == 1U) {
                workspace->reconstruction_scratch = scratch;
            } else {
                workspace->reconstruction_scratch_u16 = scratch;
            }
        }
        if (workspace != NULL) {
            workspace->tile_context.above_count = geometry->context_columns;
            workspace->tile_context.left_count = geometry->context_rows;
            workspace->tile_context.colocated_luma_count = geometry->mi_cells;
            workspace->tile_context.gdf_block_flag_capacity = frame->gdf_block_count;
            workspace->tile_context.cdef_unit_strength_capacity = geometry->cdef_count;
            workspace->tile_context.ccso_unit_control_capacity =
                geometry->filter.ccso_control_count;
            workspace->tile_context.restoration_unit_flag_capacity = geometry->restoration_count;
            workspace->tile_context.restoration_unit_tap_capacity = geometry->restoration_tap_count;
        }
    }
}

static unsigned int frame_tile_worker_count(const Av2hFrameState *frame,
                                            const Av2hExecution *execution) {
    unsigned int count = (unsigned int)frame->tile_column_count * frame->tile_row_count;
    if (execution == NULL || execution->parallel_for == NULL || execution->width <= 1U ||
        count <= 1U)
        return 1U;
    if (count > execution->width)
        count = execution->width;
    if (count > AV2H_PARALLEL_TILE_WORKERS)
        count = AV2H_PARALLEL_TILE_WORKERS;
    return count;
}

static void frame_parallel_workspace_bind(Av2hArena *arena, const Av2hFrameState *frame,
                                          size_t bytes_per_sample,
                                          const Av2hFrameDecodeWorkspaceGeometry *geometry,
                                          unsigned int worker_count,
                                          Av2hFrameDecodeWorkspace *workspace) {
    Av2hFrameTileWorker *workers;
    size_t start;
    unsigned int index;
    if (workspace != NULL)
        workspace->tile_worker_count = worker_count;
    if (worker_count <= 1U)
        return;
    workers =
        workspace_arena_array(arena, worker_count, sizeof(*workers), _Alignof(Av2hFrameTileWorker));
    if (arena->status != AV2H_OK)
        return;
    start = arena->used - worker_count * sizeof(*workers);
    if (workspace != NULL)
        av2h_memory_fill(workers, 0U, worker_count * sizeof(*workers));
    for (index = 1U; index < worker_count; ++index) {
        Av2hFrameDecodeScratch *scratch =
            av2h_arena_allocate(arena, sizeof(*scratch), _Alignof(Av2hFrameDecodeScratch));
        if (workspace != NULL)
            workers[index].workspace.decode_scratch = scratch;
        frame_tile_workspace_bind(arena, frame, bytes_per_sample, geometry,
                                  workspace != NULL ? &workers[index].workspace : NULL);
    }
    if (workspace != NULL) {
        workspace->tile_workers = workers;
        workspace->parallel_size_bytes = arena->used - start;
    }
}

Av2hStatus av2h_frame_decode_workspace_query(const Av2hSequenceState *sequence,
                                             const Av2hFrameState *frame, size_t bytes_per_sample,
                                             const Av2hExecution *execution,
                                             Av2hFrameDecodeWorkspaceLayout *layout) {
    Av2hFrameDecodeWorkspaceGeometry geometry;
    Av2hFrameDecodeWorkspaceLayout result;
    Av2hArena arena;
    void *filter_memory;
    Av2hStatus status;
    if (layout == NULL)
        return AV2H_INVALID_ARGUMENT;
    if (bytes_per_sample != 1U && bytes_per_sample != 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = frame_decode_workspace_geometry(sequence, frame, bytes_per_sample, &geometry);
    if (status != AV2H_OK)
        return status;
    av2h_arena_init_sizing(&arena);
    frame_decode_workspace_bind(&arena, frame, bytes_per_sample, &geometry, NULL, &filter_memory,
                                frame_tile_worker_count(frame, execution));
    if (arena.status != AV2H_OK)
        return arena.status;
    result.size_bytes = av2h_arena_required(&arena);
    result.alignment = geometry.filter.alignment;
    result.alignment = maximum_alignment(result.alignment, _Alignof(Av2hLumaModeContext));
    result.alignment = maximum_alignment(result.alignment, _Alignof(Av2hTileLumaModeCell));
    result.alignment = maximum_alignment(
        result.alignment, bytes_per_sample == 1U ? _Alignof(Av2hReconstructionScratch)
                                                 : _Alignof(Av2hReconstructionScratchU16));
    result.alignment = maximum_alignment(result.alignment, _Alignof(Av2hFrameDecodeScratch));
    *layout = result;
    return AV2H_OK;
}

AV2H_NOINLINE Av2hStatus av2h_frame_decode_workspace_init(
    const Av2hSequenceState *sequence, const Av2hFrameState *frame, size_t bytes_per_sample,
    const Av2hFrameDecodeWorkspaceLayout *layout, void *memory, size_t memory_size_bytes,
    Av2hFrameDecodeWorkspace *workspace, Av2hFilterWorkspace *filter_workspace,
    Av2hFilterWorkspaceStorage *filter_storage, const Av2hExecution *execution) {
    Av2hFrameDecodeWorkspaceGeometry geometry;
    Av2hFrameDecodeWorkspaceLayout fresh_layout;
    Av2hFrameDecodeWorkspace local_workspace;
    Av2hFilterWorkspace local_filter_workspace;
    Av2hFilterWorkspaceStorage local_filter_storage;
    Av2hArena arena;
    void *filter_memory;
    Av2hStatus status;
    if (sequence == NULL || frame == NULL || layout == NULL || workspace == NULL ||
        filter_workspace == NULL || filter_storage == NULL) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (bytes_per_sample != 1U && bytes_per_sample != 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if ((bytes_per_sample == 1U && sequence->info.bit_depth_idc != 1U) ||
        (bytes_per_sample == 2U && sequence->info.bit_depth_idc != 0U)) {
        return AV2H_UNSUPPORTED;
    }
    status = av2h_frame_decode_workspace_query(sequence, frame, bytes_per_sample, execution,
                                               &fresh_layout);
    if (status != AV2H_OK)
        return status;
    if (layout->size_bytes != fresh_layout.size_bytes ||
        layout->alignment != fresh_layout.alignment || memory == NULL ||
        ((uintptr_t)memory & (fresh_layout.alignment - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (memory_size_bytes < fresh_layout.size_bytes)
        return AV2H_OUT_OF_MEMORY;
    status = frame_decode_workspace_geometry(sequence, frame, bytes_per_sample, &geometry);
    if (status != AV2H_OK)
        return status;
    av2h_memory_fill(&local_workspace, 0U, sizeof(local_workspace));
    av2h_memory_fill(&local_filter_workspace, 0U, sizeof(local_filter_workspace));
    av2h_memory_fill(&local_filter_storage, 0U, sizeof(local_filter_storage));
    av2h_arena_init(&arena, memory, fresh_layout.size_bytes);
    frame_decode_workspace_bind(&arena, frame, bytes_per_sample, &geometry, &local_workspace,
                                &filter_memory, frame_tile_worker_count(frame, execution));
    if (arena.status != AV2H_OK)
        return arena.status;
    status = av2h_filter_workspace_init(frame, bytes_per_sample, filter_memory,
                                        geometry.filter.size_bytes, &local_filter_workspace,
                                        &local_filter_storage);
    if (status != AV2H_OK)
        return status;
    av2h_memory_copy(filter_workspace, &local_filter_workspace, sizeof(local_filter_workspace));
    av2h_memory_copy(filter_storage, &local_filter_storage, sizeof(local_filter_storage));
    local_workspace.filter_workspace = filter_workspace;
    local_workspace.bytes_per_sample = (uint8_t)bytes_per_sample;
    av2h_memory_copy(workspace, &local_workspace, sizeof(local_workspace));
    workspace->diagnostics = &workspace->diagnostics_storage;
    workspace->filter_workspace->diagnostics = workspace->diagnostics;
    return AV2H_OK;
}

static int ranges_overlap(const void *left, size_t left_size_bytes, const void *right,
                          size_t right_size_bytes) {
    uintptr_t maximum = (uintptr_t)-1;
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    uintptr_t left_end;
    uintptr_t right_end;
    if (left_size_bytes == 0U || right_size_bytes == 0U)
        return 0;
    if (left == NULL || right == NULL || left_size_bytes > maximum - left_start ||
        right_size_bytes > maximum - right_start) {
        return 1;
    }
    left_end = left_start + left_size_bytes;
    right_end = right_start + right_size_bytes;
    return left_start < right_end && right_start < left_end;
}

static int output_overlaps(uint8_t *output, size_t output_size_bytes, const void *memory,
                           size_t memory_size_bytes) {
    return ranges_overlap(output, output_size_bytes, memory, memory_size_bytes);
}

static int filter_workspace_valid(const Av2hFrameState *frame,
                                  const Av2hFilterWorkspaceLayout *layout,
                                  const Av2hFilterWorkspace *workspace, size_t bytes_per_sample) {
    unsigned int plane;
    if (layout == NULL || workspace == NULL ||
        workspace->ccso_controls.columns != layout->ccso_columns ||
        workspace->ccso_controls.rows != layout->ccso_rows) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *deblocked = &workspace->deblocked.plane[plane];
        const Av2hFilterPlane *cdef = &workspace->cdef.plane[plane];
        const Av2hFilterPlaneU16 *deblocked_u16 = &workspace->deblocked_u16.plane[plane];
        const Av2hFilterPlaneU16 *cdef_u16 = &workspace->cdef_u16.plane[plane];
        if ((bytes_per_sample == 1U && (deblocked->data == NULL || cdef->data == NULL ||
                                        deblocked->width != layout->plane_width[plane] ||
                                        deblocked->height != layout->plane_height[plane] ||
                                        cdef->width != layout->plane_width[plane] ||
                                        cdef->height != layout->plane_height[plane] ||
                                        deblocked->stride_samples < deblocked->width ||
                                        cdef->stride_samples < cdef->width)) ||
            (bytes_per_sample == 2U && (deblocked_u16->data == NULL || cdef_u16->data == NULL ||
                                        deblocked_u16->width != layout->plane_width[plane] ||
                                        deblocked_u16->height != layout->plane_height[plane] ||
                                        cdef_u16->width != layout->plane_width[plane] ||
                                        cdef_u16->height != layout->plane_height[plane] ||
                                        deblocked_u16->stride_samples < deblocked_u16->width ||
                                        cdef_u16->stride_samples < cdef_u16->width)) ||
            workspace->deblock_map.plane[plane] == NULL ||
            workspace->deblock_map.columns[plane] != layout->deblock_columns[plane] ||
            workspace->deblock_map.rows[plane] != layout->deblock_rows[plane] ||
            workspace->deblock_map.stride_cells[plane] != layout->deblock_columns[plane] ||
            workspace->ccso_controls.plane[plane] == NULL ||
            workspace->ccso_controls.stride_units[plane] != layout->ccso_columns) {
            return 0;
        }
    }
    if (bytes_per_sample == 1U && frame->restoration_type[0] != AV2H_RESTORE_NONE &&
        (workspace->restoration.data == NULL ||
         workspace->restoration.width != layout->plane_width[0] ||
         workspace->restoration.height != layout->plane_height[0] ||
         workspace->restoration.stride_samples < workspace->restoration.width ||
         (av2h_frame_restoration_needs_tx_skip(frame) &&
          (workspace->restoration_tx_skip == NULL ||
           workspace->restoration_controls.tx_skip != workspace->restoration_tx_skip ||
           workspace->restoration_controls.tx_skip_stride != layout->deblock_columns[0])))) {
        return 0;
    }
    return 1;
}

static int decode_planes_valid(const Av2hFrameState *frame, const Av2hFilterWorkspaceLayout *layout,
                               const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t required_data;
        size_t required_ready;
        size_t reference_width = av2h_frame_plane_dimension(frame, plane, frame->width);
        size_t reference_height = av2h_frame_plane_dimension(frame, plane, frame->height);
        if (workspace->bytes_per_sample == 1U) {
            const Av2hReconstructionPlane *coded = &workspace->coded[plane];
            if (coded->data == NULL || coded->reconstructed == NULL ||
                coded->width != layout->plane_width[plane] ||
                coded->height != layout->plane_height[plane] ||
                coded->stride_samples < coded->width || coded->reference_width != reference_width ||
                coded->reference_height != reference_height ||
                coded->reconstructed_stride_cells < ((size_t)coded->width + 3U) / 4U ||
                !sized_u8_plane_bytes(coded->stride_samples, coded->height, &required_data) ||
                required_data > workspace->coded_data_size_bytes[plane] ||
                !av2h_size_multiply(coded->reconstructed_stride_cells,
                                    ((size_t)coded->height + 3U) / 4U, &required_ready) ||
                required_ready > coded->reconstructed_count_cells) {
                return 0;
            }
        } else {
            const Av2hReconstructionPlaneU16 *coded = &workspace->coded_u16[plane];
            if (coded->data == NULL || coded->reconstructed == NULL ||
                coded->width != layout->plane_width[plane] ||
                coded->height != layout->plane_height[plane] ||
                coded->stride_samples < coded->width || coded->reference_width != reference_width ||
                coded->reference_height != reference_height ||
                coded->reconstructed_stride_cells < ((size_t)coded->width + 3U) / 4U ||
                !sized_u16_plane_bytes(coded->stride_samples, coded->height, &required_data) ||
                required_data > workspace->coded_data_size_bytes[plane] ||
                !av2h_size_multiply(coded->reconstructed_stride_cells,
                                    ((size_t)coded->height + 3U) / 4U, &required_ready) ||
                required_ready > coded->reconstructed_count_cells) {
                return 0;
            }
        }
    }
    return 1;
}

static int tile_context_valid(const Av2hSequenceState *sequence, const Av2hFrameState *frame,
                              const Av2hTileContextStorage *context, Av2hFrameContextSizes *sizes) {
    size_t mi_columns;
    size_t mi_rows;
    size_t context_columns;
    size_t context_rows;
    size_t superblock_mi;
    size_t cell_count;
    if (sequence == NULL || frame == NULL || context == NULL || sizes == NULL ||
        context->luma_partition_above == NULL || context->luma_partition_left == NULL ||
        context->chroma_partition_above == NULL || context->chroma_partition_left == NULL ||
        context->luma_mode_above == NULL || context->luma_mode_left == NULL ||
        context->coefficient_above == NULL || context->coefficient_left == NULL ||
        context->colocated_luma == NULL) {
        return 0;
    }
    mi_columns = av2h_frame_mi_dimension(frame->width);
    mi_rows = av2h_frame_mi_dimension(frame->height);
    superblock_mi = sequence->superblock_size == 128U ? 32U : 16U;
    context_columns = (mi_columns + superblock_mi - 1U) / superblock_mi * superblock_mi;
    context_rows = (mi_rows + superblock_mi - 1U) / superblock_mi * superblock_mi;
    if (context->above_count < context_columns || context->left_count < context_rows ||
        !av2h_size_multiply(mi_columns, mi_rows, &cell_count) ||
        context->colocated_luma_count < cell_count ||
        !av2h_size_multiply(context->above_count, sizeof(*context->luma_mode_above),
                            &sizes->luma_mode_above) ||
        !av2h_size_multiply(context->left_count, sizeof(*context->luma_mode_left),
                            &sizes->luma_mode_left) ||
        !av2h_size_multiply(context->above_count, av2h_frame_plane_count(frame),
                            &sizes->coefficient_above) ||
        !av2h_size_multiply(context->left_count, av2h_frame_plane_count(frame),
                            &sizes->coefficient_left) ||
        !av2h_size_multiply(context->colocated_luma_count, sizeof(*context->colocated_luma),
                            &sizes->colocated_luma) ||
        (context->gdf_block_flag_capacity != 0U && context->gdf_block_flags == NULL) ||
        (context->cdef_unit_strength_capacity != 0U && context->cdef_unit_strengths == NULL) ||
        (context->restoration_unit_flag_capacity != 0U &&
         context->restoration_unit_flags == NULL)) {
        return 0;
    }
    return 1;
}

static int output_is_disjoint(uint8_t *output, size_t output_size_bytes,
                              const Av2hFrameState *frame, const Av2hFilterWorkspaceLayout *layout,
                              const Av2hFrameDecodeWorkspace *workspace,
                              const Av2hFrameContextSizes *context_sizes,
                              const Av2hFrameDecodeResult *result) {
    if (output_overlaps(output, output_size_bytes, workspace->tile_workers,
                        workspace->parallel_size_bytes))
        return 0;
    const Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const void *coded_data;
        const void *deblocked_data;
        const void *cdef_data;
        size_t coded_stride;
        size_t coded_height;
        size_t deblocked_stride;
        size_t deblocked_height;
        size_t cdef_stride;
        size_t cdef_height;
        const uint8_t *reconstructed;
        size_t reconstructed_count;
        size_t coded_size_bytes;
        size_t deblocked_size_bytes;
        size_t cdef_size_bytes;
        size_t map_size_bytes;
        size_t control_size_bytes;
        if (workspace->bytes_per_sample == 1U) {
            const Av2hReconstructionPlane *coded = &workspace->coded[plane];
            const Av2hFilterPlane *deblocked = &workspace->filter_workspace->deblocked.plane[plane];
            const Av2hFilterPlane *cdef = &workspace->filter_workspace->cdef.plane[plane];
            coded_data = coded->data;
            coded_stride = coded->stride_samples;
            coded_height = coded->height;
            reconstructed = coded->reconstructed;
            reconstructed_count = coded->reconstructed_count_cells;
            deblocked_data = deblocked->data;
            deblocked_stride = deblocked->stride_samples;
            deblocked_height = deblocked->height;
            cdef_data = cdef->data;
            cdef_stride = cdef->stride_samples;
            cdef_height = cdef->height;
        } else {
            const Av2hReconstructionPlaneU16 *coded = &workspace->coded_u16[plane];
            const Av2hFilterPlaneU16 *deblocked =
                &workspace->filter_workspace->deblocked_u16.plane[plane];
            const Av2hFilterPlaneU16 *cdef = &workspace->filter_workspace->cdef_u16.plane[plane];
            coded_data = coded->data;
            coded_stride = coded->stride_samples;
            coded_height = coded->height;
            reconstructed = coded->reconstructed;
            reconstructed_count = coded->reconstructed_count_cells;
            deblocked_data = deblocked->data;
            deblocked_stride = deblocked->stride_samples;
            deblocked_height = deblocked->height;
            cdef_data = cdef->data;
            cdef_stride = cdef->stride_samples;
            cdef_height = cdef->height;
        }
        if (!av2h_size_multiply(coded_stride, coded_height, &coded_size_bytes) ||
            !av2h_size_multiply(coded_size_bytes, workspace->bytes_per_sample, &coded_size_bytes) ||
            !av2h_size_multiply(deblocked_stride, deblocked_height, &deblocked_size_bytes) ||
            !av2h_size_multiply(deblocked_size_bytes, workspace->bytes_per_sample,
                                &deblocked_size_bytes) ||
            !av2h_size_multiply(cdef_stride, cdef_height, &cdef_size_bytes) ||
            !av2h_size_multiply(cdef_size_bytes, workspace->bytes_per_sample, &cdef_size_bytes) ||
            !av2h_size_multiply(workspace->filter_workspace->deblock_map.stride_cells[plane],
                                workspace->filter_workspace->deblock_map.rows[plane],
                                &map_size_bytes) ||
            !av2h_size_multiply(map_size_bytes, sizeof(Av2hDeblockCell), &map_size_bytes) ||
            !av2h_size_multiply(workspace->filter_workspace->ccso_controls.stride_units[plane],
                                layout->ccso_rows, &control_size_bytes) ||
            output_overlaps(output, output_size_bytes, coded_data, coded_size_bytes) ||
            output_overlaps(output, output_size_bytes, reconstructed, reconstructed_count) ||
            output_overlaps(output, output_size_bytes, deblocked_data, deblocked_size_bytes) ||
            output_overlaps(output, output_size_bytes, cdef_data, cdef_size_bytes) ||
            output_overlaps(output, output_size_bytes,
                            workspace->filter_workspace->deblock_map.plane[plane],
                            map_size_bytes) ||
            output_overlaps(output, output_size_bytes,
                            workspace->filter_workspace->ccso_controls.plane[plane],
                            control_size_bytes)) {
            return 0;
        }
    }
    if (output_overlaps(output, output_size_bytes, workspace->filter_workspace->restoration.data,
                        layout->restoration_plane_bytes) ||
        output_overlaps(output, output_size_bytes,
                        workspace->filter_workspace->restoration_controls.tx_skip,
                        layout->restoration_tx_skip_count)) {
        return 0;
    }
    if (output_overlaps(output, output_size_bytes, workspace->decode_scratch,
                        sizeof(*workspace->decode_scratch)) ||
        output_overlaps(output, output_size_bytes, context->luma_partition_above,
                        context->above_count) ||
        output_overlaps(output, output_size_bytes, context->luma_partition_left,
                        context->left_count) ||
        output_overlaps(output, output_size_bytes, context->chroma_partition_above,
                        context->above_count) ||
        output_overlaps(output, output_size_bytes, context->chroma_partition_left,
                        context->left_count) ||
        output_overlaps(output, output_size_bytes, context->luma_mode_above,
                        context_sizes->luma_mode_above) ||
        output_overlaps(output, output_size_bytes, context->luma_mode_left,
                        context_sizes->luma_mode_left) ||
        output_overlaps(output, output_size_bytes, context->coefficient_above,
                        context_sizes->coefficient_above) ||
        output_overlaps(output, output_size_bytes, context->coefficient_left,
                        context_sizes->coefficient_left) ||
        output_overlaps(output, output_size_bytes, context->colocated_luma,
                        context_sizes->colocated_luma) ||
        output_overlaps(output, output_size_bytes, context->gdf_block_flags,
                        context->gdf_block_flag_capacity) ||
        output_overlaps(output, output_size_bytes, context->cdef_unit_strengths,
                        context->cdef_unit_strength_capacity) ||
        output_overlaps(output, output_size_bytes, context->restoration_unit_flags,
                        context->restoration_unit_flag_capacity) ||
        output_overlaps(
            output, output_size_bytes,
            workspace->bytes_per_sample == 1U ? (const void *)workspace->reconstruction_scratch
                                              : (const void *)workspace->reconstruction_scratch_u16,
            workspace->bytes_per_sample == 1U ? sizeof(*workspace->reconstruction_scratch)
                                              : sizeof(*workspace->reconstruction_scratch_u16)) ||
        (result != NULL && output_overlaps(output, output_size_bytes, result, sizeof(*result)))) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (output_overlaps(output, output_size_bytes, context->ccso_unit_controls[plane],
                            context->ccso_unit_control_capacity)) {
            return 0;
        }
    }
    return 1;
}

static void clear_decode_storage(const Av2hFrameState *frame,
                                 const Av2hFilterWorkspaceLayout *layout,
                                 Av2hFrameDecodeWorkspace *workspace,
                                 const Av2hFrameContextSizes *context_sizes) {
    Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    av2h_memory_fill(&workspace->decode_scratch->tile_decoder, 0U,
                     sizeof(workspace->decode_scratch->tile_decoder));
    av2h_memory_fill(context->luma_partition_above, 0U, context->above_count);
    av2h_memory_fill(context->luma_partition_left, 0U, context->left_count);
    av2h_memory_fill(context->chroma_partition_above, 0U, context->above_count);
    av2h_memory_fill(context->chroma_partition_left, 0U, context->left_count);
    av2h_memory_fill(context->luma_mode_above, 0U, context_sizes->luma_mode_above);
    av2h_memory_fill(context->luma_mode_left, 0U, context_sizes->luma_mode_left);
    av2h_memory_fill(context->coefficient_above, 0U, context_sizes->coefficient_above);
    av2h_memory_fill(context->coefficient_left, 0U, context_sizes->coefficient_left);
    av2h_memory_fill(context->colocated_luma, 0U, context_sizes->colocated_luma);
    if (context->gdf_block_flags != NULL) {
        av2h_memory_fill(context->gdf_block_flags, 0U, context->gdf_block_flag_capacity);
    }
    if (context->cdef_unit_strengths != NULL) {
        av2h_memory_fill(context->cdef_unit_strengths, 0U, context->cdef_unit_strength_capacity);
    }
    if (workspace->filter_workspace->restoration_tx_skip != NULL) {
        av2h_memory_fill(workspace->filter_workspace->restoration_tx_skip, 0U,
                         layout->restoration_tx_skip_count);
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        size_t ready_size_bytes;
        size_t map_size_bytes = workspace->filter_workspace->deblock_map.stride_cells[plane] *
                                workspace->filter_workspace->deblock_map.rows[plane] *
                                sizeof(Av2hDeblockCell);
        size_t control_size_bytes =
            workspace->filter_workspace->ccso_controls.stride_units[plane] * layout->ccso_rows;
        if (workspace->bytes_per_sample == 1U) {
            Av2hReconstructionPlane *coded = &workspace->coded[plane];
            ready_size_bytes =
                coded->reconstructed_stride_cells * (((size_t)coded->height + 3U) / 4U);
            av2h_plane_fill_u8(coded->data, coded->stride_samples, coded->stride_samples,
                               coded->height, 0U);
            av2h_memory_fill(coded->reconstructed, 0U, ready_size_bytes);
        } else {
            Av2hReconstructionPlaneU16 *coded = &workspace->coded_u16[plane];
            ready_size_bytes =
                coded->reconstructed_stride_cells * (((size_t)coded->height + 3U) / 4U);
            av2h_plane_fill_u16(coded->data, coded->stride_samples, coded->stride_samples,
                                coded->height, 0U);
            av2h_memory_fill(coded->reconstructed, 0U, ready_size_bytes);
        }
        av2h_memory_fill((void *)workspace->filter_workspace->deblock_map.plane[plane], 0U,
                         map_size_bytes);
        av2h_memory_fill((void *)workspace->filter_workspace->ccso_controls.plane[plane], 0U,
                         control_size_bytes);
        if (context->ccso_unit_controls[plane] != NULL) {
            av2h_memory_fill(context->ccso_unit_controls[plane], 0U,
                             context->ccso_unit_control_capacity);
        }
    }
}

static int maps_and_readiness_complete(const Av2hFrameState *frame,
                                       const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    if (frame == NULL)
        return 0;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hDeblockMap *map = &workspace->filter_workspace->deblock_map;
        const uint8_t *reconstructed;
        size_t reconstructed_stride_cells;
        size_t row;
        if (workspace->bytes_per_sample == 1U) {
            reconstructed = workspace->coded[plane].reconstructed;
            reconstructed_stride_cells = workspace->coded[plane].reconstructed_stride_cells;
        } else {
            reconstructed = workspace->coded_u16[plane].reconstructed;
            reconstructed_stride_cells = workspace->coded_u16[plane].reconstructed_stride_cells;
        }
        for (row = 0U; row < map->rows[plane]; ++row) {
            size_t column;
            unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
            size_t ready_row = row >> shift;
            for (column = 0U; column < map->columns[plane]; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride_cells[plane] + column];
                size_t ready_column = column >> shift;
                if (cell->transform_width == 0U || cell->transform_height == 0U ||
                    reconstructed[ready_row * reconstructed_stride_cells + ready_column] == 0U) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

Av2hStatus av2h_frame_workspace_prepare(const Av2hSequenceState *sequence,
                                        const Av2hFrameState *frame,
                                        Av2hFrameDecodeWorkspace *workspace, uint8_t *output,
                                        size_t required_output_bytes,
                                        const Av2hFrameDecodeResult *result,
                                        Av2hFilterWorkspaceLayout *layout) {
    Av2hFrameContextSizes context_sizes;
    Av2hStatus status = av2h_filter_workspace_query(
        frame, workspace != NULL ? workspace->bytes_per_sample : 0U, layout);
    if (status != AV2H_OK)
        return status;
    if (workspace->tile_worker_count > AV2H_PARALLEL_TILE_WORKERS ||
        (workspace->tile_worker_count > 1U &&
         (workspace->tile_workers == NULL ||
          (uintptr_t)workspace->tile_workers % _Alignof(Av2hFrameTileWorker) != 0U ||
          workspace->parallel_size_bytes <
              workspace->tile_worker_count * sizeof(*workspace->tile_workers)))) {
        return AV2H_INVALID_ARGUMENT;
    }
    for (unsigned int index = 1U; index < workspace->tile_worker_count; ++index) {
        const Av2hFrameDecodeWorkspace *worker = &workspace->tile_workers[index].workspace;
        if (worker->decode_scratch == NULL ||
            (uintptr_t)worker->decode_scratch % _Alignof(Av2hFrameDecodeScratch) != 0U ||
            (workspace->bytes_per_sample == 1U && worker->reconstruction_scratch == NULL) ||
            (workspace->bytes_per_sample == 2U && worker->reconstruction_scratch_u16 == NULL) ||
            !tile_context_valid(sequence, frame, &worker->tile_context, &context_sizes)) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    if (!av2h_frame_syntax_storage_valid(frame) || workspace->decode_scratch == NULL ||
        (uintptr_t)workspace->decode_scratch % _Alignof(Av2hFrameDecodeScratch) != 0U ||
        !filter_workspace_valid(frame, layout, workspace->filter_workspace,
                                workspace->bytes_per_sample) ||
        !decode_planes_valid(frame, layout, workspace) ||
        !tile_context_valid(sequence, frame, &workspace->tile_context, &context_sizes) ||
        !output_is_disjoint(output, required_output_bytes, frame, layout, workspace, &context_sizes,
                            result)) {
        return AV2H_INVALID_ARGUMENT;
    }
    clear_decode_storage(frame, layout, workspace, &context_sizes);
    return AV2H_OK;
}

int av2h_frame_workspace_complete(const Av2hFrameState *frame,
                                  const Av2hFrameDecodeWorkspace *workspace) {
    return maps_and_readiness_complete(frame, workspace);
}

static int workspace_reserve(size_t *size_bytes, size_t count, size_t element_size,
                             size_t alignment) {
    size_t allocation_bytes;
    size_t aligned_bytes;
    if (!av2h_size_multiply(count, element_size, &allocation_bytes) ||
        !av2h_size_align(*size_bytes, alignment, &aligned_bytes) ||
        !av2h_size_add(aligned_bytes, allocation_bytes, size_bytes)) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_filter_workspace_query(const Av2hFrameState *frame, size_t bytes_per_sample,
                                       Av2hFilterWorkspaceLayout *layout) {
    size_t coded_width;
    size_t coded_height;
    size_t size_bytes = 0U;
    unsigned int stage;
    unsigned int plane;
    if (frame == NULL || layout == NULL || frame->width == 0U || frame->height == 0U ||
        (bytes_per_sample != 1U && bytes_per_sample != 2U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (av2h_filter_ccso_unit_size(frame) != 64U && av2h_filter_ccso_unit_size(frame) != 128U &&
        av2h_filter_ccso_unit_size(frame) != 256U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(layout, 0U, sizeof(*layout));
    coded_width = av2h_filter_coded_dimension(frame->width);
    coded_height = av2h_filter_coded_dimension(frame->height);
    layout->alignment = _Alignof(Av2hDeblockCell);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(frame->chroma_format_idc, plane);
        size_t width = coded_width >> shift;
        size_t height = coded_height >> shift;
        size_t grid_size = 4U >> shift;
        if (width > UINT16_MAX || height > UINT16_MAX ||
            !av2h_size_multiply(width, height, &layout->stage_plane_bytes[plane]) ||
            !av2h_size_multiply(layout->stage_plane_bytes[plane], bytes_per_sample,
                                &layout->stage_plane_bytes[plane]) ||
            !av2h_size_multiply(width / grid_size, height / grid_size,
                                &layout->deblock_cell_count[plane])) {
            return AV2H_OVERFLOW;
        }
        layout->plane_width[plane] = (uint16_t)width;
        layout->plane_height[plane] = (uint16_t)height;
        layout->deblock_columns[plane] = width / grid_size;
        layout->deblock_rows[plane] = height / grid_size;
    }
    {
        uint16_t unit_size = av2h_filter_ccso_unit_size(frame);
        layout->ccso_columns = (coded_width + unit_size - 1U) / unit_size;
        layout->ccso_rows = (coded_height + unit_size - 1U) / unit_size;
    }
    if (!av2h_size_multiply(layout->ccso_columns, layout->ccso_rows, &layout->ccso_control_count)) {
        return AV2H_OVERFLOW;
    }
    for (stage = 0U; stage < 2U; ++stage) {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (!workspace_reserve(&size_bytes, layout->stage_plane_bytes[plane], sizeof(uint8_t),
                                   bytes_per_sample)) {
                return AV2H_OVERFLOW;
            }
        }
    }
    if (bytes_per_sample == 1U && frame->restoration_type[0] != AV2H_RESTORE_NONE) {
        layout->restoration_plane_bytes = layout->stage_plane_bytes[0];
        if (!workspace_reserve(&size_bytes, layout->restoration_plane_bytes, sizeof(uint8_t),
                               _Alignof(uint8_t))) {
            return AV2H_OVERFLOW;
        }
        if (av2h_frame_restoration_needs_tx_skip(frame)) {
            layout->restoration_tx_skip_count = layout->deblock_cell_count[0];
            if (!workspace_reserve(&size_bytes, layout->restoration_tx_skip_count, sizeof(uint8_t),
                                   _Alignof(uint8_t))) {
                return AV2H_OVERFLOW;
            }
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (!workspace_reserve(&size_bytes, layout->deblock_cell_count[plane],
                               sizeof(Av2hDeblockCell), _Alignof(Av2hDeblockCell))) {
            return AV2H_OVERFLOW;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (!workspace_reserve(&size_bytes, layout->ccso_control_count, sizeof(uint8_t),
                               _Alignof(uint8_t))) {
            return AV2H_OVERFLOW;
        }
    }
    layout->size_bytes = size_bytes;
    return AV2H_OK;
}

Av2hStatus av2h_filter_workspace_init(const Av2hFrameState *frame, size_t bytes_per_sample,
                                      void *memory, size_t memory_size_bytes,
                                      Av2hFilterWorkspace *workspace,
                                      Av2hFilterWorkspaceStorage *storage) {
    Av2hFilterWorkspaceLayout layout;
    Av2hArena arena;
    unsigned int plane;
    Av2hStatus status;
    if (workspace == NULL || storage == NULL)
        return AV2H_INVALID_ARGUMENT;
    status = av2h_filter_workspace_query(frame, bytes_per_sample, &layout);
    if (status != AV2H_OK)
        return status;
    if (memory == NULL || ((uintptr_t)memory & (layout.alignment - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (memory_size_bytes < layout.size_bytes)
        return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(workspace, 0U, sizeof(*workspace));
    av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_memory_fill(memory, 0U, layout.size_bytes);
    av2h_arena_init(&arena, memory, layout.size_bytes);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        void *samples =
            av2h_arena_allocate(&arena, layout.stage_plane_bytes[plane], bytes_per_sample);
        if (bytes_per_sample == 1U) {
            workspace->deblocked.plane[plane] =
                (Av2hFilterPlane){samples, layout.plane_width[plane], layout.plane_width[plane],
                                  layout.plane_height[plane]};
        } else {
            workspace->deblocked_u16.plane[plane] =
                (Av2hFilterPlaneU16){samples, layout.plane_width[plane], layout.plane_width[plane],
                                     layout.plane_height[plane]};
        }
    }
    if (layout.restoration_plane_bytes != 0U) {
        void *samples =
            av2h_arena_allocate(&arena, layout.restoration_plane_bytes, _Alignof(uint8_t));
        workspace->restoration = (Av2hFilterPlane){samples, layout.plane_width[0],
                                                   layout.plane_width[0], layout.plane_height[0]};
        if (layout.restoration_tx_skip_count != 0U) {
            storage->restoration_tx_skip =
                av2h_arena_allocate(&arena, layout.restoration_tx_skip_count, _Alignof(uint8_t));
            workspace->restoration_tx_skip = storage->restoration_tx_skip;
            workspace->restoration_controls.tx_skip = storage->restoration_tx_skip;
            workspace->restoration_controls.tx_skip_stride = layout.deblock_columns[0];
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        void *samples =
            av2h_arena_allocate(&arena, layout.stage_plane_bytes[plane], bytes_per_sample);
        if (bytes_per_sample == 1U) {
            workspace->cdef.plane[plane] =
                (Av2hFilterPlane){samples, layout.plane_width[plane], layout.plane_width[plane],
                                  layout.plane_height[plane]};
        } else {
            workspace->cdef_u16.plane[plane] =
                (Av2hFilterPlaneU16){samples, layout.plane_width[plane], layout.plane_width[plane],
                                     layout.plane_height[plane]};
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        storage->deblock_cells[plane] =
            av2h_arena_allocate(&arena, layout.deblock_cell_count[plane] * sizeof(Av2hDeblockCell),
                                _Alignof(Av2hDeblockCell));
        workspace->deblock_map.plane[plane] = storage->deblock_cells[plane];
        workspace->deblock_map.stride_cells[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.columns[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.rows[plane] = layout.deblock_rows[plane];
    }
    workspace->ccso_controls.columns = layout.ccso_columns;
    workspace->ccso_controls.rows = layout.ccso_rows;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        storage->ccso_controls[plane] =
            av2h_arena_allocate(&arena, layout.ccso_control_count, _Alignof(uint8_t));
        workspace->ccso_controls.plane[plane] = storage->ccso_controls[plane];
        workspace->ccso_controls.stride_units[plane] = layout.ccso_columns;
    }
    if (arena.status != AV2H_OK || av2h_arena_required(&arena) > layout.size_bytes) {
        return arena.status != AV2H_OK ? arena.status : AV2H_OVERFLOW;
    }
    return AV2H_OK;
}

/* Instantiate typed kernels after the shared helpers. */
#include "internal/sample_kernel_u8.inc"
#include "internal/frame_workspace_kernel.inc"
#include "internal/sample_kernel_end.inc"
#include "internal/sample_kernel_u16.inc"
#include "internal/frame_workspace_kernel.inc"
#include "internal/sample_kernel_end.inc"

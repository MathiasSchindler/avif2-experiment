#include "decoder/filter.h"
#include "decoder/frame_output.h"
#include "decoder/internal/filter_geometry.h"
#include "decoder/internal/frame_workspace.h"

#include "output_layout.h"
#include "decoder/tx_geometry.h"

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
} Av2hFrameDecodeWorkspaceGeometry;

static void *workspace_arena_array(Av2hArena *arena,
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

static size_t maximum_alignment(size_t left, size_t right) {
    return left > right ? left : right;
}

static void *workspace_reconstruction_scratch(
    Av2hArena *arena,
    size_t bytes_per_sample) {
    const size_t coefficient_bytes =
        3U * AV2H_FIRST_COEFF_CAPACITY * sizeof(int32_t);
    const size_t sample_count = AV2H_RECON_SAMPLE_CAPACITY + 4U * 128U;
    size_t sample_bytes;
    size_t scratch_bytes;
    if (!av2h_size_multiply(
            sample_count, bytes_per_sample, &sample_bytes) ||
        !av2h_size_add(
            coefficient_bytes, sample_bytes, &scratch_bytes)) {
        arena->status = AV2H_OVERFLOW;
        return 0;
    }
    return av2h_arena_allocate(
        arena, scratch_bytes, _Alignof(Av2hReconstructionScratch));
}

static Av2hStatus frame_decode_workspace_geometry(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    Av2hFrameDecodeWorkspaceGeometry *geometry) {
    size_t mi_columns;
    size_t mi_rows;
    size_t superblock_mi;
    size_t rounded;
    size_t cdef_columns;
    size_t cdef_rows;
    Av2hStatus status;
    if (sequence == 0 || frame == 0 || geometry == 0 ||
        (sequence->superblock_size != 64U &&
         sequence->superblock_size != 128U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_filter_workspace_query_bytes(
        frame, bytes_per_sample, &geometry->filter);
    if (status != AV2H_OK) return status;
    mi_columns = av2h_frame_mi_dimension(frame->width);
    mi_rows = av2h_frame_mi_dimension(frame->height);
    superblock_mi = sequence->superblock_size == 128U ? 32U : 16U;
    if (!av2h_size_add(mi_columns, superblock_mi - 1U, &rounded) ||
        !av2h_size_multiply(
            rounded / superblock_mi, superblock_mi,
            &geometry->context_columns) ||
        !av2h_size_add(mi_rows, superblock_mi - 1U, &rounded) ||
        !av2h_size_multiply(
            rounded / superblock_mi, superblock_mi,
            &geometry->context_rows) ||
        !av2h_size_multiply(
            mi_columns, mi_rows, &geometry->mi_cells) ||
        !av2h_size_multiply(
            geometry->context_columns, av2h_frame_plane_count(frame),
            &geometry->coefficient_above) ||
        !av2h_size_multiply(
            geometry->context_rows, av2h_frame_plane_count(frame),
            &geometry->coefficient_left)) {
        return AV2H_OVERFLOW;
    }
    cdef_columns = ((size_t)frame->width + 63U) / 64U;
    cdef_rows = ((size_t)frame->height + 63U) / 64U;
    if (!av2h_size_multiply(
            cdef_columns, cdef_rows, &geometry->cdef_count)) {
        return AV2H_OVERFLOW;
    }
    return AV2H_OK;
}

static void frame_decode_workspace_bind(
    Av2hArena *arena,
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    const Av2hFrameDecodeWorkspaceGeometry *geometry,
    Av2hFrameDecodeWorkspace *workspace,
    void **filter_memory) {
    unsigned int plane;
    *filter_memory = av2h_arena_allocate(
        arena, geometry->filter.size_bytes, geometry->filter.alignment);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint8_t *data = workspace_arena_array(
            arena, geometry->filter.stage_plane_bytes[plane],
            sizeof(uint8_t), bytes_per_sample);
        uint8_t *reconstructed = workspace_arena_array(
            arena, geometry->filter.deblock_cell_count[plane],
            sizeof(uint8_t), _Alignof(uint8_t));
        if (workspace != 0) {
            workspace->coded[plane] = (Av2hReconstructionPlane){
                data,
                geometry->filter.plane_width[plane],
                geometry->filter.plane_width[plane],
                geometry->filter.plane_height[plane],
                (uint16_t)av2h_frame_plane_dimension(
                    frame, plane, frame->width),
                (uint16_t)av2h_frame_plane_dimension(
                    frame, plane, frame->height),
                reconstructed,
                geometry->filter.deblock_columns[plane],
                geometry->filter.deblock_cell_count[plane]
            };
            workspace->coded_data_size_bytes[plane] =
                geometry->filter.stage_plane_bytes[plane];
        }
    }
#define AV2H_BIND_WORKSPACE_FIELD(field, count, type)                         \
    do {                                                                       \
        void *allocated = workspace_arena_array(                               \
            arena, (count), sizeof(type), _Alignof(type));                     \
        if (workspace != 0) {                                                  \
            workspace->tile_context.field = (type *)allocated;                \
        }                                                                      \
    } while (0)
    AV2H_BIND_WORKSPACE_FIELD(
        luma_partition_above, geometry->context_columns, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        luma_partition_left, geometry->context_rows, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        chroma_partition_above, geometry->context_columns, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        chroma_partition_left, geometry->context_rows, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        luma_mode_above, geometry->context_columns, Av2hLumaModeContext);
    AV2H_BIND_WORKSPACE_FIELD(
        luma_mode_left, geometry->context_rows, Av2hLumaModeContext);
    AV2H_BIND_WORKSPACE_FIELD(
        coefficient_above, geometry->coefficient_above, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        coefficient_left, geometry->coefficient_left, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        colocated_luma, geometry->mi_cells, Av2hTileLumaModeCell);
    AV2H_BIND_WORKSPACE_FIELD(
        gdf_block_flags, frame->gdf_block_count, uint8_t);
    AV2H_BIND_WORKSPACE_FIELD(
        cdef_unit_strengths, geometry->cdef_count, uint8_t);
#undef AV2H_BIND_WORKSPACE_FIELD
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        uint8_t *controls = workspace_arena_array(
            arena, geometry->filter.ccso_control_count,
            sizeof(uint8_t), _Alignof(uint8_t));
        if (workspace != 0) {
            workspace->tile_context.ccso_unit_controls[plane] = controls;
        }
    }
    {
        Av2hReconstructionScratch *scratch =
            (Av2hReconstructionScratch *)workspace_reconstruction_scratch(
                arena, bytes_per_sample);
        if (workspace != 0) workspace->reconstruction_scratch = scratch;
    }
}

Av2hStatus av2h_frame_decode_workspace_query(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspaceLayout *layout) {
    return av2h_frame_decode_workspace_query_bytes(
        sequence, frame, 1U, layout);
}

Av2hStatus av2h_frame_decode_workspace_query_bytes(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    Av2hFrameDecodeWorkspaceLayout *layout) {
    Av2hFrameDecodeWorkspaceGeometry geometry;
    Av2hFrameDecodeWorkspaceLayout result;
    Av2hArena arena;
    void *filter_memory;
    Av2hStatus status;
    if (layout == 0) return AV2H_INVALID_ARGUMENT;
    if (bytes_per_sample != 1U && bytes_per_sample != 2U) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = frame_decode_workspace_geometry(
        sequence, frame, bytes_per_sample, &geometry);
    if (status != AV2H_OK) return status;
    av2h_arena_init_sizing(&arena);
    frame_decode_workspace_bind(
        &arena, frame, bytes_per_sample, &geometry, 0, &filter_memory);
    if (arena.status != AV2H_OK) return arena.status;
    result.size_bytes = av2h_arena_required(&arena);
    result.alignment = geometry.filter.alignment;
    result.alignment = maximum_alignment(
        result.alignment, _Alignof(Av2hLumaModeContext));
    result.alignment = maximum_alignment(
        result.alignment, _Alignof(Av2hTileLumaModeCell));
    result.alignment = maximum_alignment(
        result.alignment, _Alignof(Av2hReconstructionScratch));
    *layout = result;
    return AV2H_OK;
}

Av2hStatus av2h_frame_decode_workspace_init(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFrameDecodeWorkspaceLayout *layout,
    void *memory,
    size_t memory_size_bytes,
    Av2hFrameDecodeWorkspace *workspace,
    Av2hFilterWorkspace *filter_workspace,
    Av2hFilterWorkspaceStorage *filter_storage) {
    Av2hFrameDecodeWorkspaceGeometry geometry;
    Av2hFrameDecodeWorkspaceLayout fresh_layout;
    Av2hFrameDecodeWorkspace local_workspace;
    Av2hFilterWorkspace local_filter_workspace;
    Av2hFilterWorkspaceStorage local_filter_storage;
    Av2hArena arena;
    void *filter_memory;
    Av2hStatus status;
    if (layout == 0 || workspace == 0 || filter_workspace == 0 ||
        filter_storage == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_frame_decode_workspace_query(
        sequence, frame, &fresh_layout);
    if (status != AV2H_OK) return status;
    if (layout->size_bytes != fresh_layout.size_bytes ||
        layout->alignment != fresh_layout.alignment || memory == 0 ||
        ((uintptr_t)memory & (fresh_layout.alignment - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (memory_size_bytes < fresh_layout.size_bytes) return AV2H_OUT_OF_MEMORY;
    status = frame_decode_workspace_geometry(sequence, frame, 1U, &geometry);
    if (status != AV2H_OK) return status;
    av2h_memory_fill(&local_workspace, 0U, sizeof(local_workspace));
    av2h_memory_fill(
        &local_filter_workspace, 0U, sizeof(local_filter_workspace));
    av2h_memory_fill(
        &local_filter_storage, 0U, sizeof(local_filter_storage));
    av2h_arena_init(&arena, memory, fresh_layout.size_bytes);
    frame_decode_workspace_bind(
        &arena, frame, 1U, &geometry, &local_workspace, &filter_memory);
    if (arena.status != AV2H_OK) return arena.status;
    local_workspace.tile_context.above_count = geometry.context_columns;
    local_workspace.tile_context.left_count = geometry.context_rows;
    local_workspace.tile_context.colocated_luma_count = geometry.mi_cells;
    local_workspace.tile_context.gdf_block_flag_capacity =
        frame->gdf_block_count;
    local_workspace.tile_context.cdef_unit_strength_capacity =
        geometry.cdef_count;
    local_workspace.tile_context.ccso_unit_control_capacity =
        geometry.filter.ccso_control_count;
    status = av2h_filter_workspace_init(
        frame, filter_memory, geometry.filter.size_bytes,
        &local_filter_workspace, &local_filter_storage);
    if (status != AV2H_OK) return status;
    av2h_memory_copy(
        filter_workspace, &local_filter_workspace,
        sizeof(local_filter_workspace));
    av2h_memory_copy(
        filter_storage, &local_filter_storage, sizeof(local_filter_storage));
    local_workspace.filter_workspace = filter_workspace;
    av2h_memory_copy(workspace, &local_workspace, sizeof(local_workspace));
    return AV2H_OK;
}

static int ranges_overlap(const void *left,
                          size_t left_size_bytes,
                          const void *right,
                          size_t right_size_bytes) {
    uintptr_t maximum = (uintptr_t)-1;
    uintptr_t left_start = (uintptr_t)left;
    uintptr_t right_start = (uintptr_t)right;
    uintptr_t left_end;
    uintptr_t right_end;
    if (left_size_bytes == 0U || right_size_bytes == 0U) return 0;
    if (left == 0 || right == 0 ||
        left_size_bytes > maximum - left_start ||
        right_size_bytes > maximum - right_start) {
        return 1;
    }
    left_end = left_start + left_size_bytes;
    right_end = right_start + right_size_bytes;
    return left_start < right_end && right_start < left_end;
}

static int output_overlaps(uint8_t *output,
                           size_t output_size_bytes,
                           const void *memory,
                           size_t memory_size_bytes) {
    return ranges_overlap(output, output_size_bytes, memory, memory_size_bytes);
}

static int sized_u8_plane_bytes(size_t stride_samples,
                                size_t height,
                                size_t *size_bytes) {
    return av2h_size_multiply(stride_samples, height, size_bytes);
}

Av2hStatus av2h_planar_output_size(
    const Av2hFrameState *frame,
    size_t *size_bytes) {
    return av2h_planar_output_size_bytes(frame, 1U, size_bytes);
}

Av2hStatus av2h_planar_output_size_bytes(
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    size_t *size_bytes) {
    Av2hPlaneLayout planes[3];
    if (frame == 0 || size_bytes == 0 || frame->width == 0U ||
        frame->height == 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    return av2h_output_layout_calculate(
        frame->width, frame->height, frame->chroma_format_idc,
        bytes_per_sample, planes, size_bytes, 0);
}

Av2hStatus av2h_deblock_map_commit_transform(
    Av2hDeblockMap *map,
    uint8_t chroma_format_idc,
    unsigned int plane,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix,
    uint8_t q_index,
    uint8_t skip,
    uint8_t all_zero,
    uint8_t lossless) {
    const Av2hTxDescriptor *descriptor;
    size_t grid_size;
    size_t block_column;
    size_t block_row;
    size_t block_columns;
    size_t block_rows;
    size_t transform_columns;
    size_t transform_rows;
    size_t row;
    Av2hStatus status;
    if (map == 0 || leaf == 0 || prefix == 0 ||
        plane >= AV2H_FRAME_PLANE_COUNT || skip > 1U ||
        all_zero > 1U || lossless > 1U || map->plane[plane] == 0 ||
        map->columns[plane] == 0U || map->rows[plane] == 0U ||
        map->stride_cells[plane] < map->columns[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_tx_descriptor_get(prefix->tx_size, &descriptor);
    if (status != AV2H_OK) return status;
    status = av2h_block_dimensions_mi(
        leaf->block_size, &block_columns, &block_rows);
    if (status != AV2H_OK) return status;
    if ((descriptor->output_width & 3U) != 0U ||
        (descriptor->output_height & 3U) != 0U) {
        return AV2H_UNSUPPORTED;
    }
    grid_size = 4U >> av2h_chroma_shift(chroma_format_idc, plane);
    block_column = leaf->column;
    block_row = leaf->row;
    transform_columns = descriptor->output_width / grid_size;
    transform_rows = descriptor->output_height / grid_size;
    if (transform_columns > block_columns) transform_columns = block_columns;
    if (transform_rows > block_rows) transform_rows = block_rows;
    if (block_column >= map->columns[plane] ||
        block_row >= map->rows[plane]) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (transform_columns > map->columns[plane] - block_column) {
        transform_columns = map->columns[plane] - block_column;
    }
    if (transform_rows > map->rows[plane] - block_row) {
        transform_rows = map->rows[plane] - block_row;
    }
    for (row = 0U; row < transform_rows; ++row) {
        size_t column;
        for (column = 0U; column < transform_columns; ++column) {
            Av2hDeblockCell *cell = (Av2hDeblockCell *)&map->plane[plane][
                (block_row + row) * map->stride_cells[plane] +
                block_column + column];
            if (cell->transform_width != 0U ||
                cell->transform_height != 0U) {
                return AV2H_INVALID_DATA;
            }
            *cell = (Av2hDeblockCell){
                (uint16_t)block_column,
                (uint16_t)block_row,
                (uint16_t)block_column,
                (uint16_t)block_row,
                descriptor->output_width,
                descriptor->output_height,
                q_index,
                (uint8_t)(skip != 0U || all_zero != 0U),
                lossless
            };
        }
    }
    return AV2H_OK;
}

static int filter_workspace_valid(
    const Av2hFrameState *frame,
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFilterWorkspace *workspace) {
    unsigned int plane;
    if (layout == 0 || workspace == 0 ||
        workspace->ccso_controls.columns != layout->ccso_columns ||
        workspace->ccso_controls.rows != layout->ccso_rows) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *deblocked =
            &workspace->deblocked.plane[plane];
        const Av2hFilterPlane *cdef = &workspace->cdef.plane[plane];
        if (deblocked->data == 0 || cdef->data == 0 ||
            deblocked->width != layout->plane_width[plane] ||
            deblocked->height != layout->plane_height[plane] ||
            cdef->width != layout->plane_width[plane] ||
            cdef->height != layout->plane_height[plane] ||
            deblocked->stride_samples < deblocked->width ||
            cdef->stride_samples < cdef->width ||
            workspace->deblock_map.plane[plane] == 0 ||
            workspace->deblock_map.columns[plane] !=
                layout->deblock_columns[plane] ||
            workspace->deblock_map.rows[plane] !=
                layout->deblock_rows[plane] ||
            workspace->deblock_map.stride_cells[plane] !=
                layout->deblock_columns[plane] ||
            workspace->ccso_controls.plane[plane] == 0 ||
            workspace->ccso_controls.stride_units[plane] !=
                layout->ccso_columns) {
            return 0;
        }
    }
    return 1;
}

static int decode_planes_valid(
    const Av2hFrameState *frame,
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t required_data;
        size_t required_ready;
        size_t reference_width = av2h_frame_plane_dimension(
            frame, plane, frame->width);
        size_t reference_height = av2h_frame_plane_dimension(
            frame, plane, frame->height);
        if (coded->data == 0 || coded->reconstructed == 0 ||
            coded->width != layout->plane_width[plane] ||
            coded->height != layout->plane_height[plane] ||
            coded->stride_samples < coded->width ||
            coded->reference_width != reference_width ||
            coded->reference_height != reference_height ||
            coded->reconstructed_stride_cells <
                ((size_t)coded->width + 3U) / 4U ||
            !sized_u8_plane_bytes(coded->stride_samples, coded->height,
                               &required_data) ||
            required_data > workspace->coded_data_size_bytes[plane] ||
            !av2h_size_multiply(
                coded->reconstructed_stride_cells,
                ((size_t)coded->height + 3U) / 4U,
                &required_ready) ||
            required_ready > coded->reconstructed_count_cells) {
            return 0;
        }
    }
    return 1;
}

static int tile_context_valid(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hTileContextStorage *context,
    Av2hFrameContextSizes *sizes) {
    size_t mi_columns;
    size_t mi_rows;
    size_t context_columns;
    size_t context_rows;
    size_t superblock_mi;
    size_t cell_count;
    if (sequence == 0 || frame == 0 || context == 0 || sizes == 0 ||
        context->luma_partition_above == 0 ||
        context->luma_partition_left == 0 ||
        context->chroma_partition_above == 0 ||
        context->chroma_partition_left == 0 ||
        context->luma_mode_above == 0 || context->luma_mode_left == 0 ||
        context->coefficient_above == 0 ||
        context->coefficient_left == 0 ||
        context->colocated_luma == 0) {
        return 0;
    }
    mi_columns = av2h_frame_mi_dimension(frame->width);
    mi_rows = av2h_frame_mi_dimension(frame->height);
    superblock_mi = sequence->superblock_size == 128U ? 32U : 16U;
    context_columns = (mi_columns + superblock_mi - 1U) /
        superblock_mi * superblock_mi;
    context_rows = (mi_rows + superblock_mi - 1U) /
        superblock_mi * superblock_mi;
    if (context->above_count < context_columns ||
        context->left_count < context_rows ||
        !av2h_size_multiply(mi_columns, mi_rows, &cell_count) ||
        context->colocated_luma_count < cell_count ||
        !av2h_size_multiply(
            context->above_count, sizeof(*context->luma_mode_above),
            &sizes->luma_mode_above) ||
        !av2h_size_multiply(
            context->left_count, sizeof(*context->luma_mode_left),
            &sizes->luma_mode_left) ||
        !av2h_size_multiply(
            context->above_count, av2h_frame_plane_count(frame),
            &sizes->coefficient_above) ||
        !av2h_size_multiply(
            context->left_count, av2h_frame_plane_count(frame),
            &sizes->coefficient_left) ||
        !av2h_size_multiply(
            context->colocated_luma_count,
            sizeof(*context->colocated_luma),
            &sizes->colocated_luma) ||
        (context->gdf_block_flag_capacity != 0U &&
         context->gdf_block_flags == 0) ||
        (context->cdef_unit_strength_capacity != 0U &&
         context->cdef_unit_strengths == 0)) {
        return 0;
    }
    return 1;
}

static int output_is_disjoint(
    uint8_t *output,
    size_t output_size_bytes,
    const Av2hFrameState *frame,
    const Av2hFilterWorkspaceLayout *layout,
    const Av2hFrameDecodeWorkspace *workspace,
    const Av2hFrameContextSizes *context_sizes,
    const Av2hFrameDecodeResult *result) {
    const Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        const Av2hFilterPlane *deblocked =
            &workspace->filter_workspace->deblocked.plane[plane];
        const Av2hFilterPlane *cdef =
            &workspace->filter_workspace->cdef.plane[plane];
        size_t coded_size_bytes;
        size_t deblocked_size_bytes;
        size_t cdef_size_bytes;
        size_t map_size_bytes;
        size_t control_size_bytes;
        if (!sized_u8_plane_bytes(coded->stride_samples, coded->height, &coded_size_bytes) ||
            !sized_u8_plane_bytes(
                deblocked->stride_samples, deblocked->height, &deblocked_size_bytes) ||
            !sized_u8_plane_bytes(cdef->stride_samples, cdef->height, &cdef_size_bytes) ||
            !av2h_size_multiply(
                workspace->filter_workspace->deblock_map.stride_cells[plane],
                workspace->filter_workspace->deblock_map.rows[plane],
                &map_size_bytes) ||
            !av2h_size_multiply(
                map_size_bytes, sizeof(Av2hDeblockCell), &map_size_bytes) ||
            !av2h_size_multiply(
                workspace->filter_workspace->ccso_controls.stride_units[plane],
                layout->ccso_rows, &control_size_bytes) ||
            output_overlaps(output, output_size_bytes, coded->data, coded_size_bytes) ||
            output_overlaps(
                output, output_size_bytes, coded->reconstructed,
                coded->reconstructed_count_cells) ||
            output_overlaps(
                output, output_size_bytes, deblocked->data, deblocked_size_bytes) ||
            output_overlaps(output, output_size_bytes, cdef->data, cdef_size_bytes) ||
            output_overlaps(
                output, output_size_bytes,
                workspace->filter_workspace->deblock_map.plane[plane],
                map_size_bytes) ||
            output_overlaps(
                output, output_size_bytes,
                workspace->filter_workspace->ccso_controls.plane[plane],
                control_size_bytes)) {
            return 0;
        }
    }
    if (output_overlaps(
            output, output_size_bytes, context->luma_partition_above,
            context->above_count) ||
        output_overlaps(
            output, output_size_bytes, context->luma_partition_left,
            context->left_count) ||
        output_overlaps(
            output, output_size_bytes, context->chroma_partition_above,
            context->above_count) ||
        output_overlaps(
            output, output_size_bytes, context->chroma_partition_left,
            context->left_count) ||
        output_overlaps(
            output, output_size_bytes, context->luma_mode_above,
            context_sizes->luma_mode_above) ||
        output_overlaps(
            output, output_size_bytes, context->luma_mode_left,
            context_sizes->luma_mode_left) ||
        output_overlaps(
            output, output_size_bytes, context->coefficient_above,
            context_sizes->coefficient_above) ||
        output_overlaps(
            output, output_size_bytes, context->coefficient_left,
            context_sizes->coefficient_left) ||
        output_overlaps(
            output, output_size_bytes, context->colocated_luma,
            context_sizes->colocated_luma) ||
        output_overlaps(
            output, output_size_bytes, context->gdf_block_flags,
            context->gdf_block_flag_capacity) ||
        output_overlaps(
            output, output_size_bytes, context->cdef_unit_strengths,
            context->cdef_unit_strength_capacity) ||
        output_overlaps(
            output, output_size_bytes, workspace->reconstruction_scratch,
            sizeof(*workspace->reconstruction_scratch)) ||
        (result != 0 &&
         output_overlaps(output, output_size_bytes, result, sizeof(*result)))) {
        return 0;
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (output_overlaps(
                output, output_size_bytes, context->ccso_unit_controls[plane],
                context->ccso_unit_control_capacity)) {
            return 0;
        }
    }
    return 1;
}

static void clear_decode_storage(
    const Av2hFrameState *frame,
    const Av2hFilterWorkspaceLayout *layout,
    Av2hFrameDecodeWorkspace *workspace,
    const Av2hFrameContextSizes *context_sizes) {
    Av2hTileContextStorage *context = &workspace->tile_context;
    unsigned int plane;
    av2h_memory_fill(
        context->luma_partition_above, 0U, context->above_count);
    av2h_memory_fill(
        context->luma_partition_left, 0U, context->left_count);
    av2h_memory_fill(
        context->chroma_partition_above, 0U, context->above_count);
    av2h_memory_fill(
        context->chroma_partition_left, 0U, context->left_count);
    av2h_memory_fill(
        context->luma_mode_above, 0U,
        context_sizes->luma_mode_above);
    av2h_memory_fill(
        context->luma_mode_left, 0U,
        context_sizes->luma_mode_left);
    av2h_memory_fill(
        context->coefficient_above, 0U, context_sizes->coefficient_above);
    av2h_memory_fill(
        context->coefficient_left, 0U, context_sizes->coefficient_left);
    av2h_memory_fill(
        context->colocated_luma, 0U, context_sizes->colocated_luma);
    if (context->gdf_block_flags != 0) {
        av2h_memory_fill(
            context->gdf_block_flags, 0U,
            context->gdf_block_flag_capacity);
    }
    if (context->cdef_unit_strengths != 0) {
        av2h_memory_fill(
            context->cdef_unit_strengths, 0U,
            context->cdef_unit_strength_capacity);
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t data_size_bytes = coded->stride_samples * coded->height;
        size_t ready_size_bytes = coded->reconstructed_stride_cells *
            (((size_t)coded->height + 3U) / 4U);
        size_t map_size_bytes =
            workspace->filter_workspace->deblock_map.stride_cells[plane] *
            workspace->filter_workspace->deblock_map.rows[plane] *
            sizeof(Av2hDeblockCell);
        size_t control_size_bytes =
            workspace->filter_workspace->ccso_controls.stride_units[plane] *
            layout->ccso_rows;
        av2h_memory_fill(coded->data, 0U, data_size_bytes);
        av2h_memory_fill(coded->reconstructed, 0U, ready_size_bytes);
        av2h_memory_fill(
            (void *)workspace->filter_workspace->deblock_map.plane[plane],
            0U, map_size_bytes);
        av2h_memory_fill(
            (void *)workspace->filter_workspace->ccso_controls.plane[plane],
            0U, control_size_bytes);
        if (context->ccso_unit_controls[plane] != 0) {
            av2h_memory_fill(
                context->ccso_unit_controls[plane], 0U,
                context->ccso_unit_control_capacity);
        }
    }
}

static int maps_and_readiness_complete(
    const Av2hFrameState *frame,
    const Av2hFrameDecodeWorkspace *workspace) {
    unsigned int plane;
    if (frame == 0) return 0;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hDeblockMap *map =
            &workspace->filter_workspace->deblock_map;
        const Av2hReconstructionPlane *coded = &workspace->coded[plane];
        size_t row;
        for (row = 0U; row < map->rows[plane]; ++row) {
            size_t column;
            unsigned int shift = av2h_chroma_shift(
                frame->chroma_format_idc, plane);
            size_t ready_row = row >> shift;
            for (column = 0U; column < map->columns[plane]; ++column) {
                const Av2hDeblockCell *cell =
                    &map->plane[plane][row * map->stride_cells[plane] + column];
                size_t ready_column = column >> shift;
                if (cell->transform_width == 0U ||
                    cell->transform_height == 0U ||
                    coded->reconstructed[
                        ready_row * coded->reconstructed_stride_cells +
                        ready_column] == 0U) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

Av2hStatus av2h_copy_visible_planar_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes) {
    Av2hFilterWorkspaceLayout layout;
    size_t required_output_bytes;
    size_t offset = 0U;
    unsigned int plane;
    Av2hStatus status;
    if (frame == 0 || coded == 0 || coded_data_size_bytes == 0 || output == 0) {
        return AV2H_INVALID_ARGUMENT;
    }
    status = av2h_planar_output_size(frame, &required_output_bytes);
    if (status != AV2H_OK) return status;
    if (output_size_bytes < required_output_bytes) return AV2H_OUT_OF_MEMORY;
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *source = &coded->plane[plane];
        size_t source_size_bytes;
        if (source->data == 0 ||
            source->width != layout.plane_width[plane] ||
            source->height != layout.plane_height[plane] ||
            source->stride_samples < source->width ||
            !sized_u8_plane_bytes(
                source->stride_samples, source->height, &source_size_bytes) ||
            source_size_bytes > coded_data_size_bytes[plane] ||
            output_overlaps(
                output, required_output_bytes, source->data, source_size_bytes)) {
            return AV2H_INVALID_ARGUMENT;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        const Av2hFilterPlane *source = &coded->plane[plane];
        size_t width = av2h_frame_plane_dimension(
            frame, plane, frame->width);
        size_t height = av2h_frame_plane_dimension(
            frame, plane, frame->height);
        size_t row;
        for (row = 0U; row < height; ++row) {
            av2h_memory_copy(
                output + offset,
                source->data + row * source->stride_samples,
                width);
            offset += width;
        }
    }
    return AV2H_OK;
}

Av2hStatus av2h_frame_workspace_prepare(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t required_output_bytes,
    const Av2hFrameDecodeResult *result,
    Av2hFilterWorkspaceLayout *layout) {
    Av2hFrameContextSizes context_sizes;
    Av2hStatus status = av2h_filter_workspace_query(frame, layout);
    if (status != AV2H_OK) return status;
    if (!filter_workspace_valid(frame, layout, workspace->filter_workspace) ||
        !decode_planes_valid(frame, layout, workspace) ||
        !tile_context_valid(
            sequence, frame, &workspace->tile_context, &context_sizes) ||
        !output_is_disjoint(
            output, required_output_bytes, frame, layout, workspace,
            &context_sizes, result)) {
        return AV2H_INVALID_ARGUMENT;
    }
    clear_decode_storage(frame, layout, workspace, &context_sizes);
    return AV2H_OK;
}

int av2h_frame_workspace_complete(
    const Av2hFrameState *frame,
    const Av2hFrameDecodeWorkspace *workspace) {
    return maps_and_readiness_complete(frame, workspace);
}

static int workspace_reserve(size_t *size_bytes,
                             size_t count,
                             size_t element_size,
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

Av2hStatus av2h_filter_workspace_query(
    const Av2hFrameState *frame,
    Av2hFilterWorkspaceLayout *layout) {
    return av2h_filter_workspace_query_bytes(frame, 1U, layout);
}

Av2hStatus av2h_filter_workspace_query_bytes(
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    Av2hFilterWorkspaceLayout *layout) {
    size_t coded_width;
    size_t coded_height;
    size_t size_bytes = 0U;
    unsigned int stage;
    unsigned int plane;
    if (frame == 0 || layout == 0 || frame->width == 0U ||
        frame->height == 0U ||
        (bytes_per_sample != 1U && bytes_per_sample != 2U)) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (av2h_filter_ccso_unit_size(frame) != 64U &&
        av2h_filter_ccso_unit_size(frame) != 128U &&
        av2h_filter_ccso_unit_size(frame) != 256U) {
        return AV2H_INVALID_ARGUMENT;
    }
    av2h_memory_fill(layout, 0U, sizeof(*layout));
    coded_width = av2h_filter_coded_dimension(frame->width);
    coded_height = av2h_filter_coded_dimension(frame->height);
    layout->alignment = _Alignof(Av2hDeblockCell);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        unsigned int shift = av2h_chroma_shift(
            frame->chroma_format_idc, plane);
        size_t width = coded_width >> shift;
        size_t height = coded_height >> shift;
        size_t grid_size = 4U >> shift;
        if (width > UINT16_MAX || height > UINT16_MAX ||
            !av2h_size_multiply(
                width, height, &layout->stage_plane_bytes[plane]) ||
            !av2h_size_multiply(
                layout->stage_plane_bytes[plane], bytes_per_sample,
                &layout->stage_plane_bytes[plane]) ||
            !av2h_size_multiply(
                width / grid_size, height / grid_size,
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
        layout->ccso_columns =
            (coded_width + unit_size - 1U) / unit_size;
        layout->ccso_rows =
            (coded_height + unit_size - 1U) / unit_size;
    }
    if (!av2h_size_multiply(
            layout->ccso_columns, layout->ccso_rows,
            &layout->ccso_control_count)) {
        return AV2H_OVERFLOW;
    }
    for (stage = 0U; stage < 2U; ++stage) {
        for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
            if (!workspace_reserve(
                    &size_bytes, layout->stage_plane_bytes[plane],
                    sizeof(uint8_t), bytes_per_sample)) {
                return AV2H_OVERFLOW;
            }
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (!workspace_reserve(
                &size_bytes, layout->deblock_cell_count[plane],
                sizeof(Av2hDeblockCell), _Alignof(Av2hDeblockCell))) {
            return AV2H_OVERFLOW;
        }
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        if (!workspace_reserve(
                &size_bytes, layout->ccso_control_count,
                sizeof(uint8_t), _Alignof(uint8_t))) {
            return AV2H_OVERFLOW;
        }
    }
    layout->size_bytes = size_bytes;
    return AV2H_OK;
}

Av2hStatus av2h_filter_workspace_init(
    const Av2hFrameState *frame,
    void *memory,
    size_t memory_size_bytes,
    Av2hFilterWorkspace *workspace,
    Av2hFilterWorkspaceStorage *storage) {
    Av2hFilterWorkspaceLayout layout;
    Av2hArena arena;
    unsigned int plane;
    Av2hStatus status;
    if (workspace == 0 || storage == 0) return AV2H_INVALID_ARGUMENT;
    status = av2h_filter_workspace_query(frame, &layout);
    if (status != AV2H_OK) return status;
    if (memory == 0 ||
        ((uintptr_t)memory & (layout.alignment - 1U)) != 0U) {
        return AV2H_INVALID_ARGUMENT;
    }
    if (memory_size_bytes < layout.size_bytes) return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(workspace, 0U, sizeof(*workspace));
    av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_memory_fill(memory, 0U, layout.size_bytes);
    av2h_arena_init(&arena, memory, layout.size_bytes);
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        workspace->deblocked.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        workspace->cdef.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        storage->deblock_cells[plane] = av2h_arena_allocate(
            &arena,
            layout.deblock_cell_count[plane] * sizeof(Av2hDeblockCell),
            _Alignof(Av2hDeblockCell));
        workspace->deblock_map.plane[plane] = storage->deblock_cells[plane];
        workspace->deblock_map.stride_cells[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.columns[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.rows[plane] = layout.deblock_rows[plane];
    }
    workspace->ccso_controls.columns = layout.ccso_columns;
    workspace->ccso_controls.rows = layout.ccso_rows;
    for (plane = 0U; plane < av2h_frame_plane_count(frame); ++plane) {
        storage->ccso_controls[plane] = av2h_arena_allocate(
            &arena, layout.ccso_control_count, _Alignof(uint8_t));
        workspace->ccso_controls.plane[plane] = storage->ccso_controls[plane];
        workspace->ccso_controls.stride_units[plane] = layout.ccso_columns;
    }
    if (arena.status != AV2H_OK ||
        av2h_arena_required(&arena) > layout.size_bytes) {
        return arena.status != AV2H_OK ? arena.status : AV2H_OVERFLOW;
    }
    return AV2H_OK;
}
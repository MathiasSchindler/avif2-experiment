#include "decoder/filter.h"
#include "decoder/internal/filter_geometry.h"

static int workspace_reserve(size_t *size,
                             size_t count,
                             size_t element_size,
                             size_t alignment) {
    size_t bytes;
    size_t aligned;
    if (!av2h_size_multiply(count, element_size, &bytes) ||
        !av2h_size_align(*size, alignment, &aligned) ||
        !av2h_size_add(aligned, bytes, size)) {
        return 0;
    }
    return 1;
}

Av2hStatus av2h_filter_workspace_query(
    const Av2hFrameState *frame,
    Av2hFilterWorkspaceLayout *layout) {
    size_t coded_width;
    size_t coded_height;
    size_t size = 0U;
    unsigned int stage;
    unsigned int plane;
    if (frame == 0 || layout == 0 || frame->width == 0U ||
        frame->height == 0U) {
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
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        size_t width = coded_width >> (plane == 0U ? 0U : 1U);
        size_t height = coded_height >> (plane == 0U ? 0U : 1U);
        size_t grid_size = plane == 0U ? 4U : 2U;
        if (width > UINT16_MAX || height > UINT16_MAX ||
            !av2h_size_multiply(
                width, height, &layout->stage_plane_bytes[plane]) ||
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
        for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
            if (!workspace_reserve(
                    &size, layout->stage_plane_bytes[plane],
                    sizeof(uint8_t), _Alignof(uint8_t))) {
                return AV2H_OVERFLOW;
            }
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (!workspace_reserve(
                &size, layout->deblock_cell_count[plane],
                sizeof(Av2hDeblockCell), _Alignof(Av2hDeblockCell))) {
            return AV2H_OVERFLOW;
        }
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        if (!workspace_reserve(
                &size, layout->ccso_control_count,
                sizeof(uint8_t), _Alignof(uint8_t))) {
            return AV2H_OVERFLOW;
        }
    }
    layout->size = size;
    return AV2H_OK;
}

Av2hStatus av2h_filter_workspace_init(
    const Av2hFrameState *frame,
    void *memory,
    size_t memory_size,
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
    if (memory_size < layout.size) return AV2H_OUT_OF_MEMORY;
    av2h_memory_fill(workspace, 0U, sizeof(*workspace));
    av2h_memory_fill(storage, 0U, sizeof(*storage));
    av2h_memory_fill(memory, 0U, layout.size);
    av2h_arena_init(&arena, memory, layout.size);
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        workspace->deblocked.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        workspace->cdef.plane[plane] = (Av2hFilterPlane){
            av2h_arena_allocate(
                &arena, layout.stage_plane_bytes[plane],
                _Alignof(uint8_t)),
            layout.plane_width[plane],
            layout.plane_width[plane],
            layout.plane_height[plane]
        };
    }
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        storage->deblock_cells[plane] = av2h_arena_allocate(
            &arena,
            layout.deblock_cell_count[plane] * sizeof(Av2hDeblockCell),
            _Alignof(Av2hDeblockCell));
        workspace->deblock_map.plane[plane] = storage->deblock_cells[plane];
        workspace->deblock_map.stride[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.columns[plane] = layout.deblock_columns[plane];
        workspace->deblock_map.rows[plane] = layout.deblock_rows[plane];
    }
    workspace->ccso_controls.columns = layout.ccso_columns;
    workspace->ccso_controls.rows = layout.ccso_rows;
    for (plane = 0U; plane < AV2H_FRAME_PLANE_COUNT; ++plane) {
        storage->ccso_controls[plane] = av2h_arena_allocate(
            &arena, layout.ccso_control_count, _Alignof(uint8_t));
        workspace->ccso_controls.plane[plane] = storage->ccso_controls[plane];
        workspace->ccso_controls.stride[plane] = layout.ccso_columns;
    }
    if (arena.status != AV2H_OK ||
        av2h_arena_required(&arena) > layout.size) {
        return arena.status != AV2H_OK ? arena.status : AV2H_OVERFLOW;
    }
    return AV2H_OK;
}
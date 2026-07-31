#ifndef AV2H_DECODER_FILTER_H
#define AV2H_DECODER_FILTER_H

#include "frame.h"

#define AV2H_FILTER_STAGE_COUNT 5U

extern uint64_t av2h_debug_filter_stage_hash[
    AV2H_FILTER_STAGE_COUNT][AV2H_FRAME_PLANE_COUNT];

typedef struct {
    uint8_t *data;
    size_t stride;
    uint16_t width;
    uint16_t height;
} Av2hFilterPlane;

typedef struct {
    Av2hFilterPlane plane[AV2H_FRAME_PLANE_COUNT];
} Av2hFilterPlanes;

extern Av2hFilterPlanes av2h_debug_filter_stage_planes[4U];

typedef struct {
    const uint8_t *plane[AV2H_FRAME_PLANE_COUNT];
    size_t stride[AV2H_FRAME_PLANE_COUNT];
    size_t columns;
    size_t rows;
} Av2hCcsoControlMap;

typedef struct {
    const uint8_t *strengths;
    size_t columns;
    size_t rows;
} Av2hCdefControlMap;

typedef struct {
    uint16_t block_column;
    uint16_t block_row;
    uint16_t transform_column;
    uint16_t transform_row;
    uint8_t transform_width;
    uint8_t transform_height;
    uint8_t q_index;
    uint8_t skip;
    uint8_t lossless;
} Av2hDeblockCell;

typedef struct {
    const Av2hDeblockCell *plane[AV2H_FRAME_PLANE_COUNT];
    size_t stride[AV2H_FRAME_PLANE_COUNT];
    size_t columns[AV2H_FRAME_PLANE_COUNT];
    size_t rows[AV2H_FRAME_PLANE_COUNT];
} Av2hDeblockMap;

typedef struct {
    Av2hFilterPlanes deblocked;
    Av2hFilterPlanes cdef;
    Av2hDeblockMap deblock_map;
    Av2hCdefControlMap cdef_controls;
    Av2hCcsoControlMap ccso_controls;
} Av2hFilterWorkspace;

typedef struct {
    uint16_t plane_width[AV2H_FRAME_PLANE_COUNT];
    uint16_t plane_height[AV2H_FRAME_PLANE_COUNT];
    size_t stage_plane_bytes[AV2H_FRAME_PLANE_COUNT];
    size_t deblock_columns[AV2H_FRAME_PLANE_COUNT];
    size_t deblock_rows[AV2H_FRAME_PLANE_COUNT];
    size_t deblock_cell_count[AV2H_FRAME_PLANE_COUNT];
    size_t ccso_columns;
    size_t ccso_rows;
    size_t ccso_control_count;
    size_t size;
    size_t alignment;
} Av2hFilterWorkspaceLayout;

typedef struct {
    Av2hDeblockCell *deblock_cells[AV2H_FRAME_PLANE_COUNT];
    uint8_t *ccso_controls[AV2H_FRAME_PLANE_COUNT];
} Av2hFilterWorkspaceStorage;

Av2hStatus av2h_filter_workspace_query(
    const Av2hFrameState *frame,
    Av2hFilterWorkspaceLayout *layout);
Av2hStatus av2h_filter_workspace_init(
    const Av2hFrameState *frame,
    void *memory,
    size_t memory_size,
    Av2hFilterWorkspace *workspace,
    Av2hFilterWorkspaceStorage *storage);

Av2hStatus av2h_deblock_edge(
    Av2hFilterPlane *plane,
    uint16_t x,
    uint16_t y,
    uint8_t vertical,
    uint8_t negative_length,
    uint8_t positive_length,
    uint16_t q_threshold,
    uint16_t side_threshold);
Av2hStatus av2h_deblock_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hDeblockMap *map);

Av2hStatus av2h_cdef_find_direction(
    const Av2hFilterPlane *luma,
    uint16_t x,
    uint16_t y,
    uint8_t *direction,
    uint32_t *variance);
Av2hStatus av2h_cdef_filter_block(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *source,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    unsigned int plane_index,
    uint8_t primary_strength,
    uint8_t secondary_strength,
    uint8_t damping,
    uint8_t direction,
    uint32_t variance);
Av2hStatus av2h_cdef_filter_frame(
    Av2hFilterPlanes *destination,
    const Av2hFilterPlanes *deblocked,
    const Av2hFrameState *frame);

Av2hStatus av2h_cdef_filter_frame_with_controls(
    Av2hFilterPlanes *destination,
    const Av2hFilterPlanes *deblocked,
    const Av2hFrameState *frame,
    const Av2hCdefControlMap *controls);

Av2hStatus av2h_gdf_filter_frame(
    Av2hFilterPlane *destination,
    const Av2hFilterPlane *cdef_luma,
    const Av2hFilterPlane *deblocked_luma,
    const Av2hFrameState *frame);

Av2hStatus av2h_ccso_filter_frame(
    Av2hFilterPlanes *planes,
    const uint8_t *pre_cdef_luma,
    size_t pre_cdef_luma_stride,
    const Av2hFrameState *frame,
    const Av2hCcsoControlMap *controls);

Av2hStatus av2h_filter_frame(
    Av2hFilterPlanes *planes,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFilterWorkspace *workspace);

#endif
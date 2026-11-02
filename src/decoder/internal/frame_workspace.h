#ifndef AV2H_DECODER_INTERNAL_FRAME_WORKSPACE_H
#define AV2H_DECODER_INTERNAL_FRAME_WORKSPACE_H

#include "decoder/frame_output.h"

typedef struct {
    size_t size_bytes;
    size_t alignment;
} Av2hFrameDecodeWorkspaceLayout;

Av2hStatus av2h_frame_decode_workspace_query(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspaceLayout *layout);
Av2hStatus av2h_frame_decode_workspace_query_bytes(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    Av2hFrameDecodeWorkspaceLayout *layout);

Av2hStatus av2h_frame_decode_workspace_init(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hFrameDecodeWorkspaceLayout *layout,
    void *memory,
    size_t memory_size_bytes,
    Av2hFrameDecodeWorkspace *workspace,
    Av2hFilterWorkspace *filter_workspace,
    Av2hFilterWorkspaceStorage *filter_storage);
Av2hStatus av2h_frame_decode_workspace_init_bytes(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    const Av2hFrameDecodeWorkspaceLayout *layout,
    void *memory,
    size_t memory_size_bytes,
    Av2hFrameDecodeWorkspace *workspace,
    Av2hFilterWorkspace *filter_workspace,
    Av2hFilterWorkspaceStorage *filter_storage);

Av2hStatus av2h_planar_output_bind_u16(
    const Av2hFrameState *frame,
    void *output,
    size_t output_size_bytes,
    Av2hPlanarOutputU16 *planes);

Av2hStatus av2h_copy_visible_planar_u16_private(
    const Av2hFrameState *frame,
    const Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT],
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes);
Av2hStatus av2h_copy_visible_planar_u16_with_execution_private(
    const Av2hFrameState *frame,
    const Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT],
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes,
    const Av2hExecution *execution);

Av2hStatus av2h_frame_output_validate_u8(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes);
Av2hStatus av2h_frame_output_validate_u16(
    const Av2hFrameState *frame,
    const Av2hReconstructionPlaneU16 coded[AV2H_FRAME_PLANE_COUNT],
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes);

Av2hStatus av2h_frame_workspace_prepare(
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t required_output_bytes,
    const Av2hFrameDecodeResult *result,
    Av2hFilterWorkspaceLayout *layout);

int av2h_frame_workspace_complete(
    const Av2hFrameState *frame,
    const Av2hFrameDecodeWorkspace *workspace);

#endif

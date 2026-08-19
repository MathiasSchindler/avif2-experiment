#ifndef AV2H_DECODER_FRAME_OUTPUT_H
#define AV2H_DECODER_FRAME_OUTPUT_H

#include "filter.h"
#include "tile.h"
#include "internal/decode_diagnostics.h"
#include "runtime/execution.h"

typedef struct {
    Av2hFilterPlaneU16 plane[AV2H_FRAME_PLANE_COUNT];
} Av2hPlanarOutputU16;

typedef struct {
    Av2hReconstructionPlane coded[AV2H_FRAME_PLANE_COUNT];
    Av2hReconstructionPlaneU16 coded_u16[AV2H_FRAME_PLANE_COUNT];
    size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT];
    Av2hTileContextStorage tile_context;
    Av2hReconstructionScratch *reconstruction_scratch;
    Av2hReconstructionScratchU16 *reconstruction_scratch_u16;
    Av2hFilterWorkspace *filter_workspace;
    Av2hPlanarOutputU16 output_u16;
    Av2hDecodeDiagnostics diagnostics_storage;
    Av2hDecodeDiagnostics *diagnostics;
    uint8_t bytes_per_sample;
} Av2hFrameDecodeWorkspace;

typedef enum {
    AV2H_FRAME_DECODE_STAGE_NONE = 0,
    AV2H_FRAME_DECODE_STAGE_INITIALIZE,
    AV2H_FRAME_DECODE_STAGE_LUMA_PARSE,
    AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT,
    AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK,
    AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT,
    AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE,
    AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM,
    AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT,
    AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK,
    AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT,
    AV2H_FRAME_DECODE_STAGE_COMPLETENESS,
    AV2H_FRAME_DECODE_STAGE_FILTER,
    AV2H_FRAME_DECODE_STAGE_OUTPUT
} Av2hFrameDecodeStage;

typedef struct {
    size_t luma_leaf_count;
    size_t chroma_leaf_count;
    size_t entropy_tell;
    Av2hStatus cause;
    Av2hFrameDecodeStage stage;
    uint16_t row;
    uint16_t column;
    uint8_t block_size;
    uint8_t plane;
    uint8_t syntax_stage;
    uint8_t prefix_stage;
    uint8_t txb_skip_context;
    uint8_t all_zero;
    uint8_t eob_point;
    uint8_t mode;
    int8_t angle_delta;
    uint8_t mrl_index;
    uint8_t multi_line_mrl;
    uint8_t enable_ibp;
    uint8_t use_intrabc;
    int16_t displacement_x;
    int16_t displacement_y;
    uint8_t morph_pred;
    uint8_t fsc_mode;
    uint8_t skip_transform;
    uint8_t palette_size;
    uint8_t tx_size;
    uint8_t inverse_transform_stage;
    uint16_t intrabc_source_x;
    uint16_t intrabc_source_y;
    size_t intrabc_map_index;
    uint8_t primary_type;
    uint8_t primary_symbol;
    uint8_t primary_set_symbol;
    uint8_t secondary_symbol;
    uint8_t secondary_set;
    uint8_t cctx_type;
    uint8_t is_cfl;
    uint8_t cfl_kind;
    uint8_t mhccp_direction;
    uint8_t cfl_filter_index;
    uint8_t mhccp_reason;
    uint8_t chroma_parse_reason;
    uint8_t chroma_mode_index;
    uint8_t filter_reason;
    uint16_t eob;
    uint16_t u_coefficient_count;
    uint16_t v_coefficient_count;
    size_t after_all_zero_tell;
    size_t after_eob_point_tell;
    uint32_t eob_range;
    uint32_t skip_range;
    uint32_t before_prefix_range;
    uint8_t eob_tx_size;
    uint8_t eob_class;
    uint16_t coefficient_position;
    uint8_t coefficient_context;
    uint8_t coefficient_kind;
    const Av2hDecodeDiagnostics *diagnostics;
} Av2hFrameDecodeResult;

Av2hStatus av2h_planar_output_size(
    const Av2hFrameState *frame,
    size_t *size_bytes);
Av2hStatus av2h_planar_output_size_bytes(
    const Av2hFrameState *frame,
    size_t bytes_per_sample,
    size_t *size_bytes);

Av2hStatus av2h_deblock_map_commit_transform(
    Av2hDeblockMap *map,
    uint8_t chroma_format_idc,
    unsigned int plane,
    const Av2hPartitionNode *leaf,
    const Av2hFirstCoeffPrefix *prefix,
    uint8_t skip,
    uint8_t all_zero,
    uint8_t lossless);

Av2hStatus av2h_copy_visible_planar_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes);
Av2hStatus av2h_copy_visible_planar_with_execution_private(
    const Av2hFrameState *frame,
    const Av2hFilterPlanes *coded,
    const size_t coded_data_size_bytes[AV2H_FRAME_PLANE_COUNT],
    uint8_t *output,
    size_t output_size_bytes,
    const Av2hExecution *execution);

Av2hStatus av2h_decode_frame_planar_private(
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result);
Av2hStatus av2h_decode_frame_planar_with_execution_private(
    Av2hSpan entropy,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result,
    const Av2hExecution *execution);
Av2hStatus av2h_decode_frame_group_planar_private(
    const Av2hTileGroup *group,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result);
Av2hStatus av2h_decode_frame_group_planar_with_execution_private(
    const Av2hTileGroup *group,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    Av2hFrameDecodeWorkspace *workspace,
    uint8_t *output,
    size_t output_size_bytes,
    Av2hFrameDecodeResult *result,
    const Av2hExecution *execution);

#endif

#ifndef AV2H_DECODER_INTERNAL_DECODE_DIAGNOSTICS_H
#define AV2H_DECODER_INTERNAL_DECODE_DIAGNOSTICS_H

#include "base.h"

typedef struct {
    uint32_t luma_stage_ranges[5U];
    uint8_t luma_tile_stage;
    uint8_t chroma_tile_stage;
    uint8_t chroma_parse_reason;
} Av2hTileDiagnostics;

typedef struct {
    uint8_t luma_mode_stage;
    uint8_t chroma_mode_stage;
} Av2hLeafDiagnostics;

typedef struct {
    uint32_t eob_range;
    uint32_t skip_range;
    uint8_t eob_tx;
    uint8_t eob_class;
    uint8_t luma_prefix_stage;
} Av2hCoefficientDiagnostics;

typedef struct {
    uint8_t transform_stage;
} Av2hTransformDiagnostics;

typedef struct {
    uint8_t luma_stage;
    uint8_t chroma_stage;
    uint8_t inverse_transform_stage;
    uint16_t intrabc_source_x;
    uint16_t intrabc_source_y;
    size_t intrabc_map_index;
} Av2hReconstructionDiagnostics;

typedef struct {
    uint8_t mhccp_reason;
} Av2hPredictionDiagnostics;

const Av2hTileDiagnostics *av2h_tile_diagnostics_get(void);
void av2h_tile_diagnostics_set_luma_range(size_t index, uint32_t value);
void av2h_tile_diagnostics_set_luma_stage(uint8_t value);
void av2h_tile_diagnostics_set_chroma_stage(uint8_t value);
void av2h_tile_diagnostics_set_chroma_reason(uint8_t value);

const Av2hLeafDiagnostics *av2h_leaf_diagnostics_get(void);
void av2h_leaf_diagnostics_set_luma_stage(uint8_t value);
void av2h_leaf_diagnostics_set_chroma_stage(uint8_t value);

const Av2hCoefficientDiagnostics *av2h_coefficient_diagnostics_get(void);
void av2h_coefficient_diagnostics_set_eob(
    uint32_t range, uint8_t tx_size, uint8_t eob_class);
void av2h_coefficient_diagnostics_set_skip_range(uint32_t value);
void av2h_coefficient_diagnostics_set_luma_prefix_stage(uint8_t value);

const Av2hTransformDiagnostics *av2h_transform_diagnostics_get(void);
void av2h_transform_diagnostics_set_stage(uint8_t value);

const Av2hReconstructionDiagnostics *av2h_reconstruction_diagnostics_get(
    void);
void av2h_reconstruction_diagnostics_set_luma_stage(uint8_t value);
void av2h_reconstruction_diagnostics_set_chroma_stage(uint8_t value);
void av2h_reconstruction_diagnostics_set_inverse_transform_stage(
    uint8_t value);
void av2h_reconstruction_diagnostics_set_intrabc_source(
    uint16_t x, uint16_t y, size_t map_index);

const Av2hPredictionDiagnostics *av2h_prediction_diagnostics_get(void);
void av2h_prediction_diagnostics_set_mhccp_reason(uint8_t value);

#endif
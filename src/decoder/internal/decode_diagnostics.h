#ifndef AV2H_DECODER_INTERNAL_DECODE_DIAGNOSTICS_H
#define AV2H_DECODER_INTERNAL_DECODE_DIAGNOSTICS_H

#include "base.h"

#define AV2H_PARTITION_DIAGNOSTIC_CAPACITY 131072U
#define AV2H_PARTITION_LEAF_DIAGNOSTIC_CAPACITY 512U

typedef struct Av2hPartitionDecision Av2hPartitionDecision;

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

typedef struct {
    uint8_t reason;
} Av2hFilterDiagnostics;

typedef struct Av2hPartitionLeafDiagnostic {
    uint8_t stage;
    uint8_t mode;
    uint16_t eob;
    size_t tell;
} Av2hPartitionLeafDiagnostic;

typedef struct Av2hDecodeDiagnostics {
    Av2hTileDiagnostics tile;
    Av2hLeafDiagnostics leaf;
    Av2hCoefficientDiagnostics coefficient;
    Av2hTransformDiagnostics transform;
    Av2hReconstructionDiagnostics reconstruction;
    Av2hPredictionDiagnostics prediction;
    Av2hFilterDiagnostics filter;
    Av2hPartitionDecision *partition_trace;
    uint8_t *partition_trace_structure;
    size_t partition_trace_capacity;
    size_t partition_trace_count;
    Av2hPartitionLeafDiagnostic *partition_leaf_trace;
    size_t partition_leaf_trace_capacity;
    size_t partition_leaf_trace_count;
    uint16_t coefficient_level_cdf[4U];
    uint64_t coefficient_level_range[4U];
    int16_t intrabc_candidates[8U];
    size_t intrabc_candidate_count;
    int16_t intrabc_bank[8U];
    size_t intrabc_bank_count;
} Av2hDecodeDiagnostics;

const Av2hTileDiagnostics *av2h_tile_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_tile_diagnostics_set_luma_range(
    Av2hDecodeDiagnostics *diagnostics, size_t index, uint32_t value);
void av2h_tile_diagnostics_set_luma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_tile_diagnostics_set_chroma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_tile_diagnostics_set_chroma_reason(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);

const Av2hLeafDiagnostics *av2h_leaf_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_leaf_diagnostics_set_luma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_leaf_diagnostics_set_chroma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);

const Av2hCoefficientDiagnostics *av2h_coefficient_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_coefficient_diagnostics_set_eob(
    Av2hDecodeDiagnostics *diagnostics, uint32_t range,
    uint8_t tx_size, uint8_t eob_class);
void av2h_coefficient_diagnostics_set_skip_range(
    Av2hDecodeDiagnostics *diagnostics, uint32_t value);
void av2h_coefficient_diagnostics_set_luma_prefix_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);

const Av2hTransformDiagnostics *av2h_transform_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_transform_diagnostics_set_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);

const Av2hReconstructionDiagnostics *av2h_reconstruction_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_reconstruction_diagnostics_set_luma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_reconstruction_diagnostics_set_chroma_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_reconstruction_diagnostics_set_inverse_transform_stage(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);
void av2h_reconstruction_diagnostics_set_intrabc_source(
    Av2hDecodeDiagnostics *diagnostics,
    uint16_t x, uint16_t y, size_t map_index);

const Av2hPredictionDiagnostics *av2h_prediction_diagnostics_get(
    const Av2hDecodeDiagnostics *diagnostics);
void av2h_prediction_diagnostics_set_mhccp_reason(
    Av2hDecodeDiagnostics *diagnostics, uint8_t value);

#endif
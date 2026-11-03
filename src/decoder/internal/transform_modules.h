#ifndef AV2H_DECODER_INTERNAL_TRANSFORM_MODULES_H
#define AV2H_DECODER_INTERNAL_TRANSFORM_MODULES_H

#include "decoder/leaf_mode.h"
#include "decoder/transform_type.h"
#include "decoder/tx_geometry.h"

Av2hIntraMode av2h_transform_mode(
    Av2hIntraMode mode, int8_t angle_delta,
    uint8_t mrl_index,
    const Av2hTxDescriptor *descriptor);
Av2hStatus av2h_chroma_primary_type_map(
    const Av2hChromaLeafMode *mode,
    const Av2hTxDescriptor *descriptor,
    uint8_t reduced_tx_set,
    Av2hTxType *tx_type);

Av2hStatus av2h_secondary_tx_set_map(
    Av2hIntraMode mode, uint8_t symbol, uint8_t *set);
Av2hStatus av2h_secondary_adst_tx_set_map(
    Av2hIntraMode mode, uint8_t symbol, uint8_t *set);

#endif
#ifndef AV2H_TESTS_DECODER_TEST_HELPERS_H
#define AV2H_TESTS_DECODER_TEST_HELPERS_H

#include "decoder/coeff.h"
#include "decoder/leaf.h"

#include <stddef.h>

static inline Av2hStatus test_first_leaf_mode_read(
    Av2hRangeDecoder *decoder,
    Av2hLeafCdfs *cdfs,
    const Av2hSequenceState *sequence,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    Av2hLeafMode *mode) {
    const Av2hLumaModeNeighbors neighbors = { 0 };
    if (leaf == NULL) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_luma_mode_read(
        decoder, cdfs, sequence, frame, leaf, &neighbors, mode);
}

static inline Av2hStatus test_first_coeff_prefix_read(
    Av2hRangeDecoder *decoder,
    Av2hCoeffCdfs *cdfs,
    const Av2hFrameState *frame,
    const Av2hPartitionNode *leaf,
    const Av2hLeafMode *mode,
    Av2hCoeffPrefix *prefix) {
    if (leaf == NULL) return AV2H_INVALID_ARGUMENT;
    if (leaf->row != 0U || leaf->column != 0U) return AV2H_UNSUPPORTED;
    return av2h_coeff_prefix_read(
        decoder, cdfs, frame, leaf, mode, 0U, prefix);
}

#endif

#ifndef AV2H_DECODE_H
#define AV2H_DECODE_H

#include "decoder/internal/frame_workspace.h"

typedef struct {
    Av2hSequenceState sequence;
    Av2hFrameState frame;
    Av2hTileGroup group;
    Av2hFrameDecodeWorkspaceLayout workspace;
    size_t output_size;
} Av2hDecodePreparation;

Av2hStatus av2h_decode_prepare(const Av2hHeifImage *image, Av2hDecodePreparation *preparation,
                               Av2hError *error);

#endif
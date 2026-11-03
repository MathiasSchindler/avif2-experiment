#ifndef AV2H_DECODE_H
#define AV2H_DECODE_H

#include "decoder/internal/frame_workspace.h"

Av2hStatus av2h_decode_prepare(const Av2hHeifImage *image, Av2hDecodePreparation *preparation,
                               Av2hFrameSyntaxStorage *storage, Av2hTileGroup *group,
                               Av2hError *error);

#endif
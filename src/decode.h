#ifndef AV2H_DECODE_H
#define AV2H_DECODE_H

#include "decoder/internal/frame_workspace.h"
#include "decoder/qmatrix.h"

/* Borrowed syntax references are valid only while the validated input stays immutable. */
typedef struct {
    Av2hFrameSyntaxReferences frame;
    Av2hQMatrixSyntaxReferences qmatrix;
    Av2hObu frame_obu;
    size_t payload_obu_count;
} Av2hDecodeSyntaxReferences;

Av2hStatus av2h_decode_prepare(const Av2hHeifImage *image, Av2hDecodePreparation *preparation,
                               Av2hFrameSyntaxStorage *storage, Av2hTileGroup *group,
                               Av2hDecodeSyntaxReferences *references, Av2hError *error);
Av2hStatus av2h_decode_materialize(const Av2hHeifImage *image, Av2hDecodePreparation *preparation,
                                   const Av2hDecodeSyntaxReferences *references,
                                   Av2hFrameSyntaxStorage *storage, Av2hTileGroup *group,
                                   Av2hError *error);
Av2hStatus av2h_query_with_preparation(Av2hSpan input, const Av2hLimits *limits, Av2hInfo *info,
                                       Av2hDecodePreparation *preparation,
                                       Av2hDecodeSyntaxReferences *references, Av2hError *error);

#endif
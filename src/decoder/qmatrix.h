#ifndef AV2H_DECODER_QMATRIX_H
#define AV2H_DECODER_QMATRIX_H

#include "frame.h"

typedef struct {
    Av2hSpan payload;
    size_t bit_position;
    size_t byte_offset;
} Av2hQMatrixSyntaxReference;

typedef struct {
    Av2hQMatrixSyntaxReference definition[AV2H_CUSTOM_QM_COUNT];
} Av2hQMatrixSyntaxReferences;

void av2h_qmatrix_state_reset(Av2hFrameState *frame);
Av2hStatus av2h_qmatrix_obu_parse(Av2hSpan input, const Av2hObu *obu, Av2hFrameState *frame,
                                  Av2hQMatrixSyntaxReferences *references, Av2hError *error);
Av2hStatus av2h_qmatrix_ledger_parse(Av2hSpan input, const Av2hObuLedger *ledger,
                                     Av2hFrameState *frame, Av2hQMatrixSyntaxReferences *references,
                                     Av2hError *error);
Av2hStatus av2h_qmatrix_materialize(Av2hFrameState *frame,
                                    const Av2hQMatrixSyntaxReferences *references,
                                    Av2hError *error);
Av2hStatus av2h_qmatrix_references_validate(const Av2hFrameState *frame);

#endif
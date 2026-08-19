#ifndef AV2H_DECODER_QMATRIX_H
#define AV2H_DECODER_QMATRIX_H

#include "frame.h"

void av2h_qmatrix_state_reset(Av2hFrameState *frame);
Av2hStatus av2h_qmatrix_obu_parse(
    Av2hSpan input,
    const Av2hObu *obu,
    Av2hFrameState *frame,
    Av2hError *error);
Av2hStatus av2h_qmatrix_ledger_parse(
    Av2hSpan input,
    const Av2hObuLedger *ledger,
    Av2hFrameState *frame,
    Av2hError *error);
Av2hStatus av2h_qmatrix_references_validate(
    const Av2hFrameState *frame);

#endif
#ifndef AV2H_CARRIAGE_H
#define AV2H_CARRIAGE_H

#include "av2heif.h"

Av2hStatus av2h_carriage_validate_metadata(Av2hSpan config_obus, Av2hSpan item_payload,
                                           Av2hSequenceInfo *sequence, Av2hColorInfo *color,
                                           uint8_t *color_present, Av2hError *error);

#endif

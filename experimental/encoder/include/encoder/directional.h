#ifndef EXPERIMENTAL_ENCODER_DIRECTIONAL_H
#define EXPERIMENTAL_ENCODER_DIRECTIONAL_H

#include "encoder/base.h"

#define ENC_DIRECTIONAL_FIRST_TOKEN 9U
#define ENC_DIRECTIONAL_LAST_TOKEN 64U

int enc_directional_token_to_joint(uint8_t token, uint8_t *joint_mode);
int enc_directional_joint_to_mode(
    uint8_t joint_mode, uint8_t *mode, int8_t *angle_delta);
int enc_directional_joint_is_directional(uint8_t joint_mode);
int enc_directional_predict(
    uint8_t token, unsigned int plane,
    const uint8_t *above, size_t above_count,
    const uint8_t *left, size_t left_count, uint8_t corner,
    size_t width, size_t height, uint8_t *output);

#endif

#ifndef OPENTEC_MOTOR_CENTER_H
#define OPENTEC_MOTOR_CENTER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t requested;
    int32_t encoder_offset;
    bool active;
} MotorCenterState;

bool motor_center_command_apply(MotorCenterState *state, int16_t requested, int32_t encoder_modulus,
                                uint16_t encoder_counter, uint16_t wrap_threshold);

#endif

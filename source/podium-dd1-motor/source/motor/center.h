#ifndef OPENTEC_MOTOR_CENTER_H
#define OPENTEC_MOTOR_CENTER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t requested;
    bool active;
} MotorCenterState;

bool motor_center_command_apply(MotorCenterState *state, int16_t requested, int32_t encoder_modulus,
                                const volatile uint32_t *encoder_counter, uint16_t wrap_threshold,
                                int32_t *encoder_offset);
int32_t motor_centered_position_resolve(int32_t position, int16_t center);

#endif

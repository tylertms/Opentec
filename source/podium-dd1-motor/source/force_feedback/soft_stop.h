#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_SOFT_STOP_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_SOFT_STOP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t previous_half_range;
    uint32_t next_ramp_tick;
    uint8_t ramp_percent;
} MotorForceFeedbackSoftStop;

bool motor_force_feedback_soft_stop_apply(MotorForceFeedbackSoftStop *soft_stop, uint32_t now,
                                          int32_t half_range, int32_t center, int32_t position,
                                          uint16_t transition_range, bool disabled, int32_t *force);

#endif

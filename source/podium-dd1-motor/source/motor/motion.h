#ifndef OPENTEC_MOTOR_MOTION_H
#define OPENTEC_MOTOR_MOTION_H

#include <stdint.h>

typedef struct {
    int32_t accumulator;
    uint16_t shift;
} MotorMotionFilter;

typedef struct {
    uint32_t previous_counter;
    int16_t previous_filtered_delta;
} MotorMotionState;

typedef struct {
    int16_t position_delta;
    int16_t filtered_position_delta;
    int16_t velocity_delta;
    int16_t filtered_velocity_delta;
} MotorMotionSample;

int16_t motor_q15_scale_saturate(uint32_t scale, int16_t value);
int16_t motor_q15_scale_wrap(uint32_t scale, int16_t value);
int16_t motor_signed_difference_saturate(int16_t value, int16_t previous);
int16_t motor_motion_filter_step(MotorMotionFilter *filter, int16_t sample);
int16_t motor_encoder_delta_scale(MotorMotionState *state, uint32_t counter, uint32_t scale);
int16_t motor_velocity_delta_scale(MotorMotionState *state, int16_t filtered_delta, uint32_t scale);
MotorMotionSample motor_motion_sample(MotorMotionState *state, MotorMotionFilter *position_filter,
                                      MotorMotionFilter *velocity_filter, uint32_t counter,
                                      uint32_t scale);

#endif

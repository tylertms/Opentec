#ifndef OPENTEC_MOTOR_ENCODER_H
#define OPENTEC_MOTOR_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    kMotorEncoderPositionPending,
    kMotorEncoderPositionUpdated,
    kMotorEncoderPositionOutOfRange,
} MotorEncoderPositionResult;

typedef struct {
    int32_t revolution_offset;
    int32_t position;
    int32_t zero_counter;
} MotorEncoderState;

void motor_encoder_overflow_apply(MotorEncoderState *state, int32_t modulus, bool increasing);
MotorEncoderPositionResult motor_encoder_position_update(MotorEncoderState *state,
                                                         bool overflow_pending, uint16_t counter,
                                                         int32_t position_limit);
void motor_encoder_position_reset(MotorEncoderState *state);

#endif

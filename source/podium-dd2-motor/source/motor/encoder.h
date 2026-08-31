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

typedef enum {
    kMotorEncoderDirectionBegin,
    kMotorEncoderDirectionFirstIndex,
    kMotorEncoderDirectionSecondIndex,
    kMotorEncoderDirectionReturn,
} MotorEncoderDirectionPhase;

typedef enum {
    kMotorEncoderDirectionPending,
    kMotorEncoderDirectionPassed,
    kMotorEncoderDirectionFailed,
} MotorEncoderDirectionResult;

typedef struct {
    MotorEncoderDirectionPhase phase;
    int32_t start_position;
    int32_t first_index_position;
    uint16_t status;
} MotorEncoderDirectionState;

typedef struct {
    MotorEncoderDirectionResult result;
    int16_t drive_current;
    uint16_t status;
    bool reset_controller;
    bool restart_index_seek;
} MotorEncoderDirectionStep;

typedef struct {
    int16_t drive_current;
    bool countdown_active;
    bool complete;
} MotorEncoderIndexSeekStep;

void motor_encoder_overflow_apply(MotorEncoderState *state, int32_t modulus, bool increasing);
MotorEncoderPositionResult motor_encoder_position_update(MotorEncoderState *state,
                                                         bool overflow_pending, uint16_t counter,
                                                         int32_t position_limit);
void motor_encoder_position_reset(MotorEncoderState *state);
uint16_t motor_encoder_relative_position(uint16_t counter, uint16_t zero_counter, uint16_t modulus);
MotorEncoderIndexSeekStep motor_encoder_index_seek_step(bool index_detected,
                                                        uint16_t timeout_remaining);
void motor_encoder_direction_initialize(MotorEncoderDirectionState *state);
MotorEncoderDirectionStep motor_encoder_direction_check_step(MotorEncoderDirectionState *state,
                                                             bool index_seek_complete,
                                                             int32_t position,
                                                             int32_t encoder_modulus);

#endif

#ifndef OPENTEC_MOTOR_ENCODER_CALIBRATION_H
#define OPENTEC_MOTOR_ENCODER_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/motion.h"

#define MOTOR_ENCODER_CORRECTION_CAPACITY 0x952U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t correction_scale;
    uint32_t sample_offset;
    int16_t forward[MOTOR_ENCODER_CORRECTION_CAPACITY];
    int16_t reverse[MOTOR_ENCODER_CORRECTION_CAPACITY];
} MotorEncoderCalibrationRecord;

typedef enum {
    kMotorEncoderCalibrationInitialize,
    kMotorEncoderCalibrationStartForward,
    kMotorEncoderCalibrationSettleForward,
    kMotorEncoderCalibrationCaptureForward,
    kMotorEncoderCalibrationSettleReverse,
    kMotorEncoderCalibrationCaptureReverse,
    kMotorEncoderCalibrationCenter,
} MotorEncoderCalibrationPhase;

typedef enum {
    kMotorEncoderCalibrationPending,
    kMotorEncoderCalibrationComplete,
} MotorEncoderCalibrationResult;

typedef struct {
    int16_t velocity;
    int16_t correction;
    int32_t position;
    uint16_t relative_position;
    uint16_t encoder_period;
    bool revolution_complete;
} MotorEncoderCalibrationInput;

typedef struct {
    MotorEncoderCalibrationResult result;
    int16_t target_velocity;
    bool reset_controller;
    bool arm_revolution;
    bool clear_revolution;
} MotorEncoderCalibrationStep;

typedef struct {
    MotorEncoderCalibrationPhase phase;
    uint32_t settle_count;
    int16_t velocity_lower;
    int16_t velocity_upper;
    uint16_t sweep_start_position;
    int16_t target_velocity;
    MotorMotionFilter correction_filter;
    MotorEncoderCalibrationRecord record;
} MotorEncoderCalibrationState;

void motor_encoder_calibration_initialize(MotorEncoderCalibrationState *state);
MotorEncoderCalibrationStep
motor_encoder_calibration_step(MotorEncoderCalibrationState *state,
                               const MotorEncoderCalibrationInput *input);
int16_t motor_encoder_correction_read(const MotorEncoderCalibrationRecord *record, bool reverse,
                                      uint16_t relative_position, uint16_t table_length);
bool motor_encoder_correction_direction_update(bool reverse, int16_t filtered_position_delta);
bool motor_encoder_calibration_record_is_valid(const MotorEncoderCalibrationRecord *record);

#endif

#ifndef OPENTEC_MOTOR_CALIBRATION_H
#define OPENTEC_MOTOR_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_CURRENT_CALIBRATION_SAMPLE_COUNT 1024U

typedef struct {
    int16_t phase_a;
    int16_t phase_b;
} MotorCurrentOffsets;

typedef enum {
    kMotorCurrentCalibrationIdle = 0,
    kMotorCurrentCalibrationPhaseA = 1,
    kMotorCurrentCalibrationPhaseB = 2,
    kMotorCurrentCalibrationComplete = 3,
} MotorCurrentCalibrationStage;

typedef enum {
    kMotorCurrentCalibrationPending,
    kMotorCurrentCalibrationPhaseBStarted,
    kMotorCurrentCalibrationFinished,
} MotorCurrentCalibrationResult;

typedef struct {
    MotorCurrentOffsets offsets;
    MotorCurrentCalibrationStage stage;
    uint16_t sample_count;
    uint32_t sample_sum;
} MotorCurrentCalibrationState;

void motor_current_calibration_start(MotorCurrentCalibrationState *state);
MotorCurrentCalibrationResult motor_current_calibration_step(MotorCurrentCalibrationState *state,
                                                             bool sample_ready, uint16_t sample);

#endif

#ifndef OPENTEC_MOTOR_DRIVE_H
#define OPENTEC_MOTOR_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_DRIVE_INTERPOLATION_SETTING_COUNT 20U

typedef struct {
    int16_t primary_current;
    int16_t secondary_current;
    uint16_t controller_coefficient;
    uint16_t controller_scale;
} MotorDriveCommand;

typedef struct {
    uint32_t accumulator;
    int16_t output;
    int16_t error;
} MotorDriveInterpolationState;

MotorDriveCommand motor_drive_command_resolve(bool positive, uint32_t primary, int32_t secondary,
                                              uint8_t normal_output_percent, bool full_torque,
                                              bool reduced_controller, bool secondary_disabled);
int16_t motor_drive_interpolation_step(MotorDriveInterpolationState *state, int16_t sample,
                                       uint8_t setting);

#endif

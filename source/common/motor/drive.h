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

typedef struct {
    int32_t anchor_position;
    int32_t previous_raw;
    uint32_t excursion_limit;
    uint32_t output_scale;
} MotorDriveFrictionState;

typedef struct {
    int16_t current_scale;
    int16_t target_scale;
    int16_t error;
} MotorDriveDeratingState;

MotorDriveCommand motor_drive_command_resolve(bool positive, uint32_t primary, int32_t secondary,
                                              uint8_t normal_output_percent, bool full_torque,
                                              bool reduced_controller, bool secondary_disabled);
int16_t motor_drive_interpolation_step(MotorDriveInterpolationState *state, int16_t sample,
                                       uint8_t setting);
int16_t motor_drive_motion_resistance_resolve(int16_t motion, uint8_t setting);
void motor_drive_friction_initialize(MotorDriveFrictionState *state, uint32_t hardware_scale);
int16_t motor_drive_friction_step(MotorDriveFrictionState *state, int32_t position,
                                  uint16_t setting);
void motor_drive_derating_initialize(MotorDriveDeratingState *state, int16_t normal_scale);
int16_t motor_drive_product_scale(MotorDriveDeratingState *state, int16_t current,
                                  uint16_t motor_temperature_sample, int16_t normal_scale,
                                  int16_t minimum_scale, bool minimum_mode);

#endif
